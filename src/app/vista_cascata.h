#pragma once
#include <QColor>
#include <QGraphicsView>
#include <QString>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "cascata.h"
#include "itens_cascata.h"

class ModeloHidr;
class LegendaCascata;
class QGraphicsScene;

class VistaCascata : public QGraphicsView {
    Q_OBJECT
public:
    explicit VistaCascata(ModeloHidr* modelo, QWidget* parent = nullptr);
    void reconstruir();
    void selecionar(int linha);
    void definirFiltro(const QString& texto);
    void definirFiltros(const std::set<int>& rees, const std::set<int>& submercados);
    void ajustar();

signals:
    void usinaEscolhida(int linha);

protected:
    void wheelEvent(QWheelEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;
    bool viewportEvent(QEvent* ev) override;

private slots:
    void aoResetarModelo();

private:
    struct ItemAresta {
        QGraphicsLineItem* linha;
        QGraphicsPolygonItem* seta;
        int origem;
        int destino;
    };

    void mapasDeRee(std::map<int, int>& ree_da_usina, std::map<int, std::string>& nome_do_ree) const;
    void desenhar(const Cascata& c, const std::unordered_map<int, QColor>& cor_do_no);
    void aplicarFiltro();
    void atualizarRotulos();
    void posicionarLegenda();
    int codigoNoPonto(const QPoint& ponto) const;
    QString descricaoDoNo(int codigo) const;

    ModeloHidr* modelo_;
    QGraphicsScene* cena_;
    LegendaCascata* legenda_;
    std::unordered_map<int, ItensNoCascata> nos_;
    std::unordered_map<int, bool> casa_filtro_;
    std::vector<ItemAresta> arestas_;
    int codigo_selecionado_ = -1;
    int codigo_sob_mouse_ = -1;
    QString filtro_;
    bool ajustar_no_proximo_ = true;
    bool usuario_mexeu_zoom_ = false;
    bool ajustando_ = false;
    std::set<int> rees_filtro_;
    std::set<int> submercados_filtro_;
};
