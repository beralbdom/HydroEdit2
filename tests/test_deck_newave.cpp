#include <QtTest>
#include <filesystem>
#include <map>
#include <string>
#include "deck_newave.h"

namespace fs = std::filesystem;

class TestDeckNewave : public QObject {
    Q_OBJECT
private slots:
    void arquivosDatAssociaDescricaoAoNome() {
        auto a = interpretarArquivosDat("DADOS GERAIS                : dger.dat\r\n"
                                        "ARQUIVO C.GUIA / PENAL.VMINT: curva.dat\r\n"
                                        "linha sem separador\r\n"
                                        "SEM NOME                    :   \r\n");
        QCOMPARE(a.size(), static_cast<size_t>(2));
        QCOMPARE(a.at("DADOS GERAIS"), std::string("dger.dat"));
        QCOMPARE(a.at("ARQUIVO C.GUIA / PENAL.VMINT"), std::string("curva.dat"));
    }
    void modifSeparaBlocosERegistros() {
        ResultadoModif r = interpretarModif(" P.CHAVE  MODIFICACOES E INDICES\r\n"
                                            " XXXXXXXX XXXXXXXXXXXXXXXXXXXXX\r\n"
                                            " USINA    1                                 CAMARGOS    \r\n"
                                            " VAZMIN       34                                        \r\n"
                                            " VMAXT    12 2026  80.600 '%'                           \r\n"
                                            "\r\n"
                                            " usina    6\r\n"
                                            " nummaq   4 2\r\n");
        QVERIFY(r.erro.empty());
        QVERIFY(r.avisos.empty());
        QCOMPARE(r.blocos.size(), static_cast<size_t>(2));
        QCOMPARE(r.blocos[0].usina, 1);
        QCOMPARE(r.blocos[0].comentario, std::string("CAMARGOS"));
        QCOMPARE(r.blocos[0].registros.size(), static_cast<size_t>(2));
        QCOMPARE(r.blocos[0].registros[1].palavra_chave, std::string("VMAXT"));
        QCOMPARE(r.blocos[0].registros[1].valores, (std::vector<std::string>{"12", "2026", "80.600", "'%'"}));
        QCOMPARE(r.blocos[0].registros[1].linha, 5);
        QCOMPARE(r.blocos[1].usina, 6);
        QCOMPARE(r.blocos[1].registros[0].palavra_chave, std::string("NUMMAQ"));
    }
    void modifAvisaRegistroForaDeBlocoEUsinaRepetida() {
        ResultadoModif r = interpretarModif("a\nb\n VAZMIN   10\n USINA    1\n USINA    1\n");
        QVERIFY(r.erro.empty());
        QCOMPARE(r.avisos.size(), static_cast<size_t>(2));
        QCOMPARE(r.blocos.size(), static_cast<size_t>(2));
    }
    void modifUsinaSemCodigoEhErro() {
        QVERIFY(!interpretarModif("a\nb\n USINA    X\n").erro.empty());
        QVERIFY(!lerModif("nao_existe/modif.dat").erro.empty());
    }
    void deckReal() {
        fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "modif.dat")) QSKIP("deck ausente");
        auto arquivos = lerArquivosDat(deck / "arquivos.dat");
        QCOMPARE(arquivos.at("ALTERACAO DADOS USINAS HIDRO"), std::string("modif.dat"));
        QCOMPARE(arquivos.at("DADOS DOS RESER.EQ.ENERGIA"), std::string("ree.dat"));
        ResultadoModif r = lerModif(deck / "modif.dat");
        QVERIFY(r.erro.empty());
        QVERIFY(r.avisos.empty());
        QCOMPARE(r.blocos.size(), static_cast<size_t>(104));
        std::map<std::string, int> por_chave;
        for (const BlocoModif& b : r.blocos)
            for (const RegistroModif& reg : b.registros) ++por_chave[reg.palavra_chave];
        QCOMPARE(por_chave["VMAXT"], 874);
        QCOMPARE(por_chave["CMONT"], 156);
        QCOMPARE(por_chave["POTEFE"], 1);
    }
};

QTEST_GUILESS_MAIN(TestDeckNewave)
#include "test_deck_newave.moc"
