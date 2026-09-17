#pragma once
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
};

struct Cascata {
    std::vector<NoCascata> nos;
    std::vector<ArestaCascata> arestas;
    int num_colunas = 0;
    int num_linhas = 0;
};

Cascata montarCascata(const std::vector<UsinaHidr>& usinas);
