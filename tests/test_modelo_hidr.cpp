#include <QtTest>
#include <QAbstractItemModelTester>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QTableView>
#include "filtro_usinas.h"
#include "modelo_hidr.h"
#include "recursos_tabela.h"

class TestModeloHidr : public QObject {
    Q_OBJECT
    static ArquivoHidr arquivoDeTeste() {
        ArquivoHidr a;
        a.usinas.resize(4);
        a.usinas[0].nome = "CAMARGOS";
        a.usinas[0].subsistema = 1;
        a.usinas[0].volume_minimo = 120;
        a.usinas[2].nome = "FURNAS";
        a.usinas[2].subsistema = 1;
        return a;
    }
private slots:
    void conformeComQAbstractItemModelTester() {
        ModeloHidr m;
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(m.rowCount(), 4);
        int escalares = 0;
        for (const Campo& c : campos()) if (c.escalar()) ++escalares;
        QCOMPARE(m.columnCount(), 1 + escalares);
    }
    void codigoEhIndiceMaisUm() {
        ModeloHidr m;
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        QCOMPARE(m.data(m.index(2, 0)).toString(), QStringLiteral("3"));
        QVERIFY(!(m.flags(m.index(2, 0)) & Qt::ItemIsEditable));
    }
    void setDataEditaEUndoRestaura() {
        ModeloHidr m;
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        int col = m.colunaDoCampo("volume_minimo");
        QVERIFY(col > 0);
        QSignalSpy spy(&m, &ModeloHidr::usinaAlterada);
        QVERIFY(m.setData(m.index(0, col), QStringLiteral("130,5")));
        QCOMPARE(m.usina(0).volume_minimo, 130.5f);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!m.pilhaUndo()->isClean());
        m.pilhaUndo()->undo();
        QCOMPARE(m.usina(0).volume_minimo, 120.0f);
        QVERIFY(m.pilhaUndo()->isClean());
        m.pilhaUndo()->redo();
        QCOMPARE(m.usina(0).volume_minimo, 130.5f);
    }
    void loteDesfazJuntoEAvisaUmaVez() {
        ModeloHidr m;
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        const int volume = m.colunaDoCampo("volume_minimo");
        const int nome = m.colunaDoCampo("nome");
        QSignalSpy spy(&m, &ModeloHidr::usinaAlterada);
        m.iniciarLote();
        QVERIFY(m.setData(m.index(0, volume), QStringLiteral("10")));
        QVERIFY(m.setData(m.index(2, volume), QStringLiteral("20")));
        QVERIFY(m.setData(m.index(2, nome), QStringLiteral("NOVA")));
        QCOMPARE(m.usina(2).volume_minimo, 20.0f);
        QCOMPARE(spy.count(), 0);
        m.concluirLote();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), -1);
        QCOMPARE(m.pilhaUndo()->count(), 1);
        m.pilhaUndo()->undo();
        QCOMPARE(m.usina(0).volume_minimo, 120.0f);
        QCOMPARE(m.usina(2).nome, std::string("FURNAS"));
        QCOMPARE(spy.count(), 1);
        QVERIFY(m.pilhaUndo()->isClean());
        m.pilhaUndo()->redo();
        QCOMPARE(m.usina(2).volume_minimo, 20.0f);
        QCOMPARE(m.usina(2).nome, std::string("NOVA"));
        QCOMPARE(spy.count(), 2);

        m.iniciarLote();
        QVERIFY(m.setData(m.index(0, volume), QStringLiteral("10")));
        m.concluirLote();
        QCOMPARE(m.pilhaUndo()->count(), 1);
    }
    void setDataInvalidoEhRejeitado() {
        ModeloHidr m;
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        QVERIFY(!m.setData(m.index(0, m.colunaDoCampo("volume_minimo")), QStringLiteral("abc")));
        QVERIFY(!m.setData(m.index(0, m.colunaDoCampo("nome")), QStringLiteral("NOME COM MAIS DE DOZE")));
        QVERIFY(m.pilhaUndo()->isClean());
    }
    void definirValorVetorialPassaPeloUndo() {
        ModeloHidr m;
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        const Campo* pj = campo("pol_jusante");
        m.definirValor(2, *pj, 7, Valor{3.5f});
        QCOMPARE(m.usina(2).pol_jusante[1][2], 3.5f);
        m.pilhaUndo()->undo();
        QCOMPARE(m.usina(2).pol_jusante[1][2], 0.0f);
    }
    void substituirUsinaEUndo() {
        ModeloHidr m;
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        UsinaHidr nova;
        nova.nome = "NOVA";
        m.substituirUsina(1, nova, QStringLiteral("Nova usina"));
        QCOMPARE(m.usina(1).nome, std::string("NOVA"));
        QCOMPARE(m.data(m.index(1, m.colunaDoCampo("nome"))).toString(), QStringLiteral("NOVA"));
        m.pilhaUndo()->undo();
        QVERIFY(m.usina(1).vazia());
    }
    void displayUsaLookupEUserRoleEhNumerico() {
        ModeloHidr m;
        DeckLookup l;
        l.subsistemas[1] = "SUDESTE";
        m.definirLookup(l);
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        QModelIndex ix = m.index(0, m.colunaDoCampo("subsistema"));
        QCOMPARE(m.data(ix, Qt::DisplayRole).toString(), QStringLiteral("1 SUDESTE"));
        QCOMPARE(m.data(ix, Qt::EditRole).toString(), QStringLiteral("1"));
        QCOMPARE(m.data(m.index(0, m.colunaDoCampo("volume_minimo")), Qt::UserRole).toDouble(), 120.0);
    }
    void cabecalhoTemRotuloLegivelEDicaComONomeDoCampo() {
        ModeloHidr m;
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        QCOMPARE(m.headerData(0, Qt::Horizontal).toString(), QStringLiteral("Código"));
        QCOMPARE(m.headerData(0, Qt::Horizontal, Qt::ToolTipRole).toString(), QStringLiteral("codigo"));
        int col = m.colunaDoCampo("posto_bdh");
        QCOMPARE(m.headerData(col, Qt::Horizontal).toString(), QStringLiteral("Posto BDH"));
        QCOMPARE(m.headerData(col, Qt::Horizontal, Qt::ToolTipRole).toString(), QStringLiteral("posto_bdh"));
        for (const Campo& c : campos()) {
            if (!c.escalar()) continue;
            QString rotulo = m.headerData(m.colunaDoCampo(c.nome), Qt::Horizontal).toString();
            QVERIFY2(rotulo != QString::fromLatin1(c.nome.data(), static_cast<int>(c.nome.size())), c.nome.data());
        }
    }
    void colunasComNomeDoLookupAlinhamAEsquerda() {
        ModeloHidr m;
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        const int esquerda = int(Qt::AlignLeft | Qt::AlignVCenter);
        const int direita = int(Qt::AlignRight | Qt::AlignVCenter);
        QCOMPARE(m.data(m.index(0, m.colunaDoCampo("subsistema")), Qt::TextAlignmentRole).toInt(), esquerda);
        QCOMPARE(m.data(m.index(0, m.colunaDoCampo("jusante")), Qt::TextAlignmentRole).toInt(), esquerda);
        QCOMPARE(m.data(m.index(0, m.colunaDoCampo("volume_minimo")), Qt::TextAlignmentRole).toInt(), direita);
        QCOMPARE(m.data(m.index(0, m.colunaDoCampo("num_conjuntos")), Qt::TextAlignmentRole).toInt(), direita);
    }
    void colarEmTabelaOrdenadaNaoTrocaAsUsinas() {
        ModeloHidr m;
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        FiltroUsinas f;
        f.setSourceModel(&m);
        QTableView tabela;
        tabela.setModel(&f);
        tabela.setEditTriggers(QAbstractItemView::DoubleClicked);
        habilitarRecursos(&tabela, true);
        const int volume = m.colunaDoCampo("volume_minimo");
        const int nome = m.colunaDoCampo("nome");
        ordenarTabela(&tabela, volume, Qt::AscendingOrder);
        QCOMPARE(f.index(0, nome).data().toString(), QStringLiteral("FURNAS"));
        QApplication::clipboard()->setText(QStringLiteral("500\n100\n"));
        tabela.setCurrentIndex(f.index(0, volume));
        const QKeySequence colar(QKeySequence::Paste);
        QKeyEvent evento(QEvent::KeyPress, colar[0].key(), colar[0].keyboardModifiers());
        QApplication::sendEvent(&tabela, &evento);
        QCOMPARE(m.usina(2).volume_minimo, 500.0f);
        QCOMPARE(m.usina(0).volume_minimo, 100.0f);
        QCOMPARE(f.index(0, nome).data().toString(), QStringLiteral("CAMARGOS"));
    }
    void filtroOcultaVaziasEBuscaPorNomeOuCodigo() {
        ModeloHidr m;
        m.definirArquivo(arquivoDeTeste(), QStringLiteral("x.dat"));
        FiltroUsinas f;
        f.setSourceModel(&m);
        f.definirOcultarVazias(true);
        QCOMPARE(f.rowCount(), 2);
        f.definirOcultarVazias(false);
        QCOMPARE(f.rowCount(), 4);
        f.definirTexto(QStringLiteral("fur"));
        QCOMPARE(f.rowCount(), 1);
        f.definirTexto(QStringLiteral("1"));
        QCOMPARE(f.rowCount(), 1);
        QCOMPARE(f.data(f.index(0, 0)).toString(), QStringLiteral("1"));
    }
    void conversaoDeTexto() {
        QCOMPARE(textoValor(Valor{12.5f}), QStringLiteral("12.5"));
        QCOMPARE(textoValor(Valor{int32_t{7}}), QStringLiteral("7"));
        QCOMPARE(textoValor(Valor{std::string("AB")}), QStringLiteral("AB"));
        QVERIFY(!valorDeTexto(*campo("posto"), QStringLiteral("1.5")).has_value());
        QCOMPARE(std::get<float>(*valorDeTexto(*campo("teif"), QStringLiteral("1e-7"))), 1e-7f);
        QCOMPARE(std::get<std::string>(*valorDeTexto(*campo("regulacao"), QStringLiteral("M"))), std::string("M"));
        QVERIFY(!valorDeTexto(*campo("regulacao"), QStringLiteral("MS")).has_value());
    }
};
QTEST_MAIN(TestModeloHidr)
#include "test_modelo_hidr.moc"
