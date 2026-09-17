#pragma once
#include <filesystem>
#include <vector>
#include "resultado.h"
#include "usina_hidr.h"

struct ArquivoHidr {
    std::vector<UsinaHidr> usinas;

    Resultado carregar(const std::filesystem::path& caminho);
    Resultado salvar(const std::filesystem::path& caminho) const;
    int numUsinasPreenchidas() const;
};
