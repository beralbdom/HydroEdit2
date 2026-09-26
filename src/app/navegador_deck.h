#pragma once
#include <QTabWidget>
#include <vector>

class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class DadosDeck;
class EditorTextoArquivo;
class ModeloHidr;
class PaginaModificacoes;

class NavegadorDeck : public QTabWidget {
    Q_OBJECT
public:
    NavegadorDeck(QWidget* editor_hidr, const ModeloHidr* modelo, QWidget* parent = nullptr);
    void carregarDeck(const QString& dir_deck);
    void definirModoTexto(bool ativo);
    void aplicarEdicoesPendentes();
    QStringList arquivosModificados() const;
    bool salvarTodos();

private:
    struct ItemArquivo {
        QTreeWidgetItem* item;
        QString titulo;
        QString nome_padrao;
    };

    void atualizarItens();

    DadosDeck* dados_;
    bool modo_texto_ = false;
    std::vector<QTreeWidget*> listas_;
    std::vector<EditorTextoArquivo*> editores_texto_;
    PaginaModificacoes* modificacoes_;
    QStackedWidget* pilha_modificacoes_;
    std::vector<ItemArquivo> itens_;
};
