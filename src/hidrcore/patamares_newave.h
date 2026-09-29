#pragma once
#include <map>
#include <string>
#include <vector>
#include "resultado.h"

struct MudancaPatamares {
    std::map<std::string, std::string> textos;
    std::vector<std::string> avisos;
};

inline constexpr int MAXIMO_PATAMARES_CARGA = 5;
inline constexpr int MAXIMO_PATAMARES_DEFICIT = 4;

const std::vector<std::string>& arquivosComPatamares();
Resultado mudarPatamaresDeCarga(const std::map<std::string, std::string>& textos, int delta, MudancaPatamares& saida);
Resultado mudarPatamaresDeDeficit(const std::map<std::string, std::string>& textos, int delta, MudancaPatamares& saida);
