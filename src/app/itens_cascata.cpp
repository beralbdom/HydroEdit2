#include "itens_cascata.h"
#include <QBrush>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QLineF>
#include <QPen>
#include <QPolygonF>
#include <cmath>
#include <numbers>
#include <utility>

namespace {
constexpr double RAIO_NO = 5.0;
constexpr double RAIO_SELECIONADO = 7.0;
constexpr double DESLOCAMENTO_ROTULO_X = 8.0;
constexpr double DESLOCAMENTO_ROTULO_Y = -7.0;
constexpr double LARGURA_ARESTA = 2.0;
constexpr double TAMANHO_SETA = 10.0;
constexpr double LARGURA_SETA = 7.0;
constexpr double RECUO_SETA = 6.0;
constexpr int ALPHA_BORDA_NO = 120;
constexpr int ALPHA_ARESTA = 170;
}  // namespace

PontoCascata::PontoCascata(int codigo, std::function<void(int, bool)> ao_pairar)
    : codigo_(codigo), ao_pairar_(std::move(ao_pairar)) {
    setAcceptHoverEvents(true);
}

void PontoCascata::hoverEnterEvent(QGraphicsSceneHoverEvent* ev) {
    QGraphicsEllipseItem::hoverEnterEvent(ev);
    if (ao_pairar_) ao_pairar_(codigo_, true);
}

void PontoCascata::hoverLeaveEvent(QGraphicsSceneHoverEvent* ev) {
    QGraphicsEllipseItem::hoverLeaveEvent(ev);
    if (ao_pairar_) ao_pairar_(codigo_, false);
}

QPointF centroDoNo(double coluna, int linha) {
    return QPointF(coluna * ESPACO_COLUNA_CASCATA, linha * ESPACO_LINHA_CASCATA);
}

// O ponto e o rotulo ignoram a transformacao da vista, entao o raio, a espessura da borda e o
// deslocamento do texto ficam constantes em pixels em qualquer zoom; so a posicao do ponto na cena
// e que escala. O rotulo nasce oculto: quem decide a visibilidade e a vista, pelo zoom atual.
ItensNoCascata criarPontoCascata(QGraphicsScene* cena, const NoCascata& no, const QString& rotulo,
                                 const QColor& cor, const QPalette& paleta, const QFont& fonte,
                                 std::function<void(int, bool)> ao_pairar) {
    auto* ponto = new PontoCascata(no.codigo, std::move(ao_pairar));
    ponto->setRect(-RAIO_NO, -RAIO_NO, RAIO_NO * 2.0, RAIO_NO * 2.0);
    ponto->setBrush(QBrush(cor));
    ponto->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    ponto->setPos(centroDoNo(no.coluna, no.linha));
    ponto->setData(0, no.codigo);
    ponto->setZValue(0);
    aplicarEstiloPonto(ponto, false, paleta);
    cena->addItem(ponto);

    auto* texto = new QGraphicsSimpleTextItem(rotulo, ponto);
    texto->setFont(fonte);
    texto->setBrush(paleta.text().color());
    texto->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    texto->setPos(DESLOCAMENTO_ROTULO_X, DESLOCAMENTO_ROTULO_Y);
    texto->setData(0, no.codigo);
    texto->setVisible(false);

    return {ponto, texto};
}

void aplicarEstiloPonto(PontoCascata* ponto, bool selecionado, const QPalette& paleta) {
    double raio = selecionado ? RAIO_SELECIONADO : RAIO_NO;
    ponto->setRect(-raio, -raio, raio * 2.0, raio * 2.0);
    if (selecionado) {
        ponto->setPen(QPen(paleta.highlight().color(), 2.0));
    } else {
        QColor borda = paleta.windowText().color();
        borda.setAlpha(ALPHA_BORDA_NO);
        ponto->setPen(QPen(borda, 1.0));
    }
}

// A linha usa caneta cosmetica (espessura constante em pixels) para nao sumir quando a cena e
// reduzida a poucos pixels por coluna, na cor do texto com alpha para ter contraste com o fundo sem
// competir com os pontos. A ponta de seta e um item proprio, tambem imune ao zoom, posicionado
// sobre o no de destino e girado pelo angulo da linha; o triangulo e desenhado recuado em
// RECUO_SETA pixels no eixo local para a ponta encostar na borda do no sem cobri-lo.
ItensArestaCascata criarArestaCascata(QGraphicsScene* cena, const QPointF& origem, const QPointF& destino,
                                      bool desvio, const QPalette& paleta) {
    QColor cor = paleta.windowText().color();
    cor.setAlpha(ALPHA_ARESTA);

    QPen pena(cor, LARGURA_ARESTA, desvio ? Qt::DashLine : Qt::SolidLine);
    pena.setCosmetic(true);
    auto* linha = cena->addLine(QLineF(origem, destino), pena);
    linha->setZValue(-1);

    QPolygonF triangulo({QPointF(-RECUO_SETA, 0.0),
                         QPointF(-RECUO_SETA - TAMANHO_SETA, -LARGURA_SETA / 2.0),
                         QPointF(-RECUO_SETA - TAMANHO_SETA, LARGURA_SETA / 2.0)});
    auto* seta = cena->addPolygon(triangulo, QPen(Qt::NoPen), QBrush(cor));
    seta->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    seta->setPos(destino);
    seta->setRotation(std::atan2(destino.y() - origem.y(), destino.x() - origem.x()) * 180.0 / std::numbers::pi);
    seta->setZValue(-1);

    return {linha, seta};
}
