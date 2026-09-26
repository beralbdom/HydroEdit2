#include "pagina_arquivo_fixo.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include "dados_deck.h"
#include "modelo_secao_fixa.h"

// Pagina de um arquivo de colunas fixas: titulo com a secao mostrada, linha de detalhes (arquivo,
// secao do manual, registros, ultimo aviso), botao Salvar habilitado so com alteracao pendente e a
// tabela editavel da secao. A secao exibida e escolhida pela arvore do navegador; os dados sao os do
// repositorio do deck, compartilhados com as outras vistas.
PaginaArquivoFixo::PaginaArquivoFixo(const ArquivoNewave& arquivo, const LayoutArquivoFixo& layout, DadosDeck* dados, QWidget* parent)
    : QWidget(parent), info_(arquivo), layout_(layout), dados_(dados) {
    auto* layout_pagina = new QVBoxLayout(this);
    layout_pagina->setContentsMargins(6, 4, 6, 4);
    layout_pagina->setSpacing(4);

    auto* cabecalho = new QHBoxLayout;
    titulo_ = new QLabel(this);
    QFont fonte = titulo_->font();
    fonte.setPointSize(fonte.pointSize() + 1);
    fonte.setBold(true);
    titulo_->setFont(fonte);
    botao_salvar_ = new QPushButton(QStringLiteral("Salvar"), this);
    cabecalho->addWidget(titulo_, 1);
    cabecalho->addWidget(botao_salvar_);
    layout_pagina->addLayout(cabecalho);

    detalhes_ = new QLabel(this);
    detalhes_->setEnabled(false);
    layout_pagina->addWidget(detalhes_);

    modelo_ = new ModeloSecaoFixa(dados_, info_.nome_padrao, 0, this);
    tabela_ = new QTableView(this);
    tabela_->setModel(modelo_);
    tabela_->setAlternatingRowColors(true);
    tabela_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    tabela_->verticalHeader()->setVisible(false);
    tabela_->verticalHeader()->setDefaultSectionSize(20);
    tabela_->horizontalHeader()->setFixedHeight(22);
    layout_pagina->addWidget(tabela_, 1);

    connect(botao_salvar_, &QPushButton::clicked, this, [this] {
        QString motivo;
        atualizar(dados_->salvar(info_.nome_padrao, &motivo) ? QStringLiteral("salvo") : motivo);
    });
    connect(modelo_, &ModeloSecaoFixa::valorRecusado, this, [this](const QString& motivo) { atualizar(motivo); });
    connect(dados_, &DadosDeck::alterado, this, [this](const QString& nome) {
        if (nome == info_.nome_padrao) atualizar();
    });
    connect(dados_, &DadosDeck::recarregado, this, [this] {
        tabela_->resizeColumnsToContents();
        atualizar();
    });
    connect(dados_, &DadosDeck::reinterpretado, this, [this](const QString& nome) {
        if (nome != info_.nome_padrao) return;
        tabela_->resizeColumnsToContents();
        atualizar();
    });
    mostrarSecao(0);
}

void PaginaArquivoFixo::mostrarSecao(int secao) {
    modelo_->definirSecao(secao);
    const QString titulo_secao = QString::fromStdString(layout_.secoes[static_cast<size_t>(secao)].titulo);
    titulo_->setText(layout_.secoes.size() > 1 ? QStringLiteral("%1  ·  %2").arg(info_.titulo, titulo_secao) : info_.titulo);
    tabela_->resizeColumnsToContents();
    atualizar();
}

void PaginaArquivoFixo::atualizar(const QString& aviso) {
    const QString secao = QStringLiteral("manual do NEWAVE, seção %1").arg(QString::fromStdString(layout_.secao_manual));
    const QString nome = dados_->nomeNoDeck(info_.nome_padrao);
    const ArquivoFixo* arquivo = dados_->arquivo(info_.nome_padrao);
    QString texto;
    if (!dados_->carregado()) texto = QStringLiteral("%1  ·  %2  ·  abra o hidr.dat de um deck para editar").arg(nome, secao);
    else if (!arquivo) texto = QStringLiteral("%1  ·  %2  ·  %3").arg(nome, secao, dados_->erro(info_.nome_padrao));
    else
        texto = QStringLiteral("%1  ·  %2  ·  %3 registros%4")
                    .arg(nome, secao)
                    .arg(modelo_->rowCount())
                    .arg(arquivo->modificado() ? QStringLiteral("  ·  alterado, não salvo") : QString());
    if (!aviso.isEmpty()) texto += QStringLiteral("  ·  ") + aviso;
    detalhes_->setText(texto);
    botao_salvar_->setEnabled(arquivo && arquivo->modificado());
}
