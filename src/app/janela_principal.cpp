#include "janela_principal.h"
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFontInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <set>
#include "ajuste_colunas.h"
#include "recursos_tabela.h"
#include "botao_combo.h"
#include "busca_deck.h"
#include "comparacao_deck.h"
#include "dados_deck.h"
#include "deck_newave.h"
#include "delegate_numerico.h"
#include "exportador_csv.h"
#include "filtro_usinas.h"
#include "formulario_usina.h"
#include "modelo_hidr.h"
#include "navegador_deck.h"
#include "painel_problemas.h"
#include "regras_gevazp.h"
#include "validacao.h"
#include "vazoes.h"
#include "vista_cascata.h"

namespace {
constexpr int LARGURA_MINIMA_ESQUERDA = 560;

std::filesystem::path paraPath(const QString& s) { return std::filesystem::path(s.toStdWString()); }

std::set<int> codigosMarcados(QMenu* menu) {
    std::set<int> marcados;
    for (QAction* acao : menu->actions()) {
        if (acao->data().isValid() && acao->isChecked()) marcados.insert(acao->data().toInt());
    }
    return marcados;
}
}

JanelaPrincipal::JanelaPrincipal(QWidget* parent) : QMainWindow(parent) {
    modelo_ = new ModeloHidr(this);
    filtro_ = new FiltroUsinas(this);
    filtro_->setSourceModel(modelo_);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setHandleWidth(4);
    criarTabela();
    formulario_ = new FormularioUsina(modelo_, splitter_);
    splitter_->addWidget(formulario_);
    campo_filtro_ = new QLineEdit(formulario_);
    campo_filtro_->setPlaceholderText(QStringLiteral("Filtrar por código ou nome"));
    campo_filtro_->setClearButtonEnabled(true);
    campo_filtro_->setMinimumWidth(160);
    campo_filtro_->setMaximumWidth(260);
    formulario_->adicionarAoCabecalho(campo_filtro_);
    connect(campo_filtro_, &QLineEdit::textChanged, filtro_, &FiltroUsinas::definirTexto);
    connect(campo_filtro_, &QLineEdit::textChanged, vista_cascata_, &VistaCascata::definirFiltro);
    botao_salvar_ = new QPushButton(QStringLiteral("Salvar"), formulario_);
    botao_salvar_->setEnabled(false);
    formulario_->adicionarAoCabecalho(botao_salvar_);
    connect(botao_salvar_, &QPushButton::clicked, this, &JanelaPrincipal::salvar);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({360, 480});
    navegador_ = new NavegadorDeck(splitter_, modelo_, this);
    setCentralWidget(navegador_);
    connect(tabela_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { formulario_->definirLinha(linhaSelecionada()); });

    painel_problemas_ = new PainelProblemas(this);
    addDockWidget(Qt::BottomDockWidgetArea, painel_problemas_);
    painel_problemas_->hide();
    connect(painel_problemas_, &PainelProblemas::problemaEscolhido, this, [this](int linha, const QString& campo) {
        selecionarLinha(linha);
        formulario_->focarCampo(campo.toStdString());
    });
    connect(modelo_, &ModeloHidr::usinaAlterada, this, [this](int linha, const Campo*) {
        if (linha < 0 || linha == formulario_->linha()) marcarProblemasDaLinha(formulario_->linha());
    });
    connect(tabela_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { marcarProblemasDaLinha(linhaSelecionada()); });
    connect(tabela_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { vista_cascata_->selecionar(linhaSelecionada()); });
    connect(vista_cascata_, &VistaCascata::usinaEscolhida, this, &JanelaPrincipal::selecionarLinha);
    connect(modelo_, &ModeloHidr::usinaAlterada, this, [this](int, const Campo* campo) {
        static constexpr std::array<std::string_view, 5> campos_cascata = {"nome", "jusante", "desvio", "subsistema", "regulacao"};
        bool afeta_cascata = !campo || std::find(campos_cascata.begin(), campos_cascata.end(), campo->nome) != campos_cascata.end();
        if (afeta_cascata) vista_cascata_->reconstruir();
    });

    criarMenus();

    statusBar()->setSizeGripEnabled(false);
    statusBar()->setContentsMargins(4, 0, 4, 0);

    status_arquivo_ = new QLabel(this);
    status_modelo_ = new QLabel(this);
    status_usinas_ = new QLabel(this);
    status_validacao_ = new QLabel(this);
    status_notas_ = new QLabel(this);
    statusBar()->addWidget(status_arquivo_, 1);
    statusBar()->addWidget(status_modelo_);
    statusBar()->addWidget(status_usinas_);
    statusBar()->addWidget(status_validacao_);
    statusBar()->addPermanentWidget(status_notas_);

    connect(modelo_->pilhaUndo(), &QUndoStack::cleanChanged, this, [this](bool) { atualizarTitulo(); });
    connect(modelo_, &ModeloHidr::usinaAlterada, this, [this](int, const Campo*) { atualizarStatus(); });

    resize(1400, 800);
    atualizarTitulo();
    atualizarStatus();
}

void JanelaPrincipal::criarTabela() {
    auto* painel = new QWidget(splitter_);
    auto* layout = new QVBoxLayout(painel);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    auto* abas_esquerda = new QTabWidget(painel);

    tabela_ = new QTableView(abas_esquerda);
    tabela_->setModel(filtro_);
    tabela_->setItemDelegate(new DelegateNumerico(modelo_, tabela_));
    tabela_->sortByColumn(0, Qt::AscendingOrder);
    tabela_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tabela_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    tabela_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tabela_->horizontalHeader()->setDefaultSectionSize(90);
    tabela_->horizontalHeader()->setFixedHeight(22);
    tabela_->verticalHeader()->setVisible(false);
    tabela_->verticalHeader()->setDefaultSectionSize(20);
    preencherLargura(tabela_);
    habilitarRecursos(tabela_, true);
    tabela_->setAlternatingRowColors(true);
    abas_esquerda->addTab(tabela_, QStringLiteral("Tabela"));

    auto* painel_cascata = new QWidget(abas_esquerda);
    auto* layout_cascata = new QVBoxLayout(painel_cascata);
    layout_cascata->setContentsMargins(0, 0, 0, 0);
    layout_cascata->setSpacing(4);
    auto* barra_cascata = new QHBoxLayout;
    auto* botao_ajustar = new QPushButton(QStringLiteral("Ajustar"), painel_cascata);
    menu_ree_ = new QMenu(painel_cascata);
    menu_ree_->setToolTipsVisible(true);
    menu_submercado_ = new QMenu(painel_cascata);
    menu_submercado_->setToolTipsVisible(true);
    ree_cascata_ = new BotaoCombo(painel_cascata);
    ree_cascata_->setMenu(menu_ree_);
    submercado_cascata_ = new BotaoCombo(painel_cascata);
    submercado_cascata_->setMenu(menu_submercado_);
    // Largura fixa pelo rotulo mais longo possivel, para a barra nao se mexer quando o texto do
    // botao alterna entre "todos" e a contagem.
    int largura_filtro = ree_cascata_->fontMetrics().horizontalAdvance(QStringLiteral("Submercado: 99 de 99")) + 28;
    ree_cascata_->setMinimumWidth(largura_filtro);
    submercado_cascata_->setMinimumWidth(largura_filtro);
    barra_cascata->addWidget(submercado_cascata_);
    barra_cascata->addWidget(ree_cascata_);
    barra_cascata->addWidget(botao_ajustar);
    barra_cascata->addStretch(1);
    layout_cascata->addLayout(barra_cascata);
    vista_cascata_ = new VistaCascata(modelo_, painel_cascata);
    layout_cascata->addWidget(vista_cascata_, 1);
    connect(botao_ajustar, &QPushButton::clicked, vista_cascata_, &VistaCascata::ajustar);
    repovoarFiltrosCascata();
    abas_esquerda->addTab(painel_cascata, QStringLiteral("Cascata"));

    layout->addWidget(abas_esquerda, 1);

    splitter_->addWidget(painel);
}

// Os dois menus saem sempre do deck (ree.dat e sistema.dat), nao do que esta desenhado, para nenhum
// REE ou submercado sumir da lista por causa de um filtro ativo. "Todos" e so o atalho que
// desmarca o resto: selecao vazia ja significa "todos" para a vista.
void JanelaPrincipal::repovoarFiltrosCascata() {
    auto povoar = [this](QMenu* menu, const std::map<int, std::string>& itens) {
        menu->clear();
        QAction* todos = menu->addAction(QStringLiteral("Todos"));
        todos->setCheckable(true);
        todos->setChecked(true);
        connect(todos, &QAction::triggered, this, [this, menu] {
            for (QAction* acao : menu->actions()) {
                if (!acao->data().isValid()) continue;
                QSignalBlocker bloqueio(acao);
                acao->setChecked(false);
            }
            aplicarFiltrosCascata();
        });
        menu->addSeparator();
        for (const auto& [codigo, nome] : itens) {
            QAction* acao = menu->addAction(QString::fromLatin1(nome.c_str()));
            acao->setCheckable(true);
            acao->setData(codigo);
            connect(acao, &QAction::toggled, this, [this](bool) { aplicarFiltrosCascata(); });
        }
    };

    std::map<int, std::string> nomes_rees;
    for (const auto& [codigo, ree] : modelo_->lookup().rees) nomes_rees[codigo] = ree.nome;
    povoar(menu_ree_, nomes_rees);
    povoar(menu_submercado_, modelo_->lookup().subsistemas);
    aplicarFiltrosCascata();
}

void JanelaPrincipal::aplicarFiltrosCascata() {
    std::set<int> rees = codigosMarcados(menu_ree_);
    std::set<int> submercados = codigosMarcados(menu_submercado_);

    // A acao "Todos" apenas espelha o estado: fica marcada quando nada individual esta marcado.
    auto sincronizarTodos = [](QMenu* menu, bool vazio) {
        QList<QAction*> acoes = menu->actions();
        if (acoes.isEmpty()) return;
        QSignalBlocker bloqueio(acoes.first());
        acoes.first()->setChecked(vazio);
    };
    sincronizarTodos(menu_ree_, rees.empty());
    sincronizarTodos(menu_submercado_, submercados.empty());

    int total_rees = static_cast<int>(modelo_->lookup().rees.size());
    int total_submercados = static_cast<int>(modelo_->lookup().subsistemas.size());
    ree_cascata_->setText(rees.empty() ? QStringLiteral("REE: todos")
                                       : QStringLiteral("REE: %1 de %2")
                                             .arg(static_cast<int>(rees.size()))
                                             .arg(total_rees));
    submercado_cascata_->setText(submercados.empty()
                                     ? QStringLiteral("Submercado: todos")
                                     : QStringLiteral("Submercado: %1 de %2")
                                           .arg(static_cast<int>(submercados.size()))
                                           .arg(total_submercados));

    vista_cascata_->definirFiltros(rees, submercados);
}

// Na primeira exibicao, dimensiona a janela e o splitter para a ultima aba do formulario terminar
// exatamente na borda direita do painel das abas, sem aba escondida atras das setas de rolagem nem
// sobra depois dela. A primeira estimativa e a largura da barra de abas mais as margens do layout do
// formulario; como o estilo ainda desloca a barra e reserva alguns pixels, a janela aplica a
// estimativa, mede onde a barra terminaria (x da barra mais a largura que ela pede) contra a largura
// do QTabWidget e corrige a diferenca, para mais ou para menos. O layout principal e ativado depois
// de cada resize para splitter_->width() ja valer o tamanho novo. So na primeira exibicao: depois
// disso o tamanho e do usuario.
void JanelaPrincipal::showEvent(QShowEvent* ev) {
    QMainWindow::showEvent(ev);
    if (dimensionado_) return;
    dimensionado_ = true;

    QTabWidget* abas = formulario_->abas();
    QTabBar* barra = abas->tabBar();
    auto aplicar = [&](int largura_direita) {
        if (QLayout* layout_principal = layout()) layout_principal->activate();
        int falta = LARGURA_MINIMA_ESQUERDA + largura_direita + splitter_->handleWidth() - splitter_->width();
        resize(width() + std::max(0, falta), std::max(height(), 640));
        if (QLayout* layout_principal = layout()) layout_principal->activate();
        splitter_->setSizes(
            {std::max(LARGURA_MINIMA_ESQUERDA, splitter_->width() - largura_direita), largura_direita});
    };

    const QMargins margens = formulario_->layout()->contentsMargins();
    int largura_direita = barra->sizeHint().width() + margens.left() + margens.right();
    aplicar(largura_direita);
    int diferenca = barra->x() + barra->sizeHint().width() - abas->width();
    if (diferenca != 0) aplicar(largura_direita + diferenca);
}

void JanelaPrincipal::criarMenus() {
    menuBar()->setNativeMenuBar(false);
    menuBar()->setStyleSheet(QStringLiteral("QMenuBar { padding: 0px; spacing: 2px; } QMenuBar::item { padding: 2px 8px; margin: 0px; }"));

    QMenu* arquivo = menuBar()->addMenu(QStringLiteral("&Arquivo"));
    arquivo->addAction(QStringLiteral("&Abrir..."), QKeySequence::Open, this, &JanelaPrincipal::abrir);
    arquivo->addAction(QStringLiteral("Abrir &deck (pasta)..."), this, &JanelaPrincipal::abrirDeck);
    menu_recentes_ = arquivo->addMenu(QStringLiteral("&Recentes"));
    acao_salvar_ = arquivo->addAction(QStringLiteral("&Salvar"), QKeySequence::Save, this, &JanelaPrincipal::salvar);
    acao_salvar_como_ = arquivo->addAction(QStringLiteral("Salvar &como..."), QKeySequence::SaveAs, this, &JanelaPrincipal::salvarComo);
    arquivo->addAction(QStringLiteral("Salvar &tudo"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S), this, &JanelaPrincipal::salvarTudo);
    acao_salvar_deck_como_ = arquivo->addAction(QStringLiteral("Salvar deck em outra &pasta..."), this, &JanelaPrincipal::salvarDeckComo);
    arquivo->addSeparator();
    acao_exportar_ = arquivo->addAction(QStringLiteral("&Exportar CSV..."), this, &JanelaPrincipal::exportarCsv);
    arquivo->addSeparator();
    arquivo->addAction(QStringLiteral("Sai&r"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* editar = menuBar()->addMenu(QStringLiteral("&Editar"));
    QAction* desfazer = editar->addAction(QStringLiteral("&Desfazer"), QKeySequence::Undo, this, [this] { pilhaAtiva()->undo(); });
    QAction* refazer = editar->addAction(QStringLiteral("&Refazer"), QKeySequence::Redo, this, [this] { pilhaAtiva()->redo(); });
    connect(editar, &QMenu::aboutToShow, this, [this, desfazer, refazer] {
        const QUndoStack* pilha = pilhaAtiva();
        desfazer->setEnabled(pilha->canUndo());
        desfazer->setText(pilha->canUndo() ? QStringLiteral("&Desfazer: %1").arg(pilha->undoText()) : QStringLiteral("&Desfazer"));
        refazer->setEnabled(pilha->canRedo());
        refazer->setText(pilha->canRedo() ? QStringLiteral("&Refazer: %1").arg(pilha->redoText()) : QStringLiteral("&Refazer"));
    });
    connect(editar, &QMenu::aboutToHide, this, [desfazer, refazer] {
        desfazer->setEnabled(true);
        refazer->setEnabled(true);
    });
    editar->addSeparator();
    acao_procurar_ = editar->addAction(QStringLiteral("&Procurar no deck..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), this,
                                       &JanelaPrincipal::procurarNoDeck);

    QMenu* ver = menuBar()->addMenu(QStringLiteral("&Ver"));
    ocultar_vazias_ = ver->addAction(QStringLiteral("Ocultar registros &vazios"));
    ocultar_vazias_->setCheckable(true);
    ocultar_vazias_->setChecked(true);
    connect(ocultar_vazias_, &QAction::toggled, filtro_, &FiltroUsinas::definirOcultarVazias);
    connect(ocultar_vazias_, &QAction::toggled, navegador_, &NavegadorDeck::definirOcultarVazios);
    ver->addSeparator();
    nomes_cascata_ = ver->addAction(QStringLiteral("&Nomes na cascata"));
    nomes_cascata_->setCheckable(true);
    nomes_cascata_->setChecked(true);
    connect(nomes_cascata_, &QAction::toggled, vista_cascata_, &VistaCascata::definirMostrarNomes);
    ficticias_cascata_ = ver->addAction(QStringLiteral("Usinas &fictícias na cascata"));
    ficticias_cascata_->setCheckable(true);
    ficticias_cascata_->setChecked(true);
    connect(ficticias_cascata_, &QAction::toggled, vista_cascata_, &VistaCascata::definirMostrarFicticias);
    ver->addSeparator();
    QAction* editor_textual = ver->addAction(QStringLiteral("Editor &textual"));
    editor_textual->setCheckable(true);
    connect(editor_textual, &QAction::toggled, navegador_, &NavegadorDeck::definirModoTexto);

    menu_usina_ = menuBar()->addMenu(QStringLiteral("&Usina"));
    menu_usina_->addAction(QStringLiteral("&Nova no primeiro código livre"), QKeySequence(Qt::CTRL | Qt::Key_N), this, &JanelaPrincipal::novaUsina);
    menu_usina_->addAction(QStringLiteral("Nova em &código..."), this, &JanelaPrincipal::novaUsinaEmCodigo);
    menu_usina_->addAction(QStringLiteral("&Duplicar"), this, &JanelaPrincipal::duplicarUsina);
    menu_usina_->addSeparator();
    menu_usina_->addAction(QStringLiteral("&Excluir (zerar registro)"), QKeySequence::Delete, this, &JanelaPrincipal::excluirUsina);
    menu_usina_->setEnabled(false);

    QMenu* ferramentas = menuBar()->addMenu(QStringLiteral("&Ferramentas"));
    acao_incrementais_ = ferramentas->addAction(QStringLiteral("Exportar vazões &incrementais..."), this,
                                                &JanelaPrincipal::exportarIncrementais);
    acao_comparar_ = ferramentas->addAction(QStringLiteral("&Comparar com outro deck..."), this, &JanelaPrincipal::compararComOutroDeck);

    acao_salvar_->setEnabled(false);
    acao_salvar_como_->setEnabled(false);
    acao_exportar_->setEnabled(false);
    acao_incrementais_->setEnabled(false);
    acao_salvar_deck_como_->setEnabled(false);
    acao_comparar_->setEnabled(false);
    acao_procurar_->setEnabled(false);

    QMenu* ajuda = menuBar()->addMenu(QStringLiteral("A&juda"));
    ajuda->addAction(QStringLiteral("&Sobre..."), this, [this] {
        QMessageBox sobre(this);
        sobre.setWindowTitle(QStringLiteral("Sobre"));
        sobre.setWindowIcon(QIcon(QStringLiteral(":/hidr.ico")));
        const QPixmap logo = QIcon(QStringLiteral(":/hidr.ico")).pixmap(QSize(64, 64), devicePixelRatio());
        QPixmap logo_com_margem(logo.width() + qRound(8 * logo.devicePixelRatio()), logo.height());
        logo_com_margem.setDevicePixelRatio(logo.devicePixelRatio());
        logo_com_margem.fill(Qt::transparent);
        QPainter(&logo_com_margem).drawPixmap(QPointF(8, 0), logo);
        sobre.setIconPixmap(logo_com_margem);
        sobre.setTextFormat(Qt::RichText);
        const QString cor_esmaecida = palette().color(QPalette::Disabled, QPalette::WindowText).name();
        const double tamanho_credito = QFontInfo(sobre.font()).pointSizeF() - 1.0;
        sobre.setText(QStringLiteral("<b>%1</b> <span style=\"font-size:%3pt; color:%4;\">v%2</span><br>Editor dos dados de entrada do NEWAVE<br>"
                                     "Licença: GNU GPL v3<br><br>"
                                     "<span style=\"font-size:%3pt; color:%4;\">NEWAVE © CEPEL, Centro de Pesquisas de Energia Elétrica<br>"
                                     "<a href=\"https://github.com/beralbdom/HydroEdit2\" style=\"color:%4;\">%1</a>"
                                     " © 2026 Bernardo Albuquerque Domingues</span>")
                          .arg(QApplication::applicationName(), QApplication::applicationVersion())
                          .arg(tamanho_credito, 0, 'f', 1)
                          .arg(cor_esmaecida));
        sobre.exec();
    });
    atualizarRecentes();
}

int JanelaPrincipal::linhaSelecionada() const {
    QModelIndex ix = tabela_->selectionModel()->currentIndex();
    if (!ix.isValid()) return -1;
    return filtro_->mapToSource(ix).row();
}

void JanelaPrincipal::selecionarLinha(int linha) {
    QModelIndex origem = modelo_->index(linha, 0);
    QModelIndex ix = filtro_->mapFromSource(origem);
    if (!ix.isValid()) {
        ocultar_vazias_->setChecked(false);
        campo_filtro_->clear();
        ix = filtro_->mapFromSource(origem);
    }
    tabela_->setCurrentIndex(ix);
    tabela_->scrollTo(ix);
}

// hidr.dat da pasta, com o nome em qualquer caixa; vazio se a pasta nao tem um.
static QString hidrNaPasta(const QString& pasta) {
    for (const QFileInfo& f : QDir(pasta).entryInfoList(QDir::Files))
        if (f.fileName().compare(QStringLiteral("hidr.dat"), Qt::CaseInsensitive) == 0) return f.absoluteFilePath();
    return {};
}

// Abre o hidr.dat escolhido ou, se o escolhido for o caso.dat ou o arquivos.dat, o hidr.dat da mesma
// pasta, que traz junto o deck inteiro.
void JanelaPrincipal::abrir() {
    if (!confirmarDescarte()) return;
    QString caminho = QFileDialog::getOpenFileName(this, QStringLiteral("Abrir cadastro hidr.dat"), {},
                                                   QStringLiteral("Cadastro de usinas ou arquivo do caso (hidr.dat caso.dat arquivos.dat *.dat);;Todos (*.*)"));
    if (caminho.isEmpty()) return;
    const QString nome = QFileInfo(caminho).fileName().toLower();
    if (nome == QStringLiteral("caso.dat") || nome == QStringLiteral("arquivos.dat")) {
        const QString hidr = hidrNaPasta(QFileInfo(caminho).absolutePath());
        if (hidr.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Abrir deck"), QStringLiteral("A pasta do %1 não tem um hidr.dat.").arg(QFileInfo(caminho).fileName()));
            return;
        }
        caminho = hidr;
    }
    abrirCaminho(caminho);
}

// Abre o deck pela pasta: o hidr.dat dela e, com ele, os demais arquivos.
void JanelaPrincipal::abrirDeck() {
    if (!confirmarDescarte()) return;
    const QString pasta = QFileDialog::getExistingDirectory(this, QStringLiteral("Abrir deck"), {});
    if (pasta.isEmpty()) return;
    const QString hidr = hidrNaPasta(pasta);
    if (hidr.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Abrir deck"), QStringLiteral("A pasta escolhida não tem um hidr.dat."));
        return;
    }
    abrirCaminho(hidr);
}

// Revisao nova a partir da atual: copia todos os arquivos da pasta do deck (sem subpastas) para a
// pasta escolhida, grava nela o que estiver alterado no cadastro de usinas e nos arquivos do deck e
// abre o deck novo. A pasta original fica como estava no disco.
void JanelaPrincipal::salvarDeckComo() {
    if (modelo_->caminho().isEmpty()) return;
    const QString origem = QFileInfo(modelo_->caminho()).absolutePath();
    const QString destino = QFileDialog::getExistingDirectory(this, QStringLiteral("Salvar deck em outra pasta"), QFileInfo(origem).absolutePath());
    if (destino.isEmpty()) return;
    if (QDir(destino) == QDir(origem)) {
        salvarTudo();
        return;
    }
    if (!QDir(destino).entryList(QDir::Files).isEmpty() &&
        QMessageBox::question(this, QStringLiteral("Salvar deck em outra pasta"),
                              QStringLiteral("A pasta escolhida já tem arquivos. Os arquivos com o mesmo nome serão substituídos. Continuar?")) !=
            QMessageBox::Yes)
        return;
    if (!modelo_->pilhaUndo()->isClean() && !validarAntesDeSalvar()) return;
    for (const QFileInfo& f : QDir(origem).entryInfoList(QDir::Files)) {
        const QString alvo = QDir(destino).filePath(f.fileName());
        if (QFile::exists(alvo)) QFile::remove(alvo);
        if (!QFile::copy(f.absoluteFilePath(), alvo)) {
            QMessageBox::critical(this, QStringLiteral("Salvar deck em outra pasta"), QStringLiteral("Não foi possível copiar %1.").arg(f.fileName()));
            return;
        }
    }
    const QString hidr_novo = QDir(destino).filePath(QFileInfo(modelo_->caminho()).fileName());
    if (!modelo_->pilhaUndo()->isClean()) {
        Resultado r = modelo_->arquivo().salvar(paraPath(hidr_novo));
        if (!r.ok) {
            QMessageBox::critical(this, QStringLiteral("Salvar deck em outra pasta"), QString::fromUtf8(r.mensagem));
            return;
        }
    }
    QString motivo;
    if (!navegador_->salvarTodosEm(destino, &motivo)) {
        QMessageBox::critical(this, QStringLiteral("Salvar deck em outra pasta"), QStringLiteral("Não foi possível salvar %1").arg(motivo));
        return;
    }
    modelo_->pilhaUndo()->setClean();
    abrirCaminho(hidr_novo);
    statusBar()->showMessage(QStringLiteral("Deck salvo em %1").arg(QDir::toNativeSeparators(destino)), 5000);
}

// Busca em todos os arquivos do deck, numa janela que fica aberta ao lado; escolher um resultado leva
// ao arquivo e ao registro.
void JanelaPrincipal::procurarNoDeck() {
    if (!busca_) {
        busca_ = new DialogoBusca(navegador_->dados(), modelo_, this);
        connect(busca_, &DialogoBusca::escolhido, this, [this](const QString& nome, int secao, int registro) {
            navegador_->mostrarRegistro(nome, secao, registro);
            if (nome == QStringLiteral("hidr.dat") && registro >= 0) selecionarLinha(registro);
        });
    }
    busca_->show();
    busca_->raise();
    busca_->activateWindow();
}

// Compara o deck aberto, com as edicoes ainda nao salvas, com o deck de outra pasta.
void JanelaPrincipal::compararComOutroDeck() {
    navegador_->aplicarEdicoesPendentes();
    const QString atual = QFileInfo(modelo_->caminho()).absolutePath();
    const QString pasta = QFileDialog::getExistingDirectory(this, QStringLiteral("Comparar com o deck da pasta"), QFileInfo(atual).absolutePath());
    if (pasta.isEmpty()) return;
    DadosDeck outro;
    outro.carregar(pasta, lerArquivosDat(paraPath(QDir(pasta).filePath(QStringLiteral("arquivos.dat")))));
    ArquivoHidr hidr_outro;
    const QString caminho_hidr = hidrNaPasta(pasta);
    const bool tem_hidr = !caminho_hidr.isEmpty() && hidr_outro.carregar(paraPath(caminho_hidr)).ok;
    auto* dialogo = new DialogoComparacao(QDir::toNativeSeparators(atual), QDir::toNativeSeparators(pasta),
                                          compararDecks(*navegador_->dados(), &modelo_->arquivo(), outro, tem_hidr ? &hidr_outro : nullptr), this);
    dialogo->setAttribute(Qt::WA_DeleteOnClose);
    dialogo->show();
}

void JanelaPrincipal::abrirCaminho(const QString& caminho) {
    ArquivoHidr a;
    Resultado r = a.carregar(paraPath(caminho));
    if (!r.ok) {
        QMessageBox::critical(this, QStringLiteral("Erro ao abrir"), QString::fromUtf8(r.mensagem));
        return;
    }
    DeckLookup lookup;
    lookup.carregarDeck(paraPath(QFileInfo(caminho).absolutePath()));
    lookup.carregarCsvs(paraPath(QApplication::applicationDirPath()));
    modelo_->definirLookup(lookup);
    repovoarFiltrosCascata();
    modelo_->definirArquivo(std::move(a), caminho);
    navegador_->carregarDeck(QFileInfo(caminho).absolutePath());
    ajustarColunas(tabela_);
    acao_salvar_->setEnabled(true);
    acao_salvar_como_->setEnabled(true);
    acao_exportar_->setEnabled(true);
    acao_incrementais_->setEnabled(true);
    acao_salvar_deck_como_->setEnabled(true);
    acao_comparar_->setEnabled(true);
    acao_procurar_->setEnabled(true);
    menu_usina_->setEnabled(true);
    painel_problemas_->definirProblemas({});
    if (modelo_->numUsinas() > 0) selecionarLinha(0);
    atualizarTitulo();
    atualizarStatus();
    registrarRecente(caminho);
}

bool JanelaPrincipal::salvarEm(const QString& caminho) {
    if (!validarAntesDeSalvar()) return false;
    Resultado r = modelo_->arquivo().salvar(paraPath(caminho));
    if (!r.ok) {
        QMessageBox::critical(this, QStringLiteral("Erro ao salvar"), QString::fromUtf8(r.mensagem));
        return false;
    }
    modelo_->definirCaminho(caminho);
    modelo_->pilhaUndo()->setClean();
    atualizarTitulo();
    atualizarStatus();
    registrarRecente(caminho);
    return true;
}

void JanelaPrincipal::salvar() {
    if (modelo_->caminho().isEmpty()) salvarComo();
    else salvarEm(modelo_->caminho());
}

// Salva o cadastro de usinas, se tiver alteracoes, e todos os arquivos do deck alterados; para no
// primeiro que falhar ou se o usuario cancelar o salvar como do cadastro.
void JanelaPrincipal::salvarTudo() {
    if (!modelo_->pilhaUndo()->isClean()) {
        salvar();
        if (!modelo_->pilhaUndo()->isClean()) return;
    }
    QString motivo;
    if (!navegador_->salvarTodos(&motivo)) {
        QMessageBox::warning(this, QStringLiteral("Salvar tudo"), QStringLiteral("Não foi possível salvar %1").arg(motivo));
        return;
    }
    statusBar()->showMessage(QStringLiteral("Tudo salvo"), 3000);
}

// Desfazer e refazer valem para o que esta na tela: o cadastro de usinas quando o editor dele aparece,
// e os arquivos do deck nas demais paginas.
QUndoStack* JanelaPrincipal::pilhaAtiva() const {
    return splitter_->isVisible() ? modelo_->pilhaUndo() : navegador_->pilhaUndo();
}

void JanelaPrincipal::salvarComo() {
    QString caminho = QFileDialog::getSaveFileName(this, QStringLiteral("Salvar cadastro como"), modelo_->caminho(),
                                                   QStringLiteral("Cadastro de usinas hidráulicas (*.dat)"));
    if (!caminho.isEmpty()) salvarEm(caminho);
}

void JanelaPrincipal::exportarCsv() {
    QString sugestao = QFileInfo(modelo_->caminho()).absolutePath() + QStringLiteral("/hidr.csv");
    QString caminho = QFileDialog::getSaveFileName(this, QStringLiteral("Exportar CSV"), sugestao, QStringLiteral("CSV (*.csv)"));
    if (caminho.isEmpty()) return;
    OpcoesCsv o;
    auto resposta = QMessageBox::question(this, QStringLiteral("Formato decimal"),
                                          QStringLiteral("Usar vírgula como separador decimal (Excel em português)?"),
                                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    o.virgula_decimal = resposta == QMessageBox::Yes;
    Resultado r = ::exportarCsv(modelo_->arquivo(), paraPath(caminho), o);
    if (!r.ok) QMessageBox::critical(this, QStringLiteral("Erro ao exportar"), QString::fromUtf8(r.mensagem));
    else statusBar()->showMessage(QStringLiteral("CSV gerado: %1").arg(caminho), 5000);
}

// Le o vazoes.dat da pasta do cadastro aberto, datado pelo postos.dat, e calcula a incremental de
// cada usina com o cadastro em memoria (inclusive edicoes ainda nao salvas) e o confhd.dat do deck.
// Antes do calculo o usuario pode aplicar um REGRAS.DAT do GEVAZP, que recalcula os postos
// artificiais; o ultimo arquivo escolhido fica guardado nas configuracoes. Ao fim, resume o periodo,
// as regras aplicadas, os meses truncados em zero e os avisos de posto invalido.
void JanelaPrincipal::exportarIncrementais() {
    const QString titulo = QStringLiteral("Vazões incrementais");
    const DeckLookup& lookup = modelo_->lookup();
    if (lookup.modelo == ModeloDeck::Dessem) {
        QMessageBox::information(this, titulo,
                                 QStringLiteral("A exportação usa o vazoes.dat, que só existe em decks do NEWAVE."));
        return;
    }
    if (lookup.anoInicialHistorico() == 0) {
        QMessageBox::warning(this, titulo,
                             QStringLiteral("O postos.dat do deck é necessário para datar o histórico do vazoes.dat."));
        return;
    }
    QString pasta = QFileInfo(modelo_->caminho()).absolutePath();
    ResultadoLeituraVazoes leitura =
        lerVazoesDat(paraPath(pasta + QStringLiteral("/vazoes.dat")), modelo_->numUsinas(), lookup.anoInicialHistorico());
    if (!leitura.erro.empty()) {
        QMessageBox::critical(this, titulo, QString::fromUtf8(leitura.erro));
        return;
    }

    QMessageBox pergunta(QMessageBox::Question, titulo,
                         QStringLiteral("Aplicar as regras de postos artificiais de um REGRAS.DAT do GEVAZP antes do cálculo?"),
                         QMessageBox::NoButton, this);
    QPushButton* com_regras = pergunta.addButton(QStringLiteral("Usar REGRAS.DAT..."), QMessageBox::ActionRole);
    QPushButton* sem_regras = pergunta.addButton(QStringLiteral("Sem regras"), QMessageBox::ActionRole);
    pergunta.addButton(QMessageBox::Cancel);
    pergunta.setDefaultButton(sem_regras);
    pergunta.exec();
    if (pergunta.clickedButton() != com_regras && pergunta.clickedButton() != sem_regras) return;

    QString resumo_regras;
    if (pergunta.clickedButton() == com_regras) {
        QSettings configuracoes;
        QString anterior = configuracoes.value(QStringLiteral("regras_gevazp/ultimo"), pasta).toString();
        QString arquivo_regras = QFileDialog::getOpenFileName(this, QStringLiteral("REGRAS.DAT do GEVAZP"), anterior,
                                                              QStringLiteral("Regras (*.dat);;Todos os arquivos (*)"));
        if (arquivo_regras.isEmpty()) return;
        ResultadoLeituraRegras regras = lerRegras(paraPath(arquivo_regras));
        if (!regras.erro.empty()) {
            QMessageBox::critical(this, titulo, QString::fromUtf8(regras.erro));
            return;
        }
        Resultado aplicacao = aplicarRegras(regras.regras, leitura.serie);
        if (!aplicacao.ok) {
            QMessageBox::critical(this, titulo, QString::fromUtf8(aplicacao.mensagem));
            return;
        }
        configuracoes.setValue(QStringLiteral("regras_gevazp/ultimo"), arquivo_regras);
        std::set<int> postos_recalculados;
        for (const RegraPosto& regra : regras.regras) postos_recalculados.insert(regra.posto);
        resumo_regras = QStringLiteral("\nRegras do GEVAZP aplicadas: %1 postos recalculados (%2).")
                            .arg(postos_recalculados.size())
                            .arg(QFileInfo(arquivo_regras).fileName());
    }

    QString caminho = QFileDialog::getSaveFileName(this, QStringLiteral("Exportar vazões incrementais"),
                                                   pasta + QStringLiteral("/vazoes_incrementais.csv"),
                                                   QStringLiteral("CSV (*.csv)"));
    if (caminho.isEmpty()) return;

    Incrementais incrementais = calcularIncrementais(modelo_->arquivo().usinas, lookup.confhd, leitura.serie);
    Resultado r = exportarIncrementaisCsv(incrementais, leitura.serie, paraPath(caminho));
    if (!r.ok) {
        QMessageBox::critical(this, titulo, QString::fromUtf8(r.mensagem));
        return;
    }

    int usinas_truncadas = 0;
    int meses_truncados = 0;
    for (const UsinaIncremental& u : incrementais.usinas) {
        if (u.meses_truncados == 0) continue;
        ++usinas_truncadas;
        meses_truncados += u.meses_truncados;
    }
    int ano_final = leitura.serie.ano_inicial + (leitura.serie.meses() + 11) / 12 - 1;
    QString resumo = QStringLiteral("%1 usinas exportadas, de %2 a %3.%4\n%5 usinas tiveram valores negativos truncados "
                                    "em zero, %6 meses no total.")
                         .arg(incrementais.usinas.size())
                         .arg(leitura.serie.ano_inicial)
                         .arg(ano_final)
                         .arg(resumo_regras)
                         .arg(usinas_truncadas)
                         .arg(meses_truncados);
    if (!incrementais.avisos.empty()) {
        constexpr size_t kMaximoAvisos = 10;
        resumo += QStringLiteral("\n\nAvisos:");
        for (size_t i = 0; i < std::min(incrementais.avisos.size(), kMaximoAvisos); ++i)
            resumo += QStringLiteral("\n") + QString::fromUtf8(incrementais.avisos[i]);
        if (incrementais.avisos.size() > kMaximoAvisos)
            resumo += QStringLiteral("\n+%1 outros").arg(incrementais.avisos.size() - kMaximoAvisos);
    }
    QMessageBox::information(this, titulo, resumo);
}

void JanelaPrincipal::atualizarTitulo() {
    QString nome = modelo_->caminho().isEmpty() ? QStringLiteral("sem arquivo") : QFileInfo(modelo_->caminho()).fileName();
    QString sujo = modelo_->pilhaUndo()->isClean() ? QString() : QStringLiteral("*");
    ModeloDeck deck_modelo = modelo_->lookup().modelo;
    QString modelo_titulo = deck_modelo == ModeloDeck::Desconhecido
                                 ? QString()
                                 : QStringLiteral(" (%1)").arg(QString::fromUtf8(DeckLookup::nomeModelo(deck_modelo)));
    setWindowTitle(QStringLiteral("HydroEdit 2 - %1%2%3").arg(nome, modelo_titulo, sujo));
    botao_salvar_->setEnabled(!modelo_->caminho().isEmpty() && !modelo_->pilhaUndo()->isClean());
    formulario_->atualizarArquivo();
}

void JanelaPrincipal::atualizarStatus() {
    status_arquivo_->setText(modelo_->caminho());
    status_modelo_->setText(modelo_->caminho().isEmpty()
                                 ? QString()
                                 : QString::fromUtf8(DeckLookup::nomeModelo(modelo_->lookup().modelo)));
    status_usinas_->setText(QStringLiteral("%1 / %2 usinas")
                                .arg(modelo_->arquivo().numUsinasPreenchidas())
                                .arg(modelo_->numUsinas()));
    if (modelo_->caminho().isEmpty()) {
        status_validacao_->setText(QString());
    } else {
        std::vector<ProblemaUsina> problemas = validarTudo();
        int erros = 0, avisos = 0;
        for (const ProblemaUsina& p : problemas) (p.problema.severidade == Severidade::Erro ? erros : avisos)++;
        status_validacao_->setText(QStringLiteral("%1 erros, %2 avisos").arg(erros).arg(avisos));
    }
    QStringList notas;
    for (const std::string& n : modelo_->lookup().notas) notas << QString::fromUtf8(n);
    status_notas_->setText(notas.join(QStringLiteral(" | ")));
}

std::vector<ProblemaUsina> JanelaPrincipal::validarTudo() const {
    std::vector<ProblemaUsina> saida;
    ContextoValidacao ctx;
    ctx.num_usinas = modelo_->numUsinas();
    for (int i = 0; i < modelo_->numUsinas(); ++i) {
        const UsinaHidr& u = modelo_->usina(i);
        if (u.vazia()) continue;
        ctx.codigo = i + 1;
        for (Problema& p : validar(u, ctx)) saida.push_back({i, std::move(p)});
    }
    return saida;
}

bool JanelaPrincipal::validarAntesDeSalvar() {
    std::vector<ProblemaUsina> problemas = validarTudo();
    painel_problemas_->definirProblemas(problemas);
    int erros = 0;
    for (const ProblemaUsina& p : problemas)
        if (p.problema.severidade == Severidade::Erro) ++erros;
    if (erros > 0) {
        QMessageBox::critical(this, QStringLiteral("Não é possível salvar"),
                              QStringLiteral("%1 erro(s) de consistência. Corrija os itens do painel de problemas.").arg(erros));
        return false;
    }
    return true;
}

void JanelaPrincipal::marcarProblemasDaLinha(int linha) {
    std::vector<std::string> campos;
    if (linha >= 0 && !modelo_->usina(linha).vazia()) {
        ContextoValidacao ctx{modelo_->numUsinas(), linha + 1};
        for (const Problema& p : validar(modelo_->usina(linha), ctx))
            if (p.severidade == Severidade::Erro) campos.push_back(p.campo);
    }
    formulario_->marcarProblemas(campos);
}

int JanelaPrincipal::primeiroCodigoLivre() const {
    for (int i = 0; i < modelo_->numUsinas(); ++i)
        if (modelo_->usina(i).vazia()) return i + 1;
    return -1;
}

void JanelaPrincipal::novaUsina() {
    int codigo = primeiroCodigoLivre();
    if (codigo < 0) {
        QMessageBox::warning(this, QStringLiteral("Sem espaço"), QStringLiteral("Não há registro livre no arquivo."));
        return;
    }
    UsinaHidr u;
    u.nome = "NOVA";
    u.regulacao = "M";
    u.num_pol_jusante = 1;
    modelo_->substituirUsina(codigo - 1, u, QStringLiteral("Nova usina %1").arg(codigo));
    selecionarLinha(codigo - 1);
    formulario_->focarCampo("nome");
}

void JanelaPrincipal::novaUsinaEmCodigo() {
    bool ok = false;
    int codigo = QInputDialog::getInt(this, QStringLiteral("Nova usina"), QStringLiteral("Código (1..%1):").arg(modelo_->numUsinas()),
                                      std::max(1, primeiroCodigoLivre()), 1, modelo_->numUsinas(), 1, &ok);
    if (!ok) return;
    if (!modelo_->usina(codigo - 1).vazia()) {
        QMessageBox::warning(this, QStringLiteral("Código ocupado"),
                             QStringLiteral("O código %1 já corresponde à usina %2.").arg(codigo).arg(QString::fromLatin1(modelo_->usina(codigo - 1).nome.c_str())));
        return;
    }
    UsinaHidr u;
    u.nome = "NOVA";
    u.regulacao = "M";
    u.num_pol_jusante = 1;
    modelo_->substituirUsina(codigo - 1, u, QStringLiteral("Nova usina %1").arg(codigo));
    selecionarLinha(codigo - 1);
    formulario_->focarCampo("nome");
}

void JanelaPrincipal::duplicarUsina() {
    int origem = linhaSelecionada();
    if (origem < 0 || modelo_->usina(origem).vazia()) return;
    int codigo = primeiroCodigoLivre();
    if (codigo < 0) {
        QMessageBox::warning(this, QStringLiteral("Sem espaço"), QStringLiteral("Não há registro livre no arquivo."));
        return;
    }
    modelo_->substituirUsina(codigo - 1, modelo_->usina(origem), QStringLiteral("Duplicar usina %1 em %2").arg(origem + 1).arg(codigo));
    selecionarLinha(codigo - 1);
}

void JanelaPrincipal::excluirUsina() {
    int linha = linhaSelecionada();
    if (linha < 0 || modelo_->usina(linha).vazia()) return;
    auto r = QMessageBox::question(this, QStringLiteral("Excluir usina"),
                                   QStringLiteral("Zerar o registro %1 (%2)? A operação pode ser desfeita com Ctrl+Z.")
                                       .arg(linha + 1).arg(QString::fromLatin1(modelo_->usina(linha).nome.c_str())));
    if (r != QMessageBox::Yes) return;
    modelo_->substituirUsina(linha, UsinaHidr{}, QStringLiteral("Excluir usina %1").arg(linha + 1));
}

// Pergunta primeiro pelo hidr.dat e depois, numa pergunta so, pelos arquivos do deck editados nas
// abas; Cancelar em qualquer uma interrompe, e uma falha ao salvar tambem.
bool JanelaPrincipal::confirmarDescarte() {
    if (!modelo_->pilhaUndo()->isClean()) {
        auto r = QMessageBox::question(this, QStringLiteral("Alterações não salvas"),
                                       QStringLiteral("Salvar as alterações em %1?").arg(QFileInfo(modelo_->caminho()).fileName()),
                                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (r == QMessageBox::Cancel) return false;
        if (r == QMessageBox::Save) {
            salvar();
            if (!modelo_->pilhaUndo()->isClean()) return false;
        }
    }
    navegador_->aplicarEdicoesPendentes();
    const QStringList outros = navegador_->arquivosModificados();
    if (outros.isEmpty()) return true;
    auto r = QMessageBox::question(this, QStringLiteral("Alterações não salvas"),
                                   QStringLiteral("Salvar as alterações em %1?").arg(outros.join(QStringLiteral(", "))),
                                   QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (r == QMessageBox::Cancel) return false;
    if (r == QMessageBox::Save) return navegador_->salvarTodos();
    return true;
}

void JanelaPrincipal::closeEvent(QCloseEvent* ev) {
    if (confirmarDescarte()) ev->accept();
    else ev->ignore();
}

void JanelaPrincipal::registrarRecente(const QString& caminho) {
    QSettings s;
    QStringList lista = s.value(QStringLiteral("recentes")).toStringList();
    lista.removeAll(caminho);
    lista.prepend(caminho);
    while (lista.size() > 5) lista.removeLast();
    s.setValue(QStringLiteral("recentes"), lista);
    atualizarRecentes();
}

void JanelaPrincipal::atualizarRecentes() {
    menu_recentes_->clear();
    QStringList lista = QSettings().value(QStringLiteral("recentes")).toStringList();
    for (const QString& c : lista)
        menu_recentes_->addAction(c, this, [this, c] { if (confirmarDescarte()) abrirCaminho(c); });
    menu_recentes_->setEnabled(!lista.isEmpty());
}
