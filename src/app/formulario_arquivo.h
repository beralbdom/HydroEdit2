#pragma once
#include <QWidget>
#include <vector>
#include "arquivo_fixo.h"

class QLineEdit;
class DadosDeck;

class FormularioArquivo : public QWidget {
    Q_OBJECT
public:
    FormularioArquivo(const QString& nome_padrao, const LayoutArquivoFixo& layout, DadosDeck* dados, QWidget* parent = nullptr);
    int campos() const { return static_cast<int>(campos_.size()); }

signals:
    void valorRecusado(const QString& motivo);

private:
    struct Campo {
        QLineEdit* edit;
        int secao;
        int registro;
        int coluna;
    };

    void montar();
    void atualizarValores();
    QLineEdit* novoCampo(const ArquivoFixo& arquivo, int secao, int registro, int coluna, QWidget* pai);

    QString nome_;
    LayoutArquivoFixo layout_;
    DadosDeck* dados_;
    QWidget* conteudo_ = nullptr;
    std::vector<Campo> campos_;
    bool atualizando_ = false;
};
