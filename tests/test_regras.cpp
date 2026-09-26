#include <QtTest>
#include <QTemporaryDir>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "arquivo_hidr.h"
#include "deck_lookup.h"
#include "regras_gevazp.h"

namespace fs = std::filesystem;

class TestRegras : public QObject {
    Q_OBJECT

    static std::string arquivo(const std::vector<std::string>& linhas) {
        std::string s = " POST   MES  CONF    FORMULA\r\n XXXX    XX     X    XXXXXXXX\r\n";
        for (const std::string& l : linhas) s += l + "\r\n";
        return s + " 9999\r\n CONFIGURACAO DAS REGRAS UTILIZADAS\r\n 9999\r\n";
    }

    static std::string linha(int posto, int mes, const std::string& formula) {
        char prefixo[32];
        std::snprintf(prefixo, sizeof prefixo, "%5d%6d          ", posto, mes);
        return prefixo + formula;
    }

    static SerieVazoes serie(int num_postos, const std::vector<std::vector<int32_t>>& meses) {
        SerieVazoes s;
        s.ano_inicial = 1931;
        s.num_postos = num_postos;
        for (const auto& mes : meses) s.valores.insert(s.valores.end(), mes.begin(), mes.end());
        return s;
    }

    static int32_t calcular(const std::string& formula, std::vector<int32_t> mes) {
        ResultadoLeituraRegras r = interpretarRegras(arquivo({linha(1, 0, formula)}));
        if (!r.erro.empty()) {
            qWarning("%s", r.erro.c_str());
            return -99999;
        }
        SerieVazoes s = serie(static_cast<int>(mes.size()), {mes});
        if (!aplicarRegras(r.regras, s).ok) return -99998;
        return s.valor(1, 0);
    }

private slots:
    void leCabecalhoRegrasESentinela() {
        ResultadoLeituraRegras r = interpretarRegras(arquivo({linha(3, 0, "0"), linha(119, 2, "VAZ(301)*1.232+0.123="),
                                                              linha(37, 0, "vaz(237)-VAZ(117)")}));
        QVERIFY2(r.erro.empty(), r.erro.c_str());
        QCOMPARE(r.regras.size(), static_cast<size_t>(3));
        QCOMPARE(r.regras[1].posto, 119);
        QCOMPARE(r.regras[1].mes, 2);
        QCOMPARE(r.regras[1].formula, std::string("VAZ(301)*1.232+0.123="));
    }
    void aritmeticaRespeitaPrecedencia() {
        QCOMPARE(calcular("VAZ(2)+VAZ(3)*2", {0, 10, 5}), 20);
        QCOMPARE(calcular("(VAZ(2)+VAZ(3))*2", {0, 10, 5}), 30);
        QCOMPARE(calcular("VAZ(2)-VAZ(3)-1", {0, 10, 5}), 4);
        QCOMPARE(calcular("VAZ(2)/4", {0, 10, 5}), 3);
        QCOMPARE(calcular("-VAZ(2)+0.4", {0, 10, 5}), -10);
        QCOMPARE(calcular("VAZ(2)*1.217+0.608", {0, 100, 0}), 122);
    }
    void funcoesSeMinMax() {
        QCOMPARE(calcular("SE(VAZ(2)<=430;MAX(0;VAZ(2)-90);340)", {0, 50, 0}), 0);
        QCOMPARE(calcular("SE(VAZ(2)<=430;MAX(0;VAZ(2)-90);340)", {0, 200, 0}), 110);
        QCOMPARE(calcular("SE(VAZ(2)<=430;MAX(0;VAZ(2)-90);340)", {0, 500, 0}), 340);
        QCOMPARE(calcular("se(VAZ(2)<17;1;2)", {0, 16, 0}), 1);
        QCOMPARE(calcular("MIN(VAZ(2);144;VAZ(3))", {0, 200, 150}), 144);
        QCOMPARE(calcular("SE(VAZ(2)=5;1;SE(VAZ(2)<>5;2;3))", {0, 5, 0}), 1);
        QCOMPARE(calcular("SE(VAZ(2)>=6;1;0)+SE(VAZ(2)>5;10;0)", {0, 6, 0}), 11);
    }
    void regraDoMesTemPrioridadeSobreAGeral() {
        ResultadoLeituraRegras r = interpretarRegras(arquivo({linha(1, 0, "VAZ(2)"), linha(1, 2, "VAZ(2)*10")}));
        QVERIFY(r.erro.empty());
        SerieVazoes s = serie(2, {{0, 1}, {0, 2}, {0, 3}});
        QVERIFY(aplicarRegras(r.regras, s).ok);
        QCOMPARE(s.valor(1, 0), 1);
        QCOMPARE(s.valor(1, 1), 20);
        QCOMPARE(s.valor(1, 2), 3);
        QCOMPARE(s.valor(2, 1), 2);
    }
    void regraUsaPostoArtificialDefinidoDepois() {
        ResultadoLeituraRegras r = interpretarRegras(arquivo({linha(1, 0, "VAZ(2)+1"), linha(2, 0, "VAZ(3)*2")}));
        SerieVazoes s = serie(3, {{0, 999, 5}});
        QVERIFY(aplicarRegras(r.regras, s).ok);
        QCOMPARE(s.valor(2, 0), 10);
        QCOMPARE(s.valor(1, 0), 11);
        QCOMPARE(s.valor(3, 0), 5);
    }
    void dependenciaCircularEhErroEMantemASerie() {
        ResultadoLeituraRegras r = interpretarRegras(arquivo({linha(1, 0, "VAZ(2)"), linha(2, 0, "VAZ(1)")}));
        SerieVazoes s = serie(2, {{7, 8}});
        QVERIFY(!aplicarRegras(r.regras, s).ok);
        QCOMPARE(s.valor(1, 0), 7);
    }
    void erros() {
        QVERIFY(!interpretarRegras(arquivo({linha(1, 0, "VAZ(2)+")})).erro.empty());
        QVERIFY(!interpretarRegras(arquivo({linha(1, 0, "RAIZ(4)")})).erro.empty());
        QVERIFY(!interpretarRegras(arquivo({linha(1, 0, "SE(1;2)")})).erro.empty());
        QVERIFY(!interpretarRegras(arquivo({linha(1, 13, "0")})).erro.empty());
        QVERIFY(!interpretarRegras(" POST\r\n XXXX\r\n    1     0          0\r\n").erro.empty());
        ResultadoLeituraRegras r = interpretarRegras(arquivo({linha(5, 0, "0")}));
        SerieVazoes s = serie(2, {{1, 2}});
        QVERIFY(!aplicarRegras(r.regras, s).ok);
        r = interpretarRegras(arquivo({linha(1, 0, "VAZ(9)")}));
        QVERIFY(!aplicarRegras(r.regras, s).ok);
        r = interpretarRegras(arquivo({linha(1, 0, "0"), linha(1, 0, "1")}));
        QVERIFY(!aplicarRegras(r.regras, s).ok);
    }
    void lerDoArquivo() {
        QTemporaryDir dir;
        fs::path p = fs::path(dir.path().toStdWString()) / "REGRAS.DAT";
        std::ofstream(p, std::ios::binary) << arquivo({linha(21, 0, "VAZ(123)")});
        ResultadoLeituraRegras r = lerRegras(p);
        QVERIFY(r.erro.empty());
        QCOMPARE(r.regras.size(), static_cast<size_t>(1));
        QVERIFY(!lerRegras(fs::path(dir.path().toStdWString()) / "nao_existe.dat").erro.empty());
    }
    void exemploRealReproduzOPosto303DoDeck() {
        fs::path regras = fs::path(DIR_DOCS) / "REGRAS.DAT";
        fs::path deck = fs::path(DIR_DECK);
        if (!fs::exists(regras) || !fs::exists(deck / "vazoes.dat")) QSKIP("docs/REGRAS.DAT ou deck ausente");
        ResultadoLeituraRegras r = lerRegras(regras);
        QVERIFY2(r.erro.empty(), r.erro.c_str());
        QCOMPARE(r.regras.size(), static_cast<size_t>(55));
        DeckLookup lookup;
        lookup.carregarDeck(deck);
        ArquivoHidr a;
        QVERIFY(a.carregar(deck / "hidr.dat").ok);
        ResultadoLeituraVazoes leitura = lerVazoesDat(deck / "vazoes.dat", static_cast<int>(a.usinas.size()), lookup.anoInicialHistorico());
        QVERIFY(leitura.erro.empty());
        SerieVazoes original = leitura.serie;
        QVERIFY(aplicarRegras(r.regras, leitura.serie).ok);
        for (int t = 0; t < original.meses(); ++t) {
            QVERIFY(std::abs(leitura.serie.valor(303, t) - original.valor(303, t)) <= 1);
            QCOMPARE(leitura.serie.valor(6, t), original.valor(6, t));
        }
        QCOMPARE(leitura.serie.valor(21, 0), original.valor(123, 0));
    }
};

QTEST_GUILESS_MAIN(TestRegras)
#include "test_regras.moc"
