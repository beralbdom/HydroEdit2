#include "deck_newave.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {
std::string lerTexto(const std::filesystem::path& caminho, bool& ok) {
    std::ifstream f(caminho, std::ios::binary);
    ok = static_cast<bool>(f);
    std::stringstream conteudo;
    if (ok) conteudo << f.rdbuf();
    return conteudo.str();
}

std::string aparar(const std::string& s) {
    size_t ini = s.find_first_not_of(" \t\r");
    if (ini == std::string::npos) return {};
    return s.substr(ini, s.find_last_not_of(" \t\r") - ini + 1);
}

std::string colunas(const std::string& linha, size_t primeira, size_t ultima) {
    if (linha.size() < primeira) return {};
    return linha.substr(primeira - 1, ultima - primeira + 1);
}

std::vector<std::string> separar(const std::string& texto) {
    std::vector<std::string> partes;
    std::istringstream entrada(texto);
    for (std::string parte; entrada >> parte;) partes.push_back(parte);
    return partes;
}
}  // namespace

// arquivos.dat (manual do NEWAVE 30.0.2, secao 3.3): uma linha por arquivo, com a descricao antes
// dos dois-pontos e o nome do arquivo depois. Devolve {descricao aparada -> nome}; linhas sem
// dois-pontos ou sem nome sao ignoradas.
std::map<std::string, std::string> interpretarArquivosDat(const std::string& conteudo) {
    std::map<std::string, std::string> arquivos;
    std::istringstream entrada(conteudo);
    for (std::string linha; std::getline(entrada, linha);) {
        size_t sep = linha.find(':');
        if (sep == std::string::npos) continue;
        std::string rotulo = aparar(linha.substr(0, sep));
        std::string nome = aparar(linha.substr(sep + 1));
        if (!rotulo.empty() && !nome.empty()) arquivos[rotulo] = nome;
    }
    return arquivos;
}

std::map<std::string, std::string> lerArquivosDat(const std::filesystem::path& caminho) {
    bool ok = false;
    std::string conteudo = lerTexto(caminho, ok);
    return ok ? interpretarArquivosDat(conteudo) : std::map<std::string, std::string>{};
}

// modif.dat (manual do NEWAVE 30.0.2, secao 3.12): duas linhas de comentario, depois blocos que
// comecam por USINA (colunas 2-9) com o codigo da usina nas colunas 11-30; o texto depois da coluna
// 30 e guardado como comentario. Cada registro seguinte tem a palavra-chave nas colunas 2-9, em
// maiusculas ou minusculas, e os valores em formato livre nas colunas 11-70, guardados como texto.
// Registro antes do primeiro USINA ou segundo bloco da mesma usina geram aviso; USINA sem codigo
// valido e erro.
ResultadoModif interpretarModif(const std::string& conteudo) {
    ResultadoModif r;
    std::istringstream entrada(conteudo);
    int numero = 0;
    std::vector<int> usinas_vistas;
    for (std::string linha; std::getline(entrada, linha);) {
        ++numero;
        if (numero <= 2) continue;
        if (!linha.empty() && linha.back() == '\r') linha.pop_back();
        std::string chave = aparar(colunas(linha, 2, 9));
        if (chave.empty()) continue;
        std::transform(chave.begin(), chave.end(), chave.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (chave == "USINA") {
            std::vector<std::string> codigo = separar(colunas(linha, 11, 30));
            BlocoModif bloco;
            try {
                bloco.usina = codigo.empty() ? 0 : std::stoi(codigo[0]);
            } catch (...) {
                bloco.usina = 0;
            }
            if (bloco.usina <= 0) {
                r.erro = "Linha " + std::to_string(numero) + ": USINA sem codigo valido";
                return r;
            }
            if (std::find(usinas_vistas.begin(), usinas_vistas.end(), bloco.usina) != usinas_vistas.end())
                r.avisos.push_back("Linha " + std::to_string(numero) + ": segundo bloco da usina " + std::to_string(bloco.usina));
            usinas_vistas.push_back(bloco.usina);
            bloco.comentario = aparar(linha.size() > 30 ? linha.substr(30) : std::string());
            bloco.linha = numero;
            r.blocos.push_back(std::move(bloco));
            continue;
        }
        if (r.blocos.empty()) {
            r.avisos.push_back("Linha " + std::to_string(numero) + ": " + chave + " antes do primeiro USINA, ignorado");
            continue;
        }
        r.blocos.back().registros.push_back({chave, separar(colunas(linha, 11, 70)), numero});
    }
    return r;
}

ResultadoModif lerModif(const std::filesystem::path& caminho) {
    bool ok = false;
    std::string conteudo = lerTexto(caminho, ok);
    if (!ok) {
        ResultadoModif r;
        r.erro = "Nao foi possivel abrir " + caminho.string();
        return r;
    }
    return interpretarModif(conteudo);
}
