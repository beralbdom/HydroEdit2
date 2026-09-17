#include "validacao.h"

namespace {

struct Coletor {
    std::vector<Problema> problemas;
    void erro(const char* campo, std::string msg) { problemas.push_back({campo, Severidade::Erro, std::move(msg)}); }
    void aviso(const char* campo, std::string msg) { problemas.push_back({campo, Severidade::Aviso, std::move(msg)}); }
    void naoNegativo(const char* campo, float v, const char* rotulo) {
        if (v < 0) erro(campo, std::string(rotulo) + " negativo");
    }
};

void validarCadastro(const UsinaHidr& u, const ContextoValidacao& ctx, Coletor& c) {
    if (u.vazia()) c.erro("nome", "Nome da usina vazio");
    if (u.posto <= 0) c.erro("posto", "Posto deve ser maior que zero");
    auto refUsina = [&](const char* campo, int32_t v, const char* rotulo) {
        if (v == 0) return;
        if (v < 1 || v > ctx.num_usinas)
            c.erro(campo, std::string(rotulo) + " fora da faixa 1.." + std::to_string(ctx.num_usinas));
        else if (v == ctx.codigo)
            c.erro(campo, std::string(rotulo) + " igual a propria usina");
    };
    refUsina("jusante", u.jusante, "Usina a jusante");
    refUsina("desvio", u.desvio, "Usina de desvio");
    if (u.regulacao != "M" && u.regulacao != "S" && u.regulacao != "D")
        c.erro("regulacao", "Regulacao deve ser M, S ou D");
}

void validarReservatorio(const UsinaHidr& u, Coletor& c) {
    c.naoNegativo("volume_minimo", u.volume_minimo, "Volume minimo");
    c.naoNegativo("volume_maximo", u.volume_maximo, "Volume maximo");
    c.naoNegativo("volume_vertedouro", u.volume_vertedouro, "Volume do vertedouro");
    c.naoNegativo("volume_desvio", u.volume_desvio, "Volume do desvio");
    c.naoNegativo("volume_referencia", u.volume_referencia, "Volume de referencia");
    c.naoNegativo("cota_minima", u.cota_minima, "Cota minima");
    c.naoNegativo("cota_maxima", u.cota_maxima, "Cota maxima");
    if (u.volume_minimo > u.volume_maximo) c.erro("volume_minimo", "Volume minimo maior que o maximo");
    auto entreVminVmax = [&](const char* campo, float v, const char* rotulo) {
        if (v < u.volume_minimo) c.erro(campo, std::string(rotulo) + " menor que o volume minimo");
        if (v > u.volume_maximo) c.erro(campo, std::string(rotulo) + " maior que o volume maximo");
    };
    entreVminVmax("volume_vertedouro", u.volume_vertedouro, "Volume do vertedouro");
    entreVminVmax("volume_desvio", u.volume_desvio, "Volume do desvio");
    entreVminVmax("volume_referencia", u.volume_referencia, "Volume de referencia");
    if (u.cota_minima > u.cota_maxima) c.erro("cota_minima", "Cota minima maior que a maxima");
    if (u.pol_cota_volume[0] == 0) c.erro("pol_cota_volume", "Termo A0 do polinomio cota x volume igual a zero");
    if (u.pol_area_cota[0] == 0) c.erro("pol_area_cota", "Termo A0 do polinomio area x cota igual a zero");
}

void validarConjuntos(const UsinaHidr& u, Coletor& c) {
    if (u.num_conjuntos < 0 || u.num_conjuntos > MAX_CONJUNTOS) {
        c.erro("num_conjuntos", "Numero de conjuntos deve estar entre 0 e 5");
        return;
    }
    static const char* pol[] = {"turbina", "gerador", "potencia"};
    for (int i = 0; i < u.num_conjuntos; ++i) {
        std::string k = " do conjunto " + std::to_string(i + 1);
        if (u.num_maquinas[i] < 0) c.erro("num_maquinas", "Numero de maquinas" + k + " negativo");
        if (u.potencia_efetiva[i] < 0) c.erro("potencia_efetiva", "Potencia efetiva" + k + " negativa");
        if (u.vazao_efetiva[i] < 0) c.erro("vazao_efetiva", "Vazao efetiva" + k + " negativa");
        if (u.altura_efetiva[i] < 0) c.erro("altura_efetiva", "Altura efetiva" + k + " negativa");
        for (int p = 0; p < 3; ++p)
            if (u.pol_conjunto[i][p][0] == 0)
                c.aviso("pol_conjunto", std::string("Termo A0 do polinomio de ") + pol[p] + k + " igual a zero");
    }
}

void validarJusante(const UsinaHidr& u, Coletor& c) {
    if (u.num_pol_jusante < 1 || u.num_pol_jusante > MAX_POL_JUSANTE) {
        c.erro("num_pol_jusante", "Numero de polinomios de jusante deve estar entre 1 e 6");
    } else {
        for (int j = 0; j < u.num_pol_jusante; ++j) {
            std::string k = " do polinomio de jusante " + std::to_string(j + 1);
            if (u.pol_jusante[j][0] == 0) c.erro("pol_jusante", "Termo A0" + k + " igual a zero");
            if (u.ref_pol_jusante[j] < 0) c.erro("ref_pol_jusante", "Referencia" + k + " negativa");
        }
    }
    if (u.canal_fuga_medio < 0)
        c.erro("canal_fuga_medio", "Canal de fuga medio negativo");
    else if (u.canal_fuga_medio >= u.cota_minima)
        c.erro("canal_fuga_medio", "Canal de fuga medio deve ser menor que a cota minima");
    if (u.influencia_vertimento != 0 && u.influencia_vertimento != 1)
        c.erro("influencia_vertimento", "Influencia do vertimento deve ser 0 ou 1");
}

void validarOperacao(const UsinaHidr& u, Coletor& c) {
    c.naoNegativo("produtibilidade", u.produtibilidade, "Produtibilidade especifica");
    c.naoNegativo("perdas", u.perdas, "Perdas");
    c.naoNegativo("fator_carga_maximo", u.fator_carga_maximo, "Fator de carga maximo");
    c.naoNegativo("fator_carga_minimo", u.fator_carga_minimo, "Fator de carga minimo");
    c.naoNegativo("teif", u.teif, "TEIF");
    c.naoNegativo("ip", u.ip, "IP");
    if (u.vazao_minima_historica < 0) c.erro("vazao_minima_historica", "Vazao minima do historico negativa");
    if (u.num_unidades_base < 0) c.erro("num_unidades_base", "Numero de unidades de base negativo");
    if (u.fator_carga_minimo > u.fator_carga_maximo)
        c.erro("fator_carga_minimo", "Fator de carga minimo maior que o maximo");
}

}  // namespace

std::vector<Problema> validar(const UsinaHidr& u, const ContextoValidacao& ctx) {
    Coletor c;
    validarCadastro(u, ctx, c);
    validarReservatorio(u, c);
    validarConjuntos(u, c);
    validarJusante(u, c);
    validarOperacao(u, c);
    return std::move(c.problemas);
}

bool temErro(const std::vector<Problema>& problemas) {
    for (const Problema& p : problemas)
        if (p.severidade == Severidade::Erro) return true;
    return false;
}
