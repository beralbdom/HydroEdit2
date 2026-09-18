#include "legenda_cascata.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QScrollBar>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int LADO_AMOSTRA = 10;
constexpr int LARGURA_MINIMA = 80;
constexpr int LARGURA_MAXIMA = 170;
constexpr size_t MAXIMO_LINHAS = 15;
}  // namespace

// Painel lateral, nao mais um overlay sobre a cena: fica no layout da aba, ao lado da vista, com o
// fundo da propria paleta. Rola sozinho quando tem mais linhas do que altura disponivel.
LegendaCascata::LegendaCascata(QWidget* parent) : QScrollArea(parent) {
    conteudo_ = new QWidget(this);
    layout_ = new QVBoxLayout(conteudo_);
    layout_->setContentsMargins(6, 4, 6, 4);
    layout_->setSpacing(2);
    layout_->addStretch(1);

    setWidget(conteudo_);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::StyledPanel);
    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);
    viewport()->setBackgroundRole(QPalette::Window);
    viewport()->setAutoFillBackground(true);
    setFixedWidth(LARGURA_MINIMA);
}

// Uma linha por grupo, limitada a MAXIMO_LINHAS para a legenda nao virar uma lista interminavel; o
// excedente vira uma linha "+N outros". Nunca se esconde: uma lista vazia so deixa o painel vazio,
// para o painel nao sumir e voltar a cada reconstrucao da cena.
void LegendaCascata::definirGrupos(const std::vector<std::pair<QString, QColor>>& grupos) {
    while (QLayoutItem* item = layout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    size_t mostradas = std::min(grupos.size(), MAXIMO_LINHAS);
    for (size_t i = 0; i < mostradas; ++i) {
        auto* linha = new QWidget(conteudo_);
        auto* layout_linha = new QHBoxLayout(linha);
        layout_linha->setContentsMargins(0, 0, 0, 0);
        layout_linha->setSpacing(6);

        QPixmap amostra(LADO_AMOSTRA, LADO_AMOSTRA);
        amostra.fill(grupos[i].second);
        auto* quadrado = new QLabel(linha);
        quadrado->setPixmap(amostra);
        quadrado->setFixedSize(LADO_AMOSTRA, LADO_AMOSTRA);

        layout_linha->addWidget(quadrado);
        layout_linha->addWidget(new QLabel(grupos[i].first, linha), 1);
        layout_->addWidget(linha);
    }
    if (grupos.size() > mostradas) {
        layout_->addWidget(
            new QLabel(QStringLiteral("+%1 outros").arg(static_cast<int>(grupos.size() - mostradas)), conteudo_));
    }
    layout_->addStretch(1);

    int largura_conteudo = conteudo_->sizeHint().width() + 2 * frameWidth() + verticalScrollBar()->sizeHint().width();
    setFixedWidth(std::clamp(largura_conteudo, LARGURA_MINIMA, LARGURA_MAXIMA));
}
