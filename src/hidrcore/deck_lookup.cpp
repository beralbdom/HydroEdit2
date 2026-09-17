#include "deck_lookup.h"
#include <cstring>
#include <fstream>
#include "texto.h"

namespace fs = std::filesystem;

namespace {

std::string buscar(const std::map<int, std::string>& m, int codigo) {
    auto it = m.find(codigo);
    return it == m.end() ? std::string() : it->second;
}

bool comecaCom(const std::string& linha, const char* prefixo) {
    size_t i = linha.find_first_not_of(' ');
    return i != std::string::npos && linha.compare(i, std::strlen(prefixo), prefixo) == 0;
}

// Bloco "CUSTO DO DEFICIT" do sistema.dat: duas linhas de cabecalho, depois
// "NUM NOME" por linha ate a sentinela 999 (manual NEWAVE, secao 3.2).
std::map<int, std::string> lerSistema(const fs::path& p, std::vector<std::string>& notas) {
    std::map<int, std::string> m;
    std::ifstream f(p);
    if (!f) {
        notas.push_back("sistema.dat nao encontrado");
        return m;
    }
    std::string linha;
    bool no_bloco = false;
    int cabecalhos = 0;
    while (std::getline(f, linha)) {
        if (!no_bloco) {
            if (comecaCom(linha, "CUSTO DO DEFICIT")) no_bloco = true;
            continue;
        }
        if (cabecalhos < 2) {
            ++cabecalhos;
            continue;
        }
        if (comecaCom(linha, "999")) break;
        if (linha.size() < 6) continue;
        int codigo = 0;
        try {
            codigo = std::stoi(linha.substr(0, 5));
        } catch (...) {
            continue;
        }
        std::string nome = apararDireita(linha.substr(5, 10));
        size_t ini = nome.find_first_not_of(' ');
        m[codigo] = ini == std::string::npos ? std::string() : nome.substr(ini);
    }
    if (m.empty()) notas.push_back("sistema.dat sem bloco CUSTO DO DEFICIT");
    return m;
}

// postos.dat: registros de 20 bytes (nome A12, ano inicial I4, ano final I4); codigo = indice + 1.
std::map<int, std::string> lerPostos(const fs::path& p, std::vector<std::string>& notas) {
    std::map<int, std::string> m;
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        notas.push_back("postos.dat nao encontrado");
        return m;
    }
    char reg[20];
    int codigo = 1;
    while (f.read(reg, 20)) {
        std::string nome = apararDireita(std::string_view(reg, 12));
        if (!nome.empty()) m[codigo] = nome;
        ++codigo;
    }
    return m;
}

std::map<int, std::string> lerCsvCodigoNome(const fs::path& p, std::vector<std::string>& notas) {
    std::map<int, std::string> m;
    std::ifstream f(p);
    if (!f) return m;
    std::string linha;
    bool primeira = true;
    while (std::getline(f, linha)) {
        if (primeira) {
            primeira = false;
            continue;
        }
        size_t sep = linha.find(';');
        if (sep == std::string::npos) continue;
        try {
            m[std::stoi(linha.substr(0, sep))] = apararDireita(linha.substr(sep + 1));
        } catch (...) {
            notas.push_back(p.filename().string() + ": linha invalida: " + linha);
        }
    }
    return m;
}

}  // namespace

void DeckLookup::carregarDeck(const fs::path& dir_deck) {
    subsistemas = lerSistema(dir_deck / "sistema.dat", notas);
    postos = lerPostos(dir_deck / "postos.dat", notas);
}

void DeckLookup::carregarCsvs(const fs::path& dir_exe) {
    empresas = lerCsvCodigoNome(dir_exe / "empresas.csv", notas);
    turbinas = lerCsvCodigoNome(dir_exe / "turbinas.csv", notas);
}

std::string DeckLookup::nomeSubsistema(int codigo) const { return buscar(subsistemas, codigo); }
std::string DeckLookup::nomePosto(int codigo) const { return buscar(postos, codigo); }
std::string DeckLookup::nomeEmpresa(int codigo) const { return buscar(empresas, codigo); }
std::string DeckLookup::nomeTurbina(int codigo) const { return buscar(turbinas, codigo); }
