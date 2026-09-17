#include <QtTest>
#include "registro.h"

class TestRegistro : public QObject {
    Q_OBJECT
    static UsinaHidr usinaCompleta() {
        UsinaHidr u;
        u.nome = "FURNAS";
        u.posto = 6;
        u.posto_bdh = "16070980";
        u.subsistema = 1;
        u.empresa = 339;
        u.jusante = 7;
        u.desvio = 0;
        u.volume_minimo = 5733.0f;
        u.volume_maximo = 22950.0f;
        u.volume_vertedouro = 6173.76f;
        u.volume_desvio = 5733.0f;
        u.cota_minima = 750.0f;
        u.cota_maxima = 768.0f;
        u.pol_cota_volume = {735.2458f, 0.0034966f, -1.97437e-07f, 6.91705e-12f, -9.77365e-17f};
        u.pol_area_cota = {178827.0f, -385.277f, 0.0221355f, 0.000232793f, 0.0f};
        u.evaporacao = {13, 2, 25, 40, 52, 51, 43, 42, 39, 18, 12, 27};
        u.num_conjuntos = 2;
        u.num_maquinas = {6, 2, 0, 0, 0};
        u.potencia_efetiva = {152.0f, 152.0f, 0, 0, 0};
        for (int c = 0; c < 5; ++c)
            for (int p = 0; p < 3; ++p)
                for (int k = 0; k < 5; ++k) u.pol_conjunto[c][p][k] = static_cast<float>(c * 100 + p * 10 + k);
        u.altura_efetiva = {90.0f, 89.3f, 0, 0, 0};
        u.vazao_efetiva = {188, 189, 0, 0, 0};
        u.produtibilidade = 0.0089956f;
        u.perdas = 0.803f;
        u.num_pol_jusante = 1;
        u.pol_jusante[0] = {671.633f, 0.00101738f, -1.79972e-07f, 2.51328e-11f, 0.0f};
        u.ref_pol_jusante = {1, 2, 3, 4, 5, 6};
        u.canal_fuga_medio = 672.204f;
        u.influencia_vertimento = 1;
        u.fator_carga_maximo = 100.0f;
        u.fator_carga_minimo = 0.0f;
        u.vazao_minima_historica = 102;
        u.num_unidades_base = 4;
        u.tipo_turbina = 1;
        u.representacao_conjunto = 2;
        u.teif = 2.985f;
        u.ip = 1.39f;
        u.tipo_perda = 2;
        u.data = "27-08-26";
        u.observacao = "teste de observacao";
        u.volume_referencia = 22950.0f;
        u.regulacao = "M";
        return u;
    }
private slots:
    void roundtripPreservaTodosOsCampos() {
        UsinaHidr u = usinaCompleta();
        QVERIFY(desserializar(serializar(u)) == u);
    }
    void textosSaoPreenchidosComEspaco() {
        UsinaHidr u = usinaCompleta();
        Registro r = serializar(u);
        QCOMPARE(std::string(reinterpret_cast<const char*>(r.data()), 12), std::string("FURNAS      "));
        QCOMPARE(r[791], static_cast<unsigned char>('M'));
    }
    void nomeMaiorQue12EhTruncado() {
        UsinaHidr u;
        u.nome = "NOME MUITO COMPRIDO";
        UsinaHidr volta = desserializar(serializar(u));
        QCOMPARE(volta.nome, std::string("NOME MUITO C"));
    }
    void usinaVaziaViraEspacosENaoZeros() {
        Registro r = serializar(UsinaHidr{});
        QCOMPARE(r[0], static_cast<unsigned char>(' '));
        QCOMPARE(r[791], static_cast<unsigned char>(' '));
        QVERIFY(desserializar(r).vazia());
    }
};
QTEST_APPLESS_MAIN(TestRegistro)
#include "test_registro.moc"
