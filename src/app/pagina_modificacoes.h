#pragma once
#include <QSplitter>
#include <vector>
#include "deck_newave.h"

class QLabel;
class QMenu;
class QModelIndex;
class QPushButton;
class QStandardItemModel;
class QTableView;
class QTreeView;
class DadosDeck;
class ModeloHidr;
class ModeloModif;

class PaginaModificacoes : public QSplitter {
    Q_OBJECT
public:
    PaginaModificacoes(const ModeloHidr* modelo, DadosDeck* deck, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* evento) override;

private:
    void recarregar();
    void montarArvore();
    void mostrar(const QModelIndex& item);
    void atualizarDetalhes(const QString& aviso = {});
    std::vector<int> linhasSelecionadas() const;
    void substituirLinhas(const std::vector<std::string>& linhas, int selecionar);
    void adicionarCopia();
    void novaModificacao();
    void remover();
    void preencherMenu(QMenu* menu);

    const ModeloHidr* hidr_;
    DadosDeck* deck_;
    QTreeView* arvore_;
    QStandardItemModel* itens_;
    QLabel* cabecalho_;
    QLabel* detalhes_;
    QPushButton* botao_adicionar_;
    QPushButton* botao_remover_;
    QPushButton* botao_salvar_;
    QTableView* tabela_;
    ModeloModif* modelo_;
    ResultadoModif dados_;
    QString caminho_;
    QString chave_atual_;
    QString categoria_atual_;
    int selecionar_linha_ = -1;
};
