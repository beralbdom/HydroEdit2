#pragma once
#include <QIcon>
#include <QString>

class QStandardItem;
class QStandardItemModel;

QString iconeDoArquivo(const QString& nome_padrao);
QString iconeDoTema(const QString& tema);
QString iconeDaCategoria(const QString& categoria);
QIcon iconeArvore(const QString& chave, bool esmaecido = false);
void definirIconeArvore(QStandardItem* item, const QString& chave);
void esmaecerIconeArvore(QStandardItem* item, bool esmaecido);
void atualizarIconesArvore(QStandardItemModel* modelo);
