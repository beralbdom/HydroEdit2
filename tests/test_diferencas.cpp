#include <QtTest>
#include <string>
#include <vector>
#include "diferencas.h"

class TestDiferencas : public QObject {
    Q_OBJECT

    static std::string aplicar(const std::vector<std::string>& a, const std::vector<std::string>& b, const std::vector<LinhaDiferenca>& d,
                               bool lado_b) {
        std::string texto;
        for (const LinhaDiferenca& l : d) {
            if (l.tipo == TipoDiferenca::Igual) texto += a[static_cast<size_t>(l.linha_a)] + "|";
            else if (lado_b && l.tipo == TipoDiferenca::Incluida) texto += b[static_cast<size_t>(l.linha_b)] + "|";
            else if (!lado_b && l.tipo == TipoDiferenca::Removida) texto += a[static_cast<size_t>(l.linha_a)] + "|";
        }
        return texto;
    }

    static int edicoes(const std::vector<LinhaDiferenca>& d) {
        int n = 0;
        for (const LinhaDiferenca& l : d) n += l.tipo != TipoDiferenca::Igual;
        return n;
    }

private slots:
    void reconstroiOsDoisLadosComOMinimoDeEdicoes() {
        const std::vector<std::string> a = {"a", "b", "c", "a", "b", "b", "a"};
        const std::vector<std::string> b = {"c", "b", "a", "b", "a", "c"};
        std::vector<LinhaDiferenca> d;
        QVERIFY(diferencasDeLinhas(a, b, 100, d));
        QCOMPARE(aplicar(a, b, d, false), std::string("a|b|c|a|b|b|a|"));
        QCOMPARE(aplicar(a, b, d, true), std::string("c|b|a|b|a|c|"));
        QCOMPARE(edicoes(d), 5);
    }

    void casosDeBorda() {
        std::vector<LinhaDiferenca> d;
        QVERIFY(diferencasDeLinhas({}, {}, 10, d));
        QVERIFY(d.empty());
        QVERIFY(diferencasDeLinhas({"x"}, {}, 10, d));
        QCOMPARE(d.size(), size_t(1));
        QVERIFY(d[0].tipo == TipoDiferenca::Removida);
        QVERIFY(diferencasDeLinhas({}, {"y", "z"}, 10, d));
        QCOMPARE(edicoes(d), 2);
        QVERIFY(diferencasDeLinhas({"1", "2", "3"}, {"1", "2", "3"}, 0, d));
        QCOMPARE(edicoes(d), 0);
        QVERIFY(!diferencasDeLinhas({"1", "2", "3"}, {"4", "5", "6"}, 3, d));
        QVERIFY(d.empty());
    }

    void separaLinhasComQualquerQuebra() {
        QCOMPARE(separarLinhas("a\r\nb\nc"), (std::vector<std::string>{"a", "b", "c"}));
        QCOMPARE(separarLinhas("a\n\nb\n"), (std::vector<std::string>{"a", "", "b"}));
        QVERIFY(separarLinhas("").empty());
    }
};

QTEST_APPLESS_MAIN(TestDiferencas)
#include "test_diferencas.moc"
