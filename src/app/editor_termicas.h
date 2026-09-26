#pragma once
#include <QSplitter>

class QSortFilterProxyModel;
class QTableView;
class DadosDeck;
class FormularioTermica;

class EditorTermicas : public QSplitter {
    Q_OBJECT
public:
    explicit EditorTermicas(DadosDeck* dados, QWidget* parent = nullptr);

private:
    void selecionarPrimeira();

    QTableView* tabela_;
    QSortFilterProxyModel* ordenacao_;
    FormularioTermica* formulario_;
};
