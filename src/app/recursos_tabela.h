#pragma once
#include <QString>
#include <QStringList>
#include <functional>
#include <vector>

class QMenu;
class QModelIndex;
class QTableView;

std::vector<QStringList> lerTsv(const QString& texto);
QString formatarTsv(const std::vector<QStringList>& linhas);

void habilitarRecursos(QTableView* tabela, bool ordenavel = false);
void definirLinhasOcultas(QTableView* tabela, std::function<bool(int)> oculta);
void limparFiltros(QTableView* tabela);
void definirAcoesExtras(QTableView* tabela, std::function<void(QMenu*, const QModelIndex&)> acoes);
