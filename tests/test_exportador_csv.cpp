#include <QtTest>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include "arquivo_hidr.h"
#include "campos.h"
#include "exportador_csv.h"

namespace fs = std::filesystem;

class TestExportadorCsv : public QObject {
    Q_OBJECT
private slots:
    void cabecalhoComecaComCodigoETemTodasAsColunas() {
        std::string h = cabecalhoCsv(OpcoesCsv{});
        QVERIFY(h.rfind("codigo;nome;posto;posto_bdh;subsistema;", 0) == 0);
        QVERIFY(h.find(";pol_cota_vol_a0;") != std::string::npos);
        QVERIFY(h.find(";evap_dez;") != std::string::npos);
        QVERIFY(h.find(";conj5_potp_a4;") != std::string::npos);
        QVERIFY(h.find(";pol_jus6_ref;") != std::string::npos);
        QVERIFY(h.find(";regulacao") == h.size() - 10);
        int colunas = 1;
        for (const Campo& c : campos()) colunas += c.n;
        QCOMPARE(static_cast<int>(std::count(h.begin(), h.end(), ';')), colunas - 1);
    }
    void linhaDeUsinaSimples() {
        UsinaHidr u;
        u.nome = "FURNAS";
        u.posto = 6;
        u.volume_minimo = 5733.0f;
        u.regulacao = "M";
        std::string l = linhaCsv(6, u, OpcoesCsv{});
        QVERIFY(l.rfind("6;FURNAS;6;;0;0;0;0;5733;", 0) == 0);
        QVERIFY(l.size() > 10 && l.substr(l.size() - 2) == ";M");
    }
    void virgulaDecimalEOutroSeparador() {
        UsinaHidr u;
        u.nome = "X";
        u.volume_minimo = 12.5f;
        OpcoesCsv o;
        o.separador = '\t';
        o.virgula_decimal = true;
        std::string l = linhaCsv(1, u, o);
        QVERIFY(l.find("\t12,5\t") != std::string::npos);
        QVERIFY(l.find(';') == std::string::npos);
    }
    void nomeLatin1ViraUtf8() {
        UsinaHidr u;
        u.nome = "S\xC3O JO\xC3O";
        std::string l = linhaCsv(1, u, OpcoesCsv{});
        QVERIFY(l.find("S\xC3\x83O JO\xC3\x83O") != std::string::npos);
    }
    void exportaSoUsinasNaoVaziasComBom() {
        ArquivoHidr a;
        a.usinas.resize(3);
        a.usinas[2].nome = "ULTIMA";
        fs::path p = fs::temp_directory_path() / "hydroedit_export.csv";
        QVERIFY(exportarCsv(a, p, OpcoesCsv{}).ok);
        std::ifstream f(p, std::ios::binary);
        std::string conteudo((std::istreambuf_iterator<char>(f)), {});
        f.close();
        QVERIFY(conteudo.rfind("\xEF\xBB\xBF" "codigo;", 0) == 0);
        QCOMPARE(std::count(conteudo.begin(), conteudo.end(), '\n'), static_cast<long long>(2));
        QVERIFY(conteudo.find("\r\n3;ULTIMA;") != std::string::npos);
        fs::remove(p);
    }
};
QTEST_APPLESS_MAIN(TestExportadorCsv)
#include "test_exportador_csv.moc"
