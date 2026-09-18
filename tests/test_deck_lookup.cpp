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
    void leReeDasUsinasDoConfhd() {
        DeckLookup d;
        d.carregarDeck(fs::path(DIR_FIXTURES));
        QCOMPARE(d.reeDaUsina(4), 10);
        QCOMPARE(d.reeDaUsina(20), 10);
        QCOMPARE(d.reeDaUsina(21), 10);
        QCOMPARE(d.reeDaUsina(999), 0);
        QCOMPARE(d.ree_da_usina.size(), static_cast<size_t>(3));
    }
    void leReesDoReeDat() {
        DeckLookup d;
        d.carregarDeck(fs::path(DIR_FIXTURES));
        QCOMPARE(d.nomeRee(6), std::string("MADEIRA"));
        QCOMPARE(d.nomeRee(12), std::string("PRNPANEMA"));
        QCOMPARE(d.nomeRee(99), std::string(""));
        QCOMPARE(d.submercadoDoRee(11), 2);
        QCOMPARE(d.submercadoDoRee(1), 1);
        QCOMPARE(d.submercadoDoRee(99), 0);
        QCOMPARE(d.rees.size(), static_cast<size_t>(12));
    }
    void diretorioSemArquivosGeraNotasENaoFalha() {
        DeckLookup d;
        d.carregarDeck(fs::temp_directory_path());
        QVERIFY(d.subsistemas.empty());
        QVERIFY(d.postos.empty());
        QVERIFY(d.ree_da_usina.empty());
        QVERIFY(d.rees.empty());
        QCOMPARE(d.notas.size(), static_cast<size_t>(4));
    }
    void leCsvsOpcionais() {
        DeckLookup d;
        d.carregarCsvs(fs::path(DIR_FIXTURES));
        QCOMPARE(d.nomeEmpresa(339), std::string("FURNAS"));
        QCOMPARE(d.nomeEmpresa(1), std::string(""));
        QVERIFY(d.turbinas.empty());
        QVERIFY(d.notas.empty());
    }
    void detectaDessemPeloDessemArq() {
        DeckLookup d;
        d.carregarDeck(fs::path(DIR_FIXTURES) / "dessem");
        QCOMPARE(d.modelo, ModeloDeck::Dessem);
        QCOMPARE(d.nomeSubsistema(1), std::string("SUDESTE"));
        QCOMPARE(d.nomeSubsistema(11), std::string("NOFICT1"));
        QCOMPARE(d.nomeRee(6), std::string("MADEIRA"));
        QCOMPARE(d.submercadoDoRee(2), 2);
        QCOMPARE(d.rees.size(), static_cast<size_t>(12));
        QCOMPARE(d.reeDaUsina(1), 10);
        QCOMPARE(d.reeDaUsina(999), 0);
        QVERIFY(d.postos.empty());
        QVERIFY(d.notas.empty());
    }
    void detectaNewavePeloArquivosDat() {
        DeckLookup d;
        d.carregarDeck(fs::path(DIR_FIXTURES) / "newave");
        QCOMPARE(d.modelo, ModeloDeck::Newave);
        QCOMPARE(d.notas.size(), static_cast<size_t>(4));
    }
    void diretorioSemNadaEhDesconhecido() {
        DeckLookup d;
        d.carregarDeck(fs::temp_directory_path());
        QCOMPARE(d.modelo, ModeloDeck::Desconhecido);
        QCOMPARE(d.notas.size(), static_cast<size_t>(4));
    }
};
QTEST_APPLESS_MAIN(TestDeckLookup)
#include "test_deck_lookup.moc"
