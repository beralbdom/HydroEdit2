#include "pagina_modificacoes.h"
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <map>
#include <set>
#include "catalogo_newave.h"
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
PaginaModificacoes::PaginaModificacoes(const ModeloHidr* modelo, QWidget* parent)
    : QSplitter(Qt::Horizontal, parent), modelo_(modelo) {
    setHandleWidth(4);
    arvore_ = new QTreeWidget(this);
    arvore_->setColumnCount(2);
    arvore_->setHeaderLabels({QStringLiteral("Tipo"), QStringLiteral("Registros")});
    arvore_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    arvore_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    arvore_->header()->setStretchLastSection(false);

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

    connect(arvore_, &QTreeWidget::itemSelectionChanged, this, [this] {
        const QList<QTreeWidgetItem*> selecionados = arvore_->selectedItems();
        if (!selecionados.isEmpty()) mostrar(selecionados.first());
    });
    carregar({});
}

void PaginaModificacoes::carregar(const QString& caminho_modif) {
    caminho_ = caminho_modif;
    dados_ = caminho_modif.isEmpty() ? ResultadoModif{} : lerModif(std::filesystem::path(caminho_modif.toStdWString()));
    montarArvore();
}

// Categorias na ordem da tabela de palavras-chave; palavras-chave que o manual nao lista entram em
// "Outras". A raiz "Todas as modificacoes" fica selecionada ao carregar.
void PaginaModificacoes::montarArvore() {
    arvore_->clear();
    std::map<QString, int> contagem;
    for (const BlocoModif& bloco : dados_.blocos)
        for (const RegistroModif& registro : bloco.registros) ++contagem[QString::fromStdString(registro.palavra_chave)];

    int total = 0;
    for (const auto& [chave, n] : contagem) total += n;
    auto* raiz = new QTreeWidgetItem(arvore_, {QStringLiteral("Todas as modificações"), QString::number(total)});

    std::map<QString, QTreeWidgetItem*> categorias;
    auto categoria = [&](const QString& nome) {
        auto it = categorias.find(nome);
        if (it != categorias.end()) return it->second;
        auto* item = new QTreeWidgetItem(raiz, {nome, QStringLiteral("0")});
        item->setData(0, PAPEL_CATEGORIA, nome);
        return categorias[nome] = item;
    };
    auto adicionarChave = [&](const QString& chave, const QString& descricao, const QString& nome_categoria) {
        QTreeWidgetItem* pai = categoria(nome_categoria);
        int n = contagem.count(chave) ? contagem[chave] : 0;
        auto* item = new QTreeWidgetItem(pai, {chave, QString::number(n)});
        item->setData(0, PAPEL_CHAVE, chave);
        item->setToolTip(0, descricao);
        item->setDisabled(n == 0);
        pai->setText(1, QString::number(pai->text(1).toInt() + n));
    };
    for (const PalavraChaveModif& p : palavrasChaveModif()) adicionarChave(p.chave, p.descricao, p.categoria);
    for (const auto& [chave, n] : contagem)
        if (categoriaDa(chave) == outras()) adicionarChave(chave, descricaoDa(chave), outras());

    for (int i = 0; i < 2; ++i) arvore_->headerItem()->setTextAlignment(i, Qt::AlignLeft);
    arvore_->expandAll();
    arvore_->setCurrentItem(raiz);
}

void PaginaModificacoes::mostrar(QTreeWidgetItem* item) {
    if (!item) return;
    const QString chave = item->data(0, PAPEL_CHAVE).toString();
    const QString categoria = item->data(0, PAPEL_CATEGORIA).toString();
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
