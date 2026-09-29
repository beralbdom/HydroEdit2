#include <QtTest>
#include <QTemporaryDir>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include "arquivo_fixo.h"
#include "layouts_newave.h"
#include "patamares_newave.h"

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

    static int secaoPorTitulo(const ArquivoFixo& a, const std::string& titulo) {
        for (size_t s = 0; s < a.secoes().size(); ++s)
            if (a.secoes()[s].definicao.titulo == titulo) return static_cast<int>(s);
        return -1;
    }

    void duplicaERemoveRegistrosEBlocos() {
        fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "sistema.dat")) QSKIP("deck ausente");
        const LayoutArquivoFixo& layout_sistema = *layoutNewave("sistema.dat");
        ArquivoFixo sistema;
        QVERIFY(sistema.carregar(deck / "sistema.dat", layout_sistema).ok);
        const std::string original = sistema.conteudo();
        const int limites = secaoPorTitulo(sistema, "Limites de intercâmbio");
        const int interligacoes = secaoPorTitulo(sistema, "Interligações");
        QVERIFY(limites >= 0 && interligacoes >= 0);
        const size_t anos = sistema.secoes()[static_cast<size_t>(limites)].linhas.size();
        const size_t pares = sistema.secoes()[static_cast<size_t>(interligacoes)].linhas.size();
        const int linha_par = sistema.secoes()[static_cast<size_t>(interligacoes)].linhas[0];
        size_t anos_do_par = 0;
        for (const std::vector<int>& c : sistema.secoes()[static_cast<size_t>(limites)].linhas_contexto)
            if (c[0] == linha_par) ++anos_do_par;
        QVERIFY(anos_do_par > 0);

        int nova = -1;
        QVERIFY(sistema.duplicar(interligacoes, 0, -1, layout_sistema, &nova).ok);
        QCOMPARE(sistema.secoes()[static_cast<size_t>(interligacoes)].linhas.size(), pares + 1);
        QCOMPARE(sistema.secoes()[static_cast<size_t>(limites)].linhas.size(), anos + anos_do_par);
        QCOMPARE(sistema.registroNaLinha(interligacoes, nova), 1);
        QCOMPARE(sistema.valor(interligacoes, 1, 0), sistema.valor(interligacoes, 0, 0));
        QCOMPARE(sistema.valor(interligacoes, 1, 1), sistema.valor(interligacoes, 0, 1));
        QVERIFY(sistema.modificado());
        QVERIFY(sistema.remover(interligacoes, 1, -1, layout_sistema).ok);
        QVERIFY(sistema.conteudo() == original);

        QVERIFY(sistema.duplicar(limites, 0, 0, layout_sistema).ok);
        QCOMPARE(sistema.secoes()[static_cast<size_t>(interligacoes)].linhas.size(), pares + 1);
        QVERIFY(sistema.remover(limites, static_cast<int>(anos_do_par), 0, layout_sistema).ok);
        QVERIFY(sistema.conteudo() == original);

        QVERIFY(sistema.duplicar(limites, 0, -1, layout_sistema, &nova).ok);
        QCOMPARE(sistema.secoes()[static_cast<size_t>(limites)].linhas.size(), anos + 1);
        QCOMPARE(sistema.linhas()[static_cast<size_t>(nova)], sistema.linhas()[static_cast<size_t>(nova - 1)]);
        QVERIFY(sistema.remover(limites, 1, -1, layout_sistema).ok);
        QVERIFY(sistema.conteudo() == original);

        QVERIFY(sistema.duplicar(limites, 1, -1, layout_sistema, &nova, true).ok);
        QCOMPARE(sistema.registroNaLinha(limites, nova), 2);
        QCOMPARE(sistema.valor(limites, 2, 3), std::string());
        QCOMPARE(sistema.valor(limites, 2, 4), std::string());
        QCOMPARE(sistema.valor(limites, 2, 0), sistema.valor(limites, 1, 0));
        QVERIFY(!sistema.duplicar(interligacoes, 0, -1, layout_sistema, nullptr, true).ok);
        QVERIFY(sistema.remover(limites, 2, -1, layout_sistema).ok);
        QVERIFY(sistema.conteudo() == original);

        const LayoutArquivoFixo& layout_exph = *layoutNewave("exph.dat");
        ArquivoFixo exph;
        QVERIFY(exph.carregar(deck / "exph.dat", layout_exph).ok);
        const std::string original_exph = exph.conteudo();
        const size_t unidades = exph.secoes()[0].linhas.size();
        const size_t usinas = exph.secoes()[1].linhas.size();
        QCOMPARE(exph.valor(0, 0, 0), std::string("9"));
        QVERIFY(exph.duplicar(0, 0, -1, layout_exph, &nova).ok);
        QCOMPARE(exph.secoes()[0].linhas.size(), unidades + 1);
        QCOMPARE(exph.secoes()[1].linhas.size(), usinas);
        QCOMPARE(exph.valor(0, 1, 0), std::string("9"));
        QCOMPARE(exph.linhas()[static_cast<size_t>(nova)].substr(0, 17), std::string(17, ' '));
        QVERIFY(!exph.remover(0, 0, -1, layout_exph).ok);
        QVERIFY(exph.remover(0, 1, -1, layout_exph).ok);
        QVERIFY(exph.conteudo() == original_exph);
        int sao_simao = -1;
        for (int r = 0; r < static_cast<int>(unidades) && sao_simao < 0; ++r)
            if (exph.valor(0, r, 0) == "33") sao_simao = r;
        QVERIFY(sao_simao >= 0);
        QVERIFY(exph.remover(0, sao_simao, -1, layout_exph).ok);
        QCOMPARE(exph.secoes()[1].linhas.size(), usinas - 1);
        QVERIFY(exph.conteudo().find(" SAO SIMAO") == std::string::npos);
        QVERIFY(exph.duplicar(1, 0, -1, layout_exph).ok);
        QCOMPARE(exph.secoes()[1].linhas.size(), usinas);
        QCOMPARE(exph.secoes()[0].linhas.size(), unidades - 1 + 2);

        const LayoutArquivoFixo& layout_patamar = *layoutNewave("patamar.dat");
        ArquivoFixo patamar;
        QVERIFY(patamar.carregar(deck / "patamar.dat", layout_patamar).ok);
        const int nao_simuladas = secaoPorTitulo(patamar, "Usinas não simuladas por patamar e ano");
        QVERIFY(nao_simuladas >= 0);
        QVERIFY(!patamar.aceitaRegistrosAvulsos(nao_simuladas));
        QVERIFY(!patamar.duplicar(nao_simuladas, 0, -1, layout_patamar).ok);
        const std::vector<int>& linhas_ns = patamar.secoes()[static_cast<size_t>(nao_simuladas)].linhas;
        const size_t registros_ns = linhas_ns.size();
        const int ultimo = static_cast<int>(registros_ns) - 1;
        size_t do_ultimo_bloco = 0;
        const int abertura = patamar.secoes()[static_cast<size_t>(nao_simuladas)].linhas_contexto.back()[0];
        for (const std::vector<int>& c : patamar.secoes()[static_cast<size_t>(nao_simuladas)].linhas_contexto)
            if (c[0] == abertura) ++do_ultimo_bloco;
        const size_t linhas_antes = patamar.linhas().size();
        QVERIFY(patamar.duplicar(nao_simuladas, ultimo, 0, layout_patamar).ok);
        QCOMPARE(patamar.secoes()[static_cast<size_t>(nao_simuladas)].linhas.size(), registros_ns + do_ultimo_bloco);
        QCOMPARE(patamar.linhas().size(), linhas_antes + static_cast<size_t>(linhas_antes - abertura));
    }

    static std::string semCr(std::string texto) {
        texto.erase(std::remove(texto.begin(), texto.end(), '\r'), texto.end());
        return texto;
    }

    static ArquivoFixo ler(const std::map<std::string, std::string>& textos, const std::string& nome) {
        ArquivoFixo a;
        a.interpretar(textos.at(nome), *layoutNewave(nome));
        return a;
    }

    // Tamanho de cada grupo de linhas por patamar (coluna Ordinal "Patamar") do arquivo.
    static std::vector<size_t> gruposPorPatamar(const ArquivoFixo& a) {
        std::vector<size_t> tamanhos;
        for (const SecaoLida& s : a.secoes()) {
            int ordinal = -1;
            for (size_t c = 0; c < s.definicao.colunas.size(); ++c)
                if (s.definicao.colunas[c].tipo == TipoColunaFixa::Ordinal && s.definicao.colunas[c].nome == "Patamar") ordinal = static_cast<int>(c);
            if (ordinal < 0) continue;
            const size_t nivel = static_cast<size_t>(s.definicao.colunas[static_cast<size_t>(ordinal)].contexto);
            std::map<int, size_t> grupos;
            for (const std::vector<int>& c : s.linhas_contexto) ++grupos[c[nivel]];
            for (const auto& [linha, n] : grupos) tamanhos.push_back(n);
        }
        return tamanhos;
    }

    void adicionaERemovePatamarDeCarga() {
        fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "patamar.dat")) QSKIP("deck ausente");
        std::map<std::string, std::string> textos;
        for (const std::string& nome : arquivosComPatamares())
            if (fs::exists(deck / nome)) textos[nome] = semCr(lerBytes(deck / nome));
        const ArquivoFixo patamar_antes = ler(textos, "patamar.dat");
        QCOMPARE(patamaresDeCarga(patamar_antes), 3);
        const std::vector<size_t> grupos_antes = gruposPorPatamar(patamar_antes);
        QVERIFY(!grupos_antes.empty());
        for (size_t n : grupos_antes) QCOMPARE(n, size_t(3));

        MudancaPatamares mais;
        QVERIFY(mudarPatamaresDeCarga(textos, 1, mais).ok);
        QVERIFY(mais.textos.count("patamar.dat") && mais.textos.count("agrint.dat") && mais.textos.count("adterm.dat"));
        QVERIFY(mais.textos.count("restricao-eletrica.csv"));
        QVERIFY(!mais.textos.count("ghmin.dat"));
        std::map<std::string, std::string> depois = textos;
        for (const auto& [nome, texto] : mais.textos) depois[nome] = texto;

        const ArquivoFixo patamar = ler(depois, "patamar.dat");
        QCOMPARE(patamaresDeCarga(patamar), 4);
        const std::vector<size_t> grupos = gruposPorPatamar(patamar);
        QCOMPARE(grupos.size(), grupos_antes.size());
        for (size_t n : grupos) QCOMPARE(n, size_t(4));
        const SecaoLida& duracoes = patamar.secoes()[1];
        QCOMPARE(duracoes.definicao.titulo, std::string("Duração dos patamares por ano"));
        for (size_t r = 3; r < duracoes.linhas.size(); r += 4) {
            QCOMPARE(patamar.valor(1, static_cast<int>(r), 13), std::string("0.0000"));
            QVERIFY(patamar.valor(1, static_cast<int>(r), 0) == patamar.valor(1, static_cast<int>(r - 1), 0));
        }

        const ArquivoFixo agrint = ler(depois, "agrint.dat");
        const SecaoLida& limites = agrint.secoes()[1];
        int coluna4 = -1;
        for (size_t c = 0; c < limites.definicao.colunas.size(); ++c)
            if (limites.definicao.colunas[c].patamares == Patamares::Carga && limites.definicao.colunas[c].patamar == 4) coluna4 = static_cast<int>(c);
        QVERIFY(coluna4 >= 0);
        for (size_t r = 0; r < limites.linhas.size(); ++r) QCOMPARE(agrint.valor(1, static_cast<int>(r), coluna4), std::string("-1."));
        for (const char* comentario : {"RECEBIMENTO NE", "EXPORTACAO IMP-SENE", "FNS + FNESE + XINGU->SE/CO"}) {
            const auto conta = [&](const std::string& t) {
                size_t n = 0;
                for (size_t p = t.find(comentario); p != std::string::npos; p = t.find(comentario, p + 1)) ++n;
                return n;
            };
            QCOMPARE(conta(depois["agrint.dat"]), conta(textos["agrint.dat"]));
        }

        const ArquivoFixo adterm = ler(depois, "adterm.dat");
        const SecaoLida& lags = adterm.secoes()[0];
        for (size_t r = 0; r < lags.linhas.size(); ++r) {
            const std::string& linha = adterm.linhas()[static_cast<size_t>(lags.linhas[r])];
            QCOMPARE(linha.substr(60, 10), linha.substr(48, 10));
        }

        const auto conta_patamar = [](const std::string& csv, int patamar) {
            std::istringstream entrada(csv);
            int n = 0;
            for (std::string linha; std::getline(entrada, linha);) {
                if (linha.rfind("RE-LIM-FORM-PER-PAT", 0) != 0) continue;
                std::istringstream campos(linha);
                std::string campo;
                for (int k = 0; k < 5; ++k) std::getline(campos, campo, ';');
                if (std::stoi(campo) == patamar) ++n;
            }
            return n;
        };
        QVERIFY(conta_patamar(textos["restricao-eletrica.csv"], 3) > 0);
        QCOMPARE(conta_patamar(depois["restricao-eletrica.csv"], 4), conta_patamar(textos["restricao-eletrica.csv"], 3));

        MudancaPatamares menos;
        QVERIFY(mudarPatamaresDeCarga(depois, -1, menos).ok);
        for (const auto& [nome, texto] : menos.textos) depois[nome] = texto;
        QCOMPARE(patamaresDeCarga(ler(depois, "patamar.dat")), 3);
        QVERIFY(depois["patamar.dat"] == textos["patamar.dat"]);
        QVERIFY(depois["restricao-eletrica.csv"] == textos["restricao-eletrica.csv"]);
        const ArquivoFixo agrint_de_volta = ler(depois, "agrint.dat");
        for (size_t r = 0; r < agrint_de_volta.secoes()[1].linhas.size(); ++r)
            QCOMPARE(agrint_de_volta.valor(1, static_cast<int>(r), coluna4), std::string());

        MudancaPatamares recusa;
        QVERIFY(!mudarPatamaresDeCarga(std::map<std::string, std::string>{}, 1, recusa).ok);
    }

    void adicionaERemovePatamarDeDeficit() {
        fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "sistema.dat")) QSKIP("deck ausente");
        std::map<std::string, std::string> textos = {{"sistema.dat", semCr(lerBytes(deck / "sistema.dat"))}};
        MudancaPatamares mais;
        QVERIFY(mudarPatamaresDeDeficit(textos, 1, mais).ok);
        const ArquivoFixo sistema = ler(mais.textos, "sistema.dat");
        QCOMPARE(patamaresDeDeficit(sistema), 2);
        const SecaoFixa& custo = sistema.secoes()[1].definicao;
        int custo1 = -1, custo2 = -1, profundidade2 = -1;
        for (size_t c = 0; c < custo.colunas.size(); ++c) {
            const ColunaFixa& col = custo.colunas[c];
            if (col.patamares != Patamares::Deficit) continue;
            const bool eh_custo = col.nome.rfind("Custo", 0) == 0;
            if (eh_custo && col.patamar == 1) custo1 = static_cast<int>(c);
            if (eh_custo && col.patamar == 2) custo2 = static_cast<int>(c);
            if (!eh_custo && col.patamar == 2) profundidade2 = static_cast<int>(c);
        }
        QVERIFY(custo1 >= 0 && custo2 >= 0 && profundidade2 >= 0);
        QCOMPARE(sistema.valor(1, 0, custo2), sistema.valor(1, 0, custo1));
        QCOMPARE(sistema.valor(1, 0, profundidade2), std::string("0.000"));
        const int ficticio = static_cast<int>(sistema.secoes()[1].linhas.size()) - 1;
        QCOMPARE(sistema.valor(1, ficticio, custo2), std::string());

        MudancaPatamares menos;
        QVERIFY(mudarPatamaresDeDeficit(mais.textos, -1, menos).ok);
        const ArquivoFixo de_volta = ler(menos.textos, "sistema.dat");
        QCOMPARE(patamaresDeDeficit(de_volta), 1);
        QCOMPARE(de_volta.valor(1, 0, custo2), std::string("0.00"));
        QVERIFY(!mudarPatamaresDeDeficit(menos.textos, -1, menos).ok);
    }
};

QTEST_GUILESS_MAIN(TestArquivoFixo)
#include "test_arquivo_fixo.moc"
