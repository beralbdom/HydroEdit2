#pragma once
#include <QStyledItemDelegate>

class ModeloHidr;

// Editor de celula com validador numerico conforme o tipo do campo da coluna.
class DelegateNumerico : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit DelegateNumerico(const ModeloHidr* modelo, QObject* parent = nullptr);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& opt, const QModelIndex& ix) const override;
private:
    const ModeloHidr* modelo_;
};
