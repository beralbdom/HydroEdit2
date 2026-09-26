#pragma once
#include <QColor>
#include <QPalette>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;

QColor corPainelAbas(const QPalette& paleta);
void estilizarArvore(QTreeWidget* arvore);
QTreeWidgetItem* novoGrupoArvore(QTreeWidget* arvore, const QString& titulo);
