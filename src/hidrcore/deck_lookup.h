#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct DeckLookup {
    std::map<int, std::string> subsistemas;
    std::map<int, std::string> postos;
    std::map<int, std::string> empresas;
    std::map<int, std::string> turbinas;
    std::vector<std::string> notas;

    void carregarDeck(const std::filesystem::path& dir_deck);
    void carregarCsvs(const std::filesystem::path& dir_exe);

    std::string nomeSubsistema(int codigo) const;
    std::string nomePosto(int codigo) const;
    std::string nomeEmpresa(int codigo) const;
    std::string nomeTurbina(int codigo) const;
};
