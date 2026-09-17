#pragma once
#include <array>
#include <cstdint>
#include <string>

constexpr int MAX_CONJUNTOS = 5;
constexpr int MAX_POL_JUSANTE = 6;
constexpr int GRAU_POL = 5;

struct UsinaHidr {
    std::string nome;
    int32_t posto = 0;
    std::string posto_bdh;
    int32_t subsistema = 0;
    int32_t empresa = 0;
    int32_t jusante = 0;
    int32_t desvio = 0;
    float volume_minimo = 0;
    float volume_maximo = 0;
    float volume_vertedouro = 0;
    float volume_desvio = 0;
    float cota_minima = 0;
    float cota_maxima = 0;
    std::array<float, GRAU_POL> pol_cota_volume{};
    std::array<float, GRAU_POL> pol_area_cota{};
    std::array<int32_t, 12> evaporacao{};
    int32_t num_conjuntos = 0;
    std::array<int32_t, MAX_CONJUNTOS> num_maquinas{};
    std::array<float, MAX_CONJUNTOS> potencia_efetiva{};
    std::array<std::array<std::array<float, GRAU_POL>, 3>, MAX_CONJUNTOS> pol_conjunto{};
    std::array<float, MAX_CONJUNTOS> altura_efetiva{};
    std::array<int32_t, MAX_CONJUNTOS> vazao_efetiva{};
    float produtibilidade = 0;
    float perdas = 0;
    int32_t num_pol_jusante = 0;
    std::array<std::array<float, GRAU_POL>, MAX_POL_JUSANTE> pol_jusante{};
    std::array<float, MAX_POL_JUSANTE> ref_pol_jusante{};
    float canal_fuga_medio = 0;
    int32_t influencia_vertimento = 0;
    float fator_carga_maximo = 0;
    float fator_carga_minimo = 0;
    int32_t vazao_minima_historica = 0;
    int32_t num_unidades_base = 0;
    int32_t tipo_turbina = 0;
    int32_t representacao_conjunto = 0;
    float teif = 0;
    float ip = 0;
    int32_t tipo_perda = 0;
    std::string data;
    std::string observacao;
    float volume_referencia = 0;
    std::string regulacao;

    bool vazia() const;
    bool operator==(const UsinaHidr&) const = default;
};
