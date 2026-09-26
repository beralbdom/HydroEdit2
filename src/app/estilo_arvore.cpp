#include "estilo_arvore.h"
#include <QApplication>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTreeWidget>
#include <algorithm>

namespace {
constexpr int MEIO_SETA = 5;

// Fusion com as linhas pontilhadas de hierarquia, que o Fusion nao desenha. A logica dos segmentos e
// a do estilo classico do Windows (QWindowsStyle, PE_IndicatorBranch): traco horizontal ate o item,
// vertical ate o meio da linha e, havendo irmao abaixo, ate o fim da celula; os tracos param na seta
// quando o item tem filhos. A seta continua sendo a do Fusion.
class EstiloArvore : public QProxyStyle {
public:
    EstiloArvore() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))) {}

    void drawPrimitive(PrimitiveElement elemento, const QStyleOption* opcao, QPainter* pintor, const QWidget* widget) const override {
        if (elemento != PE_IndicatorBranch) {
            QProxyStyle::drawPrimitive(elemento, opcao, pintor, widget);
            return;
        }
        const QRect r = opcao->rect;
        const int meio_h = r.x() + r.width() / 2;
        const int meio_v = r.y() + r.height() / 2;
        int antes_v = meio_v;
        int depois_h = meio_h;
        int depois_v = meio_v;
        if (opcao->state & State_Children) {
            antes_v -= MEIO_SETA;
            depois_h += MEIO_SETA;
            depois_v += MEIO_SETA;
        }
        pintor->save();
        QPen caneta(opcao->palette.color(QPalette::Disabled, QPalette::Text), 1, Qt::DotLine);
        caneta.setCosmetic(true);
        pintor->setPen(caneta);
        if (opcao->state & State_Item) pintor->drawLine(depois_h, meio_v, r.right(), meio_v);
        if (opcao->state & State_Sibling) pintor->drawLine(meio_h, depois_v, meio_h, r.bottom());
        if (opcao->state & (State_Open | State_Children | State_Item | State_Sibling)) pintor->drawLine(meio_h, r.y(), meio_h, antes_v);
        pintor->restore();
        if (opcao->state & State_Children) QProxyStyle::drawPrimitive(elemento, opcao, pintor, widget);
    }
};

QStyle* estiloArvore() {
    static EstiloArvore* estilo = [] {
        auto* e = new EstiloArvore;
        e->setParent(qApp);
        return e;
    }();
    return estilo;
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

// Visual comum das arvores: fundo no cinza do painel das abas, linhas pontilhadas de hierarquia com
// a seta de expandir tambem no primeiro nivel e selecao na linha inteira.
void estilizarArvore(QTreeWidget* arvore) {
    QPalette paleta = arvore->palette();
    paleta.setColor(QPalette::Base, corPainelAbas(paleta));
    arvore->setPalette(paleta);
    arvore->setStyle(estiloArvore());
    arvore->setRootIsDecorated(true);
    arvore->setAllColumnsShowFocus(true);
}

// Cabecalho de grupo: item comum, que expande e recolhe, mas nao pode ser selecionado.
QTreeWidgetItem* novoGrupoArvore(QTreeWidget* arvore, const QString& titulo) {
    auto* grupo = new QTreeWidgetItem(arvore, {titulo});
    grupo->setFlags(Qt::ItemIsEnabled);
    return grupo;
}
