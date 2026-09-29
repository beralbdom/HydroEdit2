#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>
#include <QToolButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QtTest>
#include <filesystem>
#include "catalogo_newave.h"
#include "dados_deck.h"
#include "delegate_referencia.h"
#include "deck_newave.h"
#include "formulario_arquivo.h"
#include "layouts_newave.h"
#include "modelo_secao_fixa.h"
#include "pagina_arquivo_fixo.h"
#include "recursos_tabela.h"

namespace fs = std::filesystem;

class TestInterfaceDeck : public QObject {
    Q_OBJECT

    static void tecla(QTableView& tabela, QKeySequence::StandardKey atalho) {
        const QKeySequence sequencia(atalho);
        QKeyEvent evento(QEvent::KeyPress, sequencia[0].key(), sequencia[0].keyboardModifiers());
        QApplication::sendEvent(&tabela, &evento);
    }

private slots:
    void lerEFormatarTsv() {
        const auto linhas = lerTsv(QStringLiteral("1\t2\r\n3\t\r\n"));
        QCOMPARE(linhas.size(), static_cast<size_t>(2));
        QCOMPARE(linhas[0], QStringList({QStringLiteral("1"), QStringLiteral("2")}));
        QCOMPARE(linhas[1], QStringList({QStringLiteral("3"), QString()}));
        QVERIFY(lerTsv(QString()).empty());
        QCOMPARE(formatarTsv(linhas), QStringLiteral("1\t2\r\n3\t\r\n"));
    }

    void copiaEColaPulandoLinhasEscondidas() {
        QStandardItemModel modelo(4, 2);
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 2; ++c) modelo.setItem(r, c, new QStandardItem(QString::number(10 * r + c)));
        QTableView tabela;
        tabela.setModel(&modelo);
        habilitarRecursos(&tabela);
        definirLinhasOcultas(&tabela, [](int r) { return r == 1; });
        QVERIFY(tabela.isRowHidden(1));

        tabela.selectionModel()->select(QItemSelection(modelo.index(0, 0), modelo.index(2, 1)), QItemSelectionModel::Select);
        tecla(tabela, QKeySequence::Copy);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("0\t1\r\n20\t21\r\n"));

        QApplication::clipboard()->setText(QStringLiteral("a\tb\nc\td\n"));
        tabela.selectionModel()->clearSelection();
        tabela.setCurrentIndex(modelo.index(0, 0));
        tecla(tabela, QKeySequence::Paste);
        QCOMPARE(modelo.item(0, 0)->text(), QStringLiteral("a"));
        QCOMPARE(modelo.item(0, 1)->text(), QStringLiteral("b"));
        QCOMPARE(modelo.item(1, 0)->text(), QStringLiteral("10"));
        QCOMPARE(modelo.item(2, 0)->text(), QStringLiteral("c"));
        QCOMPARE(modelo.item(2, 1)->text(), QStringLiteral("d"));

        tabela.setEditTriggers(QAbstractItemView::NoEditTriggers);
        tecla(tabela, QKeySequence::Paste);
        QApplication::clipboard()->setText(QStringLiteral("x"));
        tecla(tabela, QKeySequence::Paste);
        QCOMPARE(modelo.item(0, 0)->text(), QStringLiteral("a"));
    }

    void formulariosDoDeckReal() {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "patamar.dat")) QSKIP("deck ausente");
        DadosDeck dados;
        dados.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        for (const ArquivoNewave& arquivo : catalogoNewave()) {
            const LayoutArquivoFixo* layout = layoutNewave(arquivo.nome_padrao.toStdString());
            if (!layout || !PaginaArquivoFixo::temFormulario(*layout)) continue;
            FormularioArquivo formulario(arquivo.nome_padrao, *layout, &dados);
            QVERIFY2(formulario.campos() >= 0, qPrintable(arquivo.nome_padrao));
            PaginaArquivoFixo pagina(arquivo, *layout, &dados);
            for (int s = -1; s < static_cast<int>(layout->secoes.size()); ++s) pagina.mostrarSecao(s);
        }
        FormularioArquivo patamar(QStringLiteral("patamar.dat"), *layoutNewave("patamar.dat"), &dados);
        QCOMPARE(patamar.campos(), 1 + 4 + 12 * 2);
        FormularioArquivo dger(QStringLiteral("dger.dat"), *layoutNewave("dger.dat"), &dados);
        QVERIFY(dger.campos() >= 101);
        QVERIFY(!PaginaArquivoFixo::secaoEmTabela(*layoutNewave("patamar.dat"), 0));
        QVERIFY(PaginaArquivoFixo::secaoEmTabela(*layoutNewave("patamar.dat"), 1));
    }

    static QGroupBox* grupo(QWidget& formulario, const QString& titulo) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        for (QGroupBox* g : formulario.findChildren<QGroupBox*>())
            if (g->title() == titulo) return g;
        return nullptr;
    }

    void formularioAdicionaERemoveRegistros() {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "sistema.dat")) QSKIP("deck ausente");
        DadosDeck dados;
        dados.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        const QString sistema = QStringLiteral("sistema.dat");
        const QString original = dados.texto(sistema);
        const auto pares = [&] { return dados.arquivo(sistema)->secoes()[3].linhas.size(); };
        const size_t antes = pares();
        FormularioArquivo formulario(sistema, *layoutNewave("sistema.dat"), &dados);
        QGroupBox* interligacoes = grupo(formulario, QStringLiteral("Interligações"));
        QVERIFY(interligacoes);
        QCOMPARE(interligacoes->findChildren<QToolButton*>().size(), qsizetype(antes));
        QPushButton* adicionar = interligacoes->findChild<QPushButton*>();
        QVERIFY(adicionar);
        adicionar->click();
        QCOMPARE(pares(), antes + 1);
        interligacoes = grupo(formulario, QStringLiteral("Interligações"));
        QCOMPARE(interligacoes->findChildren<QToolButton*>().size(), qsizetype(antes + 1));
        interligacoes->findChildren<QToolButton*>().back()->click();
        QCOMPARE(pares(), antes);
        QCOMPARE(dados.texto(sistema), original);
    }

    void referenciasMostramNomeECodigo() {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "sistema.dat")) QSKIP("deck ausente");
        DadosDeck dados;
        dados.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        QCOMPARE(dados.rotuloReferencia(Referencia::Submercado, QStringLiteral("  1")), QStringLiteral("SUDESTE (1)"));
        QCOMPARE(dados.rotuloReferencia(Referencia::Submercado, QStringLiteral("77")), QStringLiteral("77"));
        QCOMPARE(dados.opcoes(Referencia::Submercado).size(), size_t(5));
        QVERIFY(!dados.opcoes(Referencia::Ree).empty());
        QVERIFY(!dados.opcoes(Referencia::UsinaHidro).empty());
        QVERIFY(!dados.opcoes(Referencia::UsinaTermica).empty());
        QVERIFY(!dados.opcoes(Referencia::Posto).empty());

        ModeloSecaoFixa intercambio(&dados, QStringLiteral("sistema.dat"), 2);
        const QModelIndex a = intercambio.index(0, 0);
        QCOMPARE(a.data(Qt::DisplayRole).toString(), QStringLiteral("SUDESTE (1)"));
        QCOMPARE(a.data(Qt::EditRole).toString().trimmed(), QStringLiteral("1"));
        QCOMPARE(a.data(PAPEL_OPCOES).toList().size(), qsizetype(5));
        QVERIFY(intercambio.setData(a, QStringLiteral("3"), Qt::EditRole));
        QCOMPARE(a.data(Qt::DisplayRole).toString(), QStringLiteral("NORDESTE (3)"));

        ModeloSecaoFixa interligacoes(&dados, QStringLiteral("sistema.dat"), 3);
        QTableView tabela;
        tabela.setModel(&interligacoes);
        tabela.setItemDelegate(new DelegateReferencia(&tabela));
        tabela.show();
        const QModelIndex b = interligacoes.index(0, 1);
        QCOMPARE(b.data(Qt::DisplayRole).toString(), QStringLiteral("SUL (2)"));
        QCOMPARE(interligacoes.index(0, 2).data(Qt::DisplayRole).toString(), QStringLiteral("Limite de intercâmbio (0)"));
        QCOMPARE(interligacoes.index(0, 2).data(PAPEL_OPCOES).toList().size(), qsizetype(2));

        ModeloSecaoFixa execucao(&dados, QStringLiteral("dger.dat"), 1);
        QCOMPARE(execucao.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Rodada completa (1)"));
        const ArquivoFixo* curva = dados.arquivo(QStringLiteral("curva.dat"));
        QVERIFY(curva);
        int penalizacao = -1;
        for (size_t s = 0; s < curva->secoes().size(); ++s)
            if (curva->secoes()[s].definicao.titulo == "Penalização da curva") penalizacao = static_cast<int>(s);
        QVERIFY(penalizacao >= 0);
        ModeloSecaoFixa tipo(&dados, QStringLiteral("curva.dat"), penalizacao);
        QCOMPARE(tipo.index(0, 0).data(Qt::EditRole).toString().trimmed(), QStringLiteral("001"));
        QCOMPARE(tipo.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Máxima violação (1)"));
        tabela.edit(b);
        auto* lista = tabela.findChild<QComboBox*>();
        QVERIFY(lista);
        const int norte = lista->findData(QStringLiteral("4"));
        QVERIFY(norte > 0);
        lista->setCurrentIndex(norte);
        emit lista->activated(norte);
        QCOMPARE(b.data(Qt::DisplayRole).toString(), QStringLiteral("NORTE (4)"));
        QTRY_VERIFY(!tabela.findChild<QComboBox*>());
    }
};

QTEST_MAIN(TestInterfaceDeck)
#include "test_interface_deck.moc"
