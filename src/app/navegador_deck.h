#pragma once
#include <QTabWidget>
#include <functional>
#include <vector>

class QStackedWidget;
class QStandardItem;
class QTreeView;
class DadosDeck;
class EditorTextoArquivo;
class ModeloHidr;
class PaginaModificacoes;
class PaginaPostos;
class PaginaVazoes;

class NavegadorDeck : public QTabWidget {
    Q_OBJECT
public:
    NavegadorDeck(QWidget* editor_hidr, const ModeloHidr* modelo, QWidget* parent = nullptr);
    void carregarDeck(const QString& dir_deck);
    void definirModoTexto(bool ativo);
    void definirOcultarVazios(bool ocultar);
    void aplicarEdicoesPendentes();
    QStringList arquivosModificados() const;
    bool salvarTodos();

protected:
    void changeEvent(QEvent* evento) override;

private:
    struct ItemArquivo {
        QTreeView* arvore;
        QStandardItem* item;
        QString titulo;
        QString nome_padrao;
    };

    void atualizarItens();

    DadosDeck* dados_;
    bool modo_texto_ = false;
    std::vector<std::function<void()>> mostrar_selecionados_;
    std::vector<EditorTextoArquivo*> editores_texto_;
    PaginaModificacoes* modificacoes_;
    PaginaPostos* postos_ = nullptr;
    PaginaVazoes* vazoes_ = nullptr;
    QStackedWidget* pilha_modificacoes_;
    std::vector<ItemArquivo> itens_;
};
