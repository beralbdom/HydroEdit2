#include "layouts_newave.h"
#include <map>

namespace {
using T = TipoColunaFixa;

ColunaFixa inteiro(const char* nome, int inicio, int fim) { return {nome, inicio, fim, T::Inteiro, 0}; }
ColunaFixa real(const char* nome, int inicio, int fim, int decimais) { return {nome, inicio, fim, T::Real, decimais}; }
ColunaFixa texto(const char* nome, int inicio, int fim) { return {nome, inicio, fim, T::Texto, 0}; }

// conft.dat, manual do NEWAVE 30.0.2, secao 3.15: dois registros de comentario e um registro por
// usina, campos 1 a 7.
LayoutArquivoFixo conft() {
    return {"3.15",
            {{"Usinas termoelétricas na configuração", 2, "",
              {inteiro("Usina", 2, 5), texto("Nome", 7, 18), inteiro("Submercado", 22, 25), texto("Situação", 31, 32),
               inteiro("Classe", 36, 39), inteiro("Tecnologia", 41, 43), inteiro("Classe de gás", 45, 48)},
              0}}};
}

// term.dat, secao 3.16: dois registros de comentario e um registro por usina, campos 1 a 19. Os
// campos 7 a 18 sao a geracao minima de cada mes dos anos de manutencao, de 7 em 7 colunas a partir
// da 46; o 19 e a dos demais anos.
LayoutArquivoFixo term() {
    SecaoFixa usinas{"Usinas termoelétricas", 2, "",
                     {inteiro("Usina", 2, 4), texto("Nome", 6, 17), real("Capacidade (MW)", 20, 24, 0),
                      real("FC máx. (%)", 26, 29, 0), real("TEIF (%)", 32, 37, 2), real("IP demais anos (%)", 39, 44, 2)},
                     0};
    static const char* meses[] = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};
    for (int m = 0; m < 12; ++m) {
        std::string nome = std::string("GT mín. ") + meses[m] + " (MWmês)";
        usinas.colunas.push_back({nome, 46 + 7 * m, 51 + 7 * m, T::Real, 2});
    }
    usinas.colunas.push_back(real("GT mín. demais anos (MWmês)", 130, 135, 2));
    return {"3.16", {usinas}};
}

// expt.dat, secao 3.17: dois registros de comentario e um registro por modificacao, campos 1 a 7.
LayoutArquivoFixo expt() {
    return {"3.17",
            {{"Modificações por período", 2, "",
              {inteiro("Usina", 1, 4), texto("Tipo", 6, 10), real("Valor", 12, 19, 2), inteiro("Mês início", 21, 22),
               inteiro("Ano início", 24, 27), inteiro("Mês fim", 29, 30), inteiro("Ano fim", 32, 35)},
              0}}};
}

// clast.dat, secao 3.18: dois registros de comentario, os registros tipo 1 (um por classe, com um
// custo por ano de planejamento a cada 8 colunas a partir da 31) ate o 9999 no campo 1, mais dois
// registros de comentario e os registros tipo 2 (modificacoes de custo).
LayoutArquivoFixo clast() {
    return {"3.18",
            {{"Custos das classes", 2, "9999",
              {inteiro("Classe", 2, 5), texto("Nome", 7, 18), texto("Combustível", 20, 29), real("Custo ano", 31, 37, 2)},
              8},
             {"Modificações de custo", 2, "",
              {inteiro("Classe", 2, 5), real("Custo ($/MWh)", 9, 15, 2), inteiro("Mês início", 18, 19),
               inteiro("Ano início", 21, 24), inteiro("Mês fim", 27, 28), inteiro("Ano fim", 30, 33)},
              0}}};
}

// manutt.dat, secao 3.19: dois registros de comentario e um registro por manutencao; dos 13 campos
// so os 6 que o programa le tem colunas no manual (usina, data de inicio ddmmaaaa, duracao e
// potencia).
LayoutArquivoFixo manutt() {
    return {"3.19",
            {{"Manutenções programadas", 2, "",
              {inteiro("Usina", 18, 20), inteiro("Dia início", 41, 42), inteiro("Mês início", 43, 44),
               inteiro("Ano início", 45, 48), inteiro("Duração (dias)", 50, 52), real("Potência (MW)", 56, 62, 2)},
              0}}};
}
}  // namespace

// Layout de colunas fixas dos arquivos do NEWAVE que ja tem tabela editavel, pelo nome padrao do
// arquivo; nullptr para os que ainda so tem previa.
const LayoutArquivoFixo* layoutNewave(const std::string& nome_padrao) {
    static const std::map<std::string, LayoutArquivoFixo> layouts = {
        {"conft.dat", conft()}, {"term.dat", term()}, {"expt.dat", expt()}, {"clast.dat", clast()}, {"manutt.dat", manutt()},
    };
    auto it = layouts.find(nome_padrao);
    return it == layouts.end() ? nullptr : &it->second;
}
