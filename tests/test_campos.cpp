#include <QtTest>
#include <set>
#include "campos.h"
#include "registro.h"
#include "usina_hidr.h"

class TestCampos : public QObject {
    Q_OBJECT
private slots:
    void usinaNovaEhVazia() {
        UsinaHidr u;
        QVERIFY(u.vazia());
        u.nome = "FURNAS";
        QVERIFY(!u.vazia());
    }
    void layoutCobre792BytesSemBuracoNemSobreposicao() {
        int esperado = 0;
        for (const Campo& c : campos()) {
            QCOMPARE(c.offset, esperado);
            QVERIFY(c.tamanho() > 0);
            esperado += c.tamanho();
        }
        QCOMPARE(esperado, TAMANHO_REGISTRO);
    }
    void nomesUnicosEBuscaPorNome() {
        std::set<std::string_view> nomes;
        for (const Campo& c : campos()) QVERIFY(nomes.insert(c.nome).second);
        QVERIFY(campo("volume_minimo") != nullptr);
        QCOMPARE(campo("volume_minimo")->offset, 40);
        QCOMPARE(campo("regulacao")->offset, 791);
        QVERIFY(campo("nao_existe") == nullptr);
    }
    void obterEDefinirEscalarEVetor() {
        UsinaHidr u;
        const Campo* vmin = campo("volume_minimo");
        vmin->definir(u, 0, Valor{12.5f});
        QCOMPARE(u.volume_minimo, 12.5f);
        QCOMPARE(std::get<float>(vmin->obter(u, 0)), 12.5f);

        const Campo* pc = campo("pol_conjunto");
        QCOMPARE(pc->n, 75);
        pc->definir(u, 5 * 3 * 2 + 5 * 1 + 3, Valor{7.0f});
        QCOMPARE(u.pol_conjunto[2][1][3], 7.0f);
        QCOMPARE(pc->nome_elemento(5 * 3 * 2 + 5 * 1 + 3), std::string("conj3_ger_a3"));

        const Campo* pj = campo("pol_jusante");
        pj->definir(u, 5 * 4 + 2, Valor{1.0f});
        QCOMPARE(u.pol_jusante[4][2], 1.0f);
        QCOMPARE(pj->nome_elemento(5 * 4 + 2), std::string("pol_jus5_a2"));

        QCOMPARE(campo("evaporacao")->nome_elemento(0), std::string("evap_jan"));
        QCOMPARE(campo("evaporacao")->nome_elemento(11), std::string("evap_dez"));
        QCOMPARE(campo("num_maquinas")->nome_elemento(1), std::string("conj2_nmaq"));
        QCOMPARE(campo("ref_pol_jusante")->nome_elemento(0), std::string("pol_jus1_ref"));
        QCOMPARE(campo("pol_cota_volume")->nome_elemento(4), std::string("pol_cota_vol_a4"));
        QVERIFY(campo("nome")->nome_elemento == nullptr);
    }
};
QTEST_APPLESS_MAIN(TestCampos)
#include "test_campos.moc"
