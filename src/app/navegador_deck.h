#pragma once
#include <QTabWidget>
#include <vector>

class ModeloHidr;
class PaginaArquivo;
class PaginaArquivoFixo;
class PaginaModificacoes;

class NavegadorDeck : public QTabWidget {
    Q_OBJECT
public:
    NavegadorDeck(QWidget* editor_hidr, const ModeloHidr* modelo, QWidget* parent = nullptr);
    void carregarDeck(const QString& dir_deck);
    QStringList arquivosModificados() const;
    bool salvarTodos();

private:
    std::vector<PaginaArquivo*> paginas_;
    std::vector<PaginaArquivoFixo*> paginas_fixas_;
    PaginaModificacoes* modificacoes_;
};
