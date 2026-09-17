#pragma once
#include <filesystem>
#include <string>
#include "arquivo_hidr.h"
#include "resultado.h"

struct OpcoesCsv {
    char separador = ';';
    bool virgula_decimal = false;
};

std::string cabecalhoCsv(const OpcoesCsv& o);
std::string linhaCsv(int codigo, const UsinaHidr& u, const OpcoesCsv& o);
Resultado exportarCsv(const ArquivoHidr& a, const std::filesystem::path& caminho, const OpcoesCsv& o);
