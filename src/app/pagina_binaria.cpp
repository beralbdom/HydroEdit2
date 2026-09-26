#include "pagina_binaria.h"
#include <QAbstractTableModel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <algorithm>
#include <functional>
#include "dados_deck.h"
#include "layouts_newave.h"

namespace {
const QString POSTOS = QStringLiteral("postos.dat");
const QString VAZOES = QStringLiteral("vazoes.dat");

// Primeiro ano do historico: o menor ano inicial entre os postos com nome no postos.dat, como o
// DeckLookup; e o ano do primeiro registro do vazoes.dat. 0 sem postos.
int anoInicial(const DadosDeck* dados) {
    const ArquivoBinario* postos = dados->arquivoBinario(POSTOS);
    int ano = 0;
    for (int r = 0; postos && r < postos->registros(); ++r) {
        const int inicio = postos->inteiro(r, postos_dat::ANO_INICIAL);
        if (!postos->texto(r, postos_dat::NOME, postos_dat::TAMANHO_NOME).empty() && inicio > 0 && (ano == 0 || inicio < ano))
            ano = inicio;
    }
    return ano;
}
}  // namespace

// Tabela do postos.dat, um posto por linha: codigo (a posicao do registro), nome e anos inicial e
// final do historico de vazoes.
class ModeloPostos : public QAbstractTableModel {
public:
    enum Coluna { POSTO, NOME, ANO_INICIAL, ANO_FINAL, COLUNAS };

    ModeloPostos(DadosDeck* dados, QObject* parent) : QAbstractTableModel(parent), dados_(dados) {
        QObject::connect(dados_, &DadosDeck::recarregado, this, [this] {
            beginResetModel();
            endResetModel();
        });
        QObject::connect(dados_, &DadosDeck::alterado, this, [this](const QString& nome) {
            if (nome == POSTOS && rowCount() > 0) emit dataChanged(index(0, 0), index(rowCount() - 1, COLUNAS - 1));
        });
    }

    std::function<void(const QString&)> recusado;

    int rowCount(const QModelIndex& parent = {}) const override {
        const ArquivoBinario* a = dados_->arquivoBinario(POSTOS);
        return parent.isValid() || !a ? 0 : a->registros();
    }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : COLUNAS; }

    QVariant data(const QModelIndex& ix, int role) const override {
        const ArquivoBinario* a = dados_->arquivoBinario(POSTOS);
        if (!a || !ix.isValid()) return {};
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            switch (ix.column()) {
            case POSTO: return ix.row() + 1;
            case NOME: return QString::fromLatin1(a->texto(ix.row(), postos_dat::NOME, postos_dat::TAMANHO_NOME).c_str());
            case ANO_INICIAL: return a->inteiro(ix.row(), postos_dat::ANO_INICIAL);
            default: return a->inteiro(ix.row(), postos_dat::ANO_FINAL);
            }
        }
        if (role == Qt::TextAlignmentRole)
            return ix.column() == NOME ? int(Qt::AlignLeft | Qt::AlignVCenter) : int(Qt::AlignRight | Qt::AlignVCenter);
        if (role == Qt::ForegroundRole && a->texto(ix.row(), postos_dat::NOME, postos_dat::TAMANHO_NOME).empty())
            return QPalette().color(QPalette::Disabled, QPalette::Text);
        return {};
    }

    bool setData(const QModelIndex& ix, const QVariant& valor, int role) override {
        if (!ix.isValid() || role != Qt::EditRole || ix.column() == POSTO) return false;
        if (valor.toString().trimmed() == data(ix, Qt::EditRole).toString()) return false;
        Resultado r = ix.column() == NOME
                          ? dados_->definirTextoBinario(POSTOS, ix.row(), postos_dat::NOME, postos_dat::TAMANHO_NOME, valor.toString())
                          : dados_->definirInteiroBinario(POSTOS, ix.row(),
                                                          ix.column() == ANO_INICIAL ? postos_dat::ANO_INICIAL : postos_dat::ANO_FINAL,
                                                          valor.toString());
        if (!r.ok && recusado) recusado(QString::fromUtf8(r.mensagem));
        return r.ok;
    }

    QVariant headerData(int secao, Qt::Orientation o, int role) const override {
        if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
        switch (secao) {
        case POSTO: return QStringLiteral("Posto");
        case NOME: return QStringLiteral("Nome");
        case ANO_INICIAL: return QStringLiteral("Ano inicial");
        default: return QStringLiteral("Ano final");
        }
    }

    Qt::ItemFlags flags(const QModelIndex& ix) const override {
        return QAbstractTableModel::flags(ix) | (ix.isValid() && ix.column() != POSTO ? Qt::ItemIsEditable : Qt::NoItemFlags);
    }

private:
    DadosDeck* dados_;
};

// Vazoes de um posto do vazoes.dat, uma linha por ano e uma coluna por mes, em m3/s. O registro k do
// arquivo e o mes k a partir de janeiro do primeiro ano do historico; meses depois do ultimo registro
// ficam vazios e nao se editam.
class ModeloVazoes : public QAbstractTableModel {
public:
    ModeloVazoes(DadosDeck* dados, QObject* parent) : QAbstractTableModel(parent), dados_(dados) {
        QObject::connect(dados_, &DadosDeck::recarregado, this, [this] {
            beginResetModel();
            endResetModel();
        });
        QObject::connect(dados_, &DadosDeck::alterado, this, [this](const QString& nome) {
            if ((nome == VAZOES || nome == POSTOS) && rowCount() > 0) {
                emit dataChanged(index(0, 0), index(rowCount() - 1, 12));
                emit headerDataChanged(Qt::Vertical, 0, rowCount() - 1);
            }
        });
    }

    std::function<void(const QString&)> recusado;

    void definirPosto(int posto) {
        beginResetModel();
        posto_ = posto;
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = {}) const override {
        const ArquivoBinario* a = dados_->arquivoBinario(VAZOES);
        return parent.isValid() || !a || posto_ < 1 ? 0 : (a->registros() + 11) / 12;
    }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 13; }

    QVariant data(const QModelIndex& ix, int role) const override {
        const ArquivoBinario* a = dados_->arquivoBinario(VAZOES);
        if (!a || !ix.isValid()) return {};
        if (role == Qt::TextAlignmentRole) return int(Qt::AlignRight | Qt::AlignVCenter);
        if (role != Qt::DisplayRole && role != Qt::EditRole) return {};
        if (ix.column() == 0) {
            const int ano = anoInicial(dados_);
            return ano > 0 ? QVariant(ano + ix.row()) : QVariant(QStringLiteral("%1º").arg(ix.row() + 1));
        }
        const int mes = mesDoArquivo(ix);
        return mes < a->registros() ? QVariant(a->inteiro(mes, 4 * (posto_ - 1))) : QVariant();
    }

    bool setData(const QModelIndex& ix, const QVariant& valor, int role) override {
        if (!(flags(ix) & Qt::ItemIsEditable) || role != Qt::EditRole) return false;
        if (valor.toString().trimmed() == data(ix, Qt::EditRole).toString()) return false;
        Resultado r = dados_->definirInteiroBinario(VAZOES, mesDoArquivo(ix), 4 * (posto_ - 1), valor.toString());
        if (!r.ok && recusado) recusado(QString::fromUtf8(r.mensagem));
        return r.ok;
    }

    QVariant headerData(int secao, Qt::Orientation o, int role) const override {
        static const char* meses[] = {"Jan", "Fev", "Mar", "Abr", "Mai", "Jun", "Jul", "Ago", "Set", "Out", "Nov", "Dez"};
        if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
        return secao == 0 ? QStringLiteral("Ano") : QString::fromLatin1(meses[secao - 1]);
    }

    Qt::ItemFlags flags(const QModelIndex& ix) const override {
        const ArquivoBinario* a = dados_->arquivoBinario(VAZOES);
        const bool editavel = a && ix.isValid() && ix.column() > 0 && mesDoArquivo(ix) < a->registros();
        return QAbstractTableModel::flags(ix) | (editavel ? Qt::ItemIsEditable : Qt::NoItemFlags);
    }

private:
    static int mesDoArquivo(const QModelIndex& ix) { return 12 * ix.row() + ix.column() - 1; }

    DadosDeck* dados_;
    int posto_ = 0;
};

// Pagina de um arquivo binario do deck, no molde das paginas de colunas fixas: titulo, botao Salvar
// habilitado so com alteracao pendente e linha de detalhes (arquivo, secao do manual, resumo do
// conteudo e ultimo aviso). As subclasses poem o conteudo em layout_.
PaginaBinaria::PaginaBinaria(const QString& titulo, const QString& nome_padrao, const QString& secao_manual, DadosDeck* dados,
                             QWidget* parent)
    : QWidget(parent), nome_(nome_padrao), dados_(dados), secao_manual_(secao_manual) {
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(6, 4, 6, 4);
    layout_->setSpacing(4);

    auto* cabecalho = new QHBoxLayout;
    auto* rotulo = new QLabel(titulo, this);
    QFont fonte = rotulo->font();
    fonte.setPointSize(fonte.pointSize() + 1);
    fonte.setBold(true);
    rotulo->setFont(fonte);
    botao_salvar_ = new QPushButton(QStringLiteral("Salvar"), this);
    cabecalho->addWidget(rotulo, 1);
    cabecalho->addWidget(botao_salvar_);
    layout_->addLayout(cabecalho);

    detalhes_ = new QLabel(this);
    detalhes_->setEnabled(false);
    layout_->addWidget(detalhes_);

    connect(botao_salvar_, &QPushButton::clicked, this, [this] {
        QString motivo;
        atualizar(dados_->salvar(nome_, &motivo) ? QStringLiteral("salvo") : motivo);
    });
    connect(dados_, &DadosDeck::alterado, this, [this](const QString& nome) {
        if (nome == nome_) atualizar();
    });
    connect(dados_, &DadosDeck::recarregado, this, [this] { atualizar(); });
}

QTableView* PaginaBinaria::novaTabela() {
    auto* tabela = new QTableView(this);
    tabela->setAlternatingRowColors(true);
    tabela->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    tabela->verticalHeader()->setVisible(false);
    tabela->verticalHeader()->setDefaultSectionSize(20);
    tabela->horizontalHeader()->setFixedHeight(22);
    layout_->addWidget(tabela, 1);
    return tabela;
}

void PaginaBinaria::atualizar(const QString& aviso) {
    const QString secao = QStringLiteral("manual do NEWAVE, seção %1").arg(secao_manual_);
    const ArquivoBinario* arquivo = dados_->arquivoBinario(nome_);
    QString texto;
    if (!dados_->carregado()) texto = QStringLiteral("%1  ·  %2  ·  abra o hidr.dat de um deck para editar").arg(nome_, secao);
    else if (!arquivo) texto = QStringLiteral("%1  ·  %2  ·  %3").arg(nome_, secao, dados_->erro(nome_));
    else
        texto = QStringLiteral("%1  ·  %2  ·  %3%4")
                    .arg(nome_, secao, resumo(), arquivo->modificado() ? QStringLiteral("  ·  alterado, não salvo") : QString());
    if (!aviso.isEmpty()) texto += QStringLiteral("  ·  ") + aviso;
    detalhes_->setText(texto);
    botao_salvar_->setEnabled(arquivo && arquivo->modificado());
}

PaginaPostos::PaginaPostos(DadosDeck* dados, QWidget* parent)
    : PaginaBinaria(QStringLiteral("Postos fluviométricos"), POSTOS, QStringLiteral("3.10"), dados, parent) {
    modelo_ = new ModeloPostos(dados_, this);
    modelo_->recusado = [this](const QString& motivo) { atualizar(motivo); };
    QTableView* tabela = novaTabela();
    tabela->setModel(modelo_);
    connect(dados_, &DadosDeck::recarregado, tabela, [tabela] { tabela->resizeColumnsToContents(); });
    tabela->resizeColumnsToContents();
    atualizar();
}

QString PaginaPostos::resumo() const {
    const ArquivoBinario* a = dados_->arquivoBinario(POSTOS);
    int com_nome = 0;
    for (int r = 0; r < a->registros(); ++r)
        if (!a->texto(r, postos_dat::NOME, postos_dat::TAMANHO_NOME).empty()) ++com_nome;
    return QStringLiteral("%1 postos, %2 com nome").arg(a->registros()).arg(com_nome);
}

// Vazoes de um posto por vez, escolhido na lista (codigo e nome do postos.dat); a lista se refaz
// quando o deck muda ou um nome e editado, mantendo o posto escolhido.
PaginaVazoes::PaginaVazoes(DadosDeck* dados, QWidget* parent)
    : PaginaBinaria(QStringLiteral("Vazões históricas"), VAZOES, QStringLiteral("3.14"), dados, parent) {
    auto* linha = new QHBoxLayout;
    linha->addWidget(new QLabel(QStringLiteral("Posto:"), this));
    postos_ = new QComboBox(this);
    postos_->setMinimumContentsLength(22);
    linha->addWidget(postos_);
    linha->addStretch(1);
    layout_->addLayout(linha);

    modelo_ = new ModeloVazoes(dados_, this);
    modelo_->recusado = [this](const QString& motivo) { atualizar(motivo); };
    tabela_ = novaTabela();
    tabela_->setModel(modelo_);

    connect(postos_, &QComboBox::currentIndexChanged, this, [this](int i) {
        modelo_->definirPosto(i + 1);
        tabela_->resizeColumnsToContents();
    });
    connect(dados_, &DadosDeck::recarregado, this, &PaginaVazoes::preencherPostos);
    connect(dados_, &DadosDeck::alterado, this, [this](const QString& nome) {
        if (nome == POSTOS) preencherPostos();
    });
    preencherPostos();
}

void PaginaVazoes::preencherPostos() {
    const int atual = std::max(0, postos_->currentIndex());
    const ArquivoBinario* postos = dados_->arquivoBinario(POSTOS);
    const ArquivoBinario* vazoes = dados_->arquivoBinario(VAZOES);
    const int n = vazoes ? vazoes->tamanhoRegistro() / 4 : 0;
    const QSignalBlocker bloqueio(postos_);
    postos_->clear();
    for (int p = 1; p <= n; ++p) {
        const QString nome = postos && p <= postos->registros()
                                 ? QString::fromLatin1(postos->texto(p - 1, postos_dat::NOME, postos_dat::TAMANHO_NOME).c_str())
                                 : QString();
        postos_->addItem(nome.isEmpty() ? QString::number(p) : QStringLiteral("%1  %2").arg(p).arg(nome));
    }
    postos_->setCurrentIndex(n > 0 ? std::min(atual, n - 1) : -1);
    modelo_->definirPosto(n > 0 ? postos_->currentIndex() + 1 : 0);
    tabela_->resizeColumnsToContents();
    atualizar();
}

QString PaginaVazoes::resumo() const {
    const ArquivoBinario* a = dados_->arquivoBinario(VAZOES);
    const int ano = anoInicial(dados_);
    const int meses = a->registros();
    QString texto = QStringLiteral("%1 postos, %2 meses").arg(a->tamanhoRegistro() / 4).arg(meses);
    if (ano > 0 && meses > 0) texto += QStringLiteral(" (jan/%1 a %2/%3)").arg(ano).arg((meses - 1) % 12 + 1, 2, 10, QChar('0')).arg(ano + (meses - 1) / 12);
    return texto;
}
