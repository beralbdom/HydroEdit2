#pragma once
#include <QColor>
#include <QPalette>
#include <QString>

class QStandardItem;
class QStandardItemModel;
class QTreeView;

QColor corPainelAbas(const QPalette& paleta);
void estilizarArvore(QTreeView* arvore);
QStandardItem* novoGrupoArvore(QStandardItemModel* modelo, const QString& titulo);
