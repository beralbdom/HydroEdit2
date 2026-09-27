#include <QtTest>
#include <QTemporaryDir>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "arquivo_fixo.h"
#include "layouts_newave.h"

namespace fs = std::filesystem;

class TestArquivoFixo : public QObject {
    Q_OBJECT

    static LayoutArquivoFixo layoutDeTeste() {
        return {"0.0",
                {{"Primeira", 1, "9999",
                  {{"Codigo", 2, 5, TipoColunaFixa::Inteiro, 0}, {"Nome", 7, 12, TipoColunaFixa::Texto, 0},
                   {"Valor", 14, 18, TipoColunaFixa::Real, 0}, {"Custo", 20, 25, TipoColunaFixa::Real, 2}},
                  7},
                 {"Segunda", 1, "", {{"Codigo", 2, 5, TipoColunaFixa::Inteiro, 0}}, 0}}};
    }

    static std::string conteudoDeTeste() {
        return " COD  NOME   VALOR CUSTO\r\n"
               "    1 ANGRA   640.  31.17  32.00\r\n"
               "   13 B        20.   1.50\r\n"
               "\r\n"
               " 9999\r\n"
               " COD\r\n"
               "    7 resto livre\r\n";
    }

    static std::string lerBytes(const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        std::stringstream s;
        s << f.rdbuf();
        return s.str();
    }

private slots:
    void separaSecoesERepeteAUltimaColuna() {
        ArquivoFixo a;
        QVERIFY(a.interpretar(conteudoDeTeste(), layoutDeTeste()).ok);
        QCOMPARE(a.secoes().size(), static_cast<size_t>(2));
        QCOMPARE(a.secoes()[0].linhas.size(), static_cast<size_t>(2));
        QCOMPARE(a.secoes()[1].linhas.size(), static_cast<size_t>(1));
        const auto& colunas = a.secoes()[0].definicao.colunas;
        QCOMPARE(colunas.size(), static_cast<size_t>(5));
        QCOMPARE(colunas[3].nome, std::string("Custo 1"));
        QCOMPARE(colunas[4].nome, std::string("Custo 2"));
        QCOMPARE(colunas[4].inicio, 27);
        QCOMPARE(a.valor(0, 0, 1), std::string("ANGRA"));
        QCOMPARE(a.valor(0, 0, 4), std::string("32.00"));
        QCOMPARE(a.valor(0, 1, 4), std::string(""));
        QCOMPARE(a.valor(1, 0, 0), std::string("7"));
        QCOMPARE(a.conteudo(), conteudoDeTeste());
        QVERIFY(!a.modificado());
    }
    void definirTrocaSoAsColunasDoCampo() {
        ArquivoFixo a;
        a.interpretar(conteudoDeTeste(), layoutDeTeste());
        QVERIFY(a.definir(0, 0, 2, "1350").ok);
        QVERIFY(a.definir(0, 0, 3, "5,5").ok);
        QVERIFY(a.definir(0, 1, 1, "XY").ok);
        QVERIFY(a.definir(0, 1, 4, "7").ok);
        QVERIFY(a.modificado());
        QCOMPARE(a.valor(0, 0, 2), std::string("1350."));
        QCOMPARE(a.valor(0, 0, 3), std::string("5.50"));
        std::string esperado = " COD  NOME   VALOR CUSTO\r\n"
                               "    1 ANGRA  1350.   5.50  32.00\r\n"
                               "   13 XY       20.   1.50   7.00\r\n"
                               "\r\n"
                               " 9999\r\n"
                               " COD\r\n"
                               "    7 resto livre\r\n";
        QCOMPARE(a.conteudo(), esperado);
        QVERIFY(a.definir(0, 1, 4, "").ok);
        QCOMPARE(a.valor(0, 1, 4), std::string(""));
    }
    void valoresInvalidosNaoMudamALinha() {
        ArquivoFixo a;
        a.interpretar(conteudoDeTeste(), layoutDeTeste());
        QVERIFY(!a.definir(0, 0, 0, "1.5").ok);
        QVERIFY(!a.definir(0, 0, 0, "abc").ok);
        QVERIFY(!a.definir(0, 0, 0, "123456").ok);
        QVERIFY(!a.definir(0, 0, 1, "NOME LONGO").ok);
        QVERIFY(!a.definir(0, 0, 2, "123456").ok);
        QCOMPARE(a.conteudo(), conteudoDeTeste());
        QVERIFY(!a.modificado());
    }
    void preservaQuebraLfESemQuebraFinal() {
        ArquivoFixo a;
        std::string lf = "cab\n    1 A\n    2 B";
        a.interpretar(lf, {"0.0", {{"Unica", 1, "", {{"Codigo", 2, 5, TipoColunaFixa::Inteiro, 0}}, 0}}});
        QCOMPARE(a.secoes()[0].linhas.size(), static_cast<size_t>(2));
        QCOMPARE(a.conteudo(), lf);
    }
    void textoLfESubstituicaoMantemAQuebraOriginal() {
        ArquivoFixo a;
        a.interpretar(conteudoDeTeste(), layoutDeTeste());
        std::string lf = a.textoLf();
        QVERIFY(lf.find('\r') == std::string::npos);
        a.substituirTexto(lf, layoutDeTeste());
        QVERIFY(!a.modificado());
        QCOMPARE(a.conteudo(), conteudoDeTeste());
        lf.replace(lf.find("ANGRA"), 5, "BRAVA");
        lf += "    9 nova\n";
        a.substituirTexto(lf, layoutDeTeste());
        QVERIFY(a.modificado());
        QCOMPARE(a.valor(0, 0, 1), std::string("BRAVA"));
        QCOMPARE(a.secoes()[1].linhas.size(), static_cast<size_t>(2));
        QVERIFY(a.conteudo().find("BRAVA") != std::string::npos);
        QVERIFY(a.conteudo().find("    9 nova\r\n") != std::string::npos);
    }
    void layoutVazioGuardaOTextoIgual() {
        ArquivoFixo a;
        const std::string texto = "linha 1\r\nlinha 2\r\n";
        a.interpretar(texto, {"3.3", {}});
        QVERIFY(a.secoes().empty());
        QCOMPARE(a.conteudo(), texto);
    }
    void filtroContextoOrdinalEMesmaRegiao() {
        using T = TipoColunaFixa;
        const FiltroLinha mae{1, 4, TesteFiltro::Preenchido, {}};
        LayoutArquivoFixo layout{"0.0",
                                 {{.titulo = "Valores",
                                   .linhas_cabecalho = 1,
                                   .terminador = "999",
                                   .colunas = {{"Codigo", 2, 4, T::Inteiro, 0, 0},
                                               {"Ano", 7, 10, T::Inteiro, 0, 1},
                                               {"Patamar", 0, 0, T::Ordinal, 0, 1},
                                               {"Valor", 12, 16, T::Real, 0}},
                                   .passo_repeticao = 6,
                                   .filtro = {{1, 4, TesteFiltro::Vazio, {}}},
                                   .contextos = {{{mae}}, {{{7, 10, TesteFiltro::Preenchido, {}}}}}},
                                  {.titulo = "Nomes", .colunas = {{"Nome", 6, 12, T::Texto, 0}}, .filtro = {mae}, .mesma_regiao = true},
                                  {.titulo = "A", .colunas = {{"Valor", 14, 14, T::Inteiro, 0}}, .filtro = {{1, 7, TesteFiltro::Igual, {"PARAM A"}}}},
                                  {.titulo = "B",
                                   .colunas = {{"Valor", 14, 14, T::Inteiro, 0}},
                                   .filtro = {{1, 7, TesteFiltro::Igual, {"PARAM B"}}},
                                   .mesma_regiao = true}}};
        const std::string conteudo = " CAB\n"
                                     "   1 SUDESTE\n"
                                     "      2026   10.   20.\n"
                                     "             11.   21.\n"
                                     "   2 SUL\n"
                                     "      2026   30.   40.\n"
                                     " 999\n"
                                     "PARAM A      5\n"
                                     "PARAM B      7\n";
        ArquivoFixo a;
        QVERIFY(a.interpretar(conteudo, layout).ok);
        QCOMPARE(a.secoes()[0].linhas.size(), static_cast<size_t>(3));
        QCOMPARE(a.secoes()[0].definicao.colunas.size(), static_cast<size_t>(5));
        QCOMPARE(a.valor(0, 1, 0), std::string("1"));
        QCOMPARE(a.valor(0, 2, 0), std::string("2"));
        QCOMPARE(a.valor(0, 1, 1), std::string("2026"));
        QCOMPARE(a.valor(0, 0, 2), std::string("1"));
        QCOMPARE(a.valor(0, 1, 2), std::string("2"));
        QCOMPARE(a.valor(0, 2, 2), std::string("1"));
        QCOMPARE(a.valor(0, 1, 4), std::string("21."));
        QVERIFY(!a.definir(0, 0, 2, "5").ok);
        QVERIFY(a.definir(0, 0, 0, "5").ok);
        QCOMPARE(a.valor(0, 1, 0), std::string("5"));
        QCOMPARE(a.valor(1, 0, 0), std::string("SUDESTE"));
        QVERIFY(a.definir(0, 1, 3, "12").ok);
        QCOMPARE(a.valor(0, 1, 3), std::string("12."));
        QCOMPARE(a.secoes()[1].linhas.size(), static_cast<size_t>(2));
        QCOMPARE(a.valor(1, 1, 0), std::string("SUL"));
        QCOMPARE(a.secoes()[2].linhas.size(), static_cast<size_t>(1));
        QCOMPARE(a.valor(2, 0, 0), std::string("5"));
        QCOMPARE(a.valor(3, 0, 0), std::string("7"));
    }
    void regiaoContiguaEGruposSeparadosPorLinhaEmBranco() {
        using T = TipoColunaFixa;
        const FiltroLinha dados{1, 4, TesteFiltro::Vazio, {}};
        LayoutArquivoFixo layout{"0.0",
                                 {{.titulo = "Valores",
                                   .colunas = {{"Par", 2, 4, T::Inteiro, 0, 0}, {"Sentido", 0, 0, T::Grupo, 0, 0}, {"Valor", 7, 9, T::Inteiro, 0}},
                                   .filtro = {dados},
                                   .contextos = {{{{1, 4, TesteFiltro::Preenchido, {}}}}},
                                   .contigua = false},
                                  {.titulo = "Anos",
                                   .linhas_cabecalho = 1,
                                   .colunas = {{"Ano", 1, 4, T::Inteiro, 0}},
                                   .filtro = {{6, 8, TesteFiltro::Vazio, {}}},
                                   .contigua = true},
                                  {.titulo = "Resto", .colunas = {{"Ano", 1, 4, T::Inteiro, 0}}}}};
        layout.secoes[0].terminador = "999";
        ArquivoFixo a;
        QVERIFY(a.interpretar("   1\n      10\n      11\n\n      20\n   2\n      30\n 999\n"
                              " CAB\n2026\n2027\nLINHA DE COMENTARIO\n2030\n",
                              layout)
                    .ok);
        QCOMPARE(a.secoes()[0].linhas.size(), static_cast<size_t>(4));
        QCOMPARE(a.valor(0, 1, 1), std::string("1"));
        QCOMPARE(a.valor(0, 2, 1), std::string("2"));
        QCOMPARE(a.valor(0, 2, 0), std::string("1"));
        QCOMPARE(a.valor(0, 3, 1), std::string("1"));
        QVERIFY(!a.definir(0, 0, 1, "2").ok);
        QCOMPARE(a.secoes()[1].linhas.size(), static_cast<size_t>(2));
        QCOMPARE(a.secoes()[2].linhas.size(), static_cast<size_t>(2));
        QCOMPARE(a.valor(2, 1, 0), std::string("2030"));
    }
    void maxRegistrosEncerraASecao() {
        const ColunaFixa codigo{"Codigo", 2, 2, TipoColunaFixa::Inteiro, 0};
        LayoutArquivoFixo layout{"0.0",
                                 {{.titulo = "Um", .linhas_cabecalho = 1, .colunas = {codigo}, .max_registros = 1},
                                  {.titulo = "Resto", .colunas = {codigo}}}};
        ArquivoFixo a;
        QVERIFY(a.interpretar(" CAB\n 1\n\n 2\n 3\n", layout).ok);
        QCOMPARE(a.secoes()[0].linhas.size(), static_cast<size_t>(1));
        QCOMPARE(a.secoes()[1].linhas.size(), static_cast<size_t>(2));
        QCOMPARE(a.valor(1, 0, 0), std::string("2"));
    }
    void realMantemAsCasasDigitadasQueCabem() {
        ArquivoFixo a;
        a.interpretar(conteudoDeTeste(), layoutDeTeste());
        QVERIFY(a.definir(0, 0, 2, "1.5").ok);
        QCOMPARE(a.valor(0, 0, 2), std::string("1.5"));
        QVERIFY(a.definir(0, 0, 3, "5.125").ok);
        QCOMPARE(a.valor(0, 0, 3), std::string("5.125"));
        QVERIFY(a.definir(0, 0, 3, "5.12345").ok);
        QCOMPARE(a.valor(0, 0, 3), std::string("5.1235"));
        QVERIFY(!a.definir(0, 0, 3, "12345.678").ok);
    }
    void camposSeparadosPorPontoEVirgula() {
        using T = TipoColunaFixa;
        LayoutArquivoFixo layout{"0.0",
                                 {{.titulo = "Formulas",
                                   .colunas = {{"Codigo", 2, 2, T::Inteiro, 0}, {"Formula", 3, 3, T::Texto, 0}},
                                   .filtro = {{1, 1, TesteFiltro::Igual, {"RE"}}}},
                                  {.titulo = "Horizonte",
                                   .colunas = {{"Codigo", 2, 2, T::Inteiro, 0}, {"Fim", 4, 4, T::Texto, 0}, {"Extra", 5, 5, T::Real, 0}},
                                   .filtro = {{1, 1, TesteFiltro::Igual, {"RE-HORIZ-PER"}}},
                                   .mesma_regiao = true}},
                                 false,
                                 ';'};
        ArquivoFixo a;
        QVERIFY(a.interpretar("&RE; cod; formula\n"
                              "RE ;        1;   ger_usih(285)\n"
                              " & comentario\n"
                              "RE-HORIZ-PER ;        1;2026/09;2026/10\n",
                              layout)
                    .ok);
        QCOMPARE(a.secoes()[0].linhas.size(), static_cast<size_t>(1));
        QCOMPARE(a.secoes()[1].linhas.size(), static_cast<size_t>(1));
        QCOMPARE(a.valor(0, 0, 1), std::string("ger_usih(285)"));
        QCOMPARE(a.valor(1, 0, 1), std::string("2026/10"));
        QCOMPARE(a.valor(1, 0, 2), std::string(""));
        QVERIFY(a.definir(0, 0, 0, "20").ok);
        QVERIFY(a.definir(1, 0, 1, "2027/01").ok);
        QVERIFY(a.definir(1, 0, 2, "1,5").ok);
        QVERIFY(!a.definir(1, 0, 2, "x").ok);
        QCOMPARE(a.conteudo(), std::string("&RE; cod; formula\n"
                                           "RE ;       20;   ger_usih(285)\n"
                                           " & comentario\n"
                                           "RE-HORIZ-PER ;        1;2026/09;2027/01;1.5\n"));
    }
    void salvaEReabre() {
        QTemporaryDir dir;
        fs::path p = fs::path(dir.path().toStdWString()) / "teste.dat";
        ArquivoFixo a;
        a.interpretar(conteudoDeTeste(), layoutDeTeste());
        a.definir(0, 0, 0, "2");
        QVERIFY(a.salvar(p).ok);
        QVERIFY(!a.modificado());
        ArquivoFixo b;
        QVERIFY(b.carregar(p, layoutDeTeste()).ok);
        QCOMPARE(b.valor(0, 0, 0), std::string("2"));
        QVERIFY(!fs::exists(fs::path(p).concat(".tmp")));
    }
    void todosOsLayoutsLeemODeckReal() {
        fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "dger.dat")) QSKIP("deck ausente");
        const auto numeroValido = [](const std::string& v, TipoColunaFixa tipo) {
            if (v.empty() || (tipo != TipoColunaFixa::Inteiro && tipo != TipoColunaFixa::Real)) return true;
            size_t lidos = 0;
            try {
                if (tipo == TipoColunaFixa::Inteiro) static_cast<void>(std::stoll(v, &lidos));
                else static_cast<void>(std::stod(v, &lidos));
            } catch (...) {
                return false;
            }
            return lidos == v.size();
        };
        struct Caso {
            const char* nome;
            int invalidos;
            size_t registros;
        };
        const Caso casos[] = {{"arquivos.dat", 0, 45},  {"dger.dat", 0, 101},   {"shist.dat", 0, 1},       {"sistema.dat", 0, 292},
                              {"patamar.dat", 0, 784},  {"confhd.dat", 0, 171}, {"exph.dat", 0, 20},       {"loss.dat", 0, 0},
                              {"dsvagua.dat", 0, 1065}, {"vazpast.dat", 0, 223}, {"gtminpat.dat", 0, 0},   {"penalid.dat", 0, 72},
                              {"curva.dat", 0, 54},     {"agrint.dat", 0, 39}, {"c_adic.dat", 0, 42},     {"adterm.dat", 0, 6},
                              {"ghmin.dat", 0, 91},     {"cvar.dat", 0, 13},    {"ree.dat", 0, 13},        {"re.dat", 0, 0},
                              {"selcor.dat", 0, 7},     {"tecno.dat", 0, 0},    {"polinjus.csv", 0, 1826}, {"volref_saz.dat", 0, 149},
                              {"restricao-eletrica.csv", 0, 60}};
        NumeroPatamares patamares;
        ArquivoFixo patamar;
        ArquivoFixo sistema;
        QVERIFY(patamar.carregar(deck / "patamar.dat", *layoutNewave("patamar.dat")).ok);
        QVERIFY(sistema.carregar(deck / "sistema.dat", *layoutNewave("sistema.dat")).ok);
        patamares.carga = patamaresDeCarga(patamar);
        patamares.deficit = patamaresDeDeficit(sistema);
        QCOMPARE(patamares.carga, 3);
        QCOMPARE(patamares.deficit, 1);
        for (const Caso& caso : casos) {
            QVERIFY2(layoutNewave(caso.nome), caso.nome);
            const LayoutArquivoFixo ajustado = ajustarPatamares(*layoutNewave(caso.nome), patamares);
            const LayoutArquivoFixo* layout = &ajustado;
            QVERIFY2(layout, caso.nome);
            fs::path caminho;
            for (const auto& entrada : fs::directory_iterator(deck)) {
                std::string nome = entrada.path().filename().string();
                std::transform(nome.begin(), nome.end(), nome.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (nome == caso.nome) caminho = entrada.path();
            }
            QVERIFY2(!caminho.empty(), caso.nome);
            ArquivoFixo a;
            QVERIFY(a.carregar(caminho, *layout).ok);
            QVERIFY2(a.conteudo() == lerBytes(caminho), caso.nome);
            int invalidos = 0;
            size_t registros = 0;
            for (size_t s = 0; s < a.secoes().size(); ++s) {
                const SecaoLida& secao = a.secoes()[s];
                registros += secao.linhas.size();
                for (size_t r = 0; r < secao.linhas.size(); ++r)
                    for (size_t c = 0; c < secao.definicao.colunas.size(); ++c)
                        if (!numeroValido(a.valor(static_cast<int>(s), static_cast<int>(r), static_cast<int>(c)), secao.definicao.colunas[c].tipo))
                            ++invalidos;
            }
            QVERIFY2(invalidos == caso.invalidos, caso.nome);
            QVERIFY2(registros == caso.registros, (std::string(caso.nome) + ": " + std::to_string(registros)).c_str());
        }
    }
    void colunasSeguemONumeroDePatamares() {
        fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "agrint.dat")) QSKIP("deck ausente");
        auto colunas = [&](const char* nome, NumeroPatamares n, int secao) {
            ArquivoFixo a;
            a.carregar(deck / nome, ajustarPatamares(*layoutNewave(nome), n));
            return static_cast<int>(a.secoes()[static_cast<size_t>(secao)].definicao.colunas.size());
        };
        QCOMPARE(colunas("agrint.dat", {}, 1), 5 + 5);
        QCOMPARE(colunas("agrint.dat", {3, 1}, 1), 5 + 3);
        QCOMPARE(colunas("adterm.dat", {3, 1}, 0), 3 + 3);
        QCOMPARE(colunas("sistema.dat", {3, 1}, 1), 3 + 2);
        QCOMPARE(colunas("sistema.dat", {3, 0}, 1), 3 + 8);
        QCOMPARE(colunas("patamar.dat", {3, 1}, 2), 1 + 3);
        QVERIFY(dependeDePatamares(*layoutNewave("agrint.dat")));
        QVERIFY(!dependeDePatamares(*layoutNewave("confhd.dat")));
    }
    void arquivosTermicosDoDeckReal() {
        fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "term.dat")) QSKIP("deck ausente");
        struct Caso {
            const char* nome;
            std::vector<size_t> registros;
        };
        for (const Caso& caso : {Caso{"conft.dat", {158}}, Caso{"term.dat", {158}}, Caso{"expt.dat", {955}},
                                 Caso{"manutt.dat", {47}}, Caso{"clast.dat", {158, 105}}}) {
            const LayoutArquivoFixo* layout = layoutNewave(caso.nome);
            QVERIFY2(layout, caso.nome);
            ArquivoFixo a;
            QVERIFY(a.carregar(deck / caso.nome, *layout).ok);
            QCOMPARE(a.secoes().size(), caso.registros.size());
            for (size_t s = 0; s < caso.registros.size(); ++s) QCOMPARE(a.secoes()[s].linhas.size(), caso.registros[s]);
            QVERIFY2(a.conteudo() == lerBytes(deck / caso.nome), caso.nome);
        }
        ArquivoFixo term;
        term.carregar(deck / "term.dat", *layoutNewave("term.dat"));
        QCOMPARE(term.valor(0, 0, 1), std::string("ANGRA 1"));
        QCOMPARE(term.valor(0, 0, 2), std::string("640."));
        QCOMPARE(term.secoes()[0].definicao.colunas.size(), static_cast<size_t>(19));
        ArquivoFixo clast;
        clast.carregar(deck / "clast.dat", *layoutNewave("clast.dat"));
        QCOMPARE(clast.secoes()[0].definicao.colunas.size(), static_cast<size_t>(3 + 5));
        QCOMPARE(clast.valor(1, 0, 0), std::string("211"));
        QCOMPARE(clast.valor(1, 0, 1), std::string("177.97"));
        QCOMPARE(clast.valor(1, 0, 2), std::string("9"));
    }
};

QTEST_GUILESS_MAIN(TestArquivoFixo)
#include "test_arquivo_fixo.moc"
