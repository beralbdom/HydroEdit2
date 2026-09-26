#include "modelo_secao_fixa.h"

ModeloSecaoFixa::ModeloSecaoFixa(QObject* parent) : QAbstractTableModel(parent) {}

void ModeloSecaoFixa::definirFonte(ArquivoFixo* arquivo, int secao) {
    beginResetModel();
    arquivo_ = arquivo;
    secao_ = secao;
    endResetModel();
}

const SecaoLida* ModeloSecaoFixa::secaoLida() const {
    if (!arquivo_ || secao_ < 0 || secao_ >= static_cast<int>(arquivo_->secoes().size())) return nullptr;
    return &arquivo_->secoes()[static_cast<size_t>(secao_)];
}

int ModeloSecaoFixa::rowCount(const QModelIndex& parent) const {
    const SecaoLida* s = secaoLida();
    return parent.isValid() || !s ? 0 : static_cast<int>(s->linhas.size());
}

int ModeloSecaoFixa::columnCount(const QModelIndex& parent) const {
    const SecaoLida* s = secaoLida();
    return parent.isValid() || !s ? 0 : static_cast<int>(s->definicao.colunas.size());
}

// O texto do arquivo e Latin-1, a codificacao dos decks; numeros ficam alinhados a direita.
QVariant ModeloSecaoFixa::data(const QModelIndex& ix, int role) const {
    const SecaoLida* s = secaoLida();
    if (!s || !ix.isValid()) return {};
    const ColunaFixa& c = s->definicao.colunas[static_cast<size_t>(ix.column())];
    if (role == Qt::DisplayRole || role == Qt::EditRole)
        return QString::fromLatin1(arquivo_->valor(secao_, ix.row(), ix.column()).c_str());
    if (role == Qt::TextAlignmentRole)
        return c.tipo == TipoColunaFixa::Texto ? int(Qt::AlignLeft | Qt::AlignVCenter) : int(Qt::AlignRight | Qt::AlignVCenter);
    return {};
}

// Valor recusado pelo arquivo (numero invalido ou que nao cabe nas colunas) nao muda nada e sai pelo
// sinal valorRecusado, para a pagina mostrar o motivo.
bool ModeloSecaoFixa::setData(const QModelIndex& ix, const QVariant& valor, int role) {
    if (!secaoLida() || !ix.isValid() || role != Qt::EditRole) return false;
    Resultado r = arquivo_->definir(secao_, ix.row(), ix.column(), valor.toString().toLatin1().toStdString());
    if (!r.ok) {
        emit valorRecusado(QString::fromUtf8(r.mensagem));
        return false;
    }
    emit dataChanged(ix, ix, {Qt::DisplayRole, Qt::EditRole});
    emit alterado();
    return true;
}

// O cabecalho mostra o nome da coluna; a dica, as colunas do arquivo e o formato, como no manual.
QVariant ModeloSecaoFixa::headerData(int secao, Qt::Orientation o, int role) const {
    const SecaoLida* s = secaoLida();
    if (!s || o != Qt::Horizontal) return {};
    const ColunaFixa& c = s->definicao.colunas[static_cast<size_t>(secao)];
    if (role == Qt::DisplayRole) return QString::fromStdString(c.nome);
    if (role == Qt::ToolTipRole) {
        const int largura = c.fim - c.inicio + 1;
        QString formato = c.tipo == TipoColunaFixa::Texto     ? QStringLiteral("A%1").arg(largura)
                          : c.tipo == TipoColunaFixa::Inteiro ? QStringLiteral("I%1").arg(largura)
                                                              : QStringLiteral("F%1.%2").arg(largura).arg(c.decimais);
        return QStringLiteral("Colunas %1 a %2, formato %3").arg(c.inicio).arg(c.fim).arg(formato);
    }
    return {};
}

Qt::ItemFlags ModeloSecaoFixa::flags(const QModelIndex& ix) const {
    return QAbstractTableModel::flags(ix) | (ix.isValid() ? Qt::ItemIsEditable : Qt::NoItemFlags);
}
