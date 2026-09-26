#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct RegistroModif {
    std::string palavra_chave;
    std::vector<std::string> valores;
    int linha = 0;
};

struct BlocoModif {
    int usina = 0;
    std::string comentario;
    int linha = 0;
    std::vector<RegistroModif> registros;
};

struct ResultadoModif {
    std::vector<BlocoModif> blocos;
    std::vector<std::string> avisos;
    std::string erro;
};

std::map<std::string, std::string> interpretarArquivosDat(const std::string& conteudo);
std::map<std::string, std::string> lerArquivosDat(const std::filesystem::path& caminho);
ResultadoModif interpretarModif(const std::string& conteudo);
ResultadoModif lerModif(const std::filesystem::path& caminho);
