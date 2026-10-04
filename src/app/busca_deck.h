#pragma once
#include <QDialog>
#include <QString>
#include <vector>

class DadosDeck;
class ModeloHidr;
class QLabel;
class QLineEdit;
class QModelIndex;
class QStandardItemModel;
class QTreeView;

struct ResultadoBusca {
    QString nome_padrao;
    int linha = -1;
    int secao = -1;
    int registro = -1;
    QString onde;
    QString texto;
    QString motivo;
};

std::vector<ResultadoBusca> procurarNoDeck(const DadosDeck& dados, const ModeloHidr* hidr, const QString& consulta);

class DialogoBusca : public QDialog {
    Q_OBJECT
public:
    DialogoBusca(const DadosDeck* dados, const ModeloHidr* hidr, QWidget* parent = nullptr);
    void procurar();

signals:
    void escolhido(const QString& nome_padrao, int secao, int registro);

private:
    void escolher(const QModelIndex& indice);

    const DadosDeck* dados_;
    const ModeloHidr* hidr_;
    QLineEdit* consulta_;
    QLabel* resumo_;
    QTreeView* lista_;
    QStandardItemModel* itens_;
};
