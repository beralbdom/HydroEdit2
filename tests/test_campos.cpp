#include <QtTest>
#include "usina_hidr.h"

class TestCampos : public QObject {
    Q_OBJECT
private slots:
    void usinaNovaEhVazia() {
        UsinaHidr u;
        QVERIFY(u.vazia());
        u.nome = "FURNAS";
        QVERIFY(!u.vazia());
        u.nome = "    ";
        QVERIFY(u.vazia());
    }
};
QTEST_APPLESS_MAIN(TestCampos)
#include "test_campos.moc"
