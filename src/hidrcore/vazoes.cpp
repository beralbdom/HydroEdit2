#include "vazoes.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#include "texto.h"

int SerieVazoes::meses() const { return num_postos > 0 ? static_cast<int>(valores.size()) / num_postos : 0; }

bool SerieVazoes::postoValido(int posto) const { return posto >= 1 && posto <= num_postos; }

int32_t SerieVazoes::valor(int posto, int mes) const {
    return valores[static_cast<size_t>(mes) * static_cast<size_t>(num_postos) + static_cast<size_t>(posto - 1)];
}

// Manual do NEWAVE 30.0.2, secao 3.14: vazoes.dat e o cadastro de vazoes naturais historicas, "arquivo
// de acesso direto, nao formatado, com 320/600 postos, cada registro correspondendo a um mes do
// historico". Cada registro tem um int32 little-endian por posto, em m3/s, na ordem 1..num_postos, e
// num_postos e o numero de registros do hidr.dat. O arquivo nao traz a data: o primeiro registro e
// janeiro de ano_inicial, que vem do postos.dat (secao 3.10). Conferido no deck de set/2026: 1152
// registros de 320 postos a partir de 1931, com os ultimos meses iguais aos do vazpast.dat.
ResultadoLeituraVazoes lerVazoesDat(const std::filesystem::path& caminho, int num_postos, int ano_inicial) {
    ResultadoLeituraVazoes r;
    if (num_postos <= 0) {
        r.erro = "Numero de postos invalido";
        return r;
    }
    std::ifstream f(caminho, std::ios::binary);
    if (!f) {
        r.erro = "Nao foi possivel abrir " + caminho.string();
        return r;
    }
    std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    size_t tamanho_registro = static_cast<size_t>(num_postos) * 4;
    if (bytes.empty() || bytes.size() % tamanho_registro != 0) {
        r.erro = "vazoes.dat com " + std::to_string(bytes.size()) + " bytes, que nao e multiplo de " +
                 std::to_string(num_postos) + " postos de 4 bytes";
        return r;
    }
    r.serie.ano_inicial = ano_inicial;
    r.serie.num_postos = num_postos;
    r.serie.valores.resize(bytes.size() / 4);
    std::memcpy(r.serie.valores.data(), bytes.data(), bytes.size());
    return r;
}

namespace {
struct NoTopologia {
    int codigo = 0;
    std::string nome;
    int posto = 0;
    int jusante = 0;
};

std::string descrever(int codigo, const std::string& nome) { return std::to_string(codigo) + " " + latin1ParaUtf8(nome); }

std::string latin1ParaCsv(const std::string& latin1) {
    std::string t = latin1ParaUtf8(latin1);
    if (t.find(',') == std::string::npos && t.find('"') == std::string::npos) return t;
    std::string entre_aspas = "\"";
    for (char c : t) {
        if (c == '"') entre_aspas += '"';
        entre_aspas += c;
    }
    return entre_aspas + "\"";
}
}  // namespace

// Regra definida pelo usuario (2026-09-22): vazao incremental de uma usina = vazao natural do posto
// dela menos a soma das vazoes naturais dos postos das usinas imediatamente a montante, ou seja, das
// usinas cuja jusante e ela. So a jusante conta como ligacao; o desvio fica de fora. Cada posto a
// montante entra uma vez, mesmo que duas usinas o compartilhem. Resultado negativo vira zero, e os
// meses truncados sao contados por usina.
//
// Topologia: usinas da configuracao (confhd.dat, manual secao 3.9, posto no campo 3 e jusante no
// campo 4) usam o posto e a jusante do confhd e so enxergam a montante outras usinas da configuracao,
// como o NEWAVE. As demais usinas do cadastro, ou todas quando nao ha confhd, usam posto e jusante do
// hidr.dat. Usina sem posto valido fica fora; posto a montante fora do arquivo e ignorado. Os dois
// casos geram aviso.
Incrementais calcularIncrementais(const std::vector<UsinaHidr>& usinas, const std::map<int, UsinaConfhd>& confhd,
                                  const SerieVazoes& serie) {
    std::vector<NoTopologia> pelo_hidr;
    std::vector<NoTopologia> pelo_confhd;
    for (size_t i = 0; i < usinas.size(); ++i) {
        const UsinaHidr& u = usinas[i];
        if (u.vazia()) continue;
        int codigo = static_cast<int>(i) + 1;
        std::string nome = apararDireita(u.nome);
        pelo_hidr.push_back({codigo, nome, u.posto, u.jusante});
        auto it = confhd.find(codigo);
        if (it != confhd.end()) pelo_confhd.push_back({codigo, nome, it->second.posto, it->second.jusante});
    }

    Incrementais resultado;
    int meses = serie.meses();
    for (const NoTopologia& no_hidr : pelo_hidr) {
        bool na_configuracao = confhd.count(no_hidr.codigo) > 0;
        const std::vector<NoTopologia>& mundo = na_configuracao ? pelo_confhd : pelo_hidr;
        const NoTopologia& alvo = *std::find_if(mundo.begin(), mundo.end(),
                                                [&](const NoTopologia& n) { return n.codigo == no_hidr.codigo; });
        if (!serie.postoValido(alvo.posto)) {
            resultado.avisos.push_back("Usina " + descrever(alvo.codigo, alvo.nome) + " sem posto valido (" +
                                       std::to_string(alvo.posto) + "), fora da exportacao");
            continue;
        }

        std::set<int> postos_montante;
        for (const NoTopologia& n : mundo) {
            if (n.jusante != alvo.codigo || n.codigo == alvo.codigo) continue;
            if (!serie.postoValido(n.posto)) {
                resultado.avisos.push_back("Usina " + descrever(n.codigo, n.nome) + ", a montante de " +
                                           descrever(alvo.codigo, alvo.nome) + ", sem posto valido (" +
                                           std::to_string(n.posto) + "), ignorada");
                continue;
            }
            postos_montante.insert(n.posto);
        }

        UsinaIncremental inc;
        inc.codigo = alvo.codigo;
        inc.nome = alvo.nome;
        inc.posto = alvo.posto;
        inc.postos_montante.assign(postos_montante.begin(), postos_montante.end());
        inc.vazoes.reserve(static_cast<size_t>(meses));
        for (int mes = 0; mes < meses; ++mes) {
            int64_t v = serie.valor(alvo.posto, mes);
            for (int p : inc.postos_montante) v -= serie.valor(p, mes);
            if (v < 0) {
                v = 0;
                ++inc.meses_truncados;
            }
            inc.vazoes.push_back(static_cast<int32_t>(v));
        }
        resultado.usinas.push_back(std::move(inc));
    }
    return resultado;
}

// Uma linha por usina e ano, com os doze meses em colunas, como o vazpast.dat. Delimitador virgula e
// ponto decimal (as vazoes sao inteiras e hoje nao tem casas), UTF-8 com BOM e fim de linha CRLF.
// Meses que o arquivo nao cobre no ultimo ano ficam vazios.
Resultado exportarIncrementaisCsv(const Incrementais& incrementais, const SerieVazoes& serie,
                                  const std::filesystem::path& caminho) {
    std::ofstream f(caminho, std::ios::binary | std::ios::trunc);
    if (!f) return Resultado::erro("Nao foi possivel criar " + caminho.string());
    f << "\xEF\xBB\xBF" << "codigo,nome,posto,ano,jan,fev,mar,abr,mai,jun,jul,ago,set,out,nov,dez\r\n";
    int meses = serie.meses();
    int anos = (meses + 11) / 12;
    for (const UsinaIncremental& u : incrementais.usinas) {
        std::string prefixo = std::to_string(u.codigo) + "," + latin1ParaCsv(u.nome) + "," + std::to_string(u.posto) + ",";
        for (int a = 0; a < anos; ++a) {
            f << prefixo << (serie.ano_inicial + a);
            for (int m = 0; m < 12; ++m) {
                int mes = a * 12 + m;
                f << ',';
                if (mes < meses) f << u.vazoes[static_cast<size_t>(mes)];
            }
            f << "\r\n";
        }
    }
    if (!f) return Resultado::erro("Falha ao escrever " + caminho.string());
    return Resultado::sucesso();
}
