#include <QtTest>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include "deck_newave.h"
#include "modif_newave.h"

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
    void camposDasPalavrasChave() {
        QCOMPARE(camposModif("VMAXT", 3).size(), size_t(4));
        QCOMPARE(camposModif("vmaxt", 3).size(), size_t(4));
        QCOMPARE(camposModif("TURBMAXT", 3).size(), size_t(5));
        QVERIFY(camposModif("TURBMAXT", 3)[3].opcional);
        QVERIFY(!camposModif("TURBMAXT", 3)[2].opcional);
        QCOMPARE(camposModif("COTAREA", 3).size(), size_t(5));
        QCOMPARE(camposModif("NUMMAQ", 3).size(), size_t(2));
        QCOMPARE(camposModif("NUMMAQ", 3)[1].nome, std::string("Conjunto"));
        QVERIFY(camposModif("XPTO", 3).empty());
        QVERIFY(chaveComData("CFUGA"));
        QVERIFY(!chaveComData("VOLMIN"));
    }

    void validaEMontaLinhas() {
        const auto vmaxt = camposModif("VMAXT", 3);
        std::string token;
        QVERIFY(!formatarCampoModif(vmaxt[0], "13", false, token).ok);
        QVERIFY(formatarCampoModif(vmaxt[0], "9", false, token).ok);
        QCOMPARE(token, std::string(" 9"));
        QVERIFY(!formatarCampoModif(vmaxt[1], "26", false, token).ok);
        QVERIFY(formatarCampoModif(vmaxt[2], "80,5", false, token).ok);
        QCOMPARE(token, std::string("80.5"));
        QVERIFY(formatarCampoModif(vmaxt[3], "%", true, token).ok);
        QCOMPARE(token, std::string("'%'"));
        QVERIFY(!formatarCampoModif(vmaxt[3], "m", true, token).ok);
        QVERIFY(!formatarCampoModif(vmaxt[2], "", true, token).ok);

        std::string linha;
        QVERIFY(montarLinhaModif(" VMAXT    12 2026  80.600 '%'", {" 9", "2027", "70", "'%'"}, vmaxt, linha).ok);
        QCOMPARE(linha, std::string(" VMAXT     9 2027 70 '%'"));
        const auto turb = camposModif("TURBMAXT", 3);
        QVERIFY(montarLinhaModif(" TURBMAXT", {" 9", "2026", "100", "", ""}, turb, linha).ok);
        QCOMPARE(linha, std::string(" TURBMAXT  9 2026 100"));
        QVERIFY(!montarLinhaModif(" TURBMAXT", {" 9", "2026", "100", "", "300"}, turb, linha).ok);
        QVERIFY(!montarLinhaModif(" COTAREA", {std::string(30, '1'), std::string(31, '2')}, camposModif("COTAREA", 1), linha).ok);
    }

    void insereERemoveModificacoes() {
        const std::vector<std::string> linhas = {"comentario", "comentario", " USINA    9          JAGUARA", " VAZMIN    30", "",
                                                 " USINA    11         VOLTA GRANDE", " NUMCNJ     1"};
        int nova = -1;
        auto inseridas = inserirModificacao(linhas, 9, "JAGUARA", " CFUGA     9 2026 70", nova);
        QCOMPARE(nova, 4);
        QCOMPARE(inseridas[4], std::string(" CFUGA     9 2026 70"));
        QCOMPARE(inseridas[5], std::string(""));
        inseridas = inserirModificacao(linhas, 33, "SAO SIMAO", " VAZMIN    10", nova);
        QCOMPARE(nova, 8);
        QVERIFY(inseridas[7].rfind(" USINA    33", 0) == 0);
        QVERIFY(inseridas[7].find("SAO SIMAO") != std::string::npos);
        auto removidas = removerModificacoes(linhas, {6});
        QCOMPARE(removidas.size(), size_t(5));
        QCOMPARE(removidas.back(), std::string(""));
        removidas = removerModificacoes(linhas, {3});
        QCOMPARE(removidas[2], std::string(""));
        QCOMPARE(removidas[3], std::string(" USINA    11         VOLTA GRANDE"));
    }

    void linhasDoDeckSeRemontamIguais() {
        const fs::path arquivo = fs::path(DIR_DECK) / "modif.dat";
        if (!fs::exists(arquivo)) QSKIP("deck ausente");
        std::ifstream f(arquivo, std::ios::binary);
        std::stringstream conteudo;
        conteudo << f.rdbuf();
        const ResultadoModif modif = interpretarModif(conteudo.str());
        std::istringstream entrada(conteudo.str());
        std::vector<std::string> linhas;
        for (std::string l; std::getline(entrada, l);) {
            if (!l.empty() && l.back() == '\r') l.pop_back();
            linhas.push_back(l);
        }
        int conferidas = 0;
        for (const BlocoModif& bloco : modif.blocos)
            for (const RegistroModif& r : bloco.registros) {
                const auto campos = camposModif(r.palavra_chave, 3);
                QVERIFY2(!campos.empty(), r.palavra_chave.c_str());
                std::string linha;
                QVERIFY(montarLinhaModif(linhas[static_cast<size_t>(r.linha - 1)], r.valores, campos, linha).ok);
                const ResultadoModif relida = interpretarModif("a\nb\n USINA    1\n" + linha + "\n");
                QCOMPARE(relida.blocos.at(0).registros.at(0).valores, r.valores);
                ++conferidas;
            }
        QVERIFY(conferidas > 1000);
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
