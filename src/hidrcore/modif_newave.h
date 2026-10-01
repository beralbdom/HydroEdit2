#pragma once
#include <string>
#include <vector>
#include "resultado.h"

enum class TipoCampoModif { Inteiro, Real, Mes, Ano, Unidade };

struct CampoModif {
    std::string nome;
    TipoCampoModif tipo = TipoCampoModif::Real;
    bool opcional = false;
};

std::vector<CampoModif> camposModif(const std::string& chave, int patamares);
bool chaveComData(const std::string& chave);
std::string valorExibidoModif(const CampoModif& campo, const std::string& token);
Resultado formatarCampoModif(const CampoModif& campo, const std::string& texto, bool aspas, std::string& token);
Resultado montarLinhaModif(const std::string& original, std::vector<std::string> tokens, const std::vector<CampoModif>& campos,
                           std::string& linha);
std::vector<std::string> valoresPadraoModif(const std::string& chave, int ano);
std::vector<std::string> inserirModificacao(const std::vector<std::string>& linhas, int usina, const std::string& nome_usina,
                                            const std::string& registro, int& linha_nova);
std::vector<std::string> removerModificacoes(const std::vector<std::string>& linhas, std::vector<int> indices);
