#pragma once
#include <QWidget>
#include "arquivo_fixo.h"
#include "catalogo_newave.h"

class QLabel;
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
    void mostrarSecao(int secao);

private:
    void atualizar(const QString& aviso = {});

    ArquivoNewave info_;
    LayoutArquivoFixo layout_;
    DadosDeck* dados_;
    QLabel* titulo_;
    QLabel* detalhes_;
    QPushButton* botao_salvar_;
    QStackedWidget* pilha_;
    FormularioArquivo* formulario_ = nullptr;
    QTabWidget* painel_formulario_ = nullptr;
    QTableView* tabela_ = nullptr;
    ModeloSecaoFixa* modelo_ = nullptr;
    bool mostrando_formulario_ = false;
};
