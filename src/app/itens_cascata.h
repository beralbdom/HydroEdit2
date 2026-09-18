#pragma once
#include <QColor>
#include <QFont>
#include <QGraphicsEllipseItem>
#include <QPalette>
#include <QPointF>
#include <map>
#include <QString>
#include <functional>
#include "cascata.h"

class QGraphicsPolygonItem;
class QGraphicsScene;
class QGraphicsSimpleTextItem;

constexpr double ESPACO_COLUNA_CASCATA = 40.0;
constexpr double ESPACO_LINHA_CASCATA = 28.0;

class PontoCascata : public QGraphicsEllipseItem {
public:
    PontoCascata(int codigo, std::function<void(int, bool)> ao_pairar);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* ev) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* ev) override;

private:
    int codigo_;
    std::function<void(int, bool)> ao_pairar_;
};

struct ItensNoCascata {
    PontoCascata* ponto;
    QGraphicsSimpleTextItem* codigo;
    QGraphicsSimpleTextItem* nome;
};

struct ItensArestaCascata {
    QGraphicsItem* traco;
    QGraphicsPolygonItem* seta;
};

QColor corSemRee();
QColor corDoIndice(int indice);
std::map<int, int> indiceDeCorDosRees(const std::map<int, int>& ree_da_usina);

QPointF centroDoNo(double coluna, int linha);

ItensNoCascata criarPontoCascata(QGraphicsScene* cena, const NoCascata& no, const QString& texto_codigo,
                                 const QString& texto_nome, const QColor& cor, const QPalette& paleta,
                                 const QFont& fonte, std::function<void(int, bool)> ao_pairar);
ItensArestaCascata criarArestaCascata(QGraphicsScene* cena, const QPointF& origem, const QPointF& destino,
                                      bool desvio, double curvatura, const QPalette& paleta);
void aplicarEstiloPonto(PontoCascata* ponto, bool selecionado, const QPalette& paleta);
