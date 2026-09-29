#include "delegate_referencia.h"
#include <QComboBox>
#include "dados_deck.h"
#include "modelo_secao_fixa.h"

namespace {
Referencia referenciaDe(const QModelIndex& ix) {
    const QVariant v = ix.data(PAPEL_REFERENCIA);
    return v.isValid() ? static_cast<Referencia>(v.toInt()) : Referencia::Nenhuma;
}
}  // namespace

// Editor das colunas que referenciam outro cadastro do deck: uma lista com "NOME (codigo)" dos itens
// desse cadastro, que grava e fecha ao escolher um item; as demais colunas usam o editor comum.
DelegateReferencia::DelegateReferencia(DadosDeck* dados, QObject* parent) : QStyledItemDelegate(parent), dados_(dados) {}

// Enche a lista com os itens do cadastro e seleciona o codigo atual; codigo que o cadastro nao tem
// entra no fim, como esta, para nao se perder, e vazio vira a opcao "(nenhum)".
void DelegateReferencia::preencher(QComboBox* lista, const DadosDeck& dados, Referencia referencia, const QString& codigo) {
    const QSignalBlocker bloqueio(lista);
    lista->clear();
    lista->addItem(QStringLiteral("(nenhum)"), QString());
    for (const OpcaoReferencia& o : dados.opcoes(referencia)) lista->addItem(o.rotulo, o.codigo);
    const QString chave = DadosDeck::normalizarCodigo(codigo);
    int indice = chave.isEmpty() ? 0 : lista->findData(chave);
    if (indice < 0) {
        lista->addItem(codigo.trimmed(), chave);
        indice = lista->count() - 1;
    }
    lista->setCurrentIndex(indice);
}

QWidget* DelegateReferencia::createEditor(QWidget* parent, const QStyleOptionViewItem& opcao, const QModelIndex& ix) const {
    if (referenciaDe(ix) == Referencia::Nenhuma) return QStyledItemDelegate::createEditor(parent, opcao, ix);
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
    const Referencia referencia = referenciaDe(ix);
    if (!lista || referencia == Referencia::Nenhuma) {
        QStyledItemDelegate::setEditorData(editor, ix);
        return;
    }
    preencher(lista, *dados_, referencia, ix.data(Qt::EditRole).toString());
    lista->showPopup();
}

void DelegateReferencia::setModelData(QWidget* editor, QAbstractItemModel* modelo, const QModelIndex& ix) const {
    auto* lista = qobject_cast<QComboBox*>(editor);
    if (!lista || referenciaDe(ix) == Referencia::Nenhuma) {
        QStyledItemDelegate::setModelData(editor, modelo, ix);
        return;
    }
    modelo->setData(ix, lista->currentData().toString(), Qt::EditRole);
}
