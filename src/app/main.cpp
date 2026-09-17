#include <QApplication>
#include "janela_principal.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("HydroEdit"));
    QApplication::setApplicationName(QStringLiteral("HydroEdit"));
    QApplication::setApplicationVersion(QStringLiteral("5.0.0"));
    JanelaPrincipal janela;
    janela.show();
    if (argc > 1) janela.abrirCaminho(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}
