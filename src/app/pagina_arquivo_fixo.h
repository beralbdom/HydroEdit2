#pragma once
#include <QWidget>
#include "arquivo_fixo.h"
#include "catalogo_newave.h"

class QLabel;
class QPushButton;
class QTableView;
class DadosDeck;
class ModeloParametros;
class ModeloSecaoFixa;

class PaginaArquivoFixo : public QWidget {
    Q_OBJECT
public:
    PaginaArquivoFixo(const ArquivoNewave& arquivo, const LayoutArquivoFixo& layout, DadosDeck* dados, QWidget* parent = nullptr);
    void mostrarSecao(int secao);

private:
    void atualizar(const QString& aviso = {});

    ArquivoNewave info_;
    LayoutArquivoFixo layout_;
    DadosDeck* dados_;
    QLabel* titulo_;
    QLabel* detalhes_;
    QPushButton* botao_salvar_;
    QTableView* tabela_;
    ModeloSecaoFixa* modelo_ = nullptr;
    ModeloParametros* parametros_ = nullptr;
};
