#include <QtTest>
#include <algorithm>
#include <filesystem>
#include <vector>
#include "arquivo_hidr.h"
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
    static const ArestaCascata* arestaDe(const Cascata& c, int origem, int destino, bool desvio) {
        for (const ArestaCascata& a : c.arestas)
            if (a.origem == origem && a.destino == destino && a.desvio == desvio) return &a;
        return nullptr;
    }
    using Rota = std::vector<std::pair<double, double>>;

    // Toda rota vai da origem ao destino em trechos horizontais ou verticais, e nenhuma usina fica
    // sobre um trecho, a nao ser a origem no primeiro ponto e o destino no ultimo. Trechos horizontais
    // de jusante so se sobrepoem quando chegam a mesma usina, e trechos internos de desvios diferentes
    // nunca correm um sobre o outro.
    static void conferirRotas(const Cascata& c) {
        for (const ArestaCascata& a : c.arestas) {
            const NoCascata* o = noDe(c, a.origem);
            const NoCascata* d = noDe(c, a.destino);
            QVERIFY(a.rota.size() >= 2);
            QCOMPARE(a.rota.front(), std::make_pair(o->coluna, static_cast<double>(o->linha)));
            QCOMPARE(a.rota.back(), std::make_pair(d->coluna, static_cast<double>(d->linha)));
            for (size_t i = 0; i + 1 < a.rota.size(); ++i) {
                const auto [x1, y1] = a.rota[i];
                const auto [x2, y2] = a.rota[i + 1];
                QVERIFY(x1 == x2 || y1 == y2);
                for (const NoCascata& no : c.nos) {
                    const double x = no.coluna;
                    const double y = no.linha;
                    const bool sobre = (x1 == x2 ? x == x1 : y == y1) && x >= std::min(x1, x2) && x <= std::max(x1, x2) &&
                                       y >= std::min(y1, y2) && y <= std::max(y1, y2);
                    if (!sobre) continue;
                    const bool ponta = (i == 0 && no.codigo == a.origem && x == x1 && y == y1) ||
                                       (i + 2 == a.rota.size() && no.codigo == a.destino && x == x2 && y == y2);
                    QVERIFY2(ponta, qPrintable(QStringLiteral("aresta %1->%2 passa sobre a usina %3")
                                                   .arg(a.origem).arg(a.destino).arg(no.codigo)));
                }
            }
        }
        for (const ArestaCascata& a : c.arestas)
            for (const ArestaCascata& b : c.arestas) {
                if (a.desvio || b.desvio || &a == &b || a.destino == b.destino || a.rota.size() < 4 || b.rota.size() < 4) continue;
                if (a.rota[1].second != b.rota[1].second) continue;
                const double a1 = std::min(a.rota[1].first, a.rota[2].first), a2 = std::max(a.rota[1].first, a.rota[2].first);
                const double b1 = std::min(b.rota[1].first, b.rota[2].first), b2 = std::max(b.rota[1].first, b.rota[2].first);
                QVERIFY(a2 < b1 || b2 < a1);
            }
        for (const ArestaCascata& a : c.arestas)
            for (const ArestaCascata& b : c.arestas) {
                if (!a.desvio || !b.desvio || &a == &b) continue;
                for (size_t i = 1; i + 2 < a.rota.size(); ++i)
                    for (size_t j = 1; j + 2 < b.rota.size(); ++j) {
                        const auto [ax1, ay1] = a.rota[i];
                        const auto [ax2, ay2] = a.rota[i + 1];
                        const auto [bx1, by1] = b.rota[j];
                        const auto [bx2, by2] = b.rota[j + 1];
                        if (ax1 == ax2 && bx1 == bx2 && ax1 == bx1)
                            QVERIFY(std::max(ay1, ay2) <= std::min(by1, by2) || std::max(by1, by2) <= std::min(ay1, ay2));
                        if (ay1 == ay2 && by1 == by2 && ay1 == by1)
                            QVERIFY(std::max(ax1, ax2) <= std::min(bx1, bx2) || std::max(bx1, bx2) <= std::min(ax1, ax2));
                    }
            }
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
        QCOMPARE(n3->coluna, 0.0);
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
        QVERIFY(n1->linha != n2->linha);
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
        std::vector<int> linhas = {n1->linha, n2->linha, n3->linha};
        std::sort(linhas.begin(), linhas.end());
        QCOMPARE(linhas, (std::vector<int>{0, 1, 2}));
    }

    void cursoPrincipalRetoEAfluentesSemSobreposicao() {
        std::vector<UsinaHidr> u(8);
        for (size_t i = 0; i < u.size(); ++i) u[i].nome = "U" + std::to_string(i + 1);
        u[0].jusante = 0;
        u[1].jusante = 1;
        u[2].jusante = 2;
        u[3].jusante = 3;
        u[4].jusante = 2;
        u[5].jusante = 5;
        u[6].jusante = 1;
        u[7].jusante = 4;
        Cascata c = montarCascata(u);
        QCOMPARE(noDe(c, 1)->coluna, noDe(c, 2)->coluna);
        QCOMPARE(noDe(c, 2)->coluna, noDe(c, 3)->coluna);
        QCOMPARE(noDe(c, 3)->coluna, noDe(c, 4)->coluna);
        for (const NoCascata& a : c.nos)
            for (const NoCascata& b : c.nos)
                if (a.codigo != b.codigo && a.linha == b.linha) QVERIFY(std::abs(a.coluna - b.coluna) >= 1.0);
        for (const ArestaCascata& aresta : c.arestas) QCOMPARE(noDe(c, aresta.origem)->linha, noDe(c, aresta.destino)->linha - 1);
    }

    void rotaDeJusanteDesceAteAFaixa() {
        std::vector<UsinaHidr> u(3);
        u[0].nome = "A";
        u[1].nome = "B";
        u[2].nome = "C";
        u[0].jusante = 3;
        u[1].jusante = 3;
        Cascata c = montarCascata(u);
        QCOMPARE(arestaDe(c, 1, 3, false)->rota, (Rota{{0, 0}, {0, 1}}));
        QCOMPARE(arestaDe(c, 2, 3, false)->rota, (Rota{{1, 0}, {1, 0.5}, {0, 0.5}, {0, 1}}));
    }

    void rotaDeDesvioNaMesmaColunaContornaPeloCorredor() {
        std::vector<UsinaHidr> u(2);
        u[0].nome = "A";
        u[1].nome = "B";
        u[0].jusante = 2;
        u[0].desvio = 2;
        Cascata c = montarCascata(u);
        QCOMPARE(arestaDe(c, 1, 2, false)->rota, (Rota{{0, 0}, {0, 1}}));
        QCOMPARE(arestaDe(c, 1, 2, true)->rota, (Rota{{0, 0}, {0.5, 0}, {0.5, 1}, {0, 1}}));
    }

    void rotaDeDesvioAtravessaPorLinhaLivre() {
        std::vector<UsinaHidr> u(5);
        for (size_t i = 0; i < u.size(); ++i) u[i].nome = "U" + std::to_string(i + 1);
        u[0].jusante = 3;
        u[1].jusante = 3;
        u[3].jusante = 3;
        u[4].desvio = 4;
        Cascata c = empacotarBacias(u, 0);
        conferirRotas(c);
        QCOMPARE(arestaDe(c, 5, 4, true)->rota, (Rota{{4, 0}, {3.5, 0}, {3.5, -1}, {0.5, -1}, {0.5, 0}, {0, 0}}));
    }

    void rotasDoDeckNaoPassamSobreUsinas() {
        const std::filesystem::path caminho = std::filesystem::path(DIR_DECK) / "hidr.dat";
        if (!std::filesystem::exists(caminho)) QSKIP("deck nao encontrado");
        ArquivoHidr arquivo;
        QVERIFY(arquivo.carregar(caminho).ok);
        conferirRotas(empacotarBacias(arquivo.usinas, 37));
        conferirRotas(empacotarBacias(arquivo.usinas, 0));
    }

    void empacotarBaciasPoeDesviosLadoALado() {
        std::vector<UsinaHidr> u(7);
        for (size_t i = 0; i < u.size(); ++i) u[i].nome = "U" + std::to_string(i + 1);
        u[0].jusante = 2;
        u[1].jusante = 3;
        u[2].jusante = 0;
        u[3].jusante = 4 + 1;
        u[4].jusante = 0;
        u[5].jusante = 0;
        u[6].jusante = 0;
        u[6].desvio = 1;
        Cascata r = empacotarBacias(u, 100);
        const NoCascata* foz_maior = noDe(r, 3);
        const NoCascata* isolada = noDe(r, 7);
        QVERIFY(foz_maior && isolada);
        QCOMPARE(isolada->coluna, foz_maior->coluna + 2.0);
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
    void empacotarBaciasOrdenaPorTamanho() {
        std::vector<UsinaHidr> u(5);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[1].nome = "U2"; u[1].jusante = 0;
        u[2].nome = "U3"; u[2].jusante = 5;
        u[3].nome = "U4"; u[3].jusante = 5;
        u[4].nome = "U5"; u[4].jusante = 0;
        Cascata r = empacotarBacias(u, 10);

        QCOMPARE(r.bacias.size(), size_t(2));
        QCOMPARE(r.bacias[0].codigo_foz, 2);
        QCOMPARE(r.bacias[0].coluna_inicial, 3);
        QCOMPARE(r.bacias[1].codigo_foz, 5);
        QCOMPARE(r.bacias[1].coluna_inicial, 0);
        const NoCascata* n1 = noDe(r, 1);
        const NoCascata* n3 = noDe(r, 3);
        const NoCascata* n4 = noDe(r, 4);
        QVERIFY(n1 && n3 && n4);
        QCOMPARE(n3->coluna, 0.0);
        QCOMPARE(n4->coluna, 1.0);
        QCOMPARE(n1->coluna, 3.0);
        QCOMPARE(n1->linha, 0);
        QCOMPARE(r.num_colunas, 4);
        QCOMPARE(r.num_linhas, 2);
    }
    void empacotarBaciasQuebraLinha() {
        std::vector<UsinaHidr> u(5);
        u[0].nome = "U1"; u[0].jusante = 2;
        u[1].nome = "U2"; u[1].jusante = 0;
        u[2].nome = "U3"; u[2].jusante = 5;
        u[3].nome = "U4"; u[3].jusante = 5;
        u[4].nome = "U5"; u[4].jusante = 0;
        Cascata r = empacotarBacias(u, 2);

        QCOMPARE(r.bacias.size(), size_t(2));
        QCOMPARE(r.bacias[0].codigo_foz, 2);
        QCOMPARE(r.bacias[0].coluna_inicial, 0);
        QCOMPARE(r.bacias[1].codigo_foz, 5);
        QCOMPARE(r.bacias[1].coluna_inicial, 0);
        const NoCascata* n1 = noDe(r, 1);
        const NoCascata* n2 = noDe(r, 2);
        const NoCascata* n3 = noDe(r, 3);
        QVERIFY(n1 && n2 && n3);
        QCOMPARE(n3->linha, 0);
        QCOMPARE(n1->linha, 3);
        QCOMPARE(n2->linha, 4);
        QCOMPARE(r.num_colunas, 2);
        QCOMPARE(r.num_linhas, 5);
    }
};
QTEST_APPLESS_MAIN(TestCascata)
#include "test_cascata.moc"
