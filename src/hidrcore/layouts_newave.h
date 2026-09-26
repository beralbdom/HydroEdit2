#pragma once
#include <string>
#include "arquivo_fixo.h"

const LayoutArquivoFixo* layoutNewave(const std::string& nome_padrao);

namespace postos_dat {
constexpr int REGISTRO = 20;
constexpr int NOME = 0;
constexpr int TAMANHO_NOME = 12;
constexpr int ANO_INICIAL = 12;
constexpr int ANO_FINAL = 16;
}  // namespace postos_dat
