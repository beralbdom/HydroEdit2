#pragma once
#include <QGraphicsView>
#include <QPen>
#include <QString>
#include <unordered_map>
#include <vector>

class ModeloHidr;
class QGraphicsScene;
class QGraphicsPathItem;
class QGraphicsLineItem;
class QGraphicsPolygonItem;
struct NoCascata;
struct ArestaCascata;
struct UsinaHidr;

// Grafo das usinas ligadas por jusante (seta cheia) e desvio (seta tracejada), layout em arvore por bacia.
class VistaCascata : public QGraphicsView {
    Q_OBJECT
public:
    explicit VistaCascata(ModeloHidr* modelo, QWidget* parent = nullptr);
    void reconstruir();
    void selecionar(int linha);
    void definirFiltro(const QString& texto);
    void ajustar();

signals:
    void usinaEscolhida(int linha);

protected:
    void wheelEvent(QWheelEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;

private:
    struct ItemAresta {
        QGraphicsLineItem* linha;
        QGraphicsPolygonItem* seta;
        int origem;
        int destino;
    };

    void criarItemNo(const NoCascata& no, const UsinaHidr& usina);
    void criarItemAresta(const ArestaCascata& aresta);
    void aplicarFiltro();

    ModeloHidr* modelo_;
    QGraphicsScene* cena_;
    std::unordered_map<int, QGraphicsPathItem*> nos_;
    std::vector<ItemAresta> arestas_;
    int codigo_selecionado_ = -1;
    QString filtro_;
    QPen pena_normal_;
    QPen pena_selecionada_;
    bool ajustar_no_proximo_ = true;
};
