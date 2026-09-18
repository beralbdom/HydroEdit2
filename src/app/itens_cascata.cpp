#include "itens_cascata.h"
#include <QBrush>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QLineF>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <array>
#include <cmath>
#include <numbers>
#include <set>
#include <utility>

namespace {
constexpr double RAIO_NO = 5.0;
constexpr double RAIO_SELECIONADO = 7.0;
constexpr double AFASTAMENTO_ROTULO = 8.0;
constexpr double DESLOCAMENTO_ROTULO_Y = -7.0;
constexpr double LARGURA_ARESTA = 2.0;
constexpr double TAMANHO_SETA = 10.0;
constexpr double LARGURA_SETA = 7.0;
constexpr double RECUO_SETA = 8.0;
constexpr double DESVIO_CURVA = 14.0;
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

// O cinza #bab0ac ficou reservado para o "Sem REE" e saiu do ciclo, que tem 12 cores proprias: com
// os 12 REEs do deck, nenhum REE repete a cor de outro nem a do "sem REE".
QColor corSemRee() { return QColor(0xba, 0xb0, 0xac); }

QColor corDoIndice(int indice) {
    static const std::array<QColor, 12> cores = {
        QColor(0x4e, 0x79, 0xa7), QColor(0xf2, 0x8e, 0x2b), QColor(0xe1, 0x57, 0x59),
        QColor(0x76, 0xb7, 0xb2), QColor(0x59, 0xa1, 0x4f), QColor(0xed, 0xc9, 0x48),
        QColor(0xb0, 0x7a, 0xa1), QColor(0xff, 0x9d, 0xa7), QColor(0x9c, 0x75, 0x5f),
        QColor(0x86, 0xbc, 0xb6), QColor(0xd3, 0x72, 0x95), QColor(0xa0, 0xcb, 0xe8),
    };
    return cores[static_cast<size_t>(((indice % 12) + 12) % 12)];
}

// Indice de cor de cada REE pela posicao dele entre todos os REEs que o deck inteiro usa, e nao
// entre os que estao desenhados: assim um REE nao troca de cor quando um filtro deixa so parte
// deles na tela. O codigo 0 fica de fora porque tem cor propria.
std::map<int, int> indiceDeCorDosRees(const std::map<int, int>& ree_da_usina) {
    std::set<int> codigos;
    for (const auto& [codigo_usina, ree] : ree_da_usina) {
        if (ree != 0) codigos.insert(ree);
    }

    std::map<int, int> indice;
    int proximo = 0;
    for (int codigo : codigos) indice[codigo] = proximo++;
    return indice;
}

QPointF centroDoNo(double coluna, int linha) {
    return QPointF(coluna * ESPACO_COLUNA_CASCATA, linha * ESPACO_LINHA_CASCATA);
}

// O ponto e os dois rotulos ignoram a transformacao da vista, entao o raio, a espessura da borda e
// o deslocamento dos textos ficam constantes em pixels em qualquer zoom; so a posicao do ponto na
// cena e que escala. O codigo fica a esquerda do ponto, alinhado a direita (termina AFASTAMENTO_
// ROTULO pixels antes dele), e o nome comeca a mesma distancia a direita. Os dois nascem ocultos:
// quem decide a visibilidade e a vista, pelo zoom atual.
ItensNoCascata criarPontoCascata(QGraphicsScene* cena, const NoCascata& no, const QString& texto_codigo,
                                 const QString& texto_nome, const QColor& cor, const QPalette& paleta,
                                 const QFont& fonte, std::function<void(int, bool)> ao_pairar) {
    auto* ponto = new PontoCascata(no.codigo, std::move(ao_pairar));
    ponto->setRect(-RAIO_NO, -RAIO_NO, RAIO_NO * 2.0, RAIO_NO * 2.0);
    ponto->setBrush(QBrush(cor));
    ponto->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    ponto->setPos(centroDoNo(no.coluna, no.linha));
    ponto->setData(0, no.codigo);
    ponto->setZValue(0);
    aplicarEstiloPonto(ponto, false, paleta);
    cena->addItem(ponto);

    auto criarTexto = [&](const QString& conteudo) {
        auto* texto = new QGraphicsSimpleTextItem(conteudo, ponto);
        texto->setFont(fonte);
        texto->setBrush(paleta.text().color());
        texto->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        texto->setData(0, no.codigo);
        texto->setVisible(false);
        return texto;
    };

    QGraphicsSimpleTextItem* codigo = criarTexto(texto_codigo);
    codigo->setPos(-AFASTAMENTO_ROTULO - codigo->boundingRect().width(), DESLOCAMENTO_ROTULO_Y);

    QGraphicsSimpleTextItem* nome = criarTexto(texto_nome);
    nome->setPos(AFASTAMENTO_ROTULO, DESLOCAMENTO_ROTULO_Y);

    return {ponto, codigo, nome};
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

// O traco usa caneta cosmetica (espessura constante em pixels) para nao sumir quando a cena e
// reduzida a poucos pixels por coluna, na cor do texto com alpha para ter contraste com o fundo sem
// competir com os pontos. Com curvatura zero e um segmento reto; com curvatura diferente de zero e
// uma Bezier quadratica cujo ponto de controle sai do meio do segmento na perpendicular a direcao de
// viagem, deslocado DESVIO_CURVA unidades de cena vezes a curvatura. A perpendicular (-dy, dx) e a
// direita de quem viaja de origem para destino, entao duas ligacoes em sentidos opostos com a mesma
// curvatura caem uma de cada lado, e duas no mesmo sentido se separam pelo sinal da curvatura.
// A ponta de seta e um item proprio, imune ao zoom, posicionado sobre o no de destino e girado pela
// direcao de chegada (a do ponto de controle para o destino, no caso curvo); o triangulo e desenhado
// recuado em RECUO_SETA pixels no eixo local para a ponta encostar na borda do no sem cobri-lo.
ItensArestaCascata criarArestaCascata(QGraphicsScene* cena, const QPointF& origem, const QPointF& destino,
                                      bool desvio, double curvatura, const QPalette& paleta) {
    QColor cor = paleta.windowText().color();
    cor.setAlpha(ALPHA_ARESTA);

    QPen pena(cor, LARGURA_ARESTA, desvio ? Qt::DashLine : Qt::SolidLine);
    pena.setCosmetic(true);

    QGraphicsItem* traco = nullptr;
    QPointF referencia = origem;
    if (curvatura == 0.0) {
        auto* linha = cena->addLine(QLineF(origem, destino), pena);
        linha->setZValue(-1);
        traco = linha;
    } else {
        QPointF delta = destino - origem;
        double comprimento = std::hypot(delta.x(), delta.y());
        QPointF perpendicular =
            comprimento > 0.0 ? QPointF(-delta.y(), delta.x()) / comprimento : QPointF(0.0, 1.0);
        QPointF controle = (origem + destino) / 2.0 + perpendicular * DESVIO_CURVA * curvatura;

        QPainterPath caminho(origem);
        caminho.quadTo(controle, destino);
        auto* curva = cena->addPath(caminho, pena, QBrush(Qt::NoBrush));
        curva->setZValue(-1);
        traco = curva;
        referencia = controle;
    }

    QPolygonF triangulo({QPointF(-RECUO_SETA, 0.0),
                         QPointF(-RECUO_SETA - TAMANHO_SETA, -LARGURA_SETA / 2.0),
                         QPointF(-RECUO_SETA - TAMANHO_SETA, LARGURA_SETA / 2.0)});
    auto* seta = cena->addPolygon(triangulo, QPen(Qt::NoPen), QBrush(cor));
    seta->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    seta->setPos(destino);
    seta->setRotation(std::atan2(destino.y() - referencia.y(), destino.x() - referencia.x()) * 180.0 /
                      std::numbers::pi);
    seta->setZValue(-1);

    return {traco, seta};
}
