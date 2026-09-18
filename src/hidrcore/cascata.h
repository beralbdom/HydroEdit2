#pragma once
#include <map>
#include <string>
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
    bool entre_grupos = false;
};

struct BaciaCascata {
    int indice;
    int codigo_foz;
    int num_usinas;
    int coluna_inicial;
    int largura;
    int altura;
};

struct GrupoCascata {
    int codigo;
    std::string nome;
    int coluna_inicial;
    int linha_inicial;
    int largura;
    int altura;
    int num_usinas;
};

struct Cascata {
    std::vector<NoCascata> nos;
    std::vector<ArestaCascata> arestas;
    std::vector<BaciaCascata> bacias;
    std::vector<GrupoCascata> grupos;
    int num_colunas = 0;
    int num_linhas = 0;
};

Cascata montarCascata(const std::vector<UsinaHidr>& usinas);
Cascata empacotarPorGrupo(const std::vector<UsinaHidr>& usinas, const std::map<int, int>& grupo_da_usina,
                          const std::map<int, std::string>& nome_do_grupo, int largura_maxima);
std::vector<int> cascataDaUsina(const std::vector<UsinaHidr>& usinas, int codigo);
