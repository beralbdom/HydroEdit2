#include "modelo_parametros.h"
#include <QPalette>
#include "arquivo_fixo.h"
#include "dados_deck.h"

namespace {
enum Coluna { PARAMETRO, CAMPO, VALOR };
}  // namespace

// Tabela de um arquivo de parametros (layout com parametros = true, uma secao por parametro): uma
// linha por campo de cada parametro, com o nome do parametro so na primeira. O valor e o do primeiro
// registro da secao; parametro que o arquivo nao traz aparece esmaecido e nao se edita.
ModeloParametros::ModeloParametros(DadosDeck* dados, const QString& nome_padrao, QObject* parent)
    : QAbstractTableModel(parent), dados_(dados), nome_(nome_padrao) {
    connect(dados_, &DadosDeck::recarregado, this, &ModeloParametros::refazer);
    connect(dados_, &DadosDeck::reinterpretado, this, [this](const QString& nome) {
        if (nome == nome_) refazer();
    });
    connect(dados_, &DadosDeck::alterado, this, [this](const QString& nome) {
        if (nome == nome_ && rowCount() > 0) emit dataChanged(index(0, VALOR), index(rowCount() - 1, VALOR));
    });
    refazer();
}

void ModeloParametros::refazer() {
    beginResetModel();
    campos_.clear();
    if (const ArquivoFixo* a = arquivo())
        for (size_t s = 0; s < a->secoes().size(); ++s)
            for (size_t c = 0; c < a->secoes()[s].definicao.colunas.size(); ++c)
                campos_.emplace_back(static_cast<int>(s), static_cast<int>(c));
    endResetModel();
}

const ArquivoFixo* ModeloParametros::arquivo() const { return dados_->arquivo(nome_); }

bool ModeloParametros::presente(int linha) const {
    return !arquivo()->secoes()[static_cast<size_t>(campos_[static_cast<size_t>(linha)].first)].linhas.empty();
}

int ModeloParametros::parametros() const {
    const ArquivoFixo* a = arquivo();
    return a ? static_cast<int>(a->secoes().size()) : 0;
}

int ModeloParametros::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(campos_.size()); }

int ModeloParametros::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : 3; }

QVariant ModeloParametros::data(const QModelIndex& ix, int role) const {
    if (!ix.isValid() || !arquivo()) return {};
    const auto [s, c] = campos_[static_cast<size_t>(ix.row())];
    const SecaoLida& secao = arquivo()->secoes()[static_cast<size_t>(s)];
    const ColunaFixa& coluna = secao.definicao.colunas[static_cast<size_t>(c)];
    const bool primeiro = c == 0;
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (ix.column() == PARAMETRO) return primeiro ? QString::fromStdString(secao.definicao.titulo) : QString();
        if (ix.column() == CAMPO)
            return secao.definicao.colunas.size() == 1 && coluna.nome == "Valor" ? QString() : QString::fromStdString(coluna.nome);
        return presente(ix.row()) ? QString::fromLatin1(arquivo()->valor(s, 0, c).c_str()) : QString();
    }
    if (role == Qt::ForegroundRole && !presente(ix.row())) return QPalette().color(QPalette::Disabled, QPalette::Text);
    if (role == Qt::ToolTipRole && ix.column() == VALOR) {
        if (!presente(ix.row())) return QStringLiteral("Parâmetro ausente no arquivo");
        return QStringLiteral("Colunas %1 a %2").arg(coluna.inicio).arg(coluna.fim);
    }
    if (role == Qt::TextAlignmentRole && ix.column() == VALOR)
        return coluna.tipo == TipoColunaFixa::Texto ? int(Qt::AlignLeft | Qt::AlignVCenter) : int(Qt::AlignRight | Qt::AlignVCenter);
    return {};
}

bool ModeloParametros::setData(const QModelIndex& ix, const QVariant& valor, int role) {
    if (!ix.isValid() || ix.column() != VALOR || role != Qt::EditRole || !arquivo() || !presente(ix.row())) return false;
    if (valor.toString().trimmed() == data(ix, Qt::EditRole).toString()) return false;
    const auto [s, c] = campos_[static_cast<size_t>(ix.row())];
    Resultado r = dados_->definir(nome_, s, 0, c, valor.toString());
    if (!r.ok) {
        emit valorRecusado(QString::fromUtf8(r.mensagem));
        return false;
    }
    return true;
}

QVariant ModeloParametros::headerData(int secao, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (secao) {
    case PARAMETRO: return QStringLiteral("Parâmetro");
    case CAMPO: return QStringLiteral("Campo");
    default: return QStringLiteral("Valor");
    }
}

Qt::ItemFlags ModeloParametros::flags(const QModelIndex& ix) const {
    Qt::ItemFlags f = QAbstractTableModel::flags(ix);
    if (ix.isValid() && ix.column() == VALOR && arquivo() && presente(ix.row())) {
        const auto [s, c] = campos_[static_cast<size_t>(ix.row())];
        if (ArquivoFixo::editavel(arquivo()->secoes()[static_cast<size_t>(s)].definicao.colunas[static_cast<size_t>(c)]))
            f |= Qt::ItemIsEditable;
    }
    return f;
}
