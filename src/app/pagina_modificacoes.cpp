#include "pagina_modificacoes.h"
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <map>
#include <set>
#include "catalogo_newave.h"
#include "dados_deck.h"
#include "estilo_arvore.h"
#include "modelo_hidr.h"

namespace {
constexpr int PAPEL_CHAVE = Qt::UserRole;
constexpr int PAPEL_CATEGORIA = Qt::UserRole + 1;

QString outras() { return QStringLiteral("Outras"); }

QString categoriaDa(const QString& chave) {
    for (const PalavraChaveModif& p : palavrasChaveModif())
        if (p.chave == chave) return p.categoria;
    return outras();
}

QString descricaoDa(const QString& chave) {
    for (const PalavraChaveModif& p : palavrasChaveModif())
        if (p.chave == chave) return p.descricao;
    return QStringLiteral("Palavra-chave fora da tabela do manual");
}
}  // namespace

// Aba Modificacoes: arvore com todas as modificacoes, as categorias e as palavras-chave do modif.dat
// com a contagem de registros de cada uma, e ao lado a tabela dos registros do item escolhido. As
// palavras-chave que o deck nao usa aparecem desabilitadas, para mostrar o que o arquivo admite.
PaginaModificacoes::PaginaModificacoes(const ModeloHidr* modelo, DadosDeck* deck, QWidget* parent)
    : QSplitter(Qt::Horizontal, parent), modelo_(modelo), deck_(deck) {
    setHandleWidth(4);
    arvore_ = new QTreeView(this);
    itens_ = new QStandardItemModel(0, 2, arvore_);
    itens_->setHorizontalHeaderLabels({QStringLiteral("Tipo"), QStringLiteral("Registros")});
    arvore_->setModel(itens_);
    arvore_->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    arvore_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    arvore_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    arvore_->header()->setStretchLastSection(false);
    estilizarArvore(arvore_);

    auto* direita = new QWidget(this);
    auto* layout = new QVBoxLayout(direita);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);
    cabecalho_ = new QLabel(direita);
    QFont fonte = cabecalho_->font();
    fonte.setPointSize(fonte.pointSize() + 1);
    fonte.setBold(true);
    cabecalho_->setFont(fonte);
    detalhes_ = new QLabel(direita);
    detalhes_->setEnabled(false);
    tabela_ = new QTableWidget(direita);
    tabela_->setColumnCount(5);
    tabela_->setHorizontalHeaderLabels({QStringLiteral("Usina"), QStringLiteral("Nome"), QStringLiteral("Palavra-chave"),
                                        QStringLiteral("Valores"), QStringLiteral("Linha")});
    tabela_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela_->setAlternatingRowColors(true);
    tabela_->verticalHeader()->setVisible(false);
    tabela_->verticalHeader()->setDefaultSectionSize(20);
    tabela_->horizontalHeader()->setFixedHeight(22);
    tabela_->horizontalHeader()->setStretchLastSection(false);
    tabela_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    layout->addWidget(cabecalho_);
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
    connect(deck_, &DadosDeck::recarregado, this, &PaginaModificacoes::recarregar);
    connect(deck_, &DadosDeck::reinterpretado, this, [this](const QString& nome) {
        if (nome == QStringLiteral("modif.dat")) recarregar();
    });
    recarregar();
}

// O modif.dat vem do repositorio do deck, e a arvore se refaz quando o texto dele e editado no
// editor textual.
void PaginaModificacoes::recarregar() {
    const ArquivoFixo* modif = deck_->arquivo(QStringLiteral("modif.dat"));
    caminho_ = deck_->carregado() ? deck_->nomeNoDeck(QStringLiteral("modif.dat")) : QString();
    dados_ = modif ? interpretarModif(modif->conteudo()) : ResultadoModif{};
    if (deck_->carregado() && !modif) dados_.erro = deck_->erro(QStringLiteral("modif.dat")).toStdString();
    montarArvore();
}

// Categorias na ordem da tabela de palavras-chave; palavras-chave que o manual nao lista entram em
// "Outras". A raiz "Todas as modificacoes" fica selecionada ao carregar.
void PaginaModificacoes::montarArvore() {
    itens_->removeRows(0, itens_->rowCount());
    std::map<QString, int> contagem;
    for (const BlocoModif& bloco : dados_.blocos)
        for (const RegistroModif& registro : bloco.registros) ++contagem[QString::fromStdString(registro.palavra_chave)];

    int total = 0;
    for (const auto& [chave, n] : contagem) total += n;
    struct Linha {
        QStandardItem* item;
        QStandardItem* registros;
    };
    auto novaLinha = [](QStandardItem* pai, const QString& texto, int n) {
        Linha linha{new QStandardItem(texto), new QStandardItem(QString::number(n))};
        pai->appendRow({linha.item, linha.registros});
        return linha;
    };
    const Linha raiz = novaLinha(itens_->invisibleRootItem(), QStringLiteral("Todas as modificações"), total);

    std::map<QString, Linha> categorias;
    auto categoria = [&](const QString& nome) {
        auto it = categorias.find(nome);
        if (it != categorias.end()) return it->second;
        Linha linha = novaLinha(raiz.item, nome, 0);
        linha.item->setData(nome, PAPEL_CATEGORIA);
        return categorias[nome] = linha;
    };
    auto adicionarChave = [&](const QString& chave, const QString& descricao, const QString& nome_categoria) {
        const Linha pai = categoria(nome_categoria);
        int n = contagem.count(chave) ? contagem[chave] : 0;
        const Linha linha = novaLinha(pai.item, chave, n);
        linha.item->setData(chave, PAPEL_CHAVE);
        linha.item->setToolTip(descricao);
        linha.item->setEnabled(n > 0);
        linha.registros->setEnabled(n > 0);
        pai.registros->setText(QString::number(pai.registros->text().toInt() + n));
    };
    for (const PalavraChaveModif& p : palavrasChaveModif()) adicionarChave(p.chave, p.descricao, p.categoria);
    for (const auto& [chave, n] : contagem)
        if (categoriaDa(chave) == outras()) adicionarChave(chave, descricaoDa(chave), outras());

    arvore_->expandAll();
    arvore_->setCurrentIndex(raiz.item->index());
}

void PaginaModificacoes::mostrar(const QModelIndex& item) {
    if (!item.isValid()) return;
    const QString chave = item.data(PAPEL_CHAVE).toString();
    const QString categoria = item.data(PAPEL_CATEGORIA).toString();
    if (!chave.isEmpty()) cabecalho_->setText(QStringLiteral("%1: %2").arg(chave, descricaoDa(chave)));
    else if (!categoria.isEmpty()) cabecalho_->setText(categoria);
    else cabecalho_->setText(QStringLiteral("Todas as modificações"));

    struct Linha {
        const BlocoModif* bloco;
        const RegistroModif* registro;
    };
    std::vector<Linha> linhas;
    std::set<int> usinas;
    for (const BlocoModif& bloco : dados_.blocos) {
        for (const RegistroModif& registro : bloco.registros) {
            const QString chave_registro = QString::fromStdString(registro.palavra_chave);
            bool entra = !chave.isEmpty() ? chave_registro == chave
                         : !categoria.isEmpty() ? categoriaDa(chave_registro) == categoria
                                                : true;
            if (!entra) continue;
            linhas.push_back({&bloco, &registro});
            usinas.insert(bloco.usina);
        }
    }

    const QString origem = QStringLiteral("%1  ·  manual do NEWAVE, seção 3.12")
                               .arg(caminho_.isEmpty() ? QStringLiteral("modif.dat") : caminho_.section('/', -1).section('\\', -1));
    if (caminho_.isEmpty()) detalhes_->setText(origem + QStringLiteral("  ·  abra o hidr.dat de um deck para ver as modificações"));
    else if (!dados_.erro.empty()) detalhes_->setText(origem + QStringLiteral("  ·  ") + QString::fromStdString(dados_.erro));
    else
        detalhes_->setText(QStringLiteral("%1  ·  %2 registros em %3 usinas  ·  somente leitura")
                               .arg(origem)
                               .arg(linhas.size())
                               .arg(usinas.size()));

    tabela_->setSortingEnabled(false);
    tabela_->setRowCount(static_cast<int>(linhas.size()));
    for (int i = 0; i < static_cast<int>(linhas.size()); ++i) {
        const BlocoModif& bloco = *linhas[static_cast<size_t>(i)].bloco;
        const RegistroModif& registro = *linhas[static_cast<size_t>(i)].registro;
        QString nome = QString::fromLatin1(bloco.comentario.c_str());
        if (modelo_ && bloco.usina >= 1 && bloco.usina <= modelo_->numUsinas()) {
            QString do_cadastro = QString::fromLatin1(modelo_->usina(bloco.usina - 1).nome.c_str()).trimmed();
            if (!do_cadastro.isEmpty()) nome = do_cadastro;
        }
        QStringList valores;
        for (const std::string& v : registro.valores) valores << QString::fromLatin1(v.c_str());

        auto* codigo = new QTableWidgetItem;
        codigo->setData(Qt::DisplayRole, bloco.usina);
        codigo->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* linha = new QTableWidgetItem;
        linha->setData(Qt::DisplayRole, registro.linha);
        linha->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tabela_->setItem(i, 0, codigo);
        tabela_->setItem(i, 1, new QTableWidgetItem(nome));
        tabela_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(registro.palavra_chave)));
        tabela_->setItem(i, 3, new QTableWidgetItem(valores.join(QStringLiteral("  "))));
        tabela_->setItem(i, 4, linha);
    }
    tabela_->setSortingEnabled(true);
    for (int c : {0, 1, 2, 4}) tabela_->resizeColumnToContents(c);
}
