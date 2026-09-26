#pragma once
#include <QTabWidget>
#include <vector>

class ModeloHidr;
class PaginaArquivo;
class PaginaModificacoes;

class NavegadorDeck : public QTabWidget {
    Q_OBJECT
public:
    NavegadorDeck(QWidget* editor_hidr, const ModeloHidr* modelo, QWidget* parent = nullptr);
    void carregarDeck(const QString& dir_deck);

private:
    std::vector<PaginaArquivo*> paginas_;
    PaginaModificacoes* modificacoes_;
};
