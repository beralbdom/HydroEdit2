#pragma once
#include <string>
#include "arquivo_fixo.h"

const LayoutArquivoFixo* layoutNewave(const std::string& nome_padrao);

struct NumeroPatamares {
    int carga = 0;
    int deficit = 0;
    bool operator==(const NumeroPatamares&) const = default;
};

int patamaresDeCarga(const ArquivoFixo& patamar);
int patamaresDeDeficit(const ArquivoFixo& sistema);
bool dependeDePatamares(const LayoutArquivoFixo& layout);
LayoutArquivoFixo ajustarPatamares(const LayoutArquivoFixo& layout, NumeroPatamares numero);

namespace postos_dat {
constexpr int REGISTRO = 20;
constexpr int NOME = 0;
constexpr int TAMANHO_NOME = 12;
constexpr int ANO_INICIAL = 12;
constexpr int ANO_FINAL = 16;
}  // namespace postos_dat
