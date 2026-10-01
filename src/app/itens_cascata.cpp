#include "itens_cascata.h"
#include <QBrush>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QLineF>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <set>
#include <utility>

namespace {
constexpr double RAIO_NO = 5.0;
constexpr double RAIO_SELECIONADO = 7.0;
constexpr double FATOR_TRIANGULO = 1.1;
constexpr double AFASTAMENTO_ROTULO = 8.0;
constexpr double DESLOCAMENTO_ROTULO_Y = -7.0;
constexpr double LARGURA_ARESTA = 2.0;
constexpr double TAMANHO_SETA = 8.0;
constexpr double LARGURA_SETA = 6.0;
constexpr double RECUO_SETA = 6.0;
constexpr double RAIO_CANTO = 7.0;
constexpr int ALPHA_BORDA_NO = 120;
constexpr int ALPHA_ARESTA = 170;

// Usina a fio d'agua e um circulo de raio `raio`; usina com reservatorio e um triangulo equilatero
// com a ponta para baixo, meia altura FATOR_TRIANGULO vezes o raio para ter peso visual parecido com
// o do circulo. O triangulo e centrado pela caixa envolvente e nao pelo baricentro, para a ponta de
// seta e os rotulos ficarem na mesma distancia do centro que no circulo.
QPainterPath formaDoPonto(double raio, bool reservatorio) {
    QPainterPath forma;
    if (!reservatorio) {
        forma.addEllipse(QPointF(0.0, 0.0), raio, raio);
        return forma;
    }
    double meia_altura = raio * FATOR_TRIANGULO;
    double meia_largura = meia_altura * 2.0 / std::sqrt(3.0);
    forma.addPolygon(QPolygonF({QPointF(-meia_largura, -meia_altura), QPointF(meia_largura, -meia_altura),
                                QPointF(0.0, meia_altura)}));
    forma.closeSubpath();
    return forma;
}
}  // namespace

PontoCascata::PontoCascata(int codigo, bool reservatorio, std::function<void(int, bool)> ao_pairar)
    : codigo_(codigo), reservatorio_(reservatorio), ao_pairar_(std::move(ao_pairar)) {
    setAcceptHoverEvents(true);
}

void PontoCascata::hoverEnterEvent(QGraphicsSceneHoverEvent* ev) {
    QGraphicsPathItem::hoverEnterEvent(ev);
    if (ao_pairar_) ao_pairar_(codigo_, true);
}

void PontoCascata::hoverLeaveEvent(QGraphicsSceneHoverEvent* ev) {
    QGraphicsPathItem::hoverLeaveEvent(ev);
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
                                 const QString& texto_nome, const QColor& cor, bool reservatorio, const QPalette& paleta,
                                 const QFont& fonte, std::function<void(int, bool)> ao_pairar) {
    auto* ponto = new PontoCascata(no.codigo, reservatorio, std::move(ao_pairar));
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
    ponto->setPath(formaDoPonto(raio, ponto->reservatorio()));
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
// competir com os pontos. Segue a rota (pontos de cena, da origem ao destino) com os cantos
// arredondados por uma Bezier quadratica de raio ate RAIO_CANTO, limitado a metade dos trechos
// vizinhos. So o desvio tem ponta de seta, porque a jusante sempre desce; a ponta e um item proprio,
// imune ao zoom, posicionado sobre o no de destino e girado pela direcao do ultimo trecho, e o
// triangulo e desenhado recuado em RECUO_SETA pixels no eixo local para a ponta encostar na borda do
// no sem cobri-lo.
ItensArestaCascata criarArestaCascata(QGraphicsScene* cena, const QPolygonF& rota, bool desvio, const QPalette& paleta) {
    QColor cor = paleta.windowText().color();
    cor.setAlpha(ALPHA_ARESTA);

    QPen pena(cor, LARGURA_ARESTA, desvio ? Qt::DashLine : Qt::SolidLine);
    pena.setCosmetic(true);

    QPainterPath caminho(rota.front());
    for (qsizetype i = 1; i + 1 < rota.size(); ++i) {
        const QLineF chegada(rota[i], rota[i - 1]);
        const QLineF saida(rota[i], rota[i + 1]);
        const double raio = std::min({RAIO_CANTO, chegada.length() / 2.0, saida.length() / 2.0});
        caminho.lineTo(chegada.pointAt(raio / chegada.length()));
        caminho.quadTo(rota[i], saida.pointAt(raio / saida.length()));
    }
    caminho.lineTo(rota.back());
    auto* traco = cena->addPath(caminho, pena, QBrush(Qt::NoBrush));
    traco->setZValue(-1);
    if (!desvio) return {traco, nullptr};

    const QPointF destino = rota.back();
    const QPointF referencia = rota[rota.size() - 2];

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
