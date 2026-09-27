#pragma once
#include <QChartView>
#include <QString>
#include <array>
#include <vector>

double avaliarPolinomio(const std::array<float, 5>& coef, double x);

// Grafico de linha para um conjunto de curvas polinomiais (Qt Charts), com marcas verticais opcionais.
// Cada curva tem uma cor da paleta, a partir de cor_inicial, para graficos diferentes do mesmo
// formulario nao repetirem cores.
class GraficoPolinomio : public QChartView {
    Q_OBJECT
public:
    struct Curva {
        QString nome;
        std::array<float, 5> coef;
        double x_min;
        double x_max;
    };

    GraficoPolinomio(const QString& titulo, const QString& rotulo_x, const QString& rotulo_y, int cor_inicial, QWidget* parent = nullptr);

    void definirCurvas(const std::vector<Curva>& curvas, const std::vector<double>& marcas_x);

private:
    QString titulo_;
    QString rotulo_x_;
    QString rotulo_y_;
    int cor_inicial_;
};
