#include "botao_combo.h"
#include <QStylePainter>

// Botao de menu que o estilo desenha como uma caixa de selecao nao editavel: texto a esquerda e a
// seta propria do estilo a direita, em vez da seta pequena de canto que o QToolButton usa no modo
// InstantPopup. Continua sendo um QToolButton, entao o menu de multipla escolha funciona igual.
BotaoCombo::BotaoCombo(QWidget* parent) : QToolButton(parent) {
    setPopupMode(QToolButton::InstantPopup);
    setAttribute(Qt::WA_Hover);
}

QStyleOptionComboBox BotaoCombo::opcaoCombo() const {
    QStyleOptionComboBox opcao;
    opcao.initFrom(this);
    opcao.editable = false;
    opcao.frame = true;
    opcao.currentText = text();
    if (isDown()) opcao.state |= QStyle::State_On | QStyle::State_Sunken;
    return opcao;
}

QSize BotaoCombo::sizeHint() const {
    QStyleOptionComboBox opcao = opcaoCombo();
    QSize texto(fontMetrics().horizontalAdvance(text()), fontMetrics().height());
    return style()->sizeFromContents(QStyle::CT_ComboBox, &opcao, texto, this);
}

QSize BotaoCombo::minimumSizeHint() const { return sizeHint(); }

void BotaoCombo::paintEvent(QPaintEvent*) {
    QStylePainter pintor(this);
    QStyleOptionComboBox opcao = opcaoCombo();
    pintor.drawComplexControl(QStyle::CC_ComboBox, opcao);
    pintor.drawControl(QStyle::CE_ComboBoxLabel, opcao);
}
