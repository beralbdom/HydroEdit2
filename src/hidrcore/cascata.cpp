#include "cascata.h"
#include <algorithm>
#include <functional>
#include <map>
#include <numeric>
#include <unordered_map>
#include <utility>

namespace {

int alvoValido(const std::vector<UsinaHidr>& usinas, int codigo, int32_t alvo) {
    if (alvo < 1 || alvo > static_cast<int32_t>(usinas.size())) return 0;
    if (alvo == codigo) return 0;
    if (usinas[static_cast<size_t>(alvo - 1)].vazia()) return 0;
    return alvo;
}

}  // namespace

// Raizes explicitas (jusante invalido) sao processadas primeiro, em ordem de codigo; depois,
// qualquer no ainda nao visitado (so ocorre em ciclo) vira raiz adicional, e a aresta de
// jusante que o levaria de volta a um no ja visitado no mesmo ciclo e descartada. Um no e
// "leaf" (recebe a proxima coluna livre) quando nao sobra nenhum filho para visitar depois
// de descartar os ja visitados; um no cuja subarvore tocou um ciclo tambem recebe coluna
// propria em vez da media dos filhos, senao ele colapsaria sobre o mesmo valor do filho
// unico que restou (media de um elemento e o proprio elemento), sobrepondo colunas ao
// longo de toda a cadeia que sobra de um ciclo resolvido.
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

    std::function<std::pair<double, bool>(int, int, int&, std::vector<int>&)> visitar =
        [&](int codigo, int profundidade, int& profundidadeMaxima, std::vector<int>& membros) -> std::pair<double, bool> {
        visitado[static_cast<size_t>(codigo)] = true;
        membros.push_back(codigo);
        profundidadeMaxima = std::max(profundidadeMaxima, profundidade);
        NoCascata& no = cascata.nos[indiceDoCodigo[codigo]];
        no.linha = profundidade;
        no.bacia = numBacias;

        double soma = 0.0;
        int visitados = 0;
        bool tocaCiclo = false;
        auto it = filhosDe.find(codigo);
        if (it != filhosDe.end()) {
            for (int filho : it->second) {
                if (visitado[static_cast<size_t>(filho)]) {
                    tocaCiclo = true;
                    continue;
                }
                auto [colunaFilho, filhoTocaCiclo] = visitar(filho, profundidade + 1, profundidadeMaxima, membros);
                soma += colunaFilho;
                ++visitados;
                if (filhoTocaCiclo) tocaCiclo = true;
                cascata.arestas.push_back(ArestaCascata{filho, codigo, false});
            }
        }

        if (visitados == 0 || tocaCiclo) no.coluna = static_cast<double>(proximaColuna++);
        else no.coluna = soma / static_cast<double>(visitados);
        return {no.coluna, tocaCiclo};
    };

    auto processarRaiz = [&](int raiz) {
        int profundidadeMaxima = 0;
        std::vector<int> membros;
        int colunaInicial = proximaColuna;
        visitar(raiz, 0, profundidadeMaxima, membros);
        for (int codigo : membros) {
            NoCascata& no = cascata.nos[indiceDoCodigo[codigo]];
            no.linha = profundidadeMaxima - no.linha;
        }
        cascata.bacias.push_back(BaciaCascata{numBacias, raiz, static_cast<int>(membros.size()), colunaInicial,
                                               proximaColuna - colunaInicial, profundidadeMaxima + 1});
        ++numBacias;
        ++proximaColuna;
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

    return cascata;
}

// Reposiciona as bacias em linhas, das maiores para as menores, para o desenho ficar aproximadamente
// quadrado em vez de uma faixa unica muito larga. A primeira bacia de cada linha sempre entra, mesmo
// que sozinha ja estoure largura_maxima, para nao travar em bacia muito larga; as demais so entram
// se sobrar espaco, com 1 coluna de folga entre bacias vizinhas e 1 linha entre linhas de bacias.
// Com largura_maxima <= 0 tudo cabe numa linha so.
Cascata empacotarBacias(const std::vector<UsinaHidr>& usinas, int largura_maxima) {
    Cascata cascata = montarCascata(usinas);

    std::vector<size_t> ordem(cascata.bacias.size());
    std::iota(ordem.begin(), ordem.end(), size_t(0));
    std::stable_sort(ordem.begin(), ordem.end(), [&](size_t a, size_t b) {
        return cascata.bacias[a].num_usinas > cascata.bacias[b].num_usinas;
    });

    std::unordered_map<int, std::vector<size_t>> nosPorBacia;
    for (size_t i = 0; i < cascata.nos.size(); ++i) nosPorBacia[cascata.nos[i].bacia].push_back(i);

    int larguraAcumulada = 0;
    int deslocamentoLinha = 0;
    int alturaMaximaLinha = 0;
    bool primeiraDaLinha = true;
    int numColunas = 0;
    int numLinhas = 0;

    for (size_t i : ordem) {
        BaciaCascata& bacia = cascata.bacias[i];
        bool cabe = primeiraDaLinha || largura_maxima <= 0 ||
                    (larguraAcumulada + bacia.largura + 1 <= largura_maxima);
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
    return cascata;
}
