#pragma once
#include <QSplitter>
#include "deck_newave.h"

class QLabel;
class QModelIndex;
class QTableWidget;
class QStandardItemModel;
class QTreeView;
class DadosDeck;
class ModeloHidr;

class PaginaModificacoes : public QSplitter {
    Q_OBJECT
public:
    PaginaModificacoes(const ModeloHidr* modelo, DadosDeck* deck, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* evento) override;

private:
    void recarregar();
    void montarArvore();
    void mostrar(const QModelIndex& item);

    const ModeloHidr* modelo_;
    DadosDeck* deck_;
    QTreeView* arvore_;
    QStandardItemModel* itens_;
    QLabel* cabecalho_;
    QLabel* detalhes_;
    QTableWidget* tabela_;
    ResultadoModif dados_;
    QString caminho_;
};
