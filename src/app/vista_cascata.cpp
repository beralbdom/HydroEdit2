#include "vista_cascata.h"
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <array>
#include <cmath>
#include "cascata.h"
#include "modelo_hidr.h"

namespace {
constexpr double LARGURA_NO = 150.0;
constexpr double ALTURA_NO = 34.0;
constexpr double ESPACO_COLUNA = 175.0;
constexpr double ESPACO_LINHA = 70.0;
constexpr double TAMANHO_SETA = 8.0;
constexpr double ABERTURA_SETA = 0.4;

QColor corSubsistema(int32_t subsistema) {
    static const std::array<QColor, 6> cores = {
        QColor(0x4a, 0x6f, 0xa5), QColor(0x4f, 0x8a, 0x5b), QColor(0xb0, 0x74, 0x3a),
        QColor(0x7a, 0x5e, 0xa8), QColor(0xa8, 0x4f, 0x5e), QColor(0x5a, 0x8f, 0x9e),
    };
    int indice = static_cast<int>(((subsistema % 6) + 6) % 6);
    return cores[static_cast<size_t>(indice)];
}
}  // namespace

VistaCascata::VistaCascata(ModeloHidr* modelo, QWidget* parent) : QGraphicsView(parent), modelo_(modelo) {
    cena_ = new QGraphicsScene(this);
    setScene(cena_);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    pena_normal_ = QPen(palette().mid().color(), 1);
    pena_selecionada_ = QPen(palette().highlight().color(), 2);

    connect(modelo_, &QAbstractItemModel::modelReset, this, &VistaCascata::aoResetarModelo);
}

// Conexao unica com o reset do modelo: liga a flag de ajuste automatico e reconstroi a cena
// aqui mesmo, sem depender da ordem de conexao com nenhum sinal externo.
void VistaCascata::aoResetarModelo() {
    ajustar_no_proximo_ = true;
    reconstruir();
}

void VistaCascata::criarItemNo(const NoCascata& no, const UsinaHidr& usina) {
    QPainterPath caminho;
    caminho.addRoundedRect(-LARGURA_NO / 2.0, 0, LARGURA_NO, ALTURA_NO, 6, 6);
    auto* item = cena_->addPath(caminho, pena_normal_, QBrush(corSubsistema(usina.subsistema)));
    item->setPos(no.coluna * ESPACO_COLUNA, no.linha * ESPACO_LINHA);
    item->setData(0, no.codigo);
    item->setZValue(0);

    auto* texto = new QGraphicsSimpleTextItem(
        QStringLiteral("%1  %2").arg(no.codigo).arg(QString::fromLatin1(usina.nome.c_str())), item);
    texto->setFont(font());
    texto->setBrush(palette().text().color());
    texto->setPos(-LARGURA_NO / 2.0 + 8.0, ALTURA_NO / 2.0 - texto->boundingRect().height() / 2.0);

    nos_[no.codigo] = item;
}

void VistaCascata::criarItemAresta(const ArestaCascata& aresta) {
    auto it_origem = nos_.find(aresta.origem);
    auto it_destino = nos_.find(aresta.destino);
    if (it_origem == nos_.end() || it_destino == nos_.end()) return;

    QPointF ponto_origem = it_origem->second->pos() + QPointF(0.0, ALTURA_NO);
    QPointF ponto_destino = it_destino->second->pos();

    QPen pena(palette().mid().color(), 1, aresta.desvio ? Qt::DashLine : Qt::SolidLine);
    auto* linha = cena_->addLine(QLineF(ponto_origem, ponto_destino), pena);
    linha->setZValue(-1);

    double angulo = std::atan2(ponto_destino.y() - ponto_origem.y(), ponto_destino.x() - ponto_origem.x());
    QPointF p2 = ponto_destino - QPointF(std::cos(angulo - ABERTURA_SETA) * TAMANHO_SETA,
                                          std::sin(angulo - ABERTURA_SETA) * TAMANHO_SETA);
    QPointF p3 = ponto_destino - QPointF(std::cos(angulo + ABERTURA_SETA) * TAMANHO_SETA,
                                          std::sin(angulo + ABERTURA_SETA) * TAMANHO_SETA);
    auto* seta = cena_->addPolygon(QPolygonF({ponto_destino, p2, p3}), QPen(Qt::NoPen), QBrush(palette().mid().color()));
    seta->setZValue(-1);

    arestas_.push_back({linha, seta, aresta.origem, aresta.destino});
}

void VistaCascata::reconstruir() {
    int selecionado_anterior = codigo_selecionado_;

    cena_->clear();
    nos_.clear();
    arestas_.clear();
    codigo_selecionado_ = -1;

    Cascata c = montarCascata(modelo_->arquivo().usinas);
    for (const NoCascata& no : c.nos) criarItemNo(no, modelo_->usina(no.codigo - 1));
    for (const ArestaCascata& aresta : c.arestas) criarItemAresta(aresta);

    auto it = nos_.find(selecionado_anterior);
    if (it != nos_.end()) {
        codigo_selecionado_ = selecionado_anterior;
        it->second->setPen(pena_selecionada_);
    }
    aplicarFiltro();

    if (ajustar_no_proximo_) {
        ajustar();
        ajustar_no_proximo_ = false;
    }
}

void VistaCascata::selecionar(int linha) {
    int novo_codigo = linha < 0 ? -1 : linha + 1;
    if (novo_codigo == codigo_selecionado_) return;

    auto it_antigo = nos_.find(codigo_selecionado_);
    if (it_antigo != nos_.end()) it_antigo->second->setPen(pena_normal_);

    codigo_selecionado_ = novo_codigo;
    auto it_novo = nos_.find(codigo_selecionado_);
    if (it_novo != nos_.end()) {
        it_novo->second->setPen(pena_selecionada_);
        ensureVisible(it_novo->second);
    }
}

void VistaCascata::definirFiltro(const QString& texto) {
    filtro_ = texto;
    aplicarFiltro();
}

void VistaCascata::aplicarFiltro() {
    QString texto = filtro_.trimmed();
    std::unordered_map<int, bool> visivel;
    for (const auto& par : nos_) {
        int codigo = par.first;
        bool mostrar = true;
        if (!texto.isEmpty()) {
            const UsinaHidr& usina = modelo_->usina(codigo - 1);
            bool bate_codigo = QString::number(codigo).startsWith(texto);
            bool bate_nome = QString::fromLatin1(usina.nome.c_str()).contains(texto, Qt::CaseInsensitive);
            mostrar = bate_codigo || bate_nome;
        }
        par.second->setOpacity(mostrar ? 1.0 : 0.25);
        visivel[codigo] = mostrar;
    }
    for (ItemAresta& aresta : arestas_) {
        bool ambos_visiveis = visivel[aresta.origem] && visivel[aresta.destino];
        double opacidade = ambos_visiveis ? 1.0 : 0.25;
        aresta.linha->setOpacity(opacidade);
        aresta.seta->setOpacity(opacidade);
    }
}

void VistaCascata::ajustar() {
    if (cena_->items().isEmpty()) return;
    fitInView(cena_->itemsBoundingRect().adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
}

void VistaCascata::wheelEvent(QWheelEvent* ev) {
    double fator = ev->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    scale(fator, fator);
}

void VistaCascata::mousePressEvent(QMouseEvent* ev) {
    QGraphicsView::mousePressEvent(ev);
    QGraphicsItem* item = itemAt(ev->position().toPoint());
    if (!item) return;
    QVariant dado = item->data(0);
    if (!dado.isValid() && item->parentItem()) dado = item->parentItem()->data(0);
    if (dado.isValid()) emit usinaEscolhida(dado.toInt() - 1);
}
