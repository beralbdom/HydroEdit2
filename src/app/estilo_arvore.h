#pragma once
#include <QColor>
#include <QIcon>
#include <QPalette>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;

enum class IconeArvore { Hidro, Termica, Tabela, Previa };

QColor corPainelAbas(const QPalette& paleta);
QIcon iconeArvore(IconeArvore tipo, const QColor& cor, qreal escala);
void estilizarArvore(QTreeWidget* arvore);
QTreeWidgetItem* novoGrupoArvore(QTreeWidget* arvore, const QString& titulo);
