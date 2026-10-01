#pragma once
#include <utility>
#include <vector>
#include "usina_hidr.h"

struct NoCascata {
    int codigo;
    double coluna;
    int linha;
    int bacia;
};

struct ArestaCascata {
    int origem;
    int destino;
    bool desvio;
    std::vector<std::pair<double, double>> rota;
};

struct BaciaCascata {
    int indice;
    int codigo_foz;
    int num_usinas;
    int coluna_inicial;
    int largura;
    int altura;
};

struct Cascata {
    std::vector<NoCascata> nos;
    std::vector<ArestaCascata> arestas;
    std::vector<BaciaCascata> bacias;
    int num_colunas = 0;
    int num_linhas = 0;
};

Cascata montarCascata(const std::vector<UsinaHidr>& usinas);
Cascata empacotarBacias(const std::vector<UsinaHidr>& usinas, int largura_maxima);
