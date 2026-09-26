#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include "deck_lookup.h"
#include "resultado.h"
#include "usina_hidr.h"

struct SerieVazoes {
    int ano_inicial = 0;
    int num_postos = 0;
    std::vector<int32_t> valores;

    int meses() const;
    bool postoValido(int posto) const;
    int32_t valor(int posto, int mes) const;
};

struct ResultadoLeituraVazoes {
    SerieVazoes serie;
    std::string erro;
};

struct UsinaIncremental {
    int codigo = 0;
    std::string nome;
    int posto = 0;
    std::vector<int> postos_montante;
    std::vector<int32_t> vazoes;
    int meses_truncados = 0;
};

struct Incrementais {
    std::vector<UsinaIncremental> usinas;
    std::vector<std::string> avisos;
};

ResultadoLeituraVazoes lerVazoesDat(const std::filesystem::path& caminho, int num_postos, int ano_inicial);
Incrementais calcularIncrementais(const std::vector<UsinaHidr>& usinas, const std::map<int, UsinaConfhd>& confhd,
                                  const SerieVazoes& serie);
Resultado exportarIncrementaisCsv(const Incrementais& incrementais, const SerieVazoes& serie,
                                  const std::filesystem::path& caminho);
