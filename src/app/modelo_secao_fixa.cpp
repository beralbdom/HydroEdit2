#include "modelo_secao_fixa.h"
#include <limits>
#include "dados_deck.h"

// Tabela de uma secao de um arquivo do repositorio do deck. Recarregar o deck ou reler o arquivo a
// partir do editor textual refaz o modelo, porque o numero de registros pode mudar; uma edicao de
// campo, feita aqui ou em outra vista, so repinta as celulas.
ModeloSecaoFixa::ModeloSecaoFixa(DadosDeck* dados, const QString& nome_padrao, int secao, QObject* parent)
    : QAbstractTableModel(parent), dados_(dados), nome_(nome_padrao), secao_(secao) {
    connect(dados_, &DadosDeck::recarregado, this, [this] {
        beginResetModel();
        endResetModel();
    });
    connect(dados_, &DadosDeck::reinterpretado, this, [this](const QString& nome) {
        if (nome != nome_) return;
        beginResetModel();
        endResetModel();
    });
    connect(dados_, &DadosDeck::alterado, this, [this](const QString& nome) {
        if (nome == nome_ && rowCount() > 0 && columnCount() > 0)
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::DisplayRole, Qt::EditRole});
    });
}

void ModeloSecaoFixa::definirSecao(int secao) {
    beginResetModel();
    secao_ = secao;
    endResetModel();
}

const SecaoLida* ModeloSecaoFixa::secaoLida() const {
    const ArquivoFixo* a = dados_->arquivo(nome_);
    if (!a || secao_ < 0 || secao_ >= static_cast<int>(a->secoes().size())) return nullptr;
    return &a->secoes()[static_cast<size_t>(secao_)];
}

int ModeloSecaoFixa::rowCount(const QModelIndex& parent) const {
    const SecaoLida* s = secaoLida();
    return parent.isValid() || !s ? 0 : static_cast<int>(s->linhas.size());
}

int ModeloSecaoFixa::columnCount(const QModelIndex& parent) const {
    const SecaoLida* s = secaoLida();
    return parent.isValid() || !s ? 0 : static_cast<int>(s->definicao.colunas.size());
}

// O texto do arquivo e Latin-1, a codificacao dos decks; numeros ficam alinhados a direita. O papel
// UserRole da o valor para ordenar: numero nas colunas numericas (vazio antes de todos) e texto nas
// demais.
QVariant ModeloSecaoFixa::data(const QModelIndex& ix, int role) const {
    const SecaoLida* s = secaoLida();
    if (!s || !ix.isValid()) return {};
    const ColunaFixa& c = s->definicao.colunas[static_cast<size_t>(ix.column())];
    if (role == Qt::DisplayRole || role == Qt::EditRole)
        return QString::fromLatin1(dados_->arquivo(nome_)->valor(secao_, ix.row(), ix.column()).c_str());
    if (role == Qt::TextAlignmentRole)
        return c.tipo == TipoColunaFixa::Texto ? int(Qt::AlignLeft | Qt::AlignVCenter) : int(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::UserRole) {
        const QString texto = QString::fromLatin1(dados_->arquivo(nome_)->valor(secao_, ix.row(), ix.column()).c_str());
        if (c.tipo == TipoColunaFixa::Texto) return texto;
        bool ok = false;
        const double numero = texto.toDouble(&ok);
        return ok ? numero : -std::numeric_limits<double>::infinity();
    }
    return {};
}

// Valor recusado pelo arquivo (numero invalido ou que nao cabe nas colunas) nao muda nada e sai pelo
// sinal valorRecusado, para a vista mostrar o motivo.
bool ModeloSecaoFixa::setData(const QModelIndex& ix, const QVariant& valor, int role) {
    if (!secaoLida() || !ix.isValid() || role != Qt::EditRole) return false;
    if (valor.toString().trimmed() == data(ix, Qt::EditRole).toString()) return false;
    Resultado r = dados_->definir(nome_, secao_, ix.row(), ix.column(), valor.toString());
    if (!r.ok) {
        emit valorRecusado(QString::fromUtf8(r.mensagem));
        return false;
    }
    return true;
}

// O cabecalho mostra o nome da coluna; a dica, as colunas do arquivo e o formato, como no manual, ou
// de onde vem o valor das colunas somente leitura.
QVariant ModeloSecaoFixa::headerData(int secao, Qt::Orientation o, int role) const {
    const SecaoLida* s = secaoLida();
    if (!s || o != Qt::Horizontal) return {};
    const ColunaFixa& c = s->definicao.colunas[static_cast<size_t>(secao)];
    if (role == Qt::DisplayRole) return QString::fromStdString(c.nome);
    if (role == Qt::ToolTipRole) {
        if (c.tipo == TipoColunaFixa::Ordinal) return QStringLiteral("Posição do registro no bloco; somente leitura");
        const int largura = c.fim - c.inicio + 1;
        QString formato = c.tipo == TipoColunaFixa::Texto     ? QStringLiteral("A%1").arg(largura)
                          : c.tipo == TipoColunaFixa::Inteiro ? QStringLiteral("I%1").arg(largura)
                                                              : QStringLiteral("F%1.%2").arg(largura).arg(c.decimais);
        const QString dica = QStringLiteral("Colunas %1 a %2, formato %3").arg(c.inicio).arg(c.fim).arg(formato);
        return c.contexto < 0 ? dica : dica + QStringLiteral(", da linha que abre o bloco; somente leitura");
    }
    return {};
}

Qt::ItemFlags ModeloSecaoFixa::flags(const QModelIndex& ix) const {
    const SecaoLida* s = secaoLida();
    const bool editavel = s && ix.isValid() && ArquivoFixo::editavel(s->definicao.colunas[static_cast<size_t>(ix.column())]);
    return QAbstractTableModel::flags(ix) | (editavel ? Qt::ItemIsEditable : Qt::NoItemFlags);
}
