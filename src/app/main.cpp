#include <QApplication>
#include <QIcon>
#include <QPalette>
#include <QProxyStyle>
#include "janela_principal.h"

// O Fusion no modo escuro do Windows usa azul nas linhas alternadas; troca por um cinza proximo do fundo.
static void ajustarLinhasAlternadas(QApplication& app) {
    QPalette pal = app.palette();
    QColor base = pal.color(QPalette::Base);
    pal.setColor(QPalette::AlternateBase, base.lightness() < 128 ? base.lighter(130) : base.darker(104));
    app.setPalette(pal);
}

// Fusion com o icone das janelas de aviso (informacao, alerta, erro, pergunta) menor que o padrao.
class EstiloAplicacao : public QProxyStyle {
public:
    EstiloAplicacao() : QProxyStyle(QStringLiteral("Fusion")) {}
    int pixelMetric(PixelMetric metrica, const QStyleOption* opcao, const QWidget* widget) const override {
        if (metrica == PM_MessageBoxIconSize) return 24;
        return QProxyStyle::pixelMetric(metrica, opcao, widget);
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle(new EstiloAplicacao);
    ajustarLinhasAlternadas(app);
    app.setWindowIcon(QIcon(QStringLiteral(":/hidr.ico")));
    QApplication::setOrganizationName(QStringLiteral("HydroEdit 2"));
    QApplication::setApplicationName(QStringLiteral("HydroEdit 2"));
    QApplication::setApplicationVersion(QStringLiteral("0.7"));
    JanelaPrincipal janela;
    janela.resize(820, 500);
    janela.show();
    if (argc > 1) janela.abrirCaminho(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}
