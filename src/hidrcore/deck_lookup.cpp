#include "deck_lookup.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
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

std::string paraMinusculas(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool contemArquivoNoDiretorio(const fs::path& dir, const char* nome) {
    std::error_code ec;
    std::string alvo = paraMinusculas(nome);
    for (const auto& entrada : fs::directory_iterator(dir, ec)) {
        if (paraMinusculas(entrada.path().filename().string()) == alvo) return true;
    }
    return false;
}

// Modelo do deck pela presenca dos arquivos de topo (manual DESSEM 19.0.44, secao III.7): o
// DESSEM referencia o cadastro hidr.dat a partir do dessem.arq; o NEWAVE/DECOMP tem arquivos.dat
// ou dger.dat na raiz do deck.
ModeloDeck detectarModelo(const fs::path& dir_deck) {
    if (contemArquivoNoDiretorio(dir_deck, "dessem.arq")) return ModeloDeck::Dessem;
    if (contemArquivoNoDiretorio(dir_deck, "arquivos.dat") || contemArquivoNoDiretorio(dir_deck, "dger.dat"))
        return ModeloDeck::Newave;
    return ModeloDeck::Desconhecido;
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
// Manual NEWAVE 30.0.2, secao 3.10: os anos sao o inicio e o fim do registro de vazoes historicas.
// Os anos so sao guardados para postos com nome.
void lerPostos(const fs::path& p, std::vector<std::string>& notas, std::map<int, std::string>& nomes,
               std::map<int, std::pair<int, int>>& anos) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        notas.push_back("postos.dat nao encontrado");
        return;
    }
    char reg[20];
    int codigo = 1;
    while (f.read(reg, 20)) {
        std::string nome = apararDireita(std::string_view(reg, 12));
        if (!nome.empty()) {
            int32_t inicio = 0;
            int32_t fim = 0;
            std::memcpy(&inicio, reg + 12, 4);
            std::memcpy(&fim, reg + 16, 4);
            nomes[codigo] = nome;
            anos[codigo] = {inicio, fim};
        }
        ++codigo;
    }
}

// confhd.dat: duas linhas de cabecalho, depois uma usina por linha (manual NEWAVE 30.0.2, secao
// 3.9, colunas em base 1: campo 1 nas colunas 2-5 = codigo da usina, campo 3 nas colunas 20-23 =
// posto, campo 4 nas colunas 26-29 = usina a jusante, campo 5 nas colunas 31-34 = REE). Linhas
// curtas ou sem numero nessas colunas sao ignoradas.
void lerConfhd(const fs::path& p, std::vector<std::string>& notas, std::map<int, int>& ree_da_usina,
               std::map<int, UsinaConfhd>& confhd) {
    std::ifstream f(p);
    if (!f) {
        notas.push_back("confhd.dat nao encontrado");
        return;
    }
    std::string linha;
    int cabecalhos = 0;
    while (std::getline(f, linha)) {
        if (cabecalhos < 2) {
            ++cabecalhos;
            continue;
        }
        if (linha.size() < 34) continue;
        try {
            int codigo = std::stoi(linha.substr(1, 4));
            UsinaConfhd u{std::stoi(linha.substr(19, 4)), std::stoi(linha.substr(25, 4))};
            int ree = std::stoi(linha.substr(30, 4));
            ree_da_usina[codigo] = ree;
            confhd[codigo] = u;
        } catch (...) {
            continue;
        }
    }
}

// ree.dat, bloco "REES X SUBMERCADOS": cabecalho "NUM|NOME REES.|...", uma linha de mascara
// "XXX|...", depois um REE por linha ate a sentinela 999. Codigo nas colunas 1-4, nome nas
// colunas 6-15 e o submercado e o primeiro inteiro depois da coluna 15 (manual NEWAVE 30.0.2).
std::map<int, Ree> lerRee(const fs::path& p, std::vector<std::string>& notas) {
    std::map<int, Ree> m;
    std::ifstream f(p);
    if (!f) {
        notas.push_back("ree.dat nao encontrado");
        return m;
    }
    std::string linha;
    bool no_bloco = false;
    bool mascara_pulada = false;
    while (std::getline(f, linha)) {
        if (!no_bloco) {
            if (comecaCom(linha, "NUM|NOME")) no_bloco = true;
            continue;
        }
        if (!mascara_pulada) {
            mascara_pulada = true;
            continue;
        }
        if (comecaCom(linha, "999")) break;
        if (linha.size() < 16) continue;
        try {
            int codigo = std::stoi(linha.substr(0, 4));
            Ree ree;
            std::string nome = apararDireita(linha.substr(5, 10));
            size_t ini = nome.find_first_not_of(' ');
            ree.nome = ini == std::string::npos ? std::string() : nome.substr(ini);
            std::string resto = linha.substr(15);
            size_t digito = resto.find_first_of("0123456789");
            // Submercado ilegivel nao invalida o REE: o nome ja serve para rotular a faixa, entao o
            // registro entra com submercado 0.
            if (digito != std::string::npos) {
                try {
                    ree.submercado = std::stoi(resto.substr(digito));
                } catch (...) {
                    ree.submercado = 0;
                }
            }
            m[codigo] = ree;
        } catch (...) {
            continue;
        }
    }
    return m;
}

// entdados.dat, registros SIST/REE/UH identificados pelo prefixo e lidos por colunas fixas em
// base 1 (manual DESSEM 19.0.44: secao III.4.2.1 SIST, III.4.2.3 REE, III.4.2.4 UH). Linhas
// curtas demais para as colunas do registro ou com numero invalido sao ignoradas; nao ha
// postos.dat no DESSEM.
void lerEntdados(const fs::path& p, std::vector<std::string>& notas, std::map<int, std::string>& subsistemas,
                  std::map<int, Ree>& rees, std::map<int, int>& ree_da_usina) {
    std::ifstream f(p);
    if (!f) {
        notas.push_back("entdados.dat nao encontrado");
        return;
    }
    std::string linha;
    while (std::getline(f, linha)) {
        try {
            if (linha.compare(0, 4, "SIST") == 0) {
                subsistemas[std::stoi(linha.substr(7, 2))] = apararDireita(linha.substr(16, 10));
            } else if (linha.compare(0, 4, "REE ") == 0) {
                Ree ree;
                ree.submercado = std::stoi(linha.substr(9, 2));
                ree.nome = apararDireita(linha.substr(12, 10));
                rees[std::stoi(linha.substr(6, 2))] = ree;
            } else if (linha.compare(0, 4, "UH  ") == 0) {
                ree_da_usina[std::stoi(linha.substr(4, 3))] = std::stoi(linha.substr(24, 2));
            }
        } catch (...) {
            continue;
        }
    }
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

const char* DeckLookup::nomeModelo(ModeloDeck m) {
    switch (m) {
        case ModeloDeck::Newave: return "NEWAVE";
        case ModeloDeck::Dessem: return "DESSEM";
        default: return "";
    }
}

void DeckLookup::carregarDeck(const fs::path& dir_deck) {
    modelo = detectarModelo(dir_deck);
    if (modelo == ModeloDeck::Dessem) {
        lerEntdados(dir_deck / "entdados.dat", notas, subsistemas, rees, ree_da_usina);
        return;
    }
    subsistemas = lerSistema(dir_deck / "sistema.dat", notas);
    lerPostos(dir_deck / "postos.dat", notas, postos, anos_postos);
    lerConfhd(dir_deck / "confhd.dat", notas, ree_da_usina, confhd);
    rees = lerRee(dir_deck / "ree.dat", notas);
}

void DeckLookup::carregarCsvs(const fs::path& dir_exe) {
    empresas = lerCsvCodigoNome(dir_exe / "empresas.csv", notas);
    turbinas = lerCsvCodigoNome(dir_exe / "turbinas.csv", notas);
}

std::string DeckLookup::nomeSubsistema(int codigo) const { return buscar(subsistemas, codigo); }
std::string DeckLookup::nomePosto(int codigo) const { return buscar(postos, codigo); }

// Primeiro ano do historico de vazoes: o menor ano inicial entre os postos do postos.dat, ou 0 sem
// postos. E o ano do primeiro registro do vazoes.dat.
int DeckLookup::anoInicialHistorico() const {
    int ano = 0;
    for (const auto& [codigo, anos] : anos_postos) {
        if (anos.first > 0 && (ano == 0 || anos.first < ano)) ano = anos.first;
    }
    return ano;
}
std::string DeckLookup::nomeEmpresa(int codigo) const { return buscar(empresas, codigo); }
std::string DeckLookup::nomeTurbina(int codigo) const { return buscar(turbinas, codigo); }

int DeckLookup::reeDaUsina(int codigo) const {
    auto it = ree_da_usina.find(codigo);
    return it == ree_da_usina.end() ? 0 : it->second;
}

std::string DeckLookup::nomeRee(int ree) const {
    auto it = rees.find(ree);
    return it == rees.end() ? std::string() : it->second.nome;
}

int DeckLookup::submercadoDoRee(int ree) const {
    auto it = rees.find(ree);
    return it == rees.end() ? 0 : it->second.submercado;
}
