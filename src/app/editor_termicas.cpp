#include "editor_termicas.h"
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTabWidget>
#include <QVBoxLayout>
#include "dados_deck.h"
#include "formulario_termica.h"
#include "modelo_secao_fixa.h"

// Editor das usinas termoeletricas no desenho do editor das hidroeletricas: a tabela do term.dat a
// esquerda, dentro de uma aba Tabela e com as mesmas margens, ordenavel e editavel, e o formulario
// da usina selecionada a direita. A usina escolhida
// na tabela e a do campo 1 (numero da usina) da linha selecionada.
EditorTermicas::EditorTermicas(DadosDeck* dados, QWidget* parent) : QSplitter(Qt::Horizontal, parent) {
    setHandleWidth(4);
    auto* modelo = new ModeloSecaoFixa(dados, QStringLiteral("term.dat"), 0, this);
    ordenacao_ = new QSortFilterProxyModel(this);
    ordenacao_->setSourceModel(modelo);
    ordenacao_->setSortRole(Qt::UserRole);

    auto* painel = new QWidget(this);
    auto* layout = new QVBoxLayout(painel);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    auto* abas = new QTabWidget(painel);
    tabela_ = new QTableView(abas);
    tabela_->setModel(ordenacao_);
    tabela_->setSortingEnabled(true);
    tabela_->sortByColumn(0, Qt::AscendingOrder);
    tabela_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela_->setSelectionMode(QAbstractItemView::SingleSelection);
    tabela_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    tabela_->setAlternatingRowColors(true);
    tabela_->verticalHeader()->setVisible(false);
    tabela_->verticalHeader()->setDefaultSectionSize(20);
    tabela_->horizontalHeader()->setFixedHeight(22);

    formulario_ = new FormularioTermica(dados, this);
    abas->addTab(tabela_, QStringLiteral("Tabela"));
    layout->addWidget(abas, 1);
    addWidget(painel);
    addWidget(formulario_);
    setStretchFactor(0, 1);
    setStretchFactor(1, 1);
    setSizes({480, 480});

    connect(tabela_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex& atual, const QModelIndex&) {
        formulario_->definirUsina(atual.isValid() ? ordenacao_->index(atual.row(), 0).data().toString() : QString());
    });
    connect(dados, &DadosDeck::recarregado, this, [this] {
        tabela_->resizeColumnsToContents();
        selecionarPrimeira();
    });
}

void EditorTermicas::selecionarPrimeira() {
    if (ordenacao_->rowCount() == 0) {
        formulario_->definirUsina({});
        return;
    }
    tabela_->setCurrentIndex(ordenacao_->index(0, 0));
    formulario_->definirUsina(ordenacao_->index(0, 0).data().toString());
}
