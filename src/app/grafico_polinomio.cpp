#include "grafico_polinomio.h"
#include <QBrush>
#include <QChart>
#include <QFont>
#include <QLegend>
#include <QLegendMarker>
#include <QLineSeries>
#include <QPainter>
#include <QPen>
#include <QValueAxis>
#include <algorithm>
#include <cmath>

double avaliarPolinomio(const std::array<float, 5>& coef, double x) {
    double resultado = 0.0;
    for (int i = 4; i >= 0; --i) resultado = resultado * x + static_cast<double>(coef[static_cast<size_t>(i)]);
    return resultado;
}

namespace {
constexpr std::array<const char*, 6> kCoresCascata = {"#4a6fa5", "#4f8a5b", "#b0743a", "#7a5ea8", "#a84f5e", "#5a8f9e"};
constexpr int kNumPontos = 100;
}  // namespace

GraficoPolinomio::GraficoPolinomio(const QString& titulo, const QString& rotulo_x, const QString& rotulo_y, QWidget* parent)
    : QChartView(new QChart(), parent), titulo_(titulo), rotulo_x_(rotulo_x), rotulo_y_(rotulo_y) {
    setRenderHint(QPainter::Antialiasing);
    setFixedHeight(220);
    chart()->setBackgroundBrush(Qt::NoBrush);
    chart()->setPlotAreaBackgroundVisible(false);
    definirCurvas({}, {});
}

void GraficoPolinomio::definirCurvas(const std::vector<Curva>& curvas, const std::vector<double>& marcas_x) {
    QChart* graf = chart();
    graf->removeAllSeries();
    const QList<QAbstractAxis*> eixos_antigos = graf->axes();
    for (QAbstractAxis* eixo : eixos_antigos) {
        graf->removeAxis(eixo);
        delete eixo;
    }

    const QColor cor_texto = palette().text().color();
    const QColor cor_grade = palette().mid().color();

    QFont fonte_titulo = graf->titleFont();
    fonte_titulo.setBold(true);
    graf->setTitleFont(fonte_titulo);
    graf->setTitleBrush(cor_texto);

    std::vector<Curva> validas;
    for (const Curva& cv : curvas) {
        bool todos_zero = std::all_of(cv.coef.begin(), cv.coef.end(), [](float v) { return v == 0.0f; });
        if (cv.x_max <= cv.x_min || todos_zero) continue;
        validas.push_back(cv);
    }

    if (validas.empty()) {
        graf->setTitle(titulo_ + QStringLiteral(" (sem dados)"));
        graf->legend()->setVisible(false);
        return;
    }
    graf->setTitle(titulo_);

    double x_min_global = validas.front().x_min;
    double x_max_global = validas.front().x_max;
    double y_min_global = 0.0;
    double y_max_global = 0.0;
    bool primeiro_ponto = true;

    std::vector<QLineSeries*> series_curvas;
    for (const Curva& cv : validas) {
        x_min_global = std::min(x_min_global, cv.x_min);
        x_max_global = std::max(x_max_global, cv.x_max);
        auto* serie = new QLineSeries(graf);
        serie->setName(cv.nome);
        for (int i = 0; i < kNumPontos; ++i) {
            double x = cv.x_min + (cv.x_max - cv.x_min) * static_cast<double>(i) / static_cast<double>(kNumPontos - 1);
            double y = avaliarPolinomio(cv.coef, x);
            serie->append(x, y);
            if (primeiro_ponto) {
                y_min_global = y;
                y_max_global = y;
                primeiro_ponto = false;
            } else {
                y_min_global = std::min(y_min_global, y);
                y_max_global = std::max(y_max_global, y);
            }
        }
        series_curvas.push_back(serie);
    }

    for (size_t i = 0; i < series_curvas.size(); ++i) {
        QColor cor = i == 0 ? palette().highlight().color() : QColor(QString::fromLatin1(kCoresCascata[(i - 1) % kCoresCascata.size()]));
        series_curvas[i]->setPen(QPen(cor));
        graf->addSeries(series_curvas[i]);
    }

    double margem_x = (x_max_global - x_min_global) * 0.05;
    double margem_y = (y_max_global - y_min_global) * 0.05;
    if (margem_y == 0.0) margem_y = y_max_global == 0.0 ? 1.0 : std::abs(y_max_global) * 0.1;

    auto* eixo_x = new QValueAxis(graf);
    eixo_x->setTitleText(rotulo_x_);
    eixo_x->setRange(x_min_global - margem_x, x_max_global + margem_x);
    auto* eixo_y = new QValueAxis(graf);
    eixo_y->setTitleText(rotulo_y_);
    eixo_y->setRange(y_min_global - margem_y, y_max_global + margem_y);
    for (QValueAxis* eixo : {eixo_x, eixo_y}) {
        eixo->setLabelsColor(cor_texto);
        eixo->setTitleBrush(QBrush(cor_texto));
        eixo->setLinePen(QPen(cor_grade));
        eixo->setGridLineColor(cor_grade);
    }
    graf->addAxis(eixo_x, Qt::AlignBottom);
    graf->addAxis(eixo_y, Qt::AlignLeft);
    for (QLineSeries* serie : series_curvas) {
        serie->attachAxis(eixo_x);
        serie->attachAxis(eixo_y);
    }

    for (double marca : marcas_x) {
        auto* linha = new QLineSeries(graf);
        linha->append(marca, eixo_y->min());
        linha->append(marca, eixo_y->max());
        QPen caneta(cor_texto);
        caneta.setStyle(Qt::DashLine);
        linha->setPen(caneta);
        graf->addSeries(linha);
        linha->attachAxis(eixo_x);
        linha->attachAxis(eixo_y);
        const QList<QLegendMarker*> marcadores = graf->legend()->markers(linha);
        for (QLegendMarker* m : marcadores) m->setVisible(false);
    }

    graf->legend()->setVisible(validas.size() > 1);
    graf->legend()->setLabelColor(cor_texto);
}
