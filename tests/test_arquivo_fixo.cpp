#include <QtTest>
#include <QTemporaryDir>
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
        QVERIFY(!a.definir(0, 0, 0, "5").ok);
        QVERIFY(!a.definir(0, 0, 2, "5").ok);
        QVERIFY(a.definir(0, 1, 3, "12").ok);
        QCOMPARE(a.valor(0, 1, 3), std::string("12."));
        QCOMPARE(a.secoes()[1].linhas.size(), static_cast<size_t>(2));
        QCOMPARE(a.valor(1, 1, 0), std::string("SUL"));
        QCOMPARE(a.secoes()[2].linhas.size(), static_cast<size_t>(1));
        QCOMPARE(a.valor(2, 0, 0), std::string("5"));
        QCOMPARE(a.valor(3, 0, 0), std::string("7"));
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
