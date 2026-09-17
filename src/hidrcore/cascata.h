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
Cascata empacotarBacias(const Cascata& c, int largura_maxima);
Cascata filtrarBacia(const Cascata& c, int codigo_foz);
std::vector<int> cascataDaUsina(const std::vector<UsinaHidr>& usinas, int codigo);
