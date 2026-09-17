#include <QtTest>
#include <algorithm>
#include "cascata.h"

class TestCascata : public QObject {
    Q_OBJECT
    static const NoCascata* noDe(const Cascata& c, int codigo) {
        for (const NoCascata& n : c.nos)
            if (n.codigo == codigo) return &n;
        return nullptr;
    }
    static bool temAresta(const Cascata& c, int origem, int destino, bool desvio) {
        for (const ArestaCascata& a : c.arestas)
            if (a.origem == origem && a.destino == destino && a.desvio == desvio) return true;
        return false;
    }
private slots:
    void cadeiaLinearUmaBacia() {
        std::vector<UsinaHidr> u(3);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[1].nome = "U2"; u[1].jusante = 3;
        u[2].nome = "U3"; u[2].jusante = 0;
        Cascata c = montarCascata(u);
        QCOMPARE(c.nos.size(), size_t(3));
        const NoCascata* n1 = noDe(c, 1);
        const NoCascata* n2 = noDe(c, 2);
        const NoCascata* n3 = noDe(c, 3);
        QVERIFY(n1 && n2 && n3);
        QCOMPARE(n1->coluna, n2->coluna);
        QCOMPARE(n2->coluna, n3->coluna);
        QCOMPARE(n1->linha, 0);
        QCOMPARE(n2->linha, 1);
        QCOMPARE(n3->linha, 2);
        QCOMPARE(n1->bacia, n2->bacia);
        QCOMPARE(n2->bacia, n3->bacia);
        QCOMPARE(c.arestas.size(), size_t(2));
        QVERIFY(temAresta(c, 1, 2, false));
        QVERIFY(temAresta(c, 2, 3, false));
    }
    void bifurcacaoConflui() {
        std::vector<UsinaHidr> u(3);
        u[0].nome = "U1"; u[0].jusante = 3;
        u[1].nome = "U2"; u[1].jusante = 3;
        u[2].nome = "U3"; u[2].jusante = 0;
        Cascata c = montarCascata(u);
        const NoCascata* n1 = noDe(c, 1);
        const NoCascata* n2 = noDe(c, 2);
        const NoCascata* n3 = noDe(c, 3);
        QVERIFY(n1 && n2 && n3);
        QCOMPARE(n1->coluna, 0.0);
        QCOMPARE(n2->coluna, 1.0);
        QCOMPARE(n3->coluna, 0.5);
        QCOMPARE(n1->linha, 0);
        QCOMPARE(n2->linha, 0);
        QCOMPARE(n3->linha, 1);
    }
    void duasBaciasSeparadasPorColuna() {
        std::vector<UsinaHidr> u(4);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[1].nome = "U2"; u[1].jusante = 0;
        u[2].nome = "U3"; u[2].jusante = 4;
        u[3].nome = "U4"; u[3].jusante = 0;
        Cascata c = montarCascata(u);
        const NoCascata* n1 = noDe(c, 1);
        const NoCascata* n2 = noDe(c, 2);
        const NoCascata* n3 = noDe(c, 3);
        const NoCascata* n4 = noDe(c, 4);
        QVERIFY(n1 && n2 && n3 && n4);
        QCOMPARE(n1->coluna, 0.0);
        QCOMPARE(n2->coluna, 0.0);
        QCOMPARE(n3->coluna, 2.0);
        QCOMPARE(n4->coluna, 2.0);
        QVERIFY(n1->bacia != n3->bacia);
        QCOMPARE(c.num_colunas, 3);
    }
    void jusanteInvalidoViraRaiz() {
        std::vector<UsinaHidr> u(3);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[2].nome = "U3"; u[2].jusante = 99;
        Cascata c = montarCascata(u);
        QCOMPARE(c.nos.size(), size_t(2));
        const NoCascata* n1 = noDe(c, 1);
        const NoCascata* n3 = noDe(c, 3);
        QVERIFY(n1 && n3);
        QVERIFY(n1->bacia != n3->bacia);
        QCOMPARE(c.arestas.size(), size_t(0));
    }
    void cicloTerminaEDescartaUmaAresta() {
        std::vector<UsinaHidr> u(2);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[1].nome = "U2"; u[1].jusante = 1;
        Cascata c = montarCascata(u);
        QCOMPARE(c.nos.size(), size_t(2));
        QCOMPARE(c.arestas.size(), size_t(1));
        QVERIFY(temAresta(c, 2, 1, false));
        QVERIFY(!temAresta(c, 1, 2, false));
        const NoCascata* n1 = noDe(c, 1);
        const NoCascata* n2 = noDe(c, 2);
        QVERIFY(n1 && n2);
        QCOMPARE(n1->bacia, n2->bacia);
        QVERIFY(n1->coluna != n2->coluna);
    }
    void cicloDeTresNosNaoSobrepoeColunas() {
        std::vector<UsinaHidr> u(3);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[1].nome = "U2"; u[1].jusante = 3;
        u[2].nome = "U3"; u[2].jusante = 1;
        Cascata c = montarCascata(u);
        QCOMPARE(c.nos.size(), size_t(3));
        QCOMPARE(c.arestas.size(), size_t(2));
        QVERIFY(temAresta(c, 2, 3, false));
        QVERIFY(temAresta(c, 3, 1, false));
        QVERIFY(!temAresta(c, 1, 2, false));
        const NoCascata* n1 = noDe(c, 1);
        const NoCascata* n2 = noDe(c, 2);
        const NoCascata* n3 = noDe(c, 3);
        QVERIFY(n1 && n2 && n3);
        QCOMPARE(n1->bacia, n2->bacia);
        QCOMPARE(n2->bacia, n3->bacia);
        QVERIFY(n1->coluna != n2->coluna);
        QVERIFY(n2->coluna != n3->coluna);
        QVERIFY(n1->coluna != n3->coluna);
        std::vector<double> colunas = {n1->coluna, n2->coluna, n3->coluna};
        std::sort(colunas.begin(), colunas.end());
        QCOMPARE(colunas[0], 0.0);
        QCOMPARE(colunas[1], 1.0);
        QCOMPARE(colunas[2], 2.0);
    }
    void desvioGeraArestaSemAlterarColunas() {
        std::vector<UsinaHidr> u(2);
        u[0].nome = "U1"; u[0].desvio = 2;
        u[1].nome = "U2";
        Cascata c = montarCascata(u);
        const NoCascata* n1 = noDe(c, 1);
        const NoCascata* n2 = noDe(c, 2);
        QVERIFY(n1 && n2);
        QVERIFY(n1->bacia != n2->bacia);
        QCOMPARE(n1->coluna, 0.0);
        QCOMPARE(n2->coluna, 2.0);
        QCOMPARE(c.arestas.size(), size_t(1));
        QVERIFY(temAresta(c, 1, 2, true));
    }
    void usinasVaziasNaoAparecem() {
        std::vector<UsinaHidr> u(3);
        u[1].nome = "U2";
        Cascata c = montarCascata(u);
        QCOMPARE(c.nos.size(), size_t(1));
        QCOMPARE(c.nos[0].codigo, 2);
    }
    void baciasPreenchidas() {
        std::vector<UsinaHidr> u(4);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[1].nome = "U2"; u[1].jusante = 0;
        u[2].nome = "U3"; u[2].jusante = 4;
        u[3].nome = "U4"; u[3].jusante = 0;
        Cascata c = montarCascata(u);
        QCOMPARE(c.bacias.size(), size_t(2));
        const BaciaCascata& b0 = c.bacias[0];
        const BaciaCascata& b1 = c.bacias[1];
        QCOMPARE(b0.codigo_foz, 2);
        QCOMPARE(b0.num_usinas, 2);
        QCOMPARE(b0.largura, 1);
        QCOMPARE(b0.altura, 2);
        QCOMPARE(b1.codigo_foz, 4);
        QCOMPARE(b1.num_usinas, 2);
        QCOMPARE(b1.largura, 1);
        QCOMPARE(b1.altura, 2);
    }
    void empacotarQuebraEmLinhas() {
        Cascata c;
        c.nos = {
            NoCascata{1, 0.0, 0, 0}, NoCascata{2, 1.0, 1, 0},
            NoCascata{3, 0.0, 0, 1}, NoCascata{4, 1.0, 1, 1},
            NoCascata{5, 0.0, 0, 2}, NoCascata{6, 1.0, 1, 2},
        };
        c.bacias = {
            BaciaCascata{0, 1, 2, 0, 2, 2},
            BaciaCascata{1, 3, 2, 0, 2, 2},
            BaciaCascata{2, 5, 2, 0, 2, 2},
        };
        Cascata r = empacotarBacias(c, 5);
        QCOMPARE(r.num_colunas, 5);
        const NoCascata* n1 = noDe(r, 1);
        const NoCascata* n2 = noDe(r, 2);
        const NoCascata* n3 = noDe(r, 3);
        const NoCascata* n4 = noDe(r, 4);
        const NoCascata* n5 = noDe(r, 5);
        const NoCascata* n6 = noDe(r, 6);
        QVERIFY(n1 && n2 && n3 && n4 && n5 && n6);
        QCOMPARE(n1->coluna, 0.0);
        QCOMPARE(n1->linha, 0);
        QCOMPARE(n2->coluna, 1.0);
        QCOMPARE(n2->linha, 1);
        QCOMPARE(n3->coluna, 3.0);
        QCOMPARE(n3->linha, 0);
        QCOMPARE(n4->coluna, 4.0);
        QCOMPARE(n4->linha, 1);
        QCOMPARE(n5->coluna, 0.0);
        QCOMPARE(n5->linha, 3);
        QCOMPARE(n6->coluna, 1.0);
        QCOMPARE(n6->linha, 4);
        QCOMPARE(r.bacias[0].coluna_inicial, 0);
        QCOMPARE(r.bacias[1].coluna_inicial, 3);
        QCOMPARE(r.bacias[2].coluna_inicial, 0);
    }
    void empacotarSemLimiteNaoMuda() {
        Cascata c;
        c.nos = {NoCascata{1, 0.0, 0, 0}, NoCascata{2, 1.0, 1, 0}};
        c.bacias = {BaciaCascata{0, 1, 2, 0, 2, 2}};
        c.num_colunas = 2;
        c.num_linhas = 2;
        Cascata r = empacotarBacias(c, 0);
        QCOMPARE(r.nos.size(), c.nos.size());
        QCOMPARE(r.nos[0].coluna, c.nos[0].coluna);
        QCOMPARE(r.nos[1].linha, c.nos[1].linha);
        QCOMPARE(r.bacias[0].coluna_inicial, c.bacias[0].coluna_inicial);
        QCOMPARE(r.num_colunas, c.num_colunas);
    }
    void filtrarBaciaExistenteMantemNosEArestasEDeslocaColunas() {
        std::vector<UsinaHidr> u(4);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[1].nome = "U2"; u[1].jusante = 0;
        u[2].nome = "U3"; u[2].jusante = 4;
        u[3].nome = "U4"; u[3].jusante = 0;
        Cascata c = montarCascata(u);
        Cascata filtrada = filtrarBacia(c, 4);
        QCOMPARE(filtrada.nos.size(), size_t(2));
        const NoCascata* n3 = noDe(filtrada, 3);
        const NoCascata* n4 = noDe(filtrada, 4);
        QVERIFY(n3 && n4);
        QCOMPARE(n3->coluna, 0.0);
        QCOMPARE(n4->coluna, 0.0);
        QCOMPARE(filtrada.arestas.size(), size_t(1));
        QVERIFY(temAresta(filtrada, 3, 4, false));
        QCOMPARE(filtrada.bacias.size(), size_t(1));
        QCOMPARE(filtrada.bacias[0].coluna_inicial, 0);
    }
    void filtrarBaciaInexistenteRetornaVazia() {
        std::vector<UsinaHidr> u(2);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[1].nome = "U2"; u[1].jusante = 0;
        Cascata c = montarCascata(u);
        Cascata filtrada = filtrarBacia(c, 99);
        QVERIFY(filtrada.nos.empty());
        QVERIFY(filtrada.arestas.empty());
    }
    void cascataDaUsinaIncluiMontanteEJusante() {
        std::vector<UsinaHidr> u(5);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[1].nome = "U2"; u[1].jusante = 3;
        u[2].nome = "U3"; u[2].jusante = 4;
        u[3].nome = "U4"; u[3].jusante = 0;
        u[4].nome = "U5"; u[4].jusante = 2;
        auto comparar = [](std::vector<int> a, std::vector<int> b) {
            std::sort(a.begin(), a.end());
            std::sort(b.begin(), b.end());
            return a == b;
        };
        QVERIFY(comparar(cascataDaUsina(u, 2), {1, 5, 2, 3, 4}));
        QVERIFY(comparar(cascataDaUsina(u, 4), {1, 5, 2, 3, 4}));
        QVERIFY(comparar(cascataDaUsina(u, 1), {1, 2, 3, 4}));
    }
    void cascataDaUsinaComCicloTermina() {
        std::vector<UsinaHidr> u(2);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[1].nome = "U2"; u[1].jusante = 1;
        std::vector<int> r = cascataDaUsina(u, 1);
        std::sort(r.begin(), r.end());
        QCOMPARE(r, (std::vector<int>{1, 2}));
        QVERIFY(cascataDaUsina(u, 0).empty());
        QVERIFY(cascataDaUsina(u, 99).empty());
    }
};
QTEST_APPLESS_MAIN(TestCascata)
#include "test_cascata.moc"
