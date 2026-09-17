#include <QtTest>
#include "grafico_polinomio.h"

class TestGraficoPolinomio : public QObject {
    Q_OBJECT
private slots:
    void constante() {
        QCOMPARE(avaliarPolinomio({5.0f, 0.0f, 0.0f, 0.0f, 0.0f}, 3.0), 5.0);
    }
    void linear() {
        QCOMPARE(avaliarPolinomio({1.0f, 2.0f, 0.0f, 0.0f, 0.0f}, 3.0), 7.0);
    }
    void grau4() {
        QCOMPARE(avaliarPolinomio({1.0f, 1.0f, 1.0f, 1.0f, 1.0f}, 2.0), 31.0);
    }
};
QTEST_APPLESS_MAIN(TestGraficoPolinomio)
#include "test_grafico_polinomio.moc"
