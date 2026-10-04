#pragma once
#include <QDialog>
#include <QString>
#include <QStringList>
#include <vector>

struct ArquivoHidr;
class DadosDeck;
class QLabel;
class QListWidget;
class QTextEdit;

struct DiferencaArquivo {
    QString nome_padrao;
    QString titulo;
    QString situacao;
    int alteracoes = 0;
    QStringList linhas;
};

std::vector<DiferencaArquivo> compararDecks(const DadosDeck& atual, const ArquivoHidr* hidr_atual, const DadosDeck& outro,
                                            const ArquivoHidr* hidr_outro);

class DialogoComparacao : public QDialog {
    Q_OBJECT
public:
    DialogoComparacao(const QString& pasta_atual, const QString& pasta_outra, std::vector<DiferencaArquivo> diferencas,
                      QWidget* parent = nullptr);

private:
    void mostrar(int indice);

    std::vector<DiferencaArquivo> diferencas_;
    QListWidget* lista_;
    QTextEdit* detalhe_;
};
