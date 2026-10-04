#include "layout_colunas.h"
#include <QWidget>
#include <algorithm>
#include <functional>
#include <limits>

namespace {
constexpr int MAXIMO_ITENS_PARA_COMBINAR = 14;
constexpr int ESPACO_PADRAO = 6;
}  // namespace

LayoutColunas::LayoutColunas(QWidget* pai) : QLayout(pai) {}

LayoutColunas::~LayoutColunas() {
    while (QLayoutItem* item = takeAt(0)) delete item;
}

void LayoutColunas::addItem(QLayoutItem* item) {
    itens_.append(item);
    invalidate();
}

int LayoutColunas::count() const { return static_cast<int>(itens_.size()); }

QLayoutItem* LayoutColunas::itemAt(int indice) const { return indice >= 0 && indice < itens_.size() ? itens_[indice] : nullptr; }

QLayoutItem* LayoutColunas::takeAt(int indice) {
    if (indice < 0 || indice >= itens_.size()) return nullptr;
    QLayoutItem* item = itens_.takeAt(indice);
    invalidate();
    return item;
}

void LayoutColunas::invalidate() {
    largura_em_cache_ = -1;
    QLayout::invalidate();
}

// Itens em colunas, na ordem: a primeira coluna recebe os primeiros itens, a segunda os seguintes, e
// assim por diante, como as colunas de um jornal. Vale o maior numero de colunas em que os itens
// cabem lado a lado na largura natural de cada um (a coluna tem a largura do seu item mais largo;
// item mais largo que a area encolhe ate ela, sem passar da sua largura minima), e,
// com esse numero, a divisao em que a coluna mais alta fica mais baixa. Com mais de
// MAXIMO_ITENS_PARA_COMBINAR itens a busca pelas divisoes ficaria cara, e eles ficam numa coluna so.
LayoutColunas::Arranjo LayoutColunas::arranjar(int largura) const {
    if (largura == largura_em_cache_) return arranjo_em_cache_;
    const int espaco = spacing() >= 0 ? spacing() : ESPACO_PADRAO;
    const QMargins m = contentsMargins();
    const int disponivel = largura - m.left() - m.right();
    std::vector<int> larguras;
    std::vector<int> alturas;
    for (QLayoutItem* item : itens_) {
        if (item->isEmpty()) continue;
        const int w = std::max(item->minimumSize().width(), std::min(item->sizeHint().width(), disponivel));
        larguras.push_back(w);
        alturas.push_back(item->hasHeightForWidth() ? item->heightForWidth(w) : item->sizeHint().height());
    }
    const int k = static_cast<int>(larguras.size());

    Arranjo melhor;
    melhor.altura = std::numeric_limits<int>::max();
    std::vector<int> cortes;
    std::function<void(int, int)> dividir = [&](int inicio, int restantes) {
        if (restantes == 1) {
            cortes.push_back(k);
            int total = 0;
            int mais_alta = 0;
            std::vector<int> larguras_colunas;
            int de = 0;
            for (int fim : cortes) {
                int w = 0;
                int h = 0;
                for (int i = de; i < fim; ++i) {
                    w = std::max(w, larguras[static_cast<size_t>(i)]);
                    h += alturas[static_cast<size_t>(i)] + (i > de ? espaco : 0);
                }
                larguras_colunas.push_back(w);
                total += w;
                mais_alta = std::max(mais_alta, h);
                de = fim;
            }
            total += espaco * (static_cast<int>(cortes.size()) - 1);
            if ((total <= disponivel || cortes.size() == 1) && mais_alta < melhor.altura) melhor = {cortes, larguras_colunas, mais_alta};
            cortes.pop_back();
            return;
        }
        for (int fim = inicio + 1; fim <= k - restantes + 1; ++fim) {
            cortes.push_back(fim);
            dividir(fim, restantes - 1);
            cortes.pop_back();
        }
    };
    const int maximo = k <= MAXIMO_ITENS_PARA_COMBINAR ? k : 1;
    for (int n = maximo; n >= 1 && melhor.fim_das_colunas.empty(); --n) dividir(0, n);
    if (melhor.fim_das_colunas.empty()) melhor = {{0}, {0}, 0};
    melhor.altura += m.top() + m.bottom();
    largura_em_cache_ = largura;
    arranjo_em_cache_ = melhor;
    return melhor;
}

// Coluna de cada item visivel na largura dada, para conferencia.
std::vector<int> LayoutColunas::colunasPara(int largura) const {
    const Arranjo a = arranjar(largura);
    std::vector<int> colunas;
    int de = 0;
    for (size_t c = 0; c < a.fim_das_colunas.size(); ++c) {
        for (int i = de; i < a.fim_das_colunas[c]; ++i) colunas.push_back(static_cast<int>(c));
        de = a.fim_das_colunas[c];
    }
    return colunas;
}

// Largura de uma linha so, com todos os itens lado a lado, e altura da coluna mais alta nessa largura.
QSize LayoutColunas::sizeHint() const {
    const int espaco = spacing() >= 0 ? spacing() : ESPACO_PADRAO;
    const QMargins m = contentsMargins();
    int largura = 0;
    int visiveis = 0;
    for (QLayoutItem* item : itens_) {
        if (item->isEmpty()) continue;
        largura += std::max(item->sizeHint().width(), item->minimumSize().width());
        ++visiveis;
    }
    largura += espaco * std::max(0, visiveis - 1) + m.left() + m.right();
    return {largura, heightForWidth(largura)};
}

// O mais estreito possivel e uma coluna so, na largura minima do item mais largo.
QSize LayoutColunas::minimumSize() const {
    const QMargins m = contentsMargins();
    int largura = 0;
    for (QLayoutItem* item : itens_)
        if (!item->isEmpty()) largura = std::max(largura, item->minimumSize().width());
    return {largura + m.left() + m.right(), m.top() + m.bottom()};
}

// Altura da coluna mais alta no arranjo para a largura dada.
int LayoutColunas::heightForWidth(int largura) const { return arranjar(largura).altura; }

// Poe os itens nas colunas do arranjo, cada coluna na largura do seu item mais largo e cada item na
// largura da sua coluna; a largura que sobra fica livre a direita, em vez de alargar os grupos com
// espaco vazio dentro.
void LayoutColunas::setGeometry(const QRect& retangulo) {
    QLayout::setGeometry(retangulo);
    const Arranjo a = arranjar(retangulo.width());
    const int espaco = spacing() >= 0 ? spacing() : ESPACO_PADRAO;
    const QRect area = retangulo.marginsRemoved(contentsMargins());
    int visivel = 0;
    size_t coluna = 0;
    int x = area.left();
    int y = area.top();
    for (QLayoutItem* item : itens_) {
        if (item->isEmpty()) continue;
        while (coluna + 1 < a.fim_das_colunas.size() && visivel >= a.fim_das_colunas[coluna]) {
            x += a.larguras[coluna] + espaco;
            y = area.top();
            ++coluna;
        }
        const int w = a.larguras[coluna];
        const int h = item->hasHeightForWidth() ? item->heightForWidth(w) : item->sizeHint().height();
        item->setGeometry(QRect(x, y, w, h));
        y += h + espaco;
        ++visivel;
    }
}
