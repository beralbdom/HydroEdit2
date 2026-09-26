#include "estilo_arvore.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <algorithm>

namespace {
constexpr int LADO_ICONE = 16;
constexpr int FOLGA_LINHA = 6;

class DelegateLinhaAlta : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& opcao, const QModelIndex& ix) const override {
        return QStyledItemDelegate::sizeHint(opcao, ix) + QSize(0, FOLGA_LINHA);
    }
};

QPainterPath gota() {
    QPainterPath p;
    p.moveTo(8, 1.5);
    p.cubicTo(10, 5, 13, 8, 13, 10.5);
    p.cubicTo(13, 13.3, 10.8, 15, 8, 15);
    p.cubicTo(5.2, 15, 3, 13.3, 3, 10.5);
    p.cubicTo(3, 8, 6, 5, 8, 1.5);
    return p;
}

QPainterPath chama() {
    QPainterPath p;
    p.moveTo(8, 1.5);
    p.cubicTo(9, 4.5, 12.5, 6, 12.5, 10);
    p.cubicTo(12.5, 13, 10.5, 15, 8, 15);
    p.cubicTo(5.5, 15, 3.5, 13, 3.5, 10.5);
    p.cubicTo(3.5, 8.5, 5, 7.5, 5.5, 6);
    p.cubicTo(6.5, 7.5, 7, 8, 7.5, 8.5);
    p.cubicTo(8, 6, 7.5, 4, 8, 1.5);
    return p;
}
}  // namespace

// Cor do painel das abas no estilo Fusion, pela mesma conta do estilo (QFusionStylePrivate::
// tabFrameColor): a cor de botao clareada conforme a sua luminosidade, com a saturacao reduzida a
// 3/4, e depois mais 4%. Com o tema escuro do Windows da #4b4b4b, o cinza medido no painel.
QColor corPainelAbas(const QPalette& paleta) {
    QColor botao = paleta.button().color();
    const int cinza = qGray(botao.rgb());
    botao = botao.lighter(100 + std::max(1, (180 - cinza) / 6));
    botao.setHsv(botao.hue(), botao.saturation() * 3 / 4, botao.value());
    return botao.lighter(104);
}

// Icones monocromaticos de 16 px na cor dada, desenhados em codigo (o Qt estatico nao tem o modulo
// de SVG): gota para usinas hidroeletricas, chama para termoeletricas, grade para arquivo ou secao
// com tabela editavel e folha com linhas para arquivo que so tem previa. A escala e a da tela, para
// o traco sair nitido.
QIcon iconeArvore(IconeArvore tipo, const QColor& cor, qreal escala) {
    QPixmap pixmap(QSize(LADO_ICONE, LADO_ICONE) * escala);
    pixmap.setDevicePixelRatio(escala);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    QPen caneta(cor, 1.3);
    caneta.setJoinStyle(Qt::RoundJoin);
    caneta.setCapStyle(Qt::RoundCap);
    p.setPen(caneta);
    QColor preenchimento = cor;
    preenchimento.setAlpha(60);
    switch (tipo) {
        case IconeArvore::Hidro:
            p.setBrush(preenchimento);
            p.drawPath(gota());
            break;
        case IconeArvore::Termica:
            p.setBrush(preenchimento);
            p.drawPath(chama());
            break;
        case IconeArvore::Tabela:
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(2.5, 3, 11, 10), 1.5, 1.5);
            p.drawLine(QPointF(2.5, 6.3), QPointF(13.5, 6.3));
            p.drawLine(QPointF(2.5, 9.6), QPointF(13.5, 9.6));
            p.drawLine(QPointF(6.5, 3), QPointF(6.5, 13));
            break;
        case IconeArvore::Previa: {
            QPainterPath folha;
            folha.moveTo(3.5, 1.5);
            folha.lineTo(9.5, 1.5);
            folha.lineTo(12.5, 4.5);
            folha.lineTo(12.5, 14.5);
            folha.lineTo(3.5, 14.5);
            folha.closeSubpath();
            p.setBrush(Qt::NoBrush);
            p.drawPath(folha);
            p.drawLine(QPointF(5.5, 7.5), QPointF(10.5, 7.5));
            p.drawLine(QPointF(5.5, 10), QPointF(10.5, 10));
            p.drawLine(QPointF(5.5, 12.5), QPointF(9, 12.5));
            break;
        }
    }
    return QIcon(pixmap);
}

// Visual comum das arvores: fundo no cinza do painel das abas, linhas um pouco mais altas, recuo
// menor e selecao na linha inteira.
void estilizarArvore(QTreeWidget* arvore) {
    QPalette paleta = arvore->palette();
    paleta.setColor(QPalette::Base, corPainelAbas(paleta));
    arvore->setPalette(paleta);
    arvore->setItemDelegate(new DelegateLinhaAlta(arvore));
    arvore->setIndentation(14);
    arvore->setIconSize(QSize(LADO_ICONE, LADO_ICONE));
    arvore->setAllColumnsShowFocus(true);
}

// Cabecalho de grupo: texto em maiusculas, menor, em negrito e esmaecido, que nao pode ser
// selecionado.
QTreeWidgetItem* novoGrupoArvore(QTreeWidget* arvore, const QString& titulo) {
    auto* grupo = new QTreeWidgetItem(arvore, {titulo.toUpper()});
    grupo->setFlags(Qt::ItemIsEnabled);
    QFont fonte = arvore->font();
    fonte.setPointSizeF(fonte.pointSizeF() - 1.0);
    fonte.setBold(true);
    grupo->setFont(0, fonte);
    grupo->setForeground(0, arvore->palette().color(QPalette::Disabled, QPalette::WindowText));
    return grupo;
}
