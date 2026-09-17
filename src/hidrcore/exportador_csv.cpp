#include "exportador_csv.h"
#include <format>
#include <fstream>
#include "campos.h"
#include "texto.h"

namespace {

std::string formatarValor(const Valor& v, const OpcoesCsv& o) {
    if (const std::string* s = std::get_if<std::string>(&v)) {
        std::string t = latin1ParaUtf8(*s);
        bool precisa_aspas = t.find(o.separador) != std::string::npos || t.find('"') != std::string::npos;
        if (!precisa_aspas) return t;
        std::string r = "\"";
        for (char c : t) {
            if (c == '"') r += '"';
            r += c;
        }
        return r + "\"";
    }
    if (const int32_t* i = std::get_if<int32_t>(&v)) return std::to_string(*i);
    std::string r = std::format("{}", std::get<float>(v));
    if (o.virgula_decimal)
        for (char& c : r)
            if (c == '.') c = ',';
    return r;
}

}  // namespace

std::string cabecalhoCsv(const OpcoesCsv& o) {
    std::string h = "codigo";
    for (const Campo& c : campos()) {
        for (int i = 0; i < c.n; ++i) {
            h += o.separador;
            h += c.escalar() ? std::string(c.nome) : c.nome_elemento(i);
        }
    }
    return h;
}

std::string linhaCsv(int codigo, const UsinaHidr& u, const OpcoesCsv& o) {
    std::string l = std::to_string(codigo);
    for (const Campo& c : campos()) {
        for (int i = 0; i < c.n; ++i) {
            l += o.separador;
            l += formatarValor(c.obter(u, i), o);
        }
    }
    return l;
}

Resultado exportarCsv(const ArquivoHidr& a, const std::filesystem::path& caminho, const OpcoesCsv& o) {
    std::ofstream f(caminho, std::ios::binary | std::ios::trunc);
    if (!f) return Resultado::erro("Nao foi possivel criar " + caminho.string());
    f << "\xEF\xBB\xBF" << cabecalhoCsv(o) << "\r\n";
    for (size_t i = 0; i < a.usinas.size(); ++i)
        if (!a.usinas[i].vazia()) f << linhaCsv(static_cast<int>(i) + 1, a.usinas[i], o) << "\r\n";
    if (!f) return Resultado::erro("Falha ao escrever " + caminho.string());
    return Resultado::sucesso();
}
