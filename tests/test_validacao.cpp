#include <QtTest>
#include "validacao.h"

class TestValidacao : public QObject {
    Q_OBJECT
    static UsinaHidr valida() {
        UsinaHidr u;
        u.nome = "TESTE";
        u.posto = 1;
        u.volume_minimo = 100; u.volume_maximo = 200; u.volume_vertedouro = 150;
        u.volume_desvio = 100; u.volume_referencia = 200;
        u.cota_minima = 500; u.cota_maxima = 520;
        u.pol_cota_volume = {490, 0.1f, 0, 0, 0};
        u.pol_area_cota = {10, 0, 0, 0, 0};
        u.num_conjuntos = 1;
        u.num_maquinas = {2, 0, 0, 0, 0};
        u.potencia_efetiva = {50, 0, 0, 0, 0};
        u.altura_efetiva = {30, 0, 0, 0, 0};
        u.vazao_efetiva = {100, 0, 0, 0, 0};
        u.pol_conjunto[0][0][0] = 1; u.pol_conjunto[0][1][0] = 1; u.pol_conjunto[0][2][0] = 1;
        u.produtibilidade = 0.0088f; u.perdas = 0.5f;
        u.num_pol_jusante = 1;
        u.pol_jusante[0] = {480, 0.001f, 0, 0, 0};
        u.canal_fuga_medio = 480;
        u.influencia_vertimento = 1;
        u.fator_carga_maximo = 100; u.fator_carga_minimo = 0;
        u.vazao_minima_historica = 10; u.num_unidades_base = 1;
        u.teif = 1; u.ip = 1;
        u.regulacao = "M";
        return u;
    }
    static ContextoValidacao ctx() { return {320, 10}; }
    static bool temErroEm(const std::vector<Problema>& ps, const char* campo) {
        for (const Problema& p : ps)
            if (p.campo == campo && p.severidade == Severidade::Erro) return true;
        return false;
    }
    static bool temAvisoEm(const std::vector<Problema>& ps, const char* campo) {
        for (const Problema& p : ps)
            if (p.campo == campo && p.severidade == Severidade::Aviso) return true;
        return false;
    }
private slots:
    void usinaValidaNaoTemProblemas() {
        auto ps = validar(valida(), ctx());
        QVERIFY2(ps.empty(), ps.empty() ? "" : ps.front().mensagem.c_str());
    }
    void nomeVazio() { auto u = valida(); u.nome = "  "; QVERIFY(temErroEm(validar(u, ctx()), "nome")); }
    void postoZero() { auto u = valida(); u.posto = 0; QVERIFY(temErroEm(validar(u, ctx()), "posto")); }
    void volumeNegativo() { auto u = valida(); u.volume_desvio = -1; QVERIFY(temErroEm(validar(u, ctx()), "volume_desvio")); }
    void vminMaiorQueVmax() { auto u = valida(); u.volume_minimo = 300; QVERIFY(temErroEm(validar(u, ctx()), "volume_minimo")); }
    void vvertForaDaFaixa() {
        auto u = valida(); u.volume_vertedouro = 250;
        auto ps = validar(u, ctx());
        QVERIFY(temAvisoEm(ps, "volume_vertedouro"));
        QVERIFY(!temErro(ps));
    }
    void vdesvForaDaFaixa() {
        auto u = valida(); u.volume_desvio = 50;
        auto ps = validar(u, ctx());
        QVERIFY(temAvisoEm(ps, "volume_desvio"));
        QVERIFY(!temErro(ps));
    }
    void vrefForaDaFaixa() {
        auto u = valida(); u.volume_referencia = 201;
        auto ps = validar(u, ctx());
        QVERIFY(temAvisoEm(ps, "volume_referencia"));
        QVERIFY(!temErro(ps));
    }
    void cotaNegativa() { auto u = valida(); u.cota_minima = -1; QVERIFY(temErroEm(validar(u, ctx()), "cota_minima")); }
    void cminMaiorQueCmax() { auto u = valida(); u.cota_maxima = 400; QVERIFY(temErroEm(validar(u, ctx()), "cota_minima")); }
    void a0CotaVolumeZero() {
        auto u = valida(); u.pol_cota_volume[0] = 0;
        auto ps = validar(u, ctx());
        QVERIFY(temAvisoEm(ps, "pol_cota_volume"));
        QVERIFY(!temErro(ps));
    }
    void a0AreaCotaZero() {
        auto u = valida(); u.pol_area_cota[0] = 0;
        auto ps = validar(u, ctx());
        QVERIFY(temAvisoEm(ps, "pol_area_cota"));
        QVERIFY(!temErro(ps));
    }
    void numConjuntosForaDaFaixa() { auto u = valida(); u.num_conjuntos = 6; QVERIFY(temErroEm(validar(u, ctx()), "num_conjuntos")); }
    void conjuntoAtivoNegativo() { auto u = valida(); u.potencia_efetiva[0] = -1; QVERIFY(temErroEm(validar(u, ctx()), "potencia_efetiva")); }
    void conjuntoInativoNaoEhValidado() { auto u = valida(); u.potencia_efetiva[3] = -1; QVERIFY(!temErroEm(validar(u, ctx()), "potencia_efetiva")); }
    void polConjuntoA0ZeroEhAviso() {
        auto u = valida(); u.pol_conjunto[0][1][0] = 0;
        auto ps = validar(u, ctx());
        QVERIFY(temAvisoEm(ps, "pol_conjunto"));
        QVERIFY(!temErro(ps));
    }
    void numPolJusanteForaDaFaixa() {
        auto u = valida(); u.num_pol_jusante = 0;
        auto ps = validar(u, ctx());
        QVERIFY(temAvisoEm(ps, "num_pol_jusante"));
        QVERIFY(!temErro(ps));
    }
    void numPolJusanteAcimaDeSeisEhErro() { auto u = valida(); u.num_pol_jusante = 7; QVERIFY(temErroEm(validar(u, ctx()), "num_pol_jusante")); }
    void a0PolJusanteAtivoZero() {
        auto u = valida(); u.num_pol_jusante = 2; u.pol_jusante[1][0] = 0;
        auto ps = validar(u, ctx());
        QVERIFY(temAvisoEm(ps, "pol_jusante"));
        QVERIFY(!temErro(ps));
    }
    void refPolJusanteNegativa() { auto u = valida(); u.ref_pol_jusante[0] = -1; QVERIFY(temErroEm(validar(u, ctx()), "ref_pol_jusante")); }
    void canalFugaMaiorOuIgualCotaMinima() {
        auto u = valida(); u.canal_fuga_medio = 500;
        auto ps = validar(u, ctx());
        QVERIFY(temAvisoEm(ps, "canal_fuga_medio"));
        QVERIFY(!temErro(ps));
    }
    void canalFugaNegativo() { auto u = valida(); u.canal_fuga_medio = -1; QVERIFY(temErroEm(validar(u, ctx()), "canal_fuga_medio")); }
    void produtibilidadeNegativa() { auto u = valida(); u.produtibilidade = -1; QVERIFY(temErroEm(validar(u, ctx()), "produtibilidade")); }
    void perdasNegativas() { auto u = valida(); u.perdas = -1; QVERIFY(temErroEm(validar(u, ctx()), "perdas")); }
    void teifNegativa() { auto u = valida(); u.teif = -1; QVERIFY(temErroEm(validar(u, ctx()), "teif")); }
    void ipNegativo() { auto u = valida(); u.ip = -1; QVERIFY(temErroEm(validar(u, ctx()), "ip")); }
    void vazaoMinimaNegativa() { auto u = valida(); u.vazao_minima_historica = -1; QVERIFY(temErroEm(validar(u, ctx()), "vazao_minima_historica")); }
    void unidadesBaseNegativas() { auto u = valida(); u.num_unidades_base = -1; QVERIFY(temErroEm(validar(u, ctx()), "num_unidades_base")); }
    void fcMinMaiorQueFcMax() { auto u = valida(); u.fator_carga_minimo = 100; u.fator_carga_maximo = 50; QVERIFY(temErroEm(validar(u, ctx()), "fator_carga_minimo")); }
    void jusanteForaDaFaixa() { auto u = valida(); u.jusante = 321; QVERIFY(temErroEm(validar(u, ctx()), "jusante")); }
    void jusanteIgualPropria() { auto u = valida(); u.jusante = 10; QVERIFY(temErroEm(validar(u, ctx()), "jusante")); }
    void jusanteZeroEhValido() { auto u = valida(); u.jusante = 0; QVERIFY(!temErroEm(validar(u, ctx()), "jusante")); }
    void desvioNegativo() { auto u = valida(); u.desvio = -1; QVERIFY(temErroEm(validar(u, ctx()), "desvio")); }
    void regulacaoInvalida() { auto u = valida(); u.regulacao = "X"; QVERIFY(temErroEm(validar(u, ctx()), "regulacao")); }
    void influenciaInvalida() { auto u = valida(); u.influencia_vertimento = 2; QVERIFY(temErroEm(validar(u, ctx()), "influencia_vertimento")); }
};
QTEST_APPLESS_MAIN(TestValidacao)
#include "test_validacao.moc"
