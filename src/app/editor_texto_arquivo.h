#pragma once
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class DadosDeck;

class EditorTextoArquivo : public QWidget {
    Q_OBJECT
public:
    EditorTextoArquivo(const QString& titulo, const QString& nome_padrao, DadosDeck* dados, QWidget* parent = nullptr);
    void recarregar();
    void aplicarPendente();

private:
    void atualizarDetalhes(const QString& aviso = {});

    QString nome_;
    DadosDeck* dados_;
    QLabel* detalhes_;
    QPushButton* botao_salvar_;
    QPlainTextEdit* texto_;
    QTimer* atraso_;
    bool ignorar_ = false;
};
