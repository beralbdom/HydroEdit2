#pragma once
#include <QSplitter>
#include "deck_newave.h"

class QLabel;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class DadosDeck;
class ModeloHidr;

class PaginaModificacoes : public QSplitter {
    Q_OBJECT
public:
    PaginaModificacoes(const ModeloHidr* modelo, DadosDeck* deck, QWidget* parent = nullptr);

private:
    void recarregar();
    void montarArvore();
    void mostrar(QTreeWidgetItem* item);

    const ModeloHidr* modelo_;
    DadosDeck* deck_;
    QTreeWidget* arvore_;
    QLabel* cabecalho_;
    QLabel* detalhes_;
    QTableWidget* tabela_;
    ResultadoModif dados_;
    QString caminho_;
};
