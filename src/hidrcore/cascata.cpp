#include "cascata.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace {

int alvoValido(const std::vector<UsinaHidr>& usinas, int codigo, int32_t alvo) {
    if (alvo < 1 || alvo > static_cast<int32_t>(usinas.size())) return 0;
    if (alvo == codigo) return 0;
    if (usinas[static_cast<size_t>(alvo - 1)].vazia()) return 0;
    return alvo;
}

// Rota de cada aresta em coordenadas de grade (coluna, linha), da origem ao destino, sem passar sobre
// nenhuma usina. As usinas ficam em colunas e linhas inteiras, entao as meias linhas (faixas) e as
// meias colunas (corredores) estao sempre livres. A aresta de jusante liga linhas vizinhas: desce ate
// a faixa entre elas, corre na horizontal ate a coluna da usina de jusante e desce nela, e os
// afluentes de uma mesma usina dividem esse trecho horizontal. O desvio sai pela lateral da origem
// ate o corredor vizinho, segue por ele ate uma linha sem usinas entre os dois corredores, atravessa
// por ela ate o corredor vizinho do destino e entra no destino pela lateral; com origem e destino na
// mesma coluna ou em colunas vizinhas, um corredor so basta. Entre as linhas livres vale a de menor
// percurso vertical, e acima e abaixo do desenho sempre ha uma. Pontos repetidos e pontos no meio de
// um trecho reto saem da rota. Trechos internos de desvios diferentes que correm sobre a mesma reta e
// se sobrepoem vao para vias paralelas (deslocamento 0, +1 ou -1 vezes 0,15 coluna ou 0,25 linha),
// para cada tracejado ficar visivel; a partir do quarto trecho sobreposto volta a via central.
void tracarRotas(Cascata& cascata) {
    std::unordered_map<int, std::pair<int, int>> posicao;
    std::set<std::pair<int, int>> ocupadas;
    for (const NoCascata& no : cascata.nos) {
        const int coluna = static_cast<int>(std::lround(no.coluna));
        posicao[no.codigo] = {coluna, no.linha};
        ocupadas.insert({coluna, no.linha});
    }
    auto linhaLivre = [&](int linha, double de, double ate) {
        const int primeira = static_cast<int>(std::ceil(std::min(de, ate)));
        const int ultima = static_cast<int>(std::floor(std::max(de, ate)));
        for (int coluna = primeira; coluna <= ultima; ++coluna)
            if (ocupadas.contains({coluna, linha})) return false;
        return true;
    };

    for (ArestaCascata& aresta : cascata.arestas) {
        aresta.rota.clear();
        auto it_origem = posicao.find(aresta.origem);
        auto it_destino = posicao.find(aresta.destino);
        if (it_origem == posicao.end() || it_destino == posicao.end()) continue;
        const auto [co, lo] = it_origem->second;
        const auto [cd, ld] = it_destino->second;

        std::vector<std::pair<double, double>> rota = {{co, lo}};
        if (!aresta.desvio) {
            const double faixa = ld - 0.5;
            rota.push_back({co, faixa});
            rota.push_back({cd, faixa});
        } else {
            const double corredor_origem = cd >= co ? co + 0.5 : co - 0.5;
            const double corredor_destino = cd > co ? cd - 0.5 : cd + 0.5;
            int travessia = lo;
            if (corredor_origem != corredor_destino) {
                int menor_percurso = std::numeric_limits<int>::max();
                for (int linha = -1; linha <= cascata.num_linhas; ++linha) {
                    if (!linhaLivre(linha, corredor_origem, corredor_destino)) continue;
                    const int percurso = std::abs(lo - linha) + std::abs(linha - ld);
                    if (percurso < menor_percurso) {
                        menor_percurso = percurso;
                        travessia = linha;
                    }
                }
            }
            rota.push_back({corredor_origem, lo});
            rota.push_back({corredor_origem, travessia});
            rota.push_back({corredor_destino, travessia});
            rota.push_back({corredor_destino, ld});
        }
        rota.push_back({cd, ld});

        std::vector<std::pair<double, double>> limpa;
        for (const auto& ponto : rota) {
            if (!limpa.empty() && limpa.back() == ponto) continue;
            if (limpa.size() >= 2) {
                const auto& a = limpa[limpa.size() - 2];
                const auto& b = limpa.back();
                if ((a.first == b.first && b.first == ponto.first) || (a.second == b.second && b.second == ponto.second))
                    limpa.pop_back();
            }
            limpa.push_back(ponto);
        }
        aresta.rota = std::move(limpa);
    }

    struct Trecho {
        std::vector<std::pair<double, double>>* rota;
        size_t indice;
        double de;
        double ate;
    };
    std::map<std::pair<bool, double>, std::vector<Trecho>> trechosPorReta;
    for (ArestaCascata& aresta : cascata.arestas) {
        if (!aresta.desvio) continue;
        auto& rota = aresta.rota;
        for (size_t i = 1; i + 2 < rota.size(); ++i) {
            const bool vertical = rota[i].first == rota[i + 1].first;
            const double de = vertical ? rota[i].second : rota[i].first;
            const double ate = vertical ? rota[i + 1].second : rota[i + 1].first;
            trechosPorReta[{vertical, vertical ? rota[i].first : rota[i].second}].push_back(
                {&rota, i, std::min(de, ate), std::max(de, ate)});
        }
    }
    constexpr int VIAS[] = {0, 1, -1};
    std::vector<std::tuple<Trecho, bool, int>> deslocamentos;
    for (const auto& [reta, trechos] : trechosPorReta) {
        std::vector<int> via(trechos.size(), 0);
        for (size_t t = 0; t < trechos.size(); ++t) {
            std::set<int> usadas;
            for (size_t anterior = 0; anterior < t; ++anterior)
                if (trechos[t].de < trechos[anterior].ate && trechos[anterior].de < trechos[t].ate) usadas.insert(via[anterior]);
            via[t] = VIAS[0];
            for (int candidata : VIAS)
                if (!usadas.contains(candidata)) {
                    via[t] = candidata;
                    break;
                }
            if (via[t] != 0) deslocamentos.push_back({trechos[t], reta.first, via[t]});
        }
    }
    for (const auto& [trecho, vertical, via] : deslocamentos) {
        auto& rota = *trecho.rota;
        if (vertical) {
            rota[trecho.indice].first += via * 0.15;
            rota[trecho.indice + 1].first += via * 0.15;
        } else {
            rota[trecho.indice].second += via * 0.25;
            rota[trecho.indice + 1].second += via * 0.25;
        }
    }
}

}  // namespace

// Bloco de uma subarvore: usinas com a coluna relativa a do no que a origina e, por profundidade, a
// coluna mais a esquerda e a mais a direita ocupadas (o contorno), para encaixar blocos vizinhos sem
// sobreposicao.
struct BlocoCascata {
    std::vector<std::pair<int, int>> nos;
    std::map<int, std::pair<int, int>> contorno;

    void deslocar(int delta) {
        for (auto& [codigo, coluna] : nos) coluna += delta;
        for (auto& [profundidade, faixa] : contorno) {
            faixa.first += delta;
            faixa.second += delta;
        }
    }

    void juntar(const BlocoCascata& outro) {
        nos.insert(nos.end(), outro.nos.begin(), outro.nos.end());
        for (const auto& [profundidade, faixa] : outro.contorno) {
            auto it = contorno.find(profundidade);
            if (it == contorno.end()) contorno[profundidade] = faixa;
            else it->second = {std::min(it->second.first, faixa.first), std::max(it->second.second, faixa.second)};
        }
    }
};

// Layout de rio: em cada usina, o afluente com mais usinas (o curso principal; empate pelo menor
// codigo) segue na mesma coluna, e os demais encostam no bloco ja montado, do maior para o menor,
// alternando direita e esquerda, o mais perto que o contorno permite (uma coluna livre entre usinas da
// mesma profundidade). A linha e a profundidade a partir da foz, com a foz embaixo. Como cada aresta
// liga profundidades vizinhas e os blocos dos afluentes ficam inteiros de um lado do curso, nenhuma
// aresta de jusante cruza outra nem passa sobre uma usina.
//
// Raizes explicitas (jusante invalido) sao processadas primeiro, em ordem de codigo; depois, qualquer
// no ainda nao visitado (so ocorre em ciclo) vira raiz adicional, e a aresta de jusante que o levaria
// de volta a um no ja visitado no mesmo ciclo e descartada.
Cascata montarCascata(const std::vector<UsinaHidr>& usinas) {
    Cascata cascata;
    int n = static_cast<int>(usinas.size());

    std::unordered_map<int, size_t> indiceDoCodigo;
    for (int codigo = 1; codigo <= n; ++codigo) {
        if (usinas[static_cast<size_t>(codigo - 1)].vazia()) continue;
        indiceDoCodigo[codigo] = cascata.nos.size();
        cascata.nos.push_back(NoCascata{codigo, 0.0, 0, -1});
    }

    std::map<int, std::vector<int>> filhosDe;
    for (int codigo = 1; codigo <= n; ++codigo) {
        if (!indiceDoCodigo.contains(codigo)) continue;
        int pai = alvoValido(usinas, codigo, usinas[static_cast<size_t>(codigo - 1)].jusante);
        if (pai != 0) filhosDe[pai].push_back(codigo);
    }

    std::vector<int> raizesExplicitas;
    for (int codigo = 1; codigo <= n; ++codigo) {
        if (!indiceDoCodigo.contains(codigo)) continue;
        if (alvoValido(usinas, codigo, usinas[static_cast<size_t>(codigo - 1)].jusante) == 0) raizesExplicitas.push_back(codigo);
    }

    std::vector<bool> visitado(static_cast<size_t>(n) + 1, false);
    int proximaColuna = 0;
    int numBacias = 0;

    std::function<BlocoCascata(int, int, int&)> dispor = [&](int codigo, int profundidade, int& profundidadeMaxima) {
        visitado[static_cast<size_t>(codigo)] = true;
        profundidadeMaxima = std::max(profundidadeMaxima, profundidade);
        NoCascata& no = cascata.nos[indiceDoCodigo[codigo]];
        no.linha = profundidade;
        no.bacia = numBacias;

        std::vector<std::pair<int, BlocoCascata>> afluentes;
        auto it = filhosDe.find(codigo);
        if (it != filhosDe.end())
            for (int filho : it->second) {
                if (visitado[static_cast<size_t>(filho)]) continue;
                afluentes.push_back({filho, dispor(filho, profundidade + 1, profundidadeMaxima)});
                cascata.arestas.push_back(ArestaCascata{filho, codigo, false});
            }
        std::stable_sort(afluentes.begin(), afluentes.end(),
                         [](const auto& a, const auto& b) { return a.second.nos.size() > b.second.nos.size(); });

        BlocoCascata bloco;
        if (!afluentes.empty()) bloco = std::move(afluentes.front().second);
        bloco.nos.push_back({codigo, 0});
        bloco.juntar(BlocoCascata{{}, {{profundidade, {0, 0}}}});
        for (size_t i = 1; i < afluentes.size(); ++i) {
            BlocoCascata& afluente = afluentes[i].second;
            const bool direita = i % 2 == 1;
            int delta = direita ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
            for (const auto& [prof, faixa] : afluente.contorno) {
                auto existente = bloco.contorno.find(prof);
                if (existente == bloco.contorno.end()) continue;
                if (direita) delta = std::max(delta, existente->second.second + 1 - faixa.first);
                else delta = std::min(delta, existente->second.first - 1 - faixa.second);
            }
            afluente.deslocar(delta);
            bloco.juntar(afluente);
        }
        return bloco;
    };

    auto processarRaiz = [&](int raiz) {
        int profundidadeMaxima = 0;
        BlocoCascata bloco = dispor(raiz, 0, profundidadeMaxima);
        int menor = 0;
        int maior = 0;
        for (const auto& [codigo, coluna] : bloco.nos) {
            menor = std::min(menor, coluna);
            maior = std::max(maior, coluna);
        }
        for (const auto& [codigo, coluna] : bloco.nos) {
            NoCascata& no = cascata.nos[indiceDoCodigo[codigo]];
            no.coluna = static_cast<double>(proximaColuna + coluna - menor);
            no.linha = profundidadeMaxima - no.linha;
        }
        const int largura = maior - menor + 1;
        cascata.bacias.push_back(BaciaCascata{numBacias, raiz, static_cast<int>(bloco.nos.size()), proximaColuna, largura,
                                               profundidadeMaxima + 1});
        ++numBacias;
        proximaColuna += largura + 1;
    };

    for (int raiz : raizesExplicitas) processarRaiz(raiz);
    for (int codigo = 1; codigo <= n; ++codigo) {
        if (!indiceDoCodigo.contains(codigo)) continue;
        if (!visitado[static_cast<size_t>(codigo)]) processarRaiz(codigo);
    }
    if (numBacias > 0) --proximaColuna;

    for (int codigo = 1; codigo <= n; ++codigo) {
        if (!indiceDoCodigo.contains(codigo)) continue;
        int alvo = alvoValido(usinas, codigo, usinas[static_cast<size_t>(codigo - 1)].desvio);
        if (alvo != 0) cascata.arestas.push_back(ArestaCascata{codigo, alvo, true});
    }

    cascata.num_colunas = proximaColuna;
    int linhaMaxima = -1;
    for (const NoCascata& no : cascata.nos) linhaMaxima = std::max(linhaMaxima, no.linha);
    cascata.num_linhas = linhaMaxima + 1;
    tracarRotas(cascata);

    return cascata;
}

// Reposiciona as bacias em linhas, para o desenho ficar aproximadamente quadrado em vez de uma faixa
// unica muito larga. Bacias ligadas por desvio formam um grupo, que fica junto: os grupos vao do maior
// para o menor (em usinas), e dentro do grupo as bacias seguem a partir da maior, cada uma ao lado de
// uma bacia com que troca desvio, para as arestas de desvio ficarem curtas. Um grupo que nao cabe no
// resto da linha comeca outra. A primeira bacia de cada linha sempre entra, mesmo que sozinha ja
// estoure largura_maxima, para nao travar em bacia muito larga; as demais so entram se sobrar espaco,
// com 1 coluna de folga entre bacias vizinhas e 1 linha entre linhas de bacias. Com largura_maxima
// <= 0 tudo cabe numa linha so.
Cascata empacotarBacias(const std::vector<UsinaHidr>& usinas, int largura_maxima) {
    Cascata cascata = montarCascata(usinas);

    std::unordered_map<int, std::vector<size_t>> nosPorBacia;
    std::unordered_map<int, int> baciaDoCodigo;
    for (size_t i = 0; i < cascata.nos.size(); ++i) {
        nosPorBacia[cascata.nos[i].bacia].push_back(i);
        baciaDoCodigo[cascata.nos[i].codigo] = cascata.nos[i].bacia;
    }

    const size_t num_bacias = cascata.bacias.size();
    std::vector<std::vector<size_t>> vizinhas(num_bacias);
    for (const ArestaCascata& a : cascata.arestas) {
        if (!a.desvio) continue;
        const size_t b1 = static_cast<size_t>(baciaDoCodigo[a.origem]);
        const size_t b2 = static_cast<size_t>(baciaDoCodigo[a.destino]);
        if (b1 == b2) continue;
        vizinhas[b1].push_back(b2);
        vizinhas[b2].push_back(b1);
    }
    auto maior = [&](size_t a, size_t b) { return cascata.bacias[a].num_usinas > cascata.bacias[b].num_usinas; };
    std::vector<size_t> porTamanho(num_bacias);
    std::iota(porTamanho.begin(), porTamanho.end(), size_t(0));
    std::stable_sort(porTamanho.begin(), porTamanho.end(), maior);

    std::vector<std::vector<size_t>> grupos;
    std::vector<bool> agrupada(num_bacias, false);
    for (size_t inicio : porTamanho) {
        if (agrupada[inicio]) continue;
        std::vector<size_t> grupo = {inicio};
        agrupada[inicio] = true;
        for (size_t k = 0; k < grupo.size(); ++k) {
            std::vector<size_t> proximas;
            for (size_t v : vizinhas[grupo[k]])
                if (!agrupada[v]) {
                    agrupada[v] = true;
                    proximas.push_back(v);
                }
            std::stable_sort(proximas.begin(), proximas.end(), maior);
            grupo.insert(grupo.end(), proximas.begin(), proximas.end());
        }
        grupos.push_back(std::move(grupo));
    }
    auto usinasDoGrupo = [&](const std::vector<size_t>& g) {
        int total = 0;
        for (size_t b : g) total += cascata.bacias[b].num_usinas;
        return total;
    };
    std::stable_sort(grupos.begin(), grupos.end(), [&](const auto& a, const auto& b) { return usinasDoGrupo(a) > usinasDoGrupo(b); });
    std::vector<size_t> ordem;
    std::vector<bool> abreGrupo(num_bacias, false);
    std::vector<int> larguraDoGrupo(num_bacias, 0);
    for (const auto& g : grupos) {
        int largura = -1;
        for (size_t b : g) largura += cascata.bacias[b].largura + 1;
        abreGrupo[g.front()] = true;
        larguraDoGrupo[g.front()] = largura;
        ordem.insert(ordem.end(), g.begin(), g.end());
    }

    int larguraAcumulada = 0;
    int deslocamentoLinha = 0;
    int alturaMaximaLinha = 0;
    bool primeiraDaLinha = true;
    int numColunas = 0;
    int numLinhas = 0;

    for (size_t i : ordem) {
        BaciaCascata& bacia = cascata.bacias[i];
        const int necessaria = abreGrupo[i] ? larguraDoGrupo[i] : bacia.largura;
        bool cabe = primeiraDaLinha || largura_maxima <= 0 || (larguraAcumulada + necessaria + 1 <= largura_maxima);
        if (!cabe && abreGrupo[i] && larguraAcumulada + bacia.largura + 1 <= largura_maxima && necessaria > largura_maxima) cabe = true;
        if (!cabe) {
            deslocamentoLinha += alturaMaximaLinha + 1;
            larguraAcumulada = 0;
            alturaMaximaLinha = 0;
            primeiraDaLinha = true;
        }

        int novaColunaInicial = larguraAcumulada + (primeiraDaLinha ? 0 : 1);
        double deltaColuna = static_cast<double>(novaColunaInicial - bacia.coluna_inicial);

        for (size_t indice : nosPorBacia[bacia.indice]) {
            cascata.nos[indice].coluna += deltaColuna;
            cascata.nos[indice].linha += deslocamentoLinha;
        }

        bacia.coluna_inicial = novaColunaInicial;
        larguraAcumulada = novaColunaInicial + bacia.largura;
        alturaMaximaLinha = std::max(alturaMaximaLinha, bacia.altura);
        numColunas = std::max(numColunas, larguraAcumulada);
        numLinhas = std::max(numLinhas, deslocamentoLinha + bacia.altura);
        primeiraDaLinha = false;
    }

    cascata.num_colunas = numColunas;
    cascata.num_linhas = numLinhas;
    tracarRotas(cascata);
    return cascata;
}
