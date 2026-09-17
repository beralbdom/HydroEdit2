#include <QApplication>
#include <QLabel>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("HydroEdit"));
    QApplication::setApplicationVersion(QStringLiteral("5.0.0"));
    QLabel rotulo(QStringLiteral("HydroEdit 5"));
    rotulo.show();
    return app.exec();
}
