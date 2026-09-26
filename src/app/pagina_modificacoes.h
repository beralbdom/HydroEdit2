#pragma once
#include <QSplitter>
#include "deck_newave.h"

class QLabel;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class ModeloHidr;

class PaginaModificacoes : public QSplitter {
    Q_OBJECT
public:
    explicit PaginaModificacoes(const ModeloHidr* modelo, QWidget* parent = nullptr);
    void carregar(const QString& caminho_modif);

private:
    void montarArvore();
    void mostrar(QTreeWidgetItem* item);

    const ModeloHidr* modelo_;
    QTreeWidget* arvore_;
    QLabel* cabecalho_;
    QLabel* detalhes_;
    QTableWidget* tabela_;
    ResultadoModif dados_;
    QString caminho_;
};
