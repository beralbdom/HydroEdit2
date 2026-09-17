#include "delegate_numerico.h"
#include <QDoubleValidator>
#include <QIntValidator>
#include <QLineEdit>
#include "modelo_hidr.h"

DelegateNumerico::DelegateNumerico(const ModeloHidr* modelo, QObject* parent)
    : QStyledItemDelegate(parent), modelo_(modelo) {}

QWidget* DelegateNumerico::createEditor(QWidget* parent, const QStyleOptionViewItem& opt, const QModelIndex& ix) const {
    QWidget* w = QStyledItemDelegate::createEditor(parent, opt, ix);
    auto* le = qobject_cast<QLineEdit*>(w);
    const Campo* c = modelo_->campoDaColuna(ix.column());
    if (!le || !c) return w;
    if (c->tipo == TipoCampo::Inteiro) {
        le->setValidator(new QIntValidator(le));
    } else if (c->tipo == TipoCampo::Real) {
        auto* v = new QDoubleValidator(le);
        v->setNotation(QDoubleValidator::ScientificNotation);
        v->setLocale(QLocale::c());
        le->setValidator(v);
    } else {
        le->setMaxLength(c->tamanho_elemento);
    }
    return le;
}
