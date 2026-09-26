#pragma once
#include <QTabWidget>
#include <vector>
#include "estilo_arvore.h"

class QTreeWidget;
class QTreeWidgetItem;
class DadosDeck;
class ModeloHidr;
class PaginaArquivo;
class PaginaModificacoes;

class NavegadorDeck : public QTabWidget {
    Q_OBJECT
public:
    NavegadorDeck(QWidget* editor_hidr, const ModeloHidr* modelo, QWidget* parent = nullptr);
    void carregarDeck(const QString& dir_deck);
    QStringList arquivosModificados() const;
    bool salvarTodos();

private:
    struct ItemArquivo {
        QTreeWidgetItem* item;
        QString titulo;
        QString nome_padrao;
        IconeArvore icone;
        PaginaArquivo* previa;
    };

    void atualizarItens();

    DadosDeck* dados_;
    std::vector<PaginaArquivo*> paginas_;
    PaginaModificacoes* modificacoes_;
    std::vector<ItemArquivo> itens_;
};
