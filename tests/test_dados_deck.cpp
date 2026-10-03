#include <QSignalSpy>
#include <QtTest>
#include <filesystem>
#include "dados_deck.h"
#include "deck_newave.h"

namespace fs = std::filesystem;

class TestDadosDeck : public QObject {
    Q_OBJECT

    static int secaoPorTitulo(const DadosDeck& dados, const char* nome, const char* titulo) {
        const auto& secoes = dados.arquivo(QString::fromLatin1(nome))->secoes();
        for (size_t s = 0; s < secoes.size(); ++s)
            if (secoes[s].definicao.titulo == titulo) return static_cast<int>(s);
        return -1;
    }

    static bool carregar(DadosDeck& dados) {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "sistema.dat")) return false;
        dados.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        return true;
    }

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

    void desfazEdicoesEVoltaANaoAlterado() {
        DadosDeck dados;
        if (!carregar(dados)) QSKIP("deck ausente");
        const QString patamar = QStringLiteral("patamar.dat");
        const QString original = dados.texto(patamar);
        QVERIFY(dados.definir(patamar, 1, 0, 3, QStringLiteral("0.5")).ok);
        const QString editado = dados.texto(patamar);
        QVERIFY(editado != original);
        QCOMPARE(dados.pilhaUndo()->count(), 1);
        QVERIFY(dados.modificados().contains(patamar));
        dados.pilhaUndo()->undo();
        QCOMPARE(dados.texto(patamar), original);
        QVERIFY(!dados.modificados().contains(patamar));
        dados.pilhaUndo()->redo();
        QCOMPARE(dados.texto(patamar), editado);
        QVERIFY(dados.modificados().contains(patamar));

        QVERIFY(dados.definir(patamar, 1, 0, 3, dados.arquivo(patamar)->valor(1, 0, 3).c_str()).ok);
        QCOMPARE(dados.pilhaUndo()->count(), 1);

        const QString postos = QStringLiteral("postos.dat");
        const int32_t ano = dados.arquivoBinario(postos)->inteiro(0, 12);
        QVERIFY(dados.definirInteiroBinario(postos, 0, 12, QStringLiteral("1900")).ok);
        dados.pilhaUndo()->undo();
        QCOMPARE(dados.arquivoBinario(postos)->inteiro(0, 12), ano);
        QVERIFY(!dados.modificados().contains(postos));
    }

    void loteDesfazDeUmaVezERelePendentesNoFim() {
        DadosDeck dados;
        if (!carregar(dados)) QSKIP("deck ausente");
        const QString patamar = QStringLiteral("patamar.dat");
        const QString modif = QStringLiteral("modif.dat");
        const QString original_patamar = dados.texto(patamar);
        const QString original_modif = dados.texto(modif);
        const int indice = static_cast<int>(dados.arquivo(modif)->linhas().size()) - 1;
        QSignalSpy alterado(&dados, &DadosDeck::alterado);

        dados.iniciarLote();
        QVERIFY(dados.definir(patamar, 1, 0, 3, QStringLiteral("0.5")).ok);
        QVERIFY(dados.definir(patamar, 1, 1, 3, QStringLiteral("0.4")).ok);
        QVERIFY(dados.substituirLinha(modif, indice, QStringLiteral(" VAZMIN       7")).ok);
        QCOMPARE(dados.linha(modif, indice), QStringLiteral(" VAZMIN       7"));
        QCOMPARE(dados.texto(modif), original_modif);
        QCOMPARE(alterado.count(), 0);
        dados.concluirLote();

        QCOMPARE(alterado.count(), 2);
        QCOMPARE(dados.arquivo(modif)->linhas()[static_cast<size_t>(indice)], std::string(" VAZMIN       7"));
        QCOMPARE(dados.pilhaUndo()->count(), 1);
        dados.pilhaUndo()->undo();
        QCOMPARE(dados.texto(patamar), original_patamar);
        QCOMPARE(dados.texto(modif), original_modif);
        QVERIFY(dados.modificados().isEmpty());
    }

    void apagarCampoQueAbreBlocoRefazAsSecoes() {
        DadosDeck dados;
        if (!carregar(dados)) QSKIP("deck ausente");
        const QString sistema = QStringLiteral("sistema.dat");
        const int interligacoes = secaoPorTitulo(dados, "sistema.dat", "Interligações");
        const int limites = secaoPorTitulo(dados, "sistema.dat", "Limites de intercâmbio");
        QVERIFY(interligacoes >= 0 && limites >= 0);
        auto blocos = [&] {
            const auto& secoes = dados.arquivo(sistema)->secoes();
            return std::make_pair(secoes[static_cast<size_t>(interligacoes)].linhas, secoes[static_cast<size_t>(limites)].linhas_contexto);
        };
        const auto antes = blocos();
        QSignalSpy relidos(&dados, &DadosDeck::reinterpretado);
        QVERIFY(dados.definir(sistema, interligacoes, 0, 0, QString()).ok);
        QVERIFY(relidos.contains({sistema}));
        QVERIFY(blocos() != antes);
    }
};

QTEST_GUILESS_MAIN(TestDadosDeck)
#include "test_dados_deck.moc"
