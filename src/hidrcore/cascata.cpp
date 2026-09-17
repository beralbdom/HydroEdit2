#include "cascata.h"
#include <algorithm>
#include <functional>
#include <map>
#include <unordered_map>

namespace {

int jusanteValido(const std::vector<UsinaHidr>& usinas, int codigo) {
    int32_t alvo = usinas[static_cast<size_t>(codigo - 1)].jusante;
    if (alvo < 1 || alvo > static_cast<int32_t>(usinas.size())) return 0;
    if (alvo == codigo) return 0;
    if (usinas[static_cast<size_t>(alvo - 1)].vazia()) return 0;
    return alvo;
}

int desvioValido(const std::vector<UsinaHidr>& usinas, int codigo) {
    int32_t alvo = usinas[static_cast<size_t>(codigo - 1)].desvio;
    if (alvo < 1 || alvo > static_cast<int32_t>(usinas.size())) return 0;
    if (alvo == codigo) return 0;
    if (usinas[static_cast<size_t>(alvo - 1)].vazia()) return 0;
    return alvo;
}

}  // namespace

// Raizes explicitas (jusante invalido) sao processadas primeiro, em ordem de codigo; depois,
// qualquer no ainda nao visitado (so ocorre em ciclo) vira raiz adicional, e a aresta de
// jusante que o levaria de volta a um no ja visitado no mesmo ciclo e descartada.
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
        int pai = jusanteValido(usinas, codigo);
        if (pai != 0) filhosDe[pai].push_back(codigo);
    }

    std::vector<int> raizesExplicitas;
    for (int codigo = 1; codigo <= n; ++codigo) {
        if (!indiceDoCodigo.contains(codigo)) continue;
        if (jusanteValido(usinas, codigo) == 0) raizesExplicitas.push_back(codigo);
    }

    std::vector<bool> visitado(static_cast<size_t>(n) + 1, false);
    int proximaColuna = 0;
    int numBacias = 0;

    std::function<double(int, int, int&, std::vector<int>&)> visitar =
        [&](int codigo, int profundidade, int& profundidadeMaxima, std::vector<int>& membros) -> double {
        visitado[static_cast<size_t>(codigo)] = true;
        membros.push_back(codigo);
        profundidadeMaxima = std::max(profundidadeMaxima, profundidade);
        NoCascata& no = cascata.nos[indiceDoCodigo[codigo]];
        no.linha = profundidade;
        no.bacia = numBacias;

        auto it = filhosDe.find(codigo);
        if (it == filhosDe.end() || it->second.empty()) {
            no.coluna = static_cast<double>(proximaColuna++);
            return no.coluna;
        }
        double soma = 0.0;
        for (int filho : it->second) {
            if (visitado[static_cast<size_t>(filho)]) continue;
            soma += visitar(filho, profundidade + 1, profundidadeMaxima, membros);
            cascata.arestas.push_back(ArestaCascata{filho, codigo, false});
        }
        no.coluna = soma / static_cast<double>(it->second.size());
        return no.coluna;
    };

    auto processarRaiz = [&](int raiz) {
        int profundidadeMaxima = 0;
        std::vector<int> membros;
        visitar(raiz, 0, profundidadeMaxima, membros);
        for (int codigo : membros) {
            NoCascata& no = cascata.nos[indiceDoCodigo[codigo]];
            no.linha = profundidadeMaxima - no.linha;
        }
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
        int alvo = desvioValido(usinas, codigo);
        if (alvo != 0) cascata.arestas.push_back(ArestaCascata{codigo, alvo, true});
    }

    cascata.num_colunas = proximaColuna;
    int linhaMaxima = -1;
    for (const NoCascata& no : cascata.nos) linhaMaxima = std::max(linhaMaxima, no.linha);
    cascata.num_linhas = linhaMaxima + 1;

    return cascata;
}
