#include "pagina_modificacoes.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QTreeView>
#include <QVBoxLayout>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include "ajuste_colunas.h"
#include "catalogo_newave.h"
#include "dados_deck.h"
#include "delegate_referencia.h"
#include "estilo_arvore.h"
#include "icones_arvore.h"
#include "modelo_hidr.h"
#include "modelo_modif.h"
#include "modif_newave.h"
#include "recursos_tabela.h"

namespace {
constexpr int PAPEL_CHAVE = Qt::UserRole;
constexpr int PAPEL_CATEGORIA = Qt::UserRole + 1;
const QString MODIF = QStringLiteral("modif.dat");

QString outras() { return QStringLiteral("Outras"); }

QString categoriaDa(const QString& chave) {
    for (const PalavraChaveModif& p : palavrasChaveModif())
        if (p.chave == chave) return p.categoria;
    return outras();
}

QString descricaoDa(const QString& chave) {
    for (const PalavraChaveModif& p : palavrasChaveModif())
        if (p.chave == chave) return p.descricao;
    return QStringLiteral("Palavra-chave não reconhecida");
}
}  // namespace

// Aba Modificacoes: arvore com as categorias e as palavras-chave (modificadores) do modif.dat e ao
// lado a tabela editavel dos registros do item
// escolhido (ModeloModif). As palavras-chave que o deck nao usa aparecem desabilitadas, para mostrar
// o que o arquivo admite. Adicionar insere a copia do registro selecionado ou uma modificacao nova
// para qualquer usina; Remover apaga os registros selecionados.
PaginaModificacoes::PaginaModificacoes(const ModeloHidr* modelo, DadosDeck* deck, QWidget* parent)
    : QSplitter(Qt::Horizontal, parent), hidr_(modelo), deck_(deck) {
    setHandleWidth(4);
    arvore_ = new QTreeView(this);
    itens_ = new QStandardItemModel(arvore_);
    arvore_->setModel(itens_);
    arvore_->setHeaderHidden(true);
    estilizarArvore(arvore_);

    auto* direita = new QWidget(this);
    auto* layout = new QVBoxLayout(direita);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);
    auto* topo = new QHBoxLayout;
    cabecalho_ = new QLabel(direita);
    QFont fonte = cabecalho_->font();
    fonte.setPointSize(fonte.pointSize() + 1);
    fonte.setBold(true);
    cabecalho_->setFont(fonte);
    botao_adicionar_ = new QPushButton(QStringLiteral("Adicionar"), direita);
    auto* menu_adicionar = new QMenu(botao_adicionar_);
    botao_adicionar_->setMenu(menu_adicionar);
    connect(menu_adicionar, &QMenu::aboutToShow, this, [this, menu_adicionar] {
        menu_adicionar->clear();
        preencherMenu(menu_adicionar);
    });
    botao_remover_ = new QPushButton(QStringLiteral("Remover"), direita);
    botao_salvar_ = new QPushButton(QStringLiteral("Salvar"), direita);
    topo->addWidget(cabecalho_, 1);
    topo->addWidget(botao_adicionar_);
    topo->addWidget(botao_remover_);
    topo->addWidget(botao_salvar_);
    detalhes_ = new QLabel(direita);
    detalhes_->setEnabled(false);

    modelo_ = new ModeloModif(deck_, hidr_, this);
    tabela_ = new QTableView(direita);
    tabela_->setModel(modelo_);
    tabela_->setItemDelegate(new DelegateReferencia(tabela_));
    tabela_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    tabela_->setAlternatingRowColors(true);
    tabela_->verticalHeader()->setVisible(false);
    tabela_->verticalHeader()->setDefaultSectionSize(20);
    tabela_->horizontalHeader()->setFixedHeight(22);
    preencherLargura(tabela_);
    habilitarRecursos(tabela_);
    definirAcoesExtras(tabela_, [this](QMenu* menu, const QModelIndex&) {
        preencherMenu(menu->addMenu(QStringLiteral("Adicionar")));
        menu->addAction(QStringLiteral("Remover"), this, &PaginaModificacoes::remover)->setEnabled(!linhasSelecionadas().empty());
    });
    layout->addLayout(topo);
    layout->addWidget(detalhes_);
    layout->addWidget(tabela_, 1);

    addWidget(arvore_);
    addWidget(direita);
    setStretchFactor(1, 1);
    setSizes({240, 640});

    connect(arvore_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        const QModelIndexList selecionados = arvore_->selectionModel()->selectedRows();
        if (!selecionados.isEmpty()) mostrar(selecionados.first());
    });
    connect(modelo_, &ModeloModif::valorRecusado, this, [this](const QString& motivo) { atualizarDetalhes(motivo); });
    connect(botao_remover_, &QPushButton::clicked, this, &PaginaModificacoes::remover);
    connect(botao_salvar_, &QPushButton::clicked, this, [this] {
        QString motivo;
        atualizarDetalhes(deck_->salvar(MODIF, &motivo) ? QStringLiteral("salvo") : motivo);
    });
    connect(tabela_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { botao_remover_->setEnabled(!linhasSelecionadas().empty()); });
    connect(deck_, &DadosDeck::recarregado, this, &PaginaModificacoes::recarregar);
    connect(deck_, &DadosDeck::reinterpretado, this, [this](const QString& nome) {
        if (nome == MODIF) recarregar();
        else if (nome == QStringLiteral("patamar.dat") && !chave_atual_.isEmpty()) modelo_->recarregar();
    });
    connect(deck_, &DadosDeck::alterado, this, [this](const QString& nome) {
        if (nome != MODIF) return;
        const ArquivoFixo* modif = deck_->arquivo(MODIF);
        dados_ = modif ? interpretarModif(modif->conteudo()) : ResultadoModif{};
        modelo_->atualizarValores();
        atualizarDetalhes();
    });
    recarregar();
}

// O modif.dat vem do repositorio do deck; a arvore se refaz quando o arquivo e relido com outras
// linhas (registro inserido ou removido, edicao no editor textual), voltando ao item que estava
// escolhido.
void PaginaModificacoes::recarregar() {
    const ArquivoFixo* modif = deck_->arquivo(MODIF);
    caminho_ = deck_->carregado() ? deck_->nomeNoDeck(MODIF) : QString();
    dados_ = modif ? interpretarModif(modif->conteudo()) : ResultadoModif{};
    if (deck_->carregado() && !modif) dados_.erro = deck_->erro(MODIF).toStdString();
    montarArvore();
}

// Com a troca de tema do sistema, os icones das categorias sao refeitos com as cores novas.
void PaginaModificacoes::changeEvent(QEvent* evento) {
    QSplitter::changeEvent(evento);
    if (evento->type() == QEvent::PaletteChange) atualizarIconesArvore(itens_);
}

// Categorias na ordem da tabela de palavras-chave; palavras-chave que o manual nao lista entram em
// "Outras". Fica escolhido o item que estava antes (palavra-chave ou categoria) ou a primeira
// categoria.
void PaginaModificacoes::montarArvore() {
    const QString chave_antes = chave_atual_;
    const QString categoria_antes = categoria_atual_;
    itens_->removeRows(0, itens_->rowCount());
    std::map<QString, int> contagem;
    for (const BlocoModif& bloco : dados_.blocos)
        for (const RegistroModif& registro : bloco.registros) ++contagem[QString::fromStdString(registro.palavra_chave)];

    auto novoItem = [](QStandardItem* pai, const QString& texto) {
        auto* item = new QStandardItem(texto);
        pai->appendRow(item);
        return item;
    };
    QStandardItem* raiz = itens_->invisibleRootItem();
    QStandardItem* escolher = nullptr;

    std::map<QString, QStandardItem*> categorias;
    auto categoria = [&](const QString& nome) {
        auto it = categorias.find(nome);
        if (it != categorias.end()) return it->second;
        QStandardItem* item = novoItem(raiz, nome);
        item->setData(nome, PAPEL_CATEGORIA);
        definirIconeArvore(item, iconeDaCategoria(nome));
        if ((chave_antes.isEmpty() && nome == categoria_antes) || !escolher) escolher = item;
        return categorias[nome] = item;
    };
    auto adicionarChave = [&](const QString& chave, const QString& descricao, const QString& nome_categoria) {
        QStandardItem* item = novoItem(categoria(nome_categoria), chave);
        item->setData(chave, PAPEL_CHAVE);
        item->setToolTip(descricao);
        item->setEnabled(contagem.count(chave) > 0);
        if (!chave_antes.isEmpty() && chave == chave_antes) escolher = item;
    };
    for (const PalavraChaveModif& p : palavrasChaveModif()) adicionarChave(p.chave, p.descricao, p.categoria);
    for (const auto& [chave, n] : contagem)
        if (categoriaDa(chave) == outras()) adicionarChave(chave, descricaoDa(chave), outras());

    arvore_->expandAll();
    if (!escolher) return;
    arvore_->setCurrentIndex(escolher->index());
    mostrar(escolher->index());
}

void PaginaModificacoes::mostrar(const QModelIndex& item) {
    if (!item.isValid()) return;
    chave_atual_ = item.data(PAPEL_CHAVE).toString();
    categoria_atual_ = item.data(PAPEL_CATEGORIA).toString();
    if (!chave_atual_.isEmpty()) {
        cabecalho_->setText(QStringLiteral("%1: %2").arg(chave_atual_, descricaoDa(chave_atual_)));
        modelo_->definirFiltro(chave_atual_, {});
    } else if (!categoria_atual_.isEmpty()) {
        cabecalho_->setText(categoria_atual_);
        const QString categoria = categoria_atual_;
        modelo_->definirFiltro({}, [categoria](const QString& chave) { return categoriaDa(chave) == categoria; });
    } else {
        cabecalho_->setText(QStringLiteral("Todas as modificações"));
        modelo_->definirFiltro({}, [](const QString&) { return true; });
    }
    limparFiltros(tabela_);
    ajustarColunas(tabela_);
    if (selecionar_linha_ >= 0) {
        for (int r = 0; r < modelo_->registros(); ++r)
            if (modelo_->linhaDoArquivo(r) == selecionar_linha_) {
                const QModelIndex ix = modelo_->index(r, std::min(2, modelo_->columnCount() - 1));
                tabela_->setCurrentIndex(ix);
                tabela_->scrollTo(ix);
                break;
            }
        selecionar_linha_ = -1;
    }
    botao_remover_->setEnabled(!linhasSelecionadas().empty());
    atualizarDetalhes();
}

void PaginaModificacoes::atualizarDetalhes(const QString& aviso) {
    const ArquivoFixo* modif = deck_->arquivo(MODIF);
    const QString origem = caminho_.isEmpty() ? MODIF : caminho_.section('/', -1).section('\\', -1);
    QString texto;
    if (caminho_.isEmpty()) texto = origem + QStringLiteral("  ·  abra o hidr.dat de um deck para ver as modificações");
    else if (!dados_.erro.empty()) texto = origem + QStringLiteral("  ·  ") + QString::fromStdString(dados_.erro);
    else {
        texto = QStringLiteral("%1  ·  %2 registros em %3 usinas").arg(origem).arg(modelo_->registros()).arg(modelo_->usinas());
        if (chave_atual_.isEmpty()) texto += QStringLiteral("  ·  escolha o modificador para editar os valores");
        if (modif && modif->modificado()) texto += QStringLiteral("  ·  alterado, não salvo");
    }
    if (!aviso.isEmpty()) texto += QStringLiteral("  ·  ") + aviso;
    detalhes_->setText(texto);
    botao_salvar_->setEnabled(modif && modif->modificado());
    botao_adicionar_->setEnabled(modif != nullptr);
}

// Linhas do arquivo (indice a partir de 0) dos registros selecionados na tabela.
std::vector<int> PaginaModificacoes::linhasSelecionadas() const {
    std::set<int> registros;
    for (const QModelIndex& ix : tabela_->selectionModel()->selectedIndexes()) registros.insert(ix.row());
    if (registros.empty() && tabela_->currentIndex().isValid()) registros.insert(tabela_->currentIndex().row());
    std::vector<int> linhas;
    for (int r : registros)
        if (r < modelo_->registros()) linhas.push_back(modelo_->linhaDoArquivo(r));
    return linhas;
}

// Troca o texto do modif.dat pelas linhas dadas (o repositorio rele o arquivo e a arvore se refaz) e
// deixa selecionado o registro na linha selecionar.
void PaginaModificacoes::substituirLinhas(const std::vector<std::string>& linhas, int selecionar) {
    QStringList texto;
    for (const std::string& l : linhas) texto << QString::fromLatin1(l.c_str());
    QString conteudo = texto.join(QLatin1Char('\n'));
    if (deck_->texto(MODIF).endsWith(QLatin1Char('\n'))) conteudo += QLatin1Char('\n');
    selecionar_linha_ = selecionar;
    deck_->substituirTexto(MODIF, conteudo);
}

void PaginaModificacoes::preencherMenu(QMenu* menu) {
    const bool carregado = deck_->arquivo(MODIF) != nullptr;
    menu->addAction(QStringLiteral("Cópia do registro selecionado"), this, &PaginaModificacoes::adicionarCopia)
        ->setEnabled(carregado && tabela_->currentIndex().isValid());
    menu->addAction(QStringLiteral("Nova modificação..."), this, &PaginaModificacoes::novaModificacao)->setEnabled(carregado);
}

// Copia do registro atual logo depois dele, no mesmo bloco de usina.
void PaginaModificacoes::adicionarCopia() {
    const QModelIndex atual = tabela_->currentIndex();
    const ArquivoFixo* modif = deck_->arquivo(MODIF);
    if (!atual.isValid() || !modif) return;
    const int linha = modelo_->linhaDoArquivo(atual.row());
    std::vector<std::string> linhas = modif->linhas();
    linhas.insert(linhas.begin() + linha + 1, linhas[static_cast<size_t>(linha)]);
    substituirLinhas(linhas, linha + 1);
}

// Pergunta a usina (do cadastro de usinas) e a palavra-chave e insere o registro com valores
// iniciais validos (valoresPadraoModif, com o ano de inicio do estudo do dger.dat), no bloco da
// usina ou num bloco novo no fim do arquivo.
void PaginaModificacoes::novaModificacao() {
    const ArquivoFixo* modif = deck_->arquivo(MODIF);
    if (!modif) return;
    QDialog dialogo(this);
    dialogo.setWindowTitle(QStringLiteral("Nova modificação"));
    auto* form = new QFormLayout(&dialogo);
    auto* usinas = new QComboBox(&dialogo);
    usinas->setMaxVisibleItems(20);
    if (hidr_)
        for (int i = 0; i < hidr_->numUsinas(); ++i) {
            const QString nome = QString::fromLatin1(hidr_->usina(i).nome.c_str()).trimmed();
            if (!nome.isEmpty()) usinas->addItem(QStringLiteral("%1 (%2)").arg(nome).arg(i + 1), i + 1);
        }
    if (tabela_->currentIndex().isValid()) {
        const int indice = usinas->findData(modelo_->usinaDoRegistro(tabela_->currentIndex().row()));
        if (indice >= 0) usinas->setCurrentIndex(indice);
    }
    auto* chaves = new QComboBox(&dialogo);
    chaves->setMaxVisibleItems(25);
    for (const PalavraChaveModif& p : palavrasChaveModif())
        if (!camposModif(p.chave.toStdString(), 1).empty()) chaves->addItem(QStringLiteral("%1: %2").arg(p.chave, p.descricao), p.chave);
    if (const int indice = chaves->findData(chave_atual_); indice >= 0) chaves->setCurrentIndex(indice);
    auto* botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialogo);
    connect(botoes, &QDialogButtonBox::accepted, &dialogo, &QDialog::accept);
    connect(botoes, &QDialogButtonBox::rejected, &dialogo, &QDialog::reject);
    form->addRow(QStringLiteral("Usina"), usinas);
    form->addRow(QStringLiteral("Modificador"), chaves);
    form->addRow(botoes);
    if (dialogo.exec() != QDialog::Accepted || usinas->currentIndex() < 0 || chaves->currentIndex() < 0) return;

    const int usina = usinas->currentData().toInt();
    const QString chave = chaves->currentData().toString();
    int ano = 2000;
    if (const ArquivoFixo* dger = deck_->arquivo(QStringLiteral("dger.dat")))
        for (size_t s = 0; s < dger->secoes().size(); ++s)
            if (dger->secoes()[s].definicao.titulo == "Ano de início do estudo" && !dger->secoes()[s].linhas.empty())
                ano = std::atoi(dger->valor(static_cast<int>(s), 0, 0).c_str());
    std::string registro;
    const Resultado r = montarLinhaModif(" " + chave.toStdString(), valoresPadraoModif(chave.toStdString(), ano),
                                         camposModif(chave.toStdString(), deck_->numeroPatamaresDeCarga()), registro);
    if (!r.ok) {
        atualizarDetalhes(QString::fromUtf8(r.mensagem));
        return;
    }
    const QString nome = hidr_ ? QString::fromLatin1(hidr_->usina(usina - 1).nome.c_str()).trimmed() : QString();
    int nova = -1;
    const std::vector<std::string> linhas = inserirModificacao(modif->linhas(), usina, nome.toLatin1().toStdString(), registro, nova);
    chave_atual_ = chave;
    categoria_atual_.clear();
    substituirLinhas(linhas, nova);
}

// Apaga os registros selecionados; bloco de usina que fica vazio sai junto.
void PaginaModificacoes::remover() {
    const ArquivoFixo* modif = deck_->arquivo(MODIF);
    const std::vector<int> linhas = linhasSelecionadas();
    if (!modif || linhas.empty()) return;
    const int primeira = *std::min_element(linhas.begin(), linhas.end());
    substituirLinhas(removerModificacoes(modif->linhas(), linhas), primeira);
}
