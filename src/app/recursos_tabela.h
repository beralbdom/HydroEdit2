#pragma once
#include <QString>
#include <QStringList>
#include <Qt>
#include <functional>
#include <vector>

class QMenu;
class QModelIndex;
class QTableView;

class EdicaoEmLote {
public:
    virtual ~EdicaoEmLote() = default;
    virtual void iniciarLote() = 0;
    virtual void concluirLote() = 0;
};

std::vector<QStringList> lerTsv(const QString& texto);
QString formatarTsv(const std::vector<QStringList>& linhas);

void habilitarRecursos(QTableView* tabela, bool ordenavel = false);
void definirLinhasOcultas(QTableView* tabela, std::function<bool(int)> oculta);
void limparFiltros(QTableView* tabela);
void ordenarTabela(QTableView* tabela, int coluna, Qt::SortOrder ordem);
void definirAcoesExtras(QTableView* tabela, std::function<void(QMenu*, const QModelIndex&)> acoes);
