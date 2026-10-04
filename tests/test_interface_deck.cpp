#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QGroupBox>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QTableView>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QtTest>
#include <filesystem>
#include "busca_deck.h"
#include "catalogo_newave.h"
#include "comparacao_deck.h"
#include "validacao_deck.h"
#include "arquivo_hidr.h"
#include "dados_deck.h"
#include "delegate_referencia.h"
#include "deck_newave.h"
#include "formulario_arquivo.h"
#include "janela_principal.h"
#include "layout_colunas.h"
#include "layouts_newave.h"
#include "modelo_modif.h"
#include "navegador_deck.h"
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
        modelo.setHorizontalHeaderLabels({QStringLiteral("Primeira"), QStringLiteral("Segunda\ncoluna")});
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 2; ++c) modelo.setItem(r, c, new QStandardItem(QString::number(10 * r + c)));
        QTableView tabela;
        tabela.setModel(&modelo);
        habilitarRecursos(&tabela);
        definirLinhasOcultas(&tabela, [](int r) { return r == 1; });
        QVERIFY(tabela.isRowHidden(1));

        tabela.selectionModel()->select(QItemSelection(modelo.index(0, 0), modelo.index(2, 1)), QItemSelectionModel::Select);
        tecla(tabela, QKeySequence::Copy);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Primeira\tSegunda coluna\r\n0\t1\r\n20\t21\r\n"));

        QApplication::clipboard()->setText(QStringLiteral("Primeira\tSegunda coluna\na\tb\n"));
        tabela.selectionModel()->clearSelection();
        tabela.setCurrentIndex(modelo.index(0, 0));
        tecla(tabela, QKeySequence::Paste);
        QCOMPARE(modelo.item(0, 0)->text(), QStringLiteral("Primeira"));
        QCOMPARE(modelo.item(0, 1)->text(), QStringLiteral("Segunda coluna"));
        QCOMPARE(modelo.item(1, 0)->text(), QStringLiteral("10"));
        QCOMPARE(modelo.item(2, 0)->text(), QStringLiteral("a"));
        QCOMPARE(modelo.item(2, 1)->text(), QStringLiteral("b"));

        tabela.setEditTriggers(QAbstractItemView::NoEditTriggers);
        tecla(tabela, QKeySequence::Paste);
        QApplication::clipboard()->setText(QStringLiteral("x"));
        tecla(tabela, QKeySequence::Paste);
        QCOMPARE(modelo.item(0, 0)->text(), QStringLiteral("Primeira"));
    }

    void ordenaNaVistaECopiaNaOrdemMostrada() {
        QStandardItemModel modelo(3, 2);
        modelo.setHorizontalHeaderLabels({QStringLiteral("Nome"), QStringLiteral("Valor")});
        const QStringList nomes = {QStringLiteral("b"), QStringLiteral("a"), QStringLiteral("c")};
        for (int r = 0; r < 3; ++r) {
            modelo.setItem(r, 0, new QStandardItem(nomes[r]));
            modelo.setItem(r, 1, new QStandardItem(QString::number(r)));
        }
        QTableView tabela;
        tabela.setModel(&modelo);
        habilitarRecursos(&tabela);
        ordenarTabela(&tabela, 0, Qt::AscendingOrder);
        QCOMPARE(modelo.item(0, 0)->text(), QStringLiteral("b"));
        tabela.selectAll();
        tecla(tabela, QKeySequence::Copy);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Nome\tValor\r\na\t1\r\nb\t0\r\nc\t2\r\n"));
        ordenarTabela(&tabela, 0, Qt::DescendingOrder);
        tabela.selectAll();
        tecla(tabela, QKeySequence::Copy);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Nome\tValor\r\nc\t2\r\nb\t0\r\na\t1\r\n"));
        tabela.clearSelection();
        tabela.setCurrentIndex(modelo.index(2, 1));
        QApplication::clipboard()->setText(QStringLiteral("x\ny"));
        tecla(tabela, QKeySequence::Paste);
        QCOMPARE(modelo.item(2, 1)->text(), QStringLiteral("x"));
        QCOMPARE(modelo.item(0, 1)->text(), QStringLiteral("y"));
        ordenarTabela(&tabela, -1, Qt::AscendingOrder);
        QVERIFY(!tabela.verticalHeader()->sectionsMoved() || tabela.verticalHeader()->visualIndex(2) == 2);
    }

    void editorDoModif() {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "modif.dat")) QSKIP("deck ausente");
        DadosDeck dados;
        dados.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        ModeloModif modelo(&dados, nullptr);
        modelo.definirFiltro(QStringLiteral("VMAXT"), {});
        QCOMPARE(modelo.columnCount(), 6);
        const QStringList titulos = {QStringLiteral("Usina"), QStringLiteral("Nome"), QStringLiteral("Mês"), QStringLiteral("Ano"),
                                     QStringLiteral("Volume"), QStringLiteral("Unidade")};
        for (int c = 0; c < 6; ++c) QCOMPARE(modelo.headerData(c, Qt::Horizontal, Qt::DisplayRole).toString(), titulos[c]);
        QVERIFY(modelo.registros() > 100);
        QCOMPARE(modelo.index(0, 5).data(Qt::EditRole).toString(), QStringLiteral("%"));
        QCOMPARE(modelo.index(0, 5).data(Qt::DisplayRole).toString(), QStringLiteral("%vu"));
        QVERIFY(!(modelo.flags(modelo.index(0, 0)) & Qt::ItemIsEditable));
        QVERIFY(modelo.flags(modelo.index(0, 4)) & Qt::ItemIsEditable);

        const int linha = modelo.linhaDoArquivo(0);
        const QStringList antes = dados.texto(QStringLiteral("modif.dat")).split(QLatin1Char('\n'));
        QVERIFY(modelo.setData(modelo.index(0, 4), QStringLiteral("55,5"), Qt::EditRole));
        const QStringList depois = dados.texto(QStringLiteral("modif.dat")).split(QLatin1Char('\n'));
        QCOMPARE(depois.size(), antes.size());
        for (int i = 0; i < antes.size(); ++i)
            if (i != linha) QCOMPARE(depois[i], antes[i]);
        QVERIFY(depois[linha].contains(QStringLiteral(" 55.5 '%'")));
        QVERIFY(depois[linha].startsWith(antes[linha].left(10)));
        QVERIFY(!modelo.setData(modelo.index(0, 2), QStringLiteral("13"), Qt::EditRole));

        modelo.definirFiltro({}, [](const QString&) { return true; });
        QCOMPARE(modelo.columnCount(), 6);
        QCOMPARE(modelo.headerData(2, Qt::Horizontal, Qt::DisplayRole).toString(), QStringLiteral("Modificador"));
    }

    void colarAvisaODeckUmaVezENaoRegravaValorIgual() {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "modif.dat")) QSKIP("deck ausente");
        DadosDeck dados;
        dados.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        ModeloModif modelo(&dados, nullptr);
        modelo.definirFiltro(QStringLiteral("VMAXT"), {});
        QTableView tabela;
        tabela.setModel(&modelo);
        tabela.setEditTriggers(QAbstractItemView::DoubleClicked);
        habilitarRecursos(&tabela);
        const QString original = dados.texto(QStringLiteral("modif.dat"));

        tabela.setCurrentIndex(modelo.index(0, 2));
        tabela.selectionModel()->select(QItemSelection(modelo.index(0, 2), modelo.index(1, 4)), QItemSelectionModel::ClearAndSelect);
        tecla(tabela, QKeySequence::Copy);
        const QString copiado = QApplication::clipboard()->text();
        QApplication::clipboard()->setText(copiado.mid(copiado.indexOf(QLatin1Char('\n')) + 1));
        QSignalSpy alterado(&dados, &DadosDeck::alterado);
        tecla(tabela, QKeySequence::Paste);
        QCOMPARE(alterado.count(), 0);
        QCOMPARE(dados.texto(QStringLiteral("modif.dat")), original);

        const QString mes = modelo.index(0, 2).data(Qt::EditRole).toString() == QStringLiteral("5") ? QStringLiteral("6") : QStringLiteral("5");
        QApplication::clipboard()->setText(mes + QStringLiteral("\t2031\t50\n") + mes + QStringLiteral("\t2032\t60\n"));
        tabela.clearSelection();
        tabela.setCurrentIndex(modelo.index(0, 2));
        tecla(tabela, QKeySequence::Paste);
        QCOMPARE(alterado.count(), 1);
        QCOMPARE(modelo.index(0, 2).data(Qt::EditRole).toString(), mes);
        QCOMPARE(modelo.index(0, 3).data(Qt::EditRole).toString(), QStringLiteral("2031"));
        QCOMPARE(modelo.index(1, 3).data(Qt::EditRole).toString(), QStringLiteral("2032"));
        QCOMPARE(modelo.index(1, 4).data(Qt::EditRole).toString().toDouble(), 60.0);
    }

    void janelaAbreHidrSeguidos() {
        const fs::path newave = fs::path(DIR_DECK) / "hidr.dat";
        const fs::path dessem = fs::path(DIR_DECK_DESSEM) / "hidr.dat";
        if (!fs::exists(newave) || !fs::exists(dessem)) QSKIP("decks ausentes");
        JanelaPrincipal janela;
        janela.show();
        for (const fs::path& caminho : {newave, dessem, newave, newave}) {
            janela.abrirCaminho(QString::fromStdWString(caminho.wstring()));
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        QVERIFY(janela.isVisible());
    }

    void janelaDesfazNoDeckEAtalhosNaoSeRepetem() {
        const fs::path newave = fs::path(DIR_DECK) / "hidr.dat";
        if (!fs::exists(newave)) QSKIP("deck ausente");
        JanelaPrincipal janela;
        janela.show();
        janela.abrirCaminho(QString::fromStdWString(newave.wstring()));
        QCoreApplication::processEvents();

        QStringList atalhos;
        QAction* desfazer = nullptr;
        for (QAction* menu : janela.menuBar()->actions())
            for (QAction* acao : menu->menu()->actions()) {
                if (acao->text() == QStringLiteral("&Desfazer")) desfazer = acao;
                if (acao->shortcut().isEmpty()) continue;
                QVERIFY2(!atalhos.contains(acao->shortcut().toString()), qPrintable(acao->shortcut().toString()));
                atalhos << acao->shortcut().toString();
            }
        QVERIFY(atalhos.contains(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S).toString()));
        QVERIFY(desfazer);

        auto* navegador = janela.findChild<NavegadorDeck*>();
        auto* dados = janela.findChild<DadosDeck*>();
        QVERIFY(navegador && dados);
        navegador->setCurrentIndex(1);
        QCoreApplication::processEvents();
        QVERIFY(dados->definir(QStringLiteral("patamar.dat"), 1, 0, 3, QStringLiteral("0.5")).ok);
        QVERIFY(!dados->modificados().isEmpty());
        desfazer->trigger();
        QVERIFY(dados->modificados().isEmpty());
    }

    void layoutColunasUsaOEspacoDisponivel() {
        QWidget pagina;
        auto* colunas = new LayoutColunas(&pagina);
        colunas->setContentsMargins(0, 0, 0, 0);
        colunas->setSpacing(10);
        for (const QSize tamanho : {QSize(200, 100), QSize(200, 300), QSize(200, 100)}) {
            auto* grupo = new QWidget(&pagina);
            grupo->setFixedSize(tamanho);
            colunas->addWidget(grupo);
        }
        QCOMPARE(colunas->colunasPara(700), (std::vector<int>{0, 1, 2}));
        QCOMPARE(colunas->colunasPara(450), (std::vector<int>{0, 1, 1}));
        QCOMPARE(colunas->colunasPara(300), (std::vector<int>{0, 0, 0}));
        QCOMPARE(colunas->heightForWidth(700), 300);
        QCOMPARE(colunas->heightForWidth(450), 410);
        QCOMPARE(colunas->heightForWidth(300), 520);
        QCOMPARE(colunas->minimumSize().width(), 200);
    }

    void procuraNoDeckPorNomeECodigo() {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "confhd.dat")) QSKIP("deck ausente");
        DadosDeck dados;
        dados.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        auto contem = [](const std::vector<ResultadoBusca>& rs, const QString& nome, const QString& motivo) {
            return std::any_of(rs.begin(), rs.end(), [&](const ResultadoBusca& r) { return r.nome_padrao == nome && r.motivo.contains(motivo); });
        };
        const std::vector<ResultadoBusca> furnas = procurarNoDeck(dados, nullptr, QStringLiteral("furnas"));
        QVERIFY(contem(furnas, QStringLiteral("confhd.dat"), QStringLiteral("FURNAS")));
        QVERIFY(contem(furnas, QStringLiteral("modif.dat"), QStringLiteral("FURNAS")));
        for (const ResultadoBusca& r : furnas) {
            QVERIFY(r.linha >= 0 || r.registro >= 0);
            if (r.linha >= 0) QVERIFY(!r.texto.isEmpty());
        }
        const std::vector<ResultadoBusca> seis = procurarNoDeck(dados, nullptr, QStringLiteral("6"));
        QVERIFY(contem(seis, QStringLiteral("confhd.dat"), QStringLiteral("FURNAS (6)")));
        QVERIFY(contem(seis, QStringLiteral("postos.dat"), QStringLiteral("código")));
        QVERIFY(procurarNoDeck(dados, nullptr, QStringLiteral("   ")).empty());
    }

    void comparaDecksESalvaEmOutraPasta() {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "patamar.dat")) QSKIP("deck ausente");
        DadosDeck a;
        DadosDeck b;
        a.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        b.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        QVERIFY(compararDecks(a, nullptr, b, nullptr).empty());
        QVERIFY(b.definir(QStringLiteral("patamar.dat"), 1, 0, 3, QStringLiteral("0.5")).ok);
        QVERIFY(b.definirInteiroBinario(QStringLiteral("vazoes.dat"), 0, 0, QStringLiteral("123456")).ok);
        const std::vector<DiferencaArquivo> d = compararDecks(a, nullptr, b, nullptr);
        QCOMPARE(d.size(), size_t(2));
        for (const DiferencaArquivo& arquivo : d)
            QCOMPARE(arquivo.alteracoes, arquivo.nome_padrao == QStringLiteral("patamar.dat") ? 2 : 1);
        QVERIFY(std::any_of(d.begin(), d.end(), [](const DiferencaArquivo& x) { return x.nome_padrao == QStringLiteral("vazoes.dat"); }));
        QVERIFY(std::any_of(d.begin(), d.end(), [](const DiferencaArquivo& x) { return x.nome_padrao == QStringLiteral("patamar.dat"); }));

        QTemporaryDir pasta;
        QVERIFY(pasta.isValid());
        QVERIFY(b.salvarEm(QStringLiteral("patamar.dat"), pasta.path()));
        QVERIFY(b.modificados().contains(QStringLiteral("patamar.dat")));
        QFile salvo(QDir(pasta.path()).filePath(b.nomeNoDeck(QStringLiteral("patamar.dat"))));
        QVERIFY(salvo.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromLatin1(salvo.readAll()).replace(QStringLiteral("\r\n"), QStringLiteral("\n")), b.texto(QStringLiteral("patamar.dat")));
    }

    void secaoDeFormularioLevaAoTemaDela() {
        const LayoutArquivoFixo& dger = *layoutNewave("dger.dat");
        for (int s = 0; s < static_cast<int>(dger.secoes.size()); ++s) QVERIFY(PaginaArquivoFixo::itemDaSecao(dger, s) <= -2);
        const LayoutArquivoFixo& sistema = *layoutNewave("sistema.dat");
        for (int s = 0; s < static_cast<int>(sistema.secoes.size()); ++s) {
            const int item = PaginaArquivoFixo::itemDaSecao(sistema, s);
            QCOMPARE(item, PaginaArquivoFixo::secaoEmTabela(sistema, s) ? s : -1);
        }
    }

    void deckOficialPassaNaValidacao() {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "confhd.dat")) QSKIP("deck ausente");
        DadosDeck dados;
        dados.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        ArquivoHidr hidr;
        QVERIFY(hidr.carregar(deck / "hidr.dat").ok);
        const std::vector<ProblemaDeck> problemas = validarDeck(dados, &hidr);
        QStringList erros;
        for (const ProblemaDeck& p : problemas) {
            const QString texto = QStringLiteral("%1 %2 [%3/%4] %5 (%6)").arg(p.erro ? "ERRO" : "aviso", p.nome_padrao).arg(p.secao).arg(p.registro).arg(p.mensagem, p.regra);
            qWarning().noquote() << texto;
            if (p.erro) erros << texto;
        }
        QVERIFY2(erros.isEmpty(), qPrintable(erros.join(QLatin1Char('\n'))));
        QVERIFY2(problemas.empty(), "o deck oficial nao deve ter avisos");

        auto secao = [&](const char* nome, const char* titulo) {
            const auto& secoes = dados.arquivo(QString::fromLatin1(nome))->secoes();
            for (size_t s = 0; s < secoes.size(); ++s)
                if (secoes[s].definicao.titulo == titulo) return static_cast<int>(s);
            return -1;
        };
        QVERIFY(dados.definir(QStringLiteral("confhd.dat"), 0, 0, 4, QStringLiteral("99")).ok);
        QVERIFY(dados.definir(QStringLiteral("penalid.dat"), 0, 0, 1, QStringLiteral("0")).ok);
        QVERIFY(dados.definir(QStringLiteral("dger.dat"), secao("dger.dat", "Delta de ZSUP (%)"), 0, 0, QStringLiteral("150")).ok);
        const int deficit = secao("sistema.dat", "Custo do déficit");
        const auto& colunas = dados.arquivo(QStringLiteral("sistema.dat"))->secoes()[static_cast<size_t>(deficit)].definicao.colunas;
        const auto profundidade = std::find_if(colunas.begin(), colunas.end(), [](const ColunaFixa& c) { return c.nome == "Profund. pat. 1 (p.u.)"; });
        QVERIFY(profundidade != colunas.end());
        QVERIFY(dados.definir(QStringLiteral("sistema.dat"), deficit, 0, static_cast<int>(profundidade - colunas.begin()), QStringLiteral("0.5")).ok);
        const std::vector<ProblemaDeck> depois = validarDeck(dados, &hidr);
        auto aponta = [&](const char* nome, const char* trecho) {
            return std::any_of(depois.begin(), depois.end(), [&](const ProblemaDeck& p) {
                return p.erro && p.nome_padrao == QString::fromLatin1(nome) && p.mensagem.contains(QString::fromUtf8(trecho));
            });
        };
        QVERIFY(aponta("confhd.dat", "REE 99"));
        QVERIFY(aponta("penalid.dat", "zero"));
        QVERIFY(aponta("dger.dat", "Delta de ZSUP"));
        QVERIFY(aponta("sistema.dat", "profundidades"));
    }

    void formulariosDoDeckReal() {
        const fs::path deck(DIR_DECK);
        if (!fs::exists(deck / "patamar.dat")) QSKIP("deck ausente");
        DadosDeck dados;
        dados.carregar(QString::fromStdWString(deck.wstring()), lerArquivosDat(deck / "arquivos.dat"));
        for (const ArquivoNewave& arquivo : catalogoNewave()) {
            const LayoutArquivoFixo* layout = layoutNewave(arquivo.nome_padrao.toStdString());
            if (!layout) continue;
            const bool com_formulario = PaginaArquivoFixo::temFormulario(*layout);
            const bool sem_formulario_testado = arquivo.nome_padrao == QStringLiteral("term.dat") || arquivo.nome_padrao == QStringLiteral("clast.dat");
            if (!com_formulario && !sem_formulario_testado) continue;
            if (com_formulario) {
                FormularioArquivo formulario(arquivo.nome_padrao, *layout, &dados);
                QVERIFY2(formulario.campos() >= 0, qPrintable(arquivo.nome_padrao));
            }
            PaginaArquivoFixo pagina(arquivo, *layout, &dados);
            for (int s = -2; s <= static_cast<int>(layout->secoes.size()); ++s) pagina.mostrarSecao(s);
        }
        FormularioArquivo patamar(QStringLiteral("patamar.dat"), *layoutNewave("patamar.dat"), &dados);
        QCOMPARE(patamar.campos(), 1 + 4 + 12 * 2);
        FormularioArquivo dger(QStringLiteral("dger.dat"), *layoutNewave("dger.dat"), &dados);
        QVERIFY(dger.campos() >= 101);
        QStackedWidget* temas = dger.findChild<QStackedWidget*>();
        QVERIFY(temas);
        QCOMPARE(temas->count(), 5);
        dger.mostrarTema(3);
        QCOMPARE(temas->currentIndex(), 3);
        QVERIFY(!PaginaArquivoFixo::secaoEmTabela(*layoutNewave("patamar.dat"), 0));
        QVERIFY(PaginaArquivoFixo::secaoEmTabela(*layoutNewave("patamar.dat"), 1));
        QVERIFY(PaginaArquivoFixo::paginaUnica(*layoutNewave("ree.dat")));
        QVERIFY(!PaginaArquivoFixo::paginaUnica(*layoutNewave("sistema.dat")));
        QVERIFY(!PaginaArquivoFixo::paginaUnica(*layoutNewave("dger.dat")));
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
        QCOMPARE(dados.rotuloReferencia(Referencia::Agrupamento, QStringLiteral("1")),
                 QStringLiteral("SUDESTE→NORDESTE + NOFICT1→NORDESTE (1)"));

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
