#pragma once
#include <QWidget>
#include "arquivo_fixo.h"
#include "catalogo_newave.h"

class QLabel;
class QSplitter;
class QMenu;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QTableView;
class DadosDeck;
class FormularioArquivo;
class ModeloSecaoFixa;

class PaginaArquivoFixo : public QWidget {
    Q_OBJECT
public:
    PaginaArquivoFixo(const ArquivoNewave& arquivo, const LayoutArquivoFixo& layout, DadosDeck* dados, QWidget* parent = nullptr);
    static bool temFormulario(const LayoutArquivoFixo& layout);
    static bool secaoEmTabela(const LayoutArquivoFixo& layout, int secao);
    static bool paginaUnica(const LayoutArquivoFixo& layout);
    void mostrarSecao(int secao);

protected:
    void showEvent(QShowEvent* evento) override;

private:
    void atualizar(const QString& aviso = {});
    void ajustarDivisor();
    QPushButton* novoBotaoRegistros(const QString& texto, bool adicionar);
    void preencherMenu(QMenu* menu, int registro, bool adicionar);
    void depoisDaEdicao(const QString& recusa, int linha_nova, int registro_anterior);

    ArquivoNewave info_;
    LayoutArquivoFixo layout_;
    DadosDeck* dados_;
    QLabel* titulo_;
    QLabel* detalhes_;
    QPushButton* botao_salvar_;
    QPushButton* botao_adicionar_;
    QPushButton* botao_remover_;
    QStackedWidget* pilha_;
    FormularioArquivo* formulario_ = nullptr;
    QTabWidget* painel_formulario_ = nullptr;
    QTableView* tabela_ = nullptr;
    QSplitter* divisor_ = nullptr;
    int primeira_tabela_ = -1;
    bool divisor_pendente_ = true;
    ModeloSecaoFixa* modelo_ = nullptr;
    bool mostrando_formulario_ = false;
};
