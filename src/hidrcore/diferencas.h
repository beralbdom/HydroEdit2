#pragma once
#include <string>
#include <vector>

enum class TipoDiferenca { Igual, Removida, Incluida };

struct LinhaDiferenca {
    TipoDiferenca tipo;
    int linha_a;
    int linha_b;
};

bool diferencasDeLinhas(const std::vector<std::string>& a, const std::vector<std::string>& b, int maximo_edicoes,
                        std::vector<LinhaDiferenca>& saida);
std::vector<std::string> separarLinhas(const std::string& texto);
