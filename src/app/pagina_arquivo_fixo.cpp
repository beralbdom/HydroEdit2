#include "pagina_arquivo_fixo.h"
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <filesystem>
#include "modelo_secao_fixa.h"

// Pagina de um arquivo de colunas fixas: titulo com a secao mostrada, linha de detalhes (arquivo,
// secao do manual, registros, ultimo aviso), botao Salvar habilitado so com alteracao pendente e a
// tabela editavel da secao. A secao exibida e escolhida pela arvore do navegador.
PaginaArquivoFixo::PaginaArquivoFixo(const ArquivoNewave& arquivo, const LayoutArquivoFixo& layout, QWidget* parent)
    : QWidget(parent), info_(arquivo), layout_(layout) {
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
    botao_salvar_->setEnabled(false);
    cabecalho->addWidget(titulo_, 1);
    cabecalho->addWidget(botao_salvar_);
    layout_pagina->addLayout(cabecalho);

    detalhes_ = new QLabel(this);
    detalhes_->setEnabled(false);
    layout_pagina->addWidget(detalhes_);

    modelo_ = new ModeloSecaoFixa(this);
    tabela_ = new QTableView(this);
    tabela_->setModel(modelo_);
    tabela_->setAlternatingRowColors(true);
    tabela_->setSelectionBehavior(QAbstractItemView::SelectItems);
    tabela_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    tabela_->verticalHeader()->setVisible(false);
    tabela_->verticalHeader()->setDefaultSectionSize(20);
    tabela_->horizontalHeader()->setFixedHeight(22);
    layout_pagina->addWidget(tabela_, 1);

    connect(botao_salvar_, &QPushButton::clicked, this, &PaginaArquivoFixo::salvar);
    connect(modelo_, &ModeloSecaoFixa::valorRecusado, this, [this](const QString& motivo) { atualizarDetalhes(motivo); });
    connect(modelo_, &ModeloSecaoFixa::alterado, this, [this] {
        botao_salvar_->setEnabled(true);
        atualizarDetalhes();
        emit modificacaoMudou(true);
    });
    carregar({}, {});
}

QString PaginaArquivoFixo::nomeArquivo() const { return caminho_.isEmpty() ? info_.nome_padrao : QFileInfo(caminho_).fileName(); }

// Resolve o nome pelo arquivos.dat como a pagina de previa e le o arquivo com o layout; sem deck ou
// com erro de leitura, a tabela fica vazia e o motivo vai para os detalhes.
void PaginaArquivoFixo::carregar(const QString& dir_deck, const std::map<std::string, std::string>& arquivos_dat) {
    caminho_.clear();
    erro_.clear();
    arquivo_ = ArquivoFixo();
    if (!dir_deck.isEmpty()) {
        QString nome = info_.nome_padrao;
        auto it = arquivos_dat.find(info_.rotulo_arquivos.toStdString());
        if (!info_.rotulo_arquivos.isEmpty() && it != arquivos_dat.end()) nome = QString::fromStdString(it->second);
        caminho_ = QDir(dir_deck).filePath(nome);
        Resultado r = arquivo_.carregar(std::filesystem::path(caminho_.toStdWString()), layout_);
        if (!r.ok) {
            erro_ = QString::fromUtf8(r.mensagem);
            arquivo_ = ArquivoFixo();
        }
    }
    botao_salvar_->setEnabled(false);
    mostrarSecao(secao_);
    emit modificacaoMudou(false);
}

void PaginaArquivoFixo::mostrarSecao(int secao) {
    secao_ = secao;
    const bool carregado = secao < static_cast<int>(arquivo_.secoes().size());
    modelo_->definirFonte(carregado ? &arquivo_ : nullptr, secao);
    const QString titulo_secao = QString::fromStdString(layout_.secoes[static_cast<size_t>(secao)].titulo);
    titulo_->setText(layout_.secoes.size() > 1 ? QStringLiteral("%1  ·  %2").arg(info_.titulo, titulo_secao) : info_.titulo);
    tabela_->resizeColumnsToContents();
    atualizarDetalhes();
}

void PaginaArquivoFixo::atualizarDetalhes(const QString& aviso) {
    const QString secao = QStringLiteral("manual do NEWAVE, seção %1").arg(QString::fromStdString(layout_.secao_manual));
    QString texto;
    if (caminho_.isEmpty()) texto = QStringLiteral("%1  ·  %2  ·  abra o hidr.dat de um deck para editar").arg(info_.nome_padrao, secao);
    else if (!erro_.isEmpty()) texto = QStringLiteral("%1  ·  %2  ·  %3").arg(nomeArquivo(), secao, erro_);
    else
        texto = QStringLiteral("%1  ·  %2  ·  %3 registros%4")
                    .arg(nomeArquivo(), secao)
                    .arg(modelo_->rowCount())
                    .arg(arquivo_.modificado() ? QStringLiteral("  ·  alterado, não salvo") : QString());
    if (!aviso.isEmpty()) texto += QStringLiteral("  ·  ") + aviso;
    detalhes_->setText(texto);
}

bool PaginaArquivoFixo::salvar() {
    if (caminho_.isEmpty() || !arquivo_.modificado()) return true;
    Resultado r = arquivo_.salvar(std::filesystem::path(caminho_.toStdWString()));
    if (!r.ok) {
        atualizarDetalhes(QString::fromUtf8(r.mensagem));
        return false;
    }
    botao_salvar_->setEnabled(false);
    atualizarDetalhes(QStringLiteral("salvo"));
    emit modificacaoMudou(false);
    return true;
}
