#include "legenda_cascata.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int LADO_AMOSTRA = 10;
constexpr int ALPHA_FUNDO = 210;
constexpr size_t MAXIMO_LINHAS = 15;
}  // namespace

LegendaCascata::LegendaCascata(QWidget* parent) : QFrame(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAutoFillBackground(false);
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(6, 4, 6, 4);
    layout_->setSpacing(2);
    layout_->setSizeConstraint(QLayout::SetFixedSize);
    hide();
}

// Redesenha o fundo a mao em vez de folha de estilo: cor da janela com alpha para a cena aparecer
// por baixo, mais uma borda de 1 px. Um filho do viewport sem autoFillBackground nao ganha fundo
// nenhum do pai, entao sem isso a legenda sairia por cima da cena sem nada atras do texto.
void LegendaCascata::paintEvent(QPaintEvent* ev) {
    QPainter pintor(this);
    QColor fundo = palette().window().color();
    fundo.setAlpha(ALPHA_FUNDO);
    pintor.fillRect(rect(), fundo);
    pintor.setPen(palette().mid().color());
    pintor.drawRect(rect().adjusted(0, 0, -1, -1));
    QFrame::paintEvent(ev);
}

// Quem ancora a legenda num canto precisa da largura e da altura dela, entao qualquer mudanca de
// tamanho tem de ser reanunciada: o layout pode redimensionar o quadro depois de a vista ja ter
// calculado a posicao pelo tamanho antigo.
void LegendaCascata::resizeEvent(QResizeEvent* ev) {
    QFrame::resizeEvent(ev);
    emit tamanhoAlterado();
}

// Uma linha por grupo, limitada a MAXIMO_LINHAS para a legenda nao cobrir a vista; o excedente vira
// uma linha "+N outros". So fica escondida quando nao ha nenhum grupo; com pelo menos uma linha ela
// se redimensiona, aparece e sobe para cima dos demais filhos do viewport. Quem a posiciona no canto
// e a vista, que e dona dela.
void LegendaCascata::definirGrupos(const std::vector<std::pair<QString, QColor>>& grupos) {
    while (QLayoutItem* item = layout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (grupos.empty()) {
        hide();
        return;
    }

    size_t mostradas = std::min(grupos.size(), MAXIMO_LINHAS);
    for (size_t i = 0; i < mostradas; ++i) {
        auto* linha = new QWidget(this);
        auto* layout_linha = new QHBoxLayout(linha);
        layout_linha->setContentsMargins(0, 0, 0, 0);
        layout_linha->setSpacing(6);

        QPixmap amostra(LADO_AMOSTRA, LADO_AMOSTRA);
        amostra.fill(grupos[i].second);
        auto* quadrado = new QLabel(linha);
        quadrado->setPixmap(amostra);
        quadrado->setFixedSize(LADO_AMOSTRA, LADO_AMOSTRA);

        layout_linha->addWidget(quadrado);
        layout_linha->addWidget(new QLabel(grupos[i].first, linha));
        layout_linha->addStretch(1);
        layout_->addWidget(linha);
    }
    if (grupos.size() > mostradas) {
        layout_->addWidget(
            new QLabel(QStringLiteral("+%1 outros").arg(static_cast<int>(grupos.size() - mostradas)), this));
    }

    // activate() em vez de adjustSize(): com SetFixedSize e ele que aplica o tamanho novo do quadro
    // na hora, em vez de deixar o redimensionamento para um LayoutRequest posterior, que chegaria
    // depois de quem ja tivesse posicionado a legenda pelo tamanho antigo.
    layout_->activate();
    show();
    raise();
}
