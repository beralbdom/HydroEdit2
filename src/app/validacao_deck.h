#pragma once
#include <QDialog>
#include <QString>
#include <vector>

struct ArquivoHidr;
class DadosDeck;
class QLabel;
class QModelIndex;
class QStandardItemModel;
class QTreeView;

struct ProblemaDeck {
    bool erro = true;
    QString nome_padrao;
    int secao = -1;
    int registro = -1;
    QString mensagem;
    QString regra;
};

std::vector<ProblemaDeck> validarDeck(const DadosDeck& dados, const ArquivoHidr* hidr);

class DialogoValidacao : public QDialog {
    Q_OBJECT
public:
    DialogoValidacao(const DadosDeck* dados, const ArquivoHidr* hidr, QWidget* parent = nullptr);
    void validar();

signals:
    void escolhido(const QString& nome_padrao, int secao, int registro);

private:
    void escolher(const QModelIndex& indice);

    const DadosDeck* dados_;
    const ArquivoHidr* hidr_;
    QLabel* resumo_;
    QTreeView* lista_;
    QStandardItemModel* itens_;
};
