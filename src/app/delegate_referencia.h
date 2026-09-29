#pragma once
#include <QStyledItemDelegate>
#include <vector>

class QComboBox;
struct OpcaoReferencia;

class DelegateReferencia : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    static void preencher(QComboBox* lista, const std::vector<OpcaoReferencia>& opcoes, const QString& codigo);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& opcao, const QModelIndex& ix) const override;
    void setEditorData(QWidget* editor, const QModelIndex& ix) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* modelo, const QModelIndex& ix) const override;
};
