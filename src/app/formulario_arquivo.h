#pragma once
#include <QWidget>
#include <vector>
#include "arquivo_fixo.h"

class QComboBox;
class QGroupBox;
class QLineEdit;
class DadosDeck;

class FormularioArquivo : public QWidget {
    Q_OBJECT
public:
    FormularioArquivo(const QString& nome_padrao, const LayoutArquivoFixo& layout, DadosDeck* dados, QWidget* parent = nullptr);
    int campos() const { return static_cast<int>(campos_.size()); }
    void mostrarTema(int tema);
    int alturaIdeal() const;

signals:
    void valorRecusado(const QString& motivo);
    void montado();

private:
    struct Campo {
        QLineEdit* edit;
        QComboBox* lista;
        int secao;
        int registro;
        int coluna;
    };

    void montar();
    void atualizarValores();
    QWidget* novoCampo(const ArquivoFixo& arquivo, int secao, int registro, int coluna, QWidget* pai);
    void editarRegistros(int secao, int registro, bool adicionar);
    QGroupBox* novoGrupo(const ArquivoFixo& arquivo, int secao, QWidget* pai, bool em_colunas = true);
    QWidget* linhaParametro(const ArquivoFixo& arquivo, int secao, QWidget* pai);
    QGroupBox* areaParametro(const ArquivoFixo& arquivo, int secao, QWidget* pai);
    QWidget* novosTemas(const ArquivoFixo& arquivo);
    QWidget* campoComPatamares(QWidget* campo, Patamares tipo, QWidget* pai);
    void mudarPatamares(Patamares tipo, int delta);

    QString nome_;
    LayoutArquivoFixo layout_;
    DadosDeck* dados_;
    QWidget* conteudo_ = nullptr;
    std::vector<Campo> campos_;
    bool atualizando_ = false;
    int tema_ = 0;
};
