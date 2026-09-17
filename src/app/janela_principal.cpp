#include "janela_principal.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QVBoxLayout>
#include <algorithm>
#include <filesystem>
#include "delegate_numerico.h"
#include "exportador_csv.h"
#include "filtro_usinas.h"
#include "formulario_usina.h"
#include "modelo_hidr.h"
#include "painel_problemas.h"
#include "validacao.h"

namespace {
std::filesystem::path paraPath(const QString& s) { return std::filesystem::path(s.toStdWString()); }
}

JanelaPrincipal::JanelaPrincipal(QWidget* parent) : QMainWindow(parent) {
    modelo_ = new ModeloHidr(this);
    filtro_ = new FiltroUsinas(this);
    filtro_->setSourceModel(modelo_);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter_);
    criarTabela();
    formulario_ = new FormularioUsina(modelo_, splitter_);
    splitter_->addWidget(formulario_);
    splitter_->setStretchFactor(0, 3);
    splitter_->setStretchFactor(1, 2);
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
        if (linha == formulario_->linha()) marcarProblemasDaLinha(linha);
    });
    connect(tabela_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { marcarProblemasDaLinha(linhaSelecionada()); });

    criarMenus();

    status_arquivo_ = new QLabel(this);
    status_usinas_ = new QLabel(this);
    status_notas_ = new QLabel(this);
    statusBar()->addWidget(status_arquivo_, 1);
    statusBar()->addWidget(status_usinas_);
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
    auto* linha_filtro = new QHBoxLayout;
    campo_filtro_ = new QLineEdit(painel);
    campo_filtro_->setPlaceholderText(QStringLiteral("Filtrar por código ou nome"));
    campo_filtro_->setClearButtonEnabled(true);
    ocultar_vazias_ = new QCheckBox(QStringLiteral("Ocultar vazias"), painel);
    ocultar_vazias_->setChecked(true);
    linha_filtro->addWidget(campo_filtro_, 1);
    linha_filtro->addWidget(ocultar_vazias_);
    layout->addLayout(linha_filtro);

    tabela_ = new QTableView(painel);
    tabela_->setModel(filtro_);
    tabela_->setItemDelegate(new DelegateNumerico(modelo_, tabela_));
    tabela_->setSortingEnabled(true);
    tabela_->sortByColumn(0, Qt::AscendingOrder);
    tabela_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela_->setSelectionMode(QAbstractItemView::SingleSelection);
    tabela_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    tabela_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tabela_->horizontalHeader()->setDefaultSectionSize(90);
    tabela_->verticalHeader()->setVisible(false);
    layout->addWidget(tabela_, 1);

    connect(campo_filtro_, &QLineEdit::textChanged, filtro_, &FiltroUsinas::definirTexto);
    connect(ocultar_vazias_, &QCheckBox::toggled, filtro_, &FiltroUsinas::definirOcultarVazias);
    splitter_->addWidget(painel);
}

void JanelaPrincipal::criarMenus() {
    QMenu* arquivo = menuBar()->addMenu(QStringLiteral("&Arquivo"));
    arquivo->addAction(QStringLiteral("&Abrir..."), QKeySequence::Open, this, &JanelaPrincipal::abrir);
    menu_recentes_ = arquivo->addMenu(QStringLiteral("&Recentes"));
    acao_salvar_ = arquivo->addAction(QStringLiteral("&Salvar"), QKeySequence::Save, this, &JanelaPrincipal::salvar);
    acao_salvar_como_ = arquivo->addAction(QStringLiteral("Salvar &como..."), QKeySequence::SaveAs, this, &JanelaPrincipal::salvarComo);
    arquivo->addSeparator();
    acao_exportar_ = arquivo->addAction(QStringLiteral("&Exportar CSV..."), this, &JanelaPrincipal::exportarCsv);
    arquivo->addSeparator();
    arquivo->addAction(QStringLiteral("Sai&r"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* editar = menuBar()->addMenu(QStringLiteral("&Editar"));
    QAction* desfazer = modelo_->pilhaUndo()->createUndoAction(this, QStringLiteral("&Desfazer"));
    desfazer->setShortcut(QKeySequence::Undo);
    QAction* refazer = modelo_->pilhaUndo()->createRedoAction(this, QStringLiteral("&Refazer"));
    refazer->setShortcut(QKeySequence::Redo);
    editar->addAction(desfazer);
    editar->addAction(refazer);

    menu_usina_ = menuBar()->addMenu(QStringLiteral("&Usina"));
    menu_usina_->addAction(QStringLiteral("&Nova no primeiro código livre"), QKeySequence(Qt::CTRL | Qt::Key_N), this, &JanelaPrincipal::novaUsina);
    menu_usina_->addAction(QStringLiteral("Nova em &código..."), this, &JanelaPrincipal::novaUsinaEmCodigo);
    menu_usina_->addAction(QStringLiteral("&Duplicar"), this, &JanelaPrincipal::duplicarUsina);
    menu_usina_->addSeparator();
    menu_usina_->addAction(QStringLiteral("&Excluir (zerar registro)"), QKeySequence::Delete, this, &JanelaPrincipal::excluirUsina);
    menu_usina_->setEnabled(false);

    acao_salvar_->setEnabled(false);
    acao_salvar_como_->setEnabled(false);
    acao_exportar_->setEnabled(false);

    QMenu* ajuda = menuBar()->addMenu(QStringLiteral("A&juda"));
    ajuda->addAction(QStringLiteral("&Sobre..."), this, [this] {
        QMessageBox::about(this, QStringLiteral("Sobre o HydroEdit"),
                           QStringLiteral("<b>HydroEdit 5.0.0</b><br>Editor do cadastro de usinas hidráulicas do NEWAVE (hidr.dat).<br><br>"
                                          "Reescrita em C++/Qt 6 do HydroEdit 4.0a (ONS, Rodrigo Vilanova).<br>"
                                          "Layout do registro: 792 bytes, %1 usinas por arquivo.")
                               .arg(modelo_->numUsinas()));
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

void JanelaPrincipal::abrir() {
    if (!confirmarDescarte()) return;
    QString caminho = QFileDialog::getOpenFileName(this, QStringLiteral("Abrir cadastro hidr.dat"), {},
                                                   QStringLiteral("Cadastro NEWAVE (hidr.dat *.dat);;Todos (*.*)"));
    if (!caminho.isEmpty()) abrirCaminho(caminho);
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
    modelo_->definirArquivo(std::move(a), caminho);
    tabela_->resizeColumnsToContents();
    acao_salvar_->setEnabled(true);
    acao_salvar_como_->setEnabled(true);
    acao_exportar_->setEnabled(true);
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

void JanelaPrincipal::salvarComo() {
    QString caminho = QFileDialog::getSaveFileName(this, QStringLiteral("Salvar cadastro como"), modelo_->caminho(),
                                                   QStringLiteral("Cadastro NEWAVE (*.dat)"));
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

void JanelaPrincipal::atualizarTitulo() {
    QString nome = modelo_->caminho().isEmpty() ? QStringLiteral("sem arquivo") : QFileInfo(modelo_->caminho()).fileName();
    QString sujo = modelo_->pilhaUndo()->isClean() ? QString() : QStringLiteral("*");
    setWindowTitle(QStringLiteral("HydroEdit 5 - %1%2").arg(nome, sujo));
}

void JanelaPrincipal::atualizarStatus() {
    status_arquivo_->setText(modelo_->caminho());
    status_usinas_->setText(QStringLiteral("%1 / %2 usinas")
                                .arg(modelo_->arquivo().numUsinasPreenchidas())
                                .arg(modelo_->numUsinas()));
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
    int erros = 0, avisos = 0;
    for (const ProblemaUsina& p : problemas) (p.problema.severidade == Severidade::Erro ? erros : avisos)++;
    if (erros > 0) {
        QMessageBox::critical(this, QStringLiteral("Não é possível salvar"),
                              QStringLiteral("%1 erro(s) de consistência. Corrija os itens do painel de problemas.").arg(erros));
        return false;
    }
    if (avisos > 0) {
        auto r = QMessageBox::question(this, QStringLiteral("Avisos"),
                                       QStringLiteral("%1 aviso(s) de consistência. Salvar mesmo assim?").arg(avisos),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        return r == QMessageBox::Yes;
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

bool JanelaPrincipal::confirmarDescarte() {
    if (modelo_->pilhaUndo()->isClean()) return true;
    auto r = QMessageBox::question(this, QStringLiteral("Alterações não salvas"),
                                   QStringLiteral("Salvar as alterações em %1?").arg(QFileInfo(modelo_->caminho()).fileName()),
                                   QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (r == QMessageBox::Cancel) return false;
    if (r == QMessageBox::Save) {
        salvar();
        return modelo_->pilhaUndo()->isClean();
    }
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
