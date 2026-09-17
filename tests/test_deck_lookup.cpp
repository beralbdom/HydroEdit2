#include <QtTest>
#include <filesystem>
#include "deck_lookup.h"

namespace fs = std::filesystem;

class TestDeckLookup : public QObject {
    Q_OBJECT
private slots:
    void leSubsistemasDoSistemaDat() {
        DeckLookup d;
        d.carregarDeck(fs::path(DIR_FIXTURES));
        QCOMPARE(d.nomeSubsistema(1), std::string("SUDESTE"));
        QCOMPARE(d.nomeSubsistema(4), std::string("NORTE"));
        QCOMPARE(d.nomeSubsistema(11), std::string("NOFICT1"));
        QCOMPARE(d.nomeSubsistema(99), std::string(""));
        QCOMPARE(d.subsistemas.size(), static_cast<size_t>(5));
    }
    void lePostosDoPostosDat() {
        DeckLookup d;
        d.carregarDeck(fs::path(DIR_FIXTURES));
        QCOMPARE(d.nomePosto(1), std::string("CAMARGOS"));
        QCOMPARE(d.nomePosto(2), std::string(""));
        QCOMPARE(d.nomePosto(3), std::string("ITUMBIARA"));
        QVERIFY(d.notas.empty());
    }
    void diretorioSemArquivosGeraNotasENaoFalha() {
        DeckLookup d;
        d.carregarDeck(fs::temp_directory_path());
        QVERIFY(d.subsistemas.empty());
        QVERIFY(d.postos.empty());
        QCOMPARE(d.notas.size(), static_cast<size_t>(2));
    }
    void leCsvsOpcionais() {
        DeckLookup d;
        d.carregarCsvs(fs::path(DIR_FIXTURES));
        QCOMPARE(d.nomeEmpresa(339), std::string("FURNAS"));
        QCOMPARE(d.nomeEmpresa(1), std::string(""));
        QVERIFY(d.turbinas.empty());
        QVERIFY(d.notas.empty());
    }
};
QTEST_APPLESS_MAIN(TestDeckLookup)
#include "test_deck_lookup.moc"
