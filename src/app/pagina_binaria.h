#pragma once
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTableView;
class QVBoxLayout;
class DadosDeck;
class ModeloPostos;
class ModeloVazoes;

class PaginaBinaria : public QWidget {
    Q_OBJECT
protected:
    PaginaBinaria(const QString& titulo, const QString& nome_padrao, DadosDeck* dados, QWidget* parent);
    void atualizar(const QString& aviso = {});
    virtual QString resumo() const = 0;
    QTableView* novaTabela();

    QString nome_;
    DadosDeck* dados_;
    QVBoxLayout* layout_;

private:
    QLabel* detalhes_;
    QPushButton* botao_salvar_;
};

class PaginaPostos : public PaginaBinaria {
    Q_OBJECT
public:
    explicit PaginaPostos(DadosDeck* dados, QWidget* parent = nullptr);
    void definirOcultarVazios(bool ocultar);

private:
    QString resumo() const override;
    void aplicarOcultos();

    ModeloPostos* modelo_;
    QTableView* tabela_;
    bool ocultar_vazios_ = true;
};

class PaginaVazoes : public PaginaBinaria {
    Q_OBJECT
public:
    explicit PaginaVazoes(DadosDeck* dados, QWidget* parent = nullptr);
    void definirOcultarVazios(bool ocultar);

private:
    QString resumo() const override;
    void preencherPostos();

    QComboBox* postos_;
    ModeloVazoes* modelo_;
    QTableView* tabela_;
    bool ocultar_vazios_ = true;
};
