#include <QApplication>
#include <QIcon>
#include <QPalette>
#include "janela_principal.h"

// O Fusion no modo escuro do Windows usa azul nas linhas alternadas; troca por um cinza proximo do fundo.
static void ajustarLinhasAlternadas(QApplication& app) {
    QPalette pal = app.palette();
    QColor base = pal.color(QPalette::Base);
    pal.setColor(QPalette::AlternateBase, base.lightness() < 128 ? base.lighter(130) : base.darker(104));
    app.setPalette(pal);
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    ajustarLinhasAlternadas(app);
    app.setWindowIcon(QIcon(QStringLiteral(":/hidro.ico")));
    QApplication::setOrganizationName(QStringLiteral("HydroEdit2"));
    QApplication::setApplicationName(QStringLiteral("HydroEdit2"));
    QApplication::setApplicationVersion(QStringLiteral("0.1"));
    JanelaPrincipal janela;
    janela.resize(838, 500);
    janela.show();
    if (argc > 1) janela.abrirCaminho(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}
