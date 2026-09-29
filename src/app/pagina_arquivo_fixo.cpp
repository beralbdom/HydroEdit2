#include "pagina_arquivo_fixo.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>
#include "ajuste_colunas.h"
#include "dados_deck.h"
#include "delegate_referencia.h"
#include "formulario_arquivo.h"
#include "formulario_usina.h"
#include "menu_registros.h"
#include "modelo_secao_fixa.h"
#include "recursos_tabela.h"

// Arquivo de parametros (dger.dat, selcor.dat) ou com alguma secao marcada para formulario.
bool PaginaArquivoFixo::temFormulario(const LayoutArquivoFixo& layout) {
    if (layout.parametros) return true;
    for (const SecaoFixa& s : layout.secoes)
        if (s.formulario) return true;
    return false;
}

bool PaginaArquivoFixo::secaoEmTabela(const LayoutArquivoFixo& layout, int secao) {
    return !layout.parametros && !layout.secoes[static_cast<size_t>(secao)].formulario;
}

// Arquivo com formulario e uma tabela so: os dois ficam na mesma pagina, o formulario em cima.
bool PaginaArquivoFixo::paginaUnica(const LayoutArquivoFixo& layout) {
    if (!temFormulario(layout) || layout.parametros) return false;
    int tabelas = 0;
    for (int s = 0; s < static_cast<int>(layout.secoes.size()); ++s)
        if (secaoEmTabela(layout, s)) ++tabelas;
    return tabelas == 1;
}

// Pagina de um arquivo de colunas fixas: titulo com a parte mostrada, linha de detalhes (arquivo,
// registros, ultimo aviso), botao Salvar habilitado so com alteracao pendente e, empilhados, o
// formulario das secoes curtas do arquivo e a tabela editavel de uma secao. O formulario fica num
// painel de abas sem a barra, para ter o mesmo fundo e borda dos formularios das usinas. A parte exibida e
// escolhida pela arvore do navegador (secao -1 e o formulario); os dados sao os do repositorio do
// deck, compartilhados com as outras vistas. Com uma tabela so (paginaUnica), formulario e tabela
// ficam juntos, separados por um divisor, e a arvore nao tem filhos para o arquivo.
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
    botao_adicionar_ = novoBotaoRegistros(QStringLiteral("Adicionar"), true);
    botao_remover_ = novoBotaoRegistros(QStringLiteral("Remover"), false);
    cabecalho->addWidget(titulo_, 1);
    cabecalho->addWidget(botao_adicionar_);
    cabecalho->addWidget(botao_remover_);
    cabecalho->addWidget(botao_salvar_);
    layout_pagina->addLayout(cabecalho);

    detalhes_ = new QLabel(this);
    detalhes_->setEnabled(false);
    layout_pagina->addWidget(detalhes_);

    pilha_ = new QStackedWidget(this);
    layout_pagina->addWidget(pilha_, 1);
    if (temFormulario(layout_)) {
        painel_formulario_ = FormularioUsina::novasAbas(pilha_);
        formulario_ = new FormularioArquivo(info_.nome_padrao, layout_, dados_, painel_formulario_);
        painel_formulario_->addTab(formulario_, info_.titulo);
        painel_formulario_->tabBar()->hide();
        if (!paginaUnica(layout_)) pilha_->addWidget(painel_formulario_);
        connect(formulario_, &FormularioArquivo::valorRecusado, this, [this](const QString& motivo) { atualizar(motivo); });
    }
    for (int s = 0; s < static_cast<int>(layout_.secoes.size()) && primeira_tabela_ < 0; ++s)
        if (secaoEmTabela(layout_, s)) primeira_tabela_ = s;
    if (primeira_tabela_ >= 0) {
        modelo_ = new ModeloSecaoFixa(dados_, info_.nome_padrao, primeira_tabela_, this);
        tabela_ = new QTableView(pilha_);
        tabela_->setModel(modelo_);
        tabela_->setItemDelegate(new DelegateReferencia(tabela_));
        preencherLargura(tabela_);
        habilitarRecursos(tabela_);
        tabela_->setAlternatingRowColors(true);
        tabela_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
        tabela_->verticalHeader()->setVisible(false);
        tabela_->verticalHeader()->setDefaultSectionSize(20);
        tabela_->horizontalHeader()->setFixedHeight(22);
        if (paginaUnica(layout_)) {
            divisor_ = new QSplitter(Qt::Vertical, pilha_);
            divisor_->setChildrenCollapsible(false);
            divisor_->addWidget(painel_formulario_);
            divisor_->addWidget(tabela_);
            divisor_->setStretchFactor(0, 0);
            divisor_->setStretchFactor(1, 1);
            divisor_->setSizes({painel_formulario_->sizeHint().height(), 100000});
            pilha_->addWidget(divisor_);
        } else {
            pilha_->addWidget(tabela_);
        }
        connect(modelo_, &ModeloSecaoFixa::valorRecusado, this, [this](const QString& motivo) { atualizar(motivo); });
        definirAcoesExtras(tabela_, [this](QMenu* menu, const QModelIndex& ix) {
            preencherMenu(menu->addMenu(QStringLiteral("Adicionar")), ix.row(), true);
            preencherMenu(menu->addMenu(QStringLiteral("Remover")), ix.row(), false);
        });
    }

    connect(botao_salvar_, &QPushButton::clicked, this, [this] {
        QString motivo;
        atualizar(dados_->salvar(info_.nome_padrao, &motivo) ? QStringLiteral("salvo") : motivo);
    });
    connect(dados_, &DadosDeck::alterado, this, [this](const QString& nome) {
        if (nome == info_.nome_padrao) atualizar();
    });
    connect(dados_, &DadosDeck::recarregado, this, [this] {
        if (tabela_) ajustarColunas(tabela_);
        atualizar();
    });
    connect(dados_, &DadosDeck::reinterpretado, this, [this](const QString& nome) {
        if (nome != info_.nome_padrao) return;
        if (tabela_) ajustarColunas(tabela_);
        atualizar();
    });
    mostrarSecao(formulario_ ? -1 : primeira_tabela_);
}

// Secao -1 (ou secao que nao e de tabela) mostra o formulario; as demais, a tabela da secao. Na
// pagina unica, formulario e tabela aparecem sempre juntos. No formulario de parametros agrupados
// por tema, secao -2 - k mostra o tema k (a secao -1, o primeiro).
void PaginaArquivoFixo::mostrarSecao(int secao) {
    if (divisor_) {
        mostrando_formulario_ = false;
        botao_adicionar_->setVisible(true);
        botao_remover_->setVisible(true);
        modelo_->definirSecao(primeira_tabela_);
        pilha_->setCurrentWidget(divisor_);
        titulo_->setText(info_.titulo);
        ajustarColunas(tabela_);
        atualizar();
        return;
    }
    mostrando_formulario_ = formulario_ && (secao < 0 || !secaoEmTabela(layout_, secao));
    botao_adicionar_->setVisible(tabela_ && !mostrando_formulario_);
    botao_remover_->setVisible(tabela_ && !mostrando_formulario_);
    if (mostrando_formulario_ || !tabela_) {
        pilha_->setCurrentWidget(formulario_ ? static_cast<QWidget*>(painel_formulario_) : static_cast<QWidget*>(tabela_));
        titulo_->setText(info_.titulo);
        if (formulario_ && !layout_.abas.empty()) {
            const int tema = secao <= -2 ? -2 - secao : 0;
            formulario_->mostrarTema(tema);
            if (tema < static_cast<int>(layout_.abas.size()))
                titulo_->setText(QStringLiteral("%1  ·  %2").arg(info_.titulo, QString::fromStdString(layout_.abas[static_cast<size_t>(tema)].titulo)));
        }
    } else {
        modelo_->definirSecao(secao);
        pilha_->setCurrentWidget(tabela_);
        const bool varias = formulario_ || layout_.secoes.size() > 1;
        const QString titulo_secao = QString::fromStdString(layout_.secoes[static_cast<size_t>(secao)].titulo);
        titulo_->setText(varias ? QStringLiteral("%1  ·  %2").arg(info_.titulo, titulo_secao) : info_.titulo);
        ajustarColunas(tabela_);
    }
    atualizar();
}

// Botao do cabecalho com o menu de adicionar ou remover o registro selecionado na tabela ou o bloco
// dele, montado na hora de abrir.
QPushButton* PaginaArquivoFixo::novoBotaoRegistros(const QString& texto, bool adicionar) {
    auto* botao = new QPushButton(texto, this);
    auto* menu = new QMenu(botao);
    botao->setMenu(menu);
    connect(menu, &QMenu::aboutToShow, this, [this, menu, adicionar] {
        menu->clear();
        preencherMenu(menu, tabela_->currentIndex().row(), adicionar);
    });
    return botao;
}

void PaginaArquivoFixo::preencherMenu(QMenu* menu, int registro, bool adicionar) {
    preencherMenuRegistros(menu, dados_, info_.nome_padrao, modelo_->secao(), registro, adicionar,
                           [this, registro](const QString& recusa, int nova) { depoisDaEdicao(recusa, nova, registro); });
}

// Seleciona o primeiro registro inserido ou, depois de remover, o que ficou no lugar do removido.
void PaginaArquivoFixo::depoisDaEdicao(const QString& recusa, int linha_nova, int registro_anterior) {
    if (!recusa.isEmpty()) {
        atualizar(recusa);
        return;
    }
    const int registro = linha_nova >= 0 ? registroAPartirDaLinha(*dados_, info_.nome_padrao, modelo_->secao(), linha_nova)
                                         : std::min(registro_anterior, modelo_->rowCount() - 1);
    if (registro >= 0) {
        const QModelIndex ix = modelo_->index(registro, 0);
        tabela_->setCurrentIndex(ix);
        tabela_->scrollTo(ix);
    }
    atualizar(linha_nova >= 0 ? QStringLiteral("cópia inserida") : QStringLiteral("removido"));
}

void PaginaArquivoFixo::atualizar(const QString& aviso) {
    const QString nome = dados_->nomeNoDeck(info_.nome_padrao);
    const ArquivoFixo* arquivo = dados_->arquivo(info_.nome_padrao);
    QString texto;
    if (!dados_->carregado()) texto = QStringLiteral("%1  ·  abra o hidr.dat de um deck para editar").arg(nome);
    else if (!arquivo) texto = QStringLiteral("%1  ·  %2").arg(nome, dados_->erro(info_.nome_padrao));
    else if (mostrando_formulario_)
        texto = QStringLiteral("%1%2").arg(nome, arquivo->modificado() ? QStringLiteral("  ·  alterado, não salvo") : QString());
    else
        texto = QStringLiteral("%1  ·  %2 registros%3")
                    .arg(nome)
                    .arg(modelo_ ? modelo_->rowCount() : 0)
                    .arg(arquivo->modificado() ? QStringLiteral("  ·  alterado, não salvo") : QString());
    if (!aviso.isEmpty()) texto += QStringLiteral("  ·  ") + aviso;
    detalhes_->setText(texto);
    botao_salvar_->setEnabled(arquivo && arquivo->modificado());
    botao_adicionar_->setEnabled(arquivo);
    botao_remover_->setEnabled(arquivo);
}
