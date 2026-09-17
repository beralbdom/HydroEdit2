#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "usina_hidr.h"

enum class TipoCampo { Texto, Inteiro, Real };
using Valor = std::variant<std::string, int32_t, float>;

struct Campo {
    std::string_view nome;
    int offset;
    int tamanho_elemento;
    int n;
    TipoCampo tipo;
    Valor (*obter)(const UsinaHidr&, int i);
    void (*definir)(UsinaHidr&, int i, const Valor& v);
    std::string (*nome_elemento)(int i);

    int tamanho() const { return tamanho_elemento * n; }
    bool escalar() const { return n == 1; }
};

const std::vector<Campo>& campos();
const Campo* campo(std::string_view nome);
