#include "editor_texto_arquivo.h"
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include "dados_deck.h"

namespace {
constexpr int ATRASO_MS = 500;
}  // namespace

// Pagina do editor textual de um arquivo do deck: o arquivo inteiro num editor de texto monoespacado,
// sem quebra automatica de linha, com o botao Salvar. O que se digita vai para o repositorio do deck
// meio segundo depois da ultima tecla (ou na hora, ao salvar ou ao sair do modo textual), e as tabelas
// do mesmo arquivo se refazem; uma edicao feita fora daqui recarrega o texto.
EditorTextoArquivo::EditorTextoArquivo(const QString& titulo, const QString& nome_padrao, const QString& secao_manual,
                                       DadosDeck* dados, QWidget* parent)
    : QWidget(parent), nome_(nome_padrao), secao_manual_(secao_manual), dados_(dados) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    auto* cabecalho = new QHBoxLayout;
    auto* rotulo = new QLabel(titulo, this);
    QFont fonte = rotulo->font();
    fonte.setPointSize(fonte.pointSize() + 1);
    fonte.setBold(true);
    rotulo->setFont(fonte);
    botao_salvar_ = new QPushButton(QStringLiteral("Salvar"), this);
    cabecalho->addWidget(rotulo, 1);
    cabecalho->addWidget(botao_salvar_);
    layout->addLayout(cabecalho);

    detalhes_ = new QLabel(this);
    detalhes_->setEnabled(false);
    layout->addWidget(detalhes_);

    texto_ = new QPlainTextEdit(this);
    texto_->setLineWrapMode(QPlainTextEdit::NoWrap);
    texto_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(texto_, 1);

    atraso_ = new QTimer(this);
    atraso_->setSingleShot(true);
    atraso_->setInterval(ATRASO_MS);
    connect(atraso_, &QTimer::timeout, this, &EditorTextoArquivo::aplicarPendente);
    connect(texto_, &QPlainTextEdit::textChanged, this, [this] {
        if (ignorar_) return;
        atraso_->start();
        botao_salvar_->setEnabled(true);
    });
    connect(botao_salvar_, &QPushButton::clicked, this, [this] {
        aplicarPendente();
        QString motivo;
        atualizarDetalhes(dados_->salvar(nome_, &motivo) ? QStringLiteral("salvo") : motivo);
    });
    connect(dados_, &DadosDeck::recarregado, this, &EditorTextoArquivo::recarregar);
    connect(dados_, &DadosDeck::alterado, this, [this](const QString& nome) {
        if (nome != nome_ || ignorar_) return;
        if (!atraso_->isActive()) recarregar();
        else atualizarDetalhes();
    });
    recarregar();
}

// Poe no editor o texto atual do arquivo, sem disparar a aplicacao de volta; sem o arquivo no deck, o
// editor fica vazio e somente leitura.
void EditorTextoArquivo::recarregar() {
    ignorar_ = true;
    const bool carregado = dados_->arquivo(nome_) != nullptr;
    const QString atual = dados_->texto(nome_);
    if (texto_->toPlainText() != atual) texto_->setPlainText(atual);
    texto_->setReadOnly(!carregado);
    ignorar_ = false;
    atualizarDetalhes();
}

void EditorTextoArquivo::aplicarPendente() {
    atraso_->stop();
    if (!dados_->arquivo(nome_) || texto_->toPlainText() == dados_->texto(nome_)) return;
    ignorar_ = true;
    dados_->substituirTexto(nome_, texto_->toPlainText());
    ignorar_ = false;
    atualizarDetalhes();
}

void EditorTextoArquivo::atualizarDetalhes(const QString& aviso) {
    const QString secao = QStringLiteral("manual do NEWAVE, seção %1").arg(secao_manual_);
    const ArquivoFixo* arquivo = dados_->arquivo(nome_);
    QString texto;
    if (!dados_->carregado()) texto = QStringLiteral("%1  ·  %2  ·  abra o hidr.dat de um deck para editar").arg(nome_, secao);
    else if (!arquivo) texto = QStringLiteral("%1  ·  %2  ·  %3").arg(dados_->nomeNoDeck(nome_), secao, dados_->erro(nome_));
    else
        texto = QStringLiteral("%1  ·  %2  ·  editor textual%3")
                    .arg(dados_->nomeNoDeck(nome_), secao,
                         arquivo->modificado() ? QStringLiteral("  ·  alterado, não salvo") : QString());
    if (!aviso.isEmpty()) texto += QStringLiteral("  ·  ") + aviso;
    detalhes_->setText(texto);
    botao_salvar_->setEnabled(arquivo && (arquivo->modificado() || atraso_->isActive()));
}
