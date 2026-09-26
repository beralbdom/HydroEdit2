#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "resultado.h"
#include "vazoes.h"

struct NoFormula;

struct RegraPosto {
    int posto = 0;
    int mes = 0;
    std::string formula;
    std::shared_ptr<const NoFormula> arvore;
};

struct ResultadoLeituraRegras {
    std::vector<RegraPosto> regras;
    std::string erro;
};

ResultadoLeituraRegras lerRegras(const std::filesystem::path& caminho);
ResultadoLeituraRegras interpretarRegras(const std::string& conteudo);
Resultado aplicarRegras(const std::vector<RegraPosto>& regras, SerieVazoes& serie);
