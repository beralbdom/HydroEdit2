#include "janela_principal.h"
#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QVBoxLayout>
#include <filesystem>
#include "delegate_numerico.h"
#include "exportador_csv.h"
#include "filtro_usinas.h"
#include "formulario_usina.h"
#include "modelo_hidr.h"

namespace {
std::filesystem::path paraPath(const QString& s) { return std::filesystem::path(s.toStdWString()); }
}

JanelaPrincipal::JanelaPrincipal(QWidget* parent) : QMainWindow(parent) {
    modelo_ = new ModeloHidr(this);
    filtro_ = new FiltroUsinas(this);
    filtro_->setSourceModel(modelo_);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter_);
    criarTabela();
    formulario_ = new FormularioUsina(modelo_, splitter_);
    splitter_->addWidget(formulario_);
    splitter_->setStretchFactor(0, 3);
    splitter_->setStretchFactor(1, 2);
    connect(tabela_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { formulario_->definirLinha(linhaSelecionada()); });
    criarMenus();

    status_arquivo_ = new QLabel(this);
    status_usinas_ = new QLabel(this);
    status_notas_ = new QLabel(this);
    statusBar()->addWidget(status_arquivo_, 1);
    statusBar()->addWidget(status_usinas_);
    statusBar()->addPermanentWidget(status_notas_);

    connect(modelo_->pilhaUndo(), &QUndoStack::cleanChanged, this, [this](bool) { atualizarTitulo(); });
    connect(modelo_, &ModeloHidr::usinaAlterada, this, [this](int, const Campo*) { atualizarStatus(); });

    resize(1400, 800);
    atualizarTitulo();
    atualizarStatus();
}

void JanelaPrincipal::criarTabela() {
    auto* painel = new QWidget(splitter_);
    auto* layout = new QVBoxLayout(painel);
    auto* linha_filtro = new QHBoxLayout;
    campo_filtro_ = new QLineEdit(painel);
    campo_filtro_->setPlaceholderText(QStringLiteral("Filtrar por código ou nome"));
    campo_filtro_->setClearButtonEnabled(true);
    ocultar_vazias_ = new QCheckBox(QStringLiteral("Ocultar vazias"), painel);
    ocultar_vazias_->setChecked(true);
    linha_filtro->addWidget(campo_filtro_, 1);
    linha_filtro->addWidget(ocultar_vazias_);
    layout->addLayout(linha_filtro);

    tabela_ = new QTableView(painel);
    tabela_->setModel(filtro_);
    tabela_->setItemDelegate(new DelegateNumerico(modelo_, tabela_));
    tabela_->setSortingEnabled(true);
    tabela_->sortByColumn(0, Qt::AscendingOrder);
    tabela_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela_->setSelectionMode(QAbstractItemView::SingleSelection);
    tabela_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    tabela_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tabela_->horizontalHeader()->setDefaultSectionSize(90);
    tabela_->verticalHeader()->setVisible(false);
    layout->addWidget(tabela_, 1);

    connect(campo_filtro_, &QLineEdit::textChanged, filtro_, &FiltroUsinas::definirTexto);
    connect(ocultar_vazias_, &QCheckBox::toggled, filtro_, &FiltroUsinas::definirOcultarVazias);
    splitter_->addWidget(painel);
}

void JanelaPrincipal::criarMenus() {
    QMenu* arquivo = menuBar()->addMenu(QStringLiteral("&Arquivo"));
    arquivo->addAction(QStringLiteral("&Abrir..."), QKeySequence::Open, this, &JanelaPrincipal::abrir);
    acao_salvar_ = arquivo->addAction(QStringLiteral("&Salvar"), QKeySequence::Save, this, &JanelaPrincipal::salvar);
    acao_salvar_como_ = arquivo->addAction(QStringLiteral("Salvar &como..."), QKeySequence::SaveAs, this, &JanelaPrincipal::salvarComo);
    arquivo->addSeparator();
    acao_exportar_ = arquivo->addAction(QStringLiteral("&Exportar CSV..."), this, &JanelaPrincipal::exportarCsv);
    arquivo->addSeparator();
    arquivo->addAction(QStringLiteral("Sai&r"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* editar = menuBar()->addMenu(QStringLiteral("&Editar"));
    QAction* desfazer = modelo_->pilhaUndo()->createUndoAction(this, QStringLiteral("&Desfazer"));
    desfazer->setShortcut(QKeySequence::Undo);
    QAction* refazer = modelo_->pilhaUndo()->createRedoAction(this, QStringLiteral("&Refazer"));
    refazer->setShortcut(QKeySequence::Redo);
    editar->addAction(desfazer);
    editar->addAction(refazer);

    acao_salvar_->setEnabled(false);
    acao_salvar_como_->setEnabled(false);
    acao_exportar_->setEnabled(false);
}

int JanelaPrincipal::linhaSelecionada() const {
    QModelIndex ix = tabela_->selectionModel()->currentIndex();
    if (!ix.isValid()) return -1;
    return filtro_->mapToSource(ix).row();
}

void JanelaPrincipal::selecionarLinha(int linha) {
    QModelIndex origem = modelo_->index(linha, 0);
    QModelIndex ix = filtro_->mapFromSource(origem);
    if (!ix.isValid()) {
        ocultar_vazias_->setChecked(false);
        campo_filtro_->clear();
        ix = filtro_->mapFromSource(origem);
    }
    tabela_->setCurrentIndex(ix);
    tabela_->scrollTo(ix);
}

void JanelaPrincipal::abrir() {
    QString caminho = QFileDialog::getOpenFileName(this, QStringLiteral("Abrir cadastro hidr.dat"), {},
                                                   QStringLiteral("Cadastro NEWAVE (hidr.dat *.dat);;Todos (*.*)"));
    if (!caminho.isEmpty()) abrirCaminho(caminho);
}

void JanelaPrincipal::abrirCaminho(const QString& caminho) {
    ArquivoHidr a;
    Resultado r = a.carregar(paraPath(caminho));
    if (!r.ok) {
        QMessageBox::critical(this, QStringLiteral("Erro ao abrir"), QString::fromUtf8(r.mensagem));
        return;
    }
    DeckLookup lookup;
    lookup.carregarDeck(paraPath(QFileInfo(caminho).absolutePath()));
    lookup.carregarCsvs(paraPath(QApplication::applicationDirPath()));
    modelo_->definirLookup(lookup);
    modelo_->definirArquivo(std::move(a), caminho);
    tabela_->resizeColumnsToContents();
    acao_salvar_->setEnabled(true);
    acao_salvar_como_->setEnabled(true);
    acao_exportar_->setEnabled(true);
    if (modelo_->numUsinas() > 0) selecionarLinha(0);
    atualizarTitulo();
    atualizarStatus();
}

bool JanelaPrincipal::salvarEm(const QString& caminho) {
    Resultado r = modelo_->arquivo().salvar(paraPath(caminho));
    if (!r.ok) {
        QMessageBox::critical(this, QStringLiteral("Erro ao salvar"), QString::fromUtf8(r.mensagem));
        return false;
    }
    modelo_->definirCaminho(caminho);
    modelo_->pilhaUndo()->setClean();
    atualizarTitulo();
    atualizarStatus();
    return true;
}

void JanelaPrincipal::salvar() {
    if (modelo_->caminho().isEmpty()) salvarComo();
    else salvarEm(modelo_->caminho());
}

void JanelaPrincipal::salvarComo() {
    QString caminho = QFileDialog::getSaveFileName(this, QStringLiteral("Salvar cadastro como"), modelo_->caminho(),
                                                   QStringLiteral("Cadastro NEWAVE (*.dat)"));
    if (!caminho.isEmpty()) salvarEm(caminho);
}

void JanelaPrincipal::exportarCsv() {
    QString sugestao = QFileInfo(modelo_->caminho()).absolutePath() + QStringLiteral("/hidr.csv");
    QString caminho = QFileDialog::getSaveFileName(this, QStringLiteral("Exportar CSV"), sugestao, QStringLiteral("CSV (*.csv)"));
    if (caminho.isEmpty()) return;
    OpcoesCsv o;
    auto resposta = QMessageBox::question(this, QStringLiteral("Formato decimal"),
                                          QStringLiteral("Usar vírgula como separador decimal (Excel em português)?"),
                                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    o.virgula_decimal = resposta == QMessageBox::Yes;
    Resultado r = ::exportarCsv(modelo_->arquivo(), paraPath(caminho), o);
    if (!r.ok) QMessageBox::critical(this, QStringLiteral("Erro ao exportar"), QString::fromUtf8(r.mensagem));
    else statusBar()->showMessage(QStringLiteral("CSV gerado: %1").arg(caminho), 5000);
}

void JanelaPrincipal::atualizarTitulo() {
    QString nome = modelo_->caminho().isEmpty() ? QStringLiteral("sem arquivo") : QFileInfo(modelo_->caminho()).fileName();
    QString sujo = modelo_->pilhaUndo()->isClean() ? QString() : QStringLiteral("*");
    setWindowTitle(QStringLiteral("HydroEdit 5 - %1%2").arg(nome, sujo));
}

void JanelaPrincipal::atualizarStatus() {
    status_arquivo_->setText(modelo_->caminho());
    status_usinas_->setText(QStringLiteral("%1 / %2 usinas")
                                .arg(modelo_->arquivo().numUsinasPreenchidas())
                                .arg(modelo_->numUsinas()));
    QStringList notas;
    for (const std::string& n : modelo_->lookup().notas) notas << QString::fromUtf8(n);
    status_notas_->setText(notas.join(QStringLiteral(" | ")));
}
