#include "campos.h"
#include <type_traits>

namespace {

template <typename T>
T deValor(const Valor& v) {
    if (const T* p = std::get_if<T>(&v)) return *p;
    if constexpr (std::is_same_v<T, float>) {
        if (const int32_t* i = std::get_if<int32_t>(&v)) return static_cast<float>(*i);
    }
    if constexpr (std::is_same_v<T, int32_t>) {
        if (const float* f = std::get_if<float>(&v)) return static_cast<int32_t>(*f);
    }
    return T{};
}

template <auto M>
Valor obterEscalar(const UsinaHidr& u, int) { return u.*M; }

template <auto M>
void definirEscalar(UsinaHidr& u, int, const Valor& v) {
    using T = std::remove_cvref_t<decltype(u.*M)>;
    u.*M = deValor<T>(v);
}

template <auto M>
Valor obterVetor(const UsinaHidr& u, int i) { return (u.*M)[i]; }

template <auto M>
void definirVetor(UsinaHidr& u, int i, const Valor& v) {
    using T = std::remove_cvref_t<decltype((u.*M)[0])>;
    (u.*M)[i] = deValor<T>(v);
}

Valor obterPolConjunto(const UsinaHidr& u, int i) {
    return u.pol_conjunto[i / 15][(i % 15) / 5][i % 5];
}
void definirPolConjunto(UsinaHidr& u, int i, const Valor& v) {
    u.pol_conjunto[i / 15][(i % 15) / 5][i % 5] = deValor<float>(v);
}
Valor obterPolJusante(const UsinaHidr& u, int i) { return u.pol_jusante[i / 5][i % 5]; }
void definirPolJusante(UsinaHidr& u, int i, const Valor& v) {
    u.pol_jusante[i / 5][i % 5] = deValor<float>(v);
}

std::string coef(const std::string& prefixo, int i) { return prefixo + "_a" + std::to_string(i); }
std::string nomePolCotaVol(int i) { return coef("pol_cota_vol", i); }
std::string nomePolAreaCota(int i) { return coef("pol_area_cota", i); }
std::string nomeEvap(int i) {
    static const char* meses[] = {"jan", "fev", "mar", "abr", "mai", "jun",
                                  "jul", "ago", "set", "out", "nov", "dez"};
    return std::string("evap_") + meses[i];
}
std::string conj(int c, const char* sufixo) { return "conj" + std::to_string(c + 1) + "_" + sufixo; }
std::string nomeNmaq(int i) { return conj(i, "nmaq"); }
std::string nomePot(int i) { return conj(i, "pot"); }
std::string nomeQef(int i) { return conj(i, "qef"); }
std::string nomeHef(int i) { return conj(i, "hef"); }
std::string nomePolConjunto(int i) {
    static const char* pol[] = {"turb", "ger", "potp"};
    return coef(conj(i / 15, pol[(i % 15) / 5]), i % 5);
}
std::string nomePolJusante(int i) { return coef("pol_jus" + std::to_string(i / 5 + 1), i % 5); }
std::string nomeRefJusante(int i) { return "pol_jus" + std::to_string(i + 1) + "_ref"; }

#define ESCALAR(nome, tipo, tam) \
    Campo{#nome, 0, tam, 1, tipo, &obterEscalar<&UsinaHidr::nome>, &definirEscalar<&UsinaHidr::nome>, nullptr}
#define VETOR(nome, tipo, tam, n, rot) \
    Campo{#nome, 0, tam, n, tipo, &obterVetor<&UsinaHidr::nome>, &definirVetor<&UsinaHidr::nome>, rot}

std::vector<Campo> construir() {
    using enum TipoCampo;
    std::vector<Campo> v = {
        ESCALAR(nome, Texto, 12),
        ESCALAR(posto, Inteiro, 4),
        ESCALAR(posto_bdh, Texto, 8),
        ESCALAR(subsistema, Inteiro, 4),
        ESCALAR(empresa, Inteiro, 4),
        ESCALAR(jusante, Inteiro, 4),
        ESCALAR(desvio, Inteiro, 4),
        ESCALAR(volume_minimo, Real, 4),
        ESCALAR(volume_maximo, Real, 4),
        ESCALAR(volume_vertedouro, Real, 4),
        ESCALAR(volume_desvio, Real, 4),
        ESCALAR(cota_minima, Real, 4),
        ESCALAR(cota_maxima, Real, 4),
        VETOR(pol_cota_volume, Real, 4, 5, &nomePolCotaVol),
        VETOR(pol_area_cota, Real, 4, 5, &nomePolAreaCota),
        VETOR(evaporacao, Inteiro, 4, 12, &nomeEvap),
        ESCALAR(num_conjuntos, Inteiro, 4),
        VETOR(num_maquinas, Inteiro, 4, 5, &nomeNmaq),
        VETOR(potencia_efetiva, Real, 4, 5, &nomePot),
        Campo{"pol_conjunto", 0, 4, 75, Real, &obterPolConjunto, &definirPolConjunto, &nomePolConjunto},
        VETOR(altura_efetiva, Real, 4, 5, &nomeHef),
        VETOR(vazao_efetiva, Inteiro, 4, 5, &nomeQef),
        ESCALAR(produtibilidade, Real, 4),
        ESCALAR(perdas, Real, 4),
        ESCALAR(num_pol_jusante, Inteiro, 4),
        Campo{"pol_jusante", 0, 4, 30, Real, &obterPolJusante, &definirPolJusante, &nomePolJusante},
        VETOR(ref_pol_jusante, Real, 4, 6, &nomeRefJusante),
        ESCALAR(canal_fuga_medio, Real, 4),
        ESCALAR(influencia_vertimento, Inteiro, 4),
        ESCALAR(fator_carga_maximo, Real, 4),
        ESCALAR(fator_carga_minimo, Real, 4),
        ESCALAR(vazao_minima_historica, Inteiro, 4),
        ESCALAR(num_unidades_base, Inteiro, 4),
        ESCALAR(tipo_turbina, Inteiro, 4),
        ESCALAR(representacao_conjunto, Inteiro, 4),
        ESCALAR(teif, Real, 4),
        ESCALAR(ip, Real, 4),
        ESCALAR(tipo_perda, Inteiro, 4),
        ESCALAR(data, Texto, 12),
        ESCALAR(observacao, Texto, 39),
        ESCALAR(volume_referencia, Real, 4),
        ESCALAR(regulacao, Texto, 1),
    };
    int offset = 0;
    for (Campo& c : v) {
        c.offset = offset;
        offset += c.tamanho();
    }
    return v;
}

#undef ESCALAR
#undef VETOR

}  // namespace

const std::vector<Campo>& campos() {
    static const std::vector<Campo> v = construir();
    return v;
}

const Campo* campo(std::string_view nome) {
    for (const Campo& c : campos())
        if (c.nome == nome) return &c;
    return nullptr;
}
