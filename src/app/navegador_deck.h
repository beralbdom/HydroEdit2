#pragma once
#include <QTabWidget>
#include <map>
#include <vector>

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
    void marcarModificados();

    DadosDeck* dados_;
    std::vector<PaginaArquivo*> paginas_;
    PaginaModificacoes* modificacoes_;
    std::multimap<QString, std::pair<QTreeWidgetItem*, QString>> itens_por_arquivo_;
};
