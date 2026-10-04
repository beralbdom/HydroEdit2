#include "diferencas.h"
#include <algorithm>

// Menor sequencia de linhas removidas de a e incluidas em b (algoritmo de Myers, "An O(ND) difference
// algorithm and its variations", 1986), com as linhas iguais entre elas, na ordem dos arquivos.
// Falha (devolve false e deixa a saida vazia) quando sao precisas mais de maximo_edicoes remocoes e
// inclusoes: o tempo e a memoria crescem com o quadrado desse numero, e arquivos tao diferentes nao
// rendem uma comparacao legivel.
bool diferencasDeLinhas(const std::vector<std::string>& a, const std::vector<std::string>& b, int maximo_edicoes,
                        std::vector<LinhaDiferenca>& saida) {
    saida.clear();
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    const int limite = std::min(n + m, std::max(0, maximo_edicoes));
    const int deslocamento = limite + 1;
    std::vector<int> v(static_cast<size_t>(2 * limite + 3), 0);
    std::vector<std::vector<int>> historico;
    int d_final = -1;
    for (int d = 0; d <= limite && d_final < 0; ++d) {
        historico.push_back(v);
        for (int k = -d; k <= d; k += 2) {
            const size_t ik = static_cast<size_t>(k + deslocamento);
            int x = (k == -d || (k != d && v[ik - 1] < v[ik + 1])) ? v[ik + 1] : v[ik - 1] + 1;
            int y = x - k;
            while (x < n && y < m && a[static_cast<size_t>(x)] == b[static_cast<size_t>(y)]) {
                ++x;
                ++y;
            }
            v[ik] = x;
            if (x >= n && y >= m) {
                d_final = d;
                break;
            }
        }
    }
    if (d_final < 0) return false;

    int x = n;
    int y = m;
    std::vector<LinhaDiferenca> reverso;
    for (int d = d_final; d > 0; --d) {
        const std::vector<int>& anterior = historico[static_cast<size_t>(d)];
        const int k = x - y;
        const size_t ik = static_cast<size_t>(k + deslocamento);
        const bool desceu = k == -d || (k != d && anterior[ik - 1] < anterior[ik + 1]);
        const int k_anterior = desceu ? k + 1 : k - 1;
        const int x_anterior = anterior[static_cast<size_t>(k_anterior + deslocamento)];
        const int y_anterior = x_anterior - k_anterior;
        const int x_meio = desceu ? x_anterior : x_anterior + 1;
        const int y_meio = desceu ? y_anterior + 1 : y_anterior;
        while (x > x_meio && y > y_meio) {
            --x;
            --y;
            reverso.push_back({TipoDiferenca::Igual, x, y});
        }
        if (desceu) reverso.push_back({TipoDiferenca::Incluida, -1, y_anterior});
        else reverso.push_back({TipoDiferenca::Removida, x_anterior, -1});
        x = x_anterior;
        y = y_anterior;
    }
    while (x > 0 && y > 0) {
        --x;
        --y;
        reverso.push_back({TipoDiferenca::Igual, x, y});
    }
    saida.assign(reverso.rbegin(), reverso.rend());
    return true;
}

// Linhas do texto, sem a quebra (LF ou CRLF); um texto que termina em quebra nao ganha linha vazia
// no fim.
std::vector<std::string> separarLinhas(const std::string& texto) {
    std::vector<std::string> linhas;
    size_t inicio = 0;
    while (inicio < texto.size()) {
        size_t fim = texto.find('\n', inicio);
        if (fim == std::string::npos) fim = texto.size();
        std::string linha = texto.substr(inicio, fim - inicio);
        if (!linha.empty() && linha.back() == '\r') linha.pop_back();
        linhas.push_back(std::move(linha));
        inicio = fim + 1;
    }
    return linhas;
}
