#pragma once
#include <QDockWidget>
#include <vector>
#include "validacao.h"

class QListWidget;

struct ProblemaUsina {
    int linha;
    Problema problema;
};

class PainelProblemas : public QDockWidget {
    Q_OBJECT
public:
    explicit PainelProblemas(QWidget* parent = nullptr);
    void definirProblemas(const std::vector<ProblemaUsina>& problemas);
signals:
    void problemaEscolhido(int linha, const QString& campo);
private:
    QListWidget* lista_;
};
