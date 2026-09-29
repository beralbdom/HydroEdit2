#pragma once
#include <QStyledItemDelegate>

class QComboBox;
class DadosDeck;
enum class Referencia;

class DelegateReferencia : public QStyledItemDelegate {
    Q_OBJECT
public:
    DelegateReferencia(DadosDeck* dados, QObject* parent = nullptr);
    static void preencher(QComboBox* lista, const DadosDeck& dados, Referencia referencia, const QString& codigo);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& opcao, const QModelIndex& ix) const override;
    void setEditorData(QWidget* editor, const QModelIndex& ix) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* modelo, const QModelIndex& ix) const override;

private:
    DadosDeck* dados_;
};
