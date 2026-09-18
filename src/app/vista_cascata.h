#pragma once
#include <QColor>
#include <QGraphicsView>
#include <QString>
#include <map>
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
    enum class Agrupamento { Ree, Submercado, Bacia };

    explicit VistaCascata(ModeloHidr* modelo, QWidget* parent = nullptr);
    void reconstruir();
    void selecionar(int linha);
    void definirFiltro(const QString& texto);
    void definirBacia(int codigo_foz);
    void definirRee(int codigo_ree);
    void definirAgrupamento(Agrupamento agrupamento);
    void definirSoSelecionada(bool ligado);
    void ajustar();

signals:
    void usinaEscolhida(int linha);
    void baciasAtualizadas(const std::vector<BaciaCascata>& bacias);
    void gruposAtualizados(const std::vector<GrupoCascata>& grupos);
    void focarCascataDe(int linha);

protected:
    void wheelEvent(QWheelEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;

private slots:
    void aoResetarModelo();

private:
    struct ItemAresta {
        QGraphicsLineItem* linha;
        QGraphicsPolygonItem* seta;
        int origem;
        int destino;
        double opacidade_base;
    };

    void mapasDeGrupo(std::map<int, int>& grupo_da_usina, std::map<int, std::string>& nome_do_grupo) const;
    void desenhar(const Cascata& c, const std::unordered_map<int, QColor>& cor_do_no,
                  const std::vector<QColor>& cor_do_grupo,
                  const std::vector<std::pair<QString, QColor>>& legenda);
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
    int bacia_filtro_ = 0;
    int ree_filtro_ = 0;
    Agrupamento agrupamento_ = Agrupamento::Ree;
    bool so_selecionada_ = false;
};
