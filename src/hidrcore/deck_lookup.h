#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct Ree {
    std::string nome;
    int submercado = 0;
};

struct DeckLookup {
    std::map<int, std::string> subsistemas;
    std::map<int, std::string> postos;
    std::map<int, std::string> empresas;
    std::map<int, std::string> turbinas;
    std::map<int, int> ree_da_usina;
    std::map<int, Ree> rees;
    std::vector<std::string> notas;

    void carregarDeck(const std::filesystem::path& dir_deck);
    void carregarCsvs(const std::filesystem::path& dir_exe);

    std::string nomeSubsistema(int codigo) const;
    std::string nomePosto(int codigo) const;
    std::string nomeEmpresa(int codigo) const;
    std::string nomeTurbina(int codigo) const;
    int reeDaUsina(int codigo) const;
    std::string nomeRee(int ree) const;
    int submercadoDoRee(int ree) const;
};
