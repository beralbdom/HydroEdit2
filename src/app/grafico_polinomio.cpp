#include "grafico_polinomio.h"
#include <QAreaSeries>
#include <QBrush>
#include <QChart>
#include <QFont>
#include <QGraphicsLayout>
#include <QLegend>
#include <QLegendMarker>
#include <QLinearGradient>
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
// Paleta do SINview (a do Plotly): uma cor por curva.
constexpr std::array<const char*, 10> kCores = {"#636efa", "#ef553b", "#00cc96", "#ab63fa", "#ffa15a",
                                                "#19d3f3", "#ff6692", "#b6e880", "#ff97ff", "#fecb52"};
constexpr int kNumPontos = 100;
constexpr double kLarguraTraco = 2.0;
constexpr int kAltura = 180;

QColor comAlfa(QColor cor, double alfa) {
    cor.setAlphaF(static_cast<float>(alfa));
    return cor;
}

// Degrade vertical sob a curva, como no SINview: 36% de opacidade junto da linha e transparente na
// base. As coordenadas sao relativas a area preenchida.
QBrush preenchimento(const QColor& cor) {
    QLinearGradient degrade(0, 0, 0, 1);
    degrade.setCoordinateMode(QGradient::ObjectBoundingMode);
    degrade.setColorAt(0.00, comAlfa(cor, 0.36));
    degrade.setColorAt(0.25, comAlfa(cor, 0.18));
    degrade.setColorAt(0.50, comAlfa(cor, 0.12));
    degrade.setColorAt(0.75, comAlfa(cor, 0.04));
    degrade.setColorAt(1.00, comAlfa(cor, 0.0));
    return QBrush(degrade);
}

// A folga antes do menor valor nao atravessa o zero: vazao, volume e area comecam em zero e um eixo
// que abre em -16 sugere valores negativos que a curva nao tem.
double inicioDoEixo(double minimo, double margem) {
    double inicio = minimo - margem;
    return minimo >= 0.0 ? std::max(0.0, inicio) : inicio;
}

// Marcas em valores redondos (1, 2 ou 5 vezes uma potencia de 10, umas quatro por eixo) sem mexer
// na faixa, que continua justa nos dados: applyNiceNumbers arredondaria a faixa para fora e uma
// curva de 120 a 792 ficaria espremida num eixo de 0 a 1000. As casas decimais seguem o passo.
void marcarValoresRedondos(QValueAxis* eixo) {
    double faixa = eixo->max() - eixo->min();
    if (faixa <= 0.0) return;
    double bruto = faixa / 4.0;
    double potencia = std::pow(10.0, std::floor(std::log10(bruto)));
    double fracao = bruto / potencia;
    double passo = (fracao < 1.5 ? 1.0 : fracao < 3.5 ? 2.0 : fracao < 7.5 ? 5.0 : 10.0) * potencia;
    eixo->setTickType(QValueAxis::TicksDynamic);
    eixo->setTickAnchor(std::ceil(eixo->min() / passo) * passo);
    eixo->setTickInterval(passo);
    int casas = std::max(0, -static_cast<int>(std::floor(std::log10(passo))));
    eixo->setLabelFormat(QStringLiteral("%.%1f").arg(casas));
}
}  // namespace

GraficoPolinomio::GraficoPolinomio(const QString& titulo, const QString& rotulo_x, const QString& rotulo_y, int cor_inicial,
                                   QWidget* parent)
    : QChartView(new QChart(), parent), titulo_(titulo), rotulo_x_(rotulo_x), rotulo_y_(rotulo_y), cor_inicial_(cor_inicial) {
    setRenderHint(QPainter::Antialiasing);
    setFixedHeight(kAltura);
    chart()->setBackgroundBrush(Qt::NoBrush);
    chart()->setPlotAreaBackgroundVisible(false);
    chart()->setBackgroundRoundness(0);
    chart()->setMargins(QMargins(2, 2, 2, 2));
    chart()->layout()->setContentsMargins(0, 0, 0, 0);
    definirCurvas({}, {});
}

// Desenha as curvas validas (coeficientes nao todos nulos e faixa de x nao vazia), cada uma com a
// sua cor, linha de 2 px e degrade ate a base do eixo y, como no SINview; grade pontilhada e eixos
// esmaecidos a partir da cor do texto, legenda so com mais de uma curva e fontes um ponto menores
// para o grafico caber compacto no formulario. As marcas verticais saem tracejadas e esmaecidas.
void GraficoPolinomio::definirCurvas(const std::vector<Curva>& curvas, const std::vector<double>& marcas_x) {
    QChart* graf = chart();
    graf->removeAllSeries();
    const QList<QAbstractAxis*> eixos_antigos = graf->axes();
    for (QAbstractAxis* eixo : eixos_antigos) {
        graf->removeAxis(eixo);
        delete eixo;
    }

    const QColor cor_texto = palette().text().color();
    QFont fonte_pequena = font();
    fonte_pequena.setPointSizeF(fonte_pequena.pointSizeF() - 1.0);

    QFont fonte_titulo = font();
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

    std::vector<QList<QPointF>> pontos;
    for (const Curva& cv : validas) {
        x_min_global = std::min(x_min_global, cv.x_min);
        x_max_global = std::max(x_max_global, cv.x_max);
        QList<QPointF> curva;
        for (int i = 0; i < kNumPontos; ++i) {
            double x = cv.x_min + (cv.x_max - cv.x_min) * static_cast<double>(i) / static_cast<double>(kNumPontos - 1);
            double y = avaliarPolinomio(cv.coef, x);
            curva.append(QPointF(x, y));
            if (primeiro_ponto) {
                y_min_global = y;
                y_max_global = y;
                primeiro_ponto = false;
            } else {
                y_min_global = std::min(y_min_global, y);
                y_max_global = std::max(y_max_global, y);
            }
        }
        pontos.push_back(std::move(curva));
    }

    double margem_x = (x_max_global - x_min_global) * 0.05;
    double margem_y = (y_max_global - y_min_global) * 0.05;
    if (margem_y == 0.0) margem_y = y_max_global == 0.0 ? 1.0 : std::abs(y_max_global) * 0.1;

    auto* eixo_x = new QValueAxis(graf);
    eixo_x->setTitleText(rotulo_x_);
    eixo_x->setRange(inicioDoEixo(x_min_global, margem_x), x_max_global + margem_x);
    auto* eixo_y = new QValueAxis(graf);
    eixo_y->setTitleText(rotulo_y_);
    eixo_y->setRange(inicioDoEixo(y_min_global, margem_y), y_max_global + margem_y);
    for (QValueAxis* eixo : {eixo_x, eixo_y}) {
        eixo->setLabelsColor(cor_texto);
        eixo->setLabelsFont(fonte_pequena);
        eixo->setTitleBrush(QBrush(cor_texto));
        eixo->setTitleFont(fonte_pequena);
        eixo->setLinePen(QPen(comAlfa(cor_texto, 0.5)));
        eixo->setGridLinePen(QPen(comAlfa(cor_texto, 0.18), 1, Qt::DotLine));
        eixo->setMinorGridLineVisible(false);
        marcarValoresRedondos(eixo);
    }
    graf->addAxis(eixo_x, Qt::AlignBottom);
    graf->addAxis(eixo_y, Qt::AlignLeft);

    auto adicionar = [&](QAbstractSeries* serie, bool na_legenda) {
        graf->addSeries(serie);
        serie->attachAxis(eixo_x);
        serie->attachAxis(eixo_y);
        if (!na_legenda)
            for (QLegendMarker* m : graf->legend()->markers(serie)) m->setVisible(false);
    };

    std::vector<QColor> cores;
    for (size_t i = 0; i < validas.size(); ++i)
        cores.emplace_back(QString::fromLatin1(kCores[(static_cast<size_t>(cor_inicial_) + i) % kCores.size()]));

    for (size_t i = 0; i < validas.size(); ++i) {
        auto* topo = new QLineSeries(graf);
        topo->append(pontos[i]);
        auto* base = new QLineSeries(graf);
        base->append(pontos[i].front().x(), eixo_y->min());
        base->append(pontos[i].back().x(), eixo_y->min());
        auto* area = new QAreaSeries(topo, base);
        area->setPen(Qt::NoPen);
        area->setBrush(preenchimento(cores[i]));
        adicionar(area, false);
    }

    for (size_t i = 0; i < validas.size(); ++i) {
        auto* linha = new QLineSeries(graf);
        linha->setName(validas[i].nome);
        linha->append(pontos[i]);
        QPen caneta(cores[i], kLarguraTraco);
        caneta.setCapStyle(Qt::RoundCap);
        caneta.setJoinStyle(Qt::RoundJoin);
        linha->setPen(caneta);
        adicionar(linha, true);
    }

    for (double marca : marcas_x) {
        auto* linha = new QLineSeries(graf);
        linha->append(marca, eixo_y->min());
        linha->append(marca, eixo_y->max());
        linha->setPen(QPen(comAlfa(cor_texto, 0.5), 1, Qt::DashLine));
        adicionar(linha, false);
    }

    graf->legend()->setVisible(validas.size() > 1);
    graf->legend()->setLabelColor(cor_texto);
    graf->legend()->setFont(fonte_pequena);
    graf->legend()->setMarkerShape(QLegend::MarkerShapeFromSeries);
}
