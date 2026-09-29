#include "delegate_referencia.h"
#include <QComboBox>
#include "dados_deck.h"
#include "modelo_secao_fixa.h"

namespace {
bool temLista(const QModelIndex& ix) { return ix.data(PAPEL_OPCOES).isValid(); }

std::vector<OpcaoReferencia> opcoesDe(const QModelIndex& ix) {
    std::vector<OpcaoReferencia> opcoes;
    for (const QVariant& item : ix.data(PAPEL_OPCOES).toList()) {
        const QStringList par = item.toStringList();
        opcoes.push_back({par.value(0), par.value(1)});
    }
    return opcoes;
}
}  // namespace

// Enche a lista com os itens e seleciona o codigo atual; codigo que a lista nao tem entra no fim,
// como esta, para nao se perder, e vazio vira a opcao "(nenhum)".
void DelegateReferencia::preencher(QComboBox* lista, const std::vector<OpcaoReferencia>& opcoes, const QString& codigo) {
    const QSignalBlocker bloqueio(lista);
    lista->clear();
    lista->addItem(QStringLiteral("(nenhum)"), QString());
    for (const OpcaoReferencia& o : opcoes) lista->addItem(o.rotulo, o.codigo);
    const QString chave = DadosDeck::normalizarCodigo(codigo);
    int indice = chave.isEmpty() ? 0 : lista->findData(chave);
    if (indice < 0) {
        lista->addItem(codigo.trimmed(), chave);
        indice = lista->count() - 1;
    }
    lista->setCurrentIndex(indice);
}

// Editor das colunas com lista (outro cadastro do deck ou valores fixos do manual): os itens como
// "NOME (codigo)", gravando e fechando ao escolher um; as demais colunas usam o editor comum.
QWidget* DelegateReferencia::createEditor(QWidget* parent, const QStyleOptionViewItem& opcao, const QModelIndex& ix) const {
    if (!temLista(ix)) return QStyledItemDelegate::createEditor(parent, opcao, ix);
    auto* lista = new QComboBox(parent);
    lista->setMaxVisibleItems(20);
    auto* delegate = const_cast<DelegateReferencia*>(this);
    connect(lista, &QComboBox::activated, delegate, [delegate, lista] {
        emit delegate->commitData(lista);
        emit delegate->closeEditor(lista);
    });
    return lista;
}

void DelegateReferencia::setEditorData(QWidget* editor, const QModelIndex& ix) const {
    auto* lista = qobject_cast<QComboBox*>(editor);
    if (!lista || !temLista(ix)) {
        QStyledItemDelegate::setEditorData(editor, ix);
        return;
    }
    preencher(lista, opcoesDe(ix), ix.data(Qt::EditRole).toString());
    lista->showPopup();
}

void DelegateReferencia::setModelData(QWidget* editor, QAbstractItemModel* modelo, const QModelIndex& ix) const {
    auto* lista = qobject_cast<QComboBox*>(editor);
    if (!lista || !temLista(ix)) {
        QStyledItemDelegate::setModelData(editor, modelo, ix);
        return;
    }
    modelo->setData(ix, lista->currentData().toString(), Qt::EditRole);
}
