#include <QtTest>
#include <QTemporaryDir>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "arquivo_binario.h"
#include "layouts_newave.h"

namespace fs = std::filesystem;

class TestArquivoBinario : public QObject {
    Q_OBJECT

    static std::string lerBytes(const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        std::stringstream s;
        s << f.rdbuf();
        return s.str();
    }

private slots:
    void recusaTamanhoQueNaoEMultiploDoRegistro() {
        QTemporaryDir dir;
        const fs::path caminho = fs::path(dir.path().toStdWString()) / "x.dat";
        std::ofstream(caminho, std::ios::binary) << "12345";
        ArquivoBinario a;
        QVERIFY(!a.carregar(caminho, 4).ok);
        QCOMPARE(a.registros(), 0);
    }
    void editaTextoEInteiroESalva() {
        QTemporaryDir dir;
        const fs::path caminho = fs::path(dir.path().toStdWString()) / "postos.dat";
        {
            std::ofstream f(caminho, std::ios::binary);
            const std::string nome = "CAMARGOS    ";
            const int32_t anos[2] = {1931, 2024};
            f.write(nome.data(), 12);
            f.write(reinterpret_cast<const char*>(anos), 8);
            f.write(std::string(20, '\0').data(), 20);
        }
        ArquivoBinario a;
        QVERIFY(a.carregar(caminho, postos_dat::REGISTRO).ok);
        QCOMPARE(a.registros(), 2);
        QCOMPARE(a.texto(0, postos_dat::NOME, postos_dat::TAMANHO_NOME), std::string("CAMARGOS"));
        QCOMPARE(a.texto(1, postos_dat::NOME, postos_dat::TAMANHO_NOME), std::string(""));
        QCOMPARE(a.inteiro(0, postos_dat::ANO_FINAL), 2024);
        QVERIFY(!a.definirTexto(1, postos_dat::NOME, postos_dat::TAMANHO_NOME, "NOME COMPRIDO DEMAIS").ok);
        QVERIFY(!a.modificado());
        QVERIFY(a.definirTexto(1, postos_dat::NOME, postos_dat::TAMANHO_NOME, "FURNAS").ok);
        a.definirInteiro(1, postos_dat::ANO_INICIAL, 1931);
        QVERIFY(a.modificado());
        QVERIFY(a.salvar(caminho).ok);
        QVERIFY(!a.modificado());
        ArquivoBinario b;
        QVERIFY(b.carregar(caminho, postos_dat::REGISTRO).ok);
        QCOMPARE(b.texto(1, postos_dat::NOME, postos_dat::TAMANHO_NOME), std::string("FURNAS"));
        QCOMPARE(b.inteiro(1, postos_dat::ANO_INICIAL), 1931);
        QCOMPARE(lerBytes(caminho).substr(20, 12), std::string("FURNAS      "));
    }
    void postosEVazoesDoDeckReal() {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "postos.dat")) QSKIP("deck ausente");
        ArquivoBinario postos;
        QVERIFY(postos.carregar(deck / "postos.dat", postos_dat::REGISTRO).ok);
        QCOMPARE(postos.registros(), 320);
        QCOMPARE(postos.texto(0, postos_dat::NOME, postos_dat::TAMANHO_NOME), std::string("CAMARGOS"));
        QCOMPARE(postos.inteiro(0, postos_dat::ANO_INICIAL), 1931);
        ArquivoBinario vazoes;
        QVERIFY(vazoes.carregar(deck / "vazoes.dat", 4 * postos.registros()).ok);
        QCOMPARE(vazoes.registros(), 1152);
        QVERIFY(vazoes.inteiro(0, 0) > 0);
    }
};

QTEST_GUILESS_MAIN(TestArquivoBinario)
#include "test_arquivo_binario.moc"
