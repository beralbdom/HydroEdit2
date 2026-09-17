#include <QtTest>
#include <filesystem>
#include <fstream>
#include "arquivo_hidr.h"
#include "registro.h"

namespace fs = std::filesystem;

class TestArquivoHidr : public QObject {
    Q_OBJECT
    fs::path tmp;
    static std::string lerBytes(const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(f), {});
    }
private slots:
    void init() {
        tmp = fs::temp_directory_path() / "hydroedit_test";
        fs::create_directories(tmp);
    }
    void cleanup() { fs::remove_all(tmp); }

    void arquivoInexistenteRetornaErro() {
        ArquivoHidr a;
        Resultado r = a.carregar(tmp / "nao_existe.dat");
        QVERIFY(!r.ok);
        QVERIFY(!r.mensagem.empty());
        QVERIFY(a.usinas.empty());
    }
    void tamanhoNaoMultiploDe792RetornaErroENaoCarrega() {
        fs::path p = tmp / "quebrado.dat";
        std::ofstream(p, std::ios::binary) << std::string(792 * 2 + 3, ' ');
        ArquivoHidr a;
        Resultado r = a.carregar(p);
        QVERIFY(!r.ok);
        QVERIFY(r.mensagem.find("792") != std::string::npos);
        QVERIFY(a.usinas.empty());
    }
    void arquivoVazioRetornaErro() {
        fs::path p = tmp / "vazio.dat";
        std::ofstream(p, std::ios::binary);
        ArquivoHidr a;
        QVERIFY(!a.carregar(p).ok);
    }
    void salvarEcarregarPreservaUsinas() {
        ArquivoHidr a;
        a.usinas.resize(3);
        a.usinas[1].nome = "FURNAS";
        a.usinas[1].volume_maximo = 22950.0f;
        a.usinas[1].regulacao = "M";
        fs::path p = tmp / "saida.dat";
        QVERIFY(a.salvar(p).ok);
        QCOMPARE(fs::file_size(p), static_cast<uintmax_t>(3 * 792));
        QVERIFY(!fs::exists(tmp / "saida.dat.tmp"));

        ArquivoHidr b;
        QVERIFY(b.carregar(p).ok);
        QCOMPARE(b.usinas.size(), static_cast<size_t>(3));
        QVERIFY(b.usinas[0].vazia());
        QCOMPARE(b.usinas[1].nome, std::string("FURNAS"));
        QCOMPARE(b.usinas[1].volume_maximo, 22950.0f);
        QCOMPARE(b.numUsinasPreenchidas(), 1);
    }
    void salvarEmDiretorioInexistenteFalhaSemCriarNada() {
        ArquivoHidr a;
        a.usinas.resize(1);
        Resultado r = a.salvar(tmp / "nao_existe" / "x.dat");
        QVERIFY(!r.ok);
    }
    void roundtripDoDeckEhByteAByte() {
        fs::path deck = fs::path(DIR_DECK) / "hidr.dat";
        if (!fs::exists(deck)) QSKIP("deck nao encontrado");
        ArquivoHidr a;
        QVERIFY(a.carregar(deck).ok);
        QCOMPARE(a.usinas.size(), static_cast<size_t>(320));
        QCOMPARE(a.usinas[5].nome, std::string("FURNAS"));
        fs::path p = tmp / "deck_regravado.dat";
        QVERIFY(a.salvar(p).ok);
        QVERIFY(lerBytes(p) == lerBytes(deck));
    }
};
QTEST_APPLESS_MAIN(TestArquivoHidr)
#include "test_arquivo_hidr.moc"
