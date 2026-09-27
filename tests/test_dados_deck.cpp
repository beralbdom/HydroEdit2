#include <QSignalSpy>
#include <QtTest>
#include <filesystem>
#include "dados_deck.h"
#include "deck_newave.h"

namespace fs = std::filesystem;

class TestDadosDeck : public QObject {
    Q_OBJECT

    static int colunas(const DadosDeck& dados, const char* nome, int secao) {
        return static_cast<int>(dados.arquivo(QString::fromLatin1(nome))->secoes()[static_cast<size_t>(secao)].definicao.colunas.size());
    }

private slots:
    void colunasDePatamarAcompanhamODeck() {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "patamar.dat")) QSKIP("deck ausente");
        DadosDeck dados;
        dados.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        QCOMPARE(colunas(dados, "agrint.dat", 1), 5 + 3);
        QCOMPARE(colunas(dados, "adterm.dat", 0), 3 + 3);
        QCOMPARE(colunas(dados, "sistema.dat", 1), 3 + 2);
        QVERIFY(!dados.arquivo(QStringLiteral("agrint.dat"))->modificado());

        QSignalSpy relidos(&dados, &DadosDeck::reinterpretado);
        QVERIFY(dados.definir(QStringLiteral("patamar.dat"), 0, 0, 0, QStringLiteral("2")).ok);
        QCOMPARE(colunas(dados, "agrint.dat", 1), 5 + 2);
        QCOMPARE(colunas(dados, "patamar.dat", 2), 1 + 2);
        QVERIFY(!dados.arquivo(QStringLiteral("agrint.dat"))->modificado());
        QVERIFY(relidos.contains({QStringLiteral("agrint.dat")}));

        relidos.clear();
        QVERIFY(dados.definir(QStringLiteral("patamar.dat"), 1, 0, 3, QStringLiteral("0.5")).ok);
        QVERIFY(relidos.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestDadosDeck)
#include "test_dados_deck.moc"
