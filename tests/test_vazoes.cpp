#include <QtTest>
#include <QTemporaryDir>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include "vazoes.h"

namespace fs = std::filesystem;

class TestVazoes : public QObject {
    Q_OBJECT

    static void gravarVazoes(const fs::path& p, int num_postos, const std::vector<std::vector<int32_t>>& meses) {
        std::ofstream f(p, std::ios::binary);
        for (const auto& mes : meses) {
            for (int posto = 0; posto < num_postos; ++posto) {
                int32_t v = mes[static_cast<size_t>(posto)];
                f.write(reinterpret_cast<const char*>(&v), 4);
            }
        }
    }

    static SerieVazoes serie(int num_postos, const std::vector<std::vector<int32_t>>& meses) {
        SerieVazoes s;
        s.ano_inicial = 1931;
        s.num_postos = num_postos;
        for (const auto& mes : meses) s.valores.insert(s.valores.end(), mes.begin(), mes.end());
        return s;
    }

    static UsinaHidr usina(const char* nome, int posto, int jusante) {
        UsinaHidr u;
        u.nome = nome;
        u.posto = posto;
        u.jusante = jusante;
        return u;
    }

    static const UsinaIncremental* achar(const Incrementais& inc, int codigo) {
        for (const auto& u : inc.usinas)
            if (u.codigo == codigo) return &u;
        return nullptr;
    }

private slots:
    void leSerieDoArquivo() {
        QTemporaryDir dir;
        fs::path p = fs::path(dir.path().toStdWString()) / "vazoes.dat";
        std::vector<std::vector<int32_t>> meses;
        for (int t = 0; t < 24; ++t) meses.push_back({t * 10 + 1, t * 10 + 2});
        gravarVazoes(p, 2, meses);
        ResultadoLeituraVazoes r = lerVazoesDat(p, 2, 1931);
        QVERIFY(r.erro.empty());
        QCOMPARE(r.serie.meses(), 24);
        QCOMPARE(r.serie.valor(2, 13), 132);
        QCOMPARE(r.serie.valor(1, 0), 1);
        QVERIFY(r.serie.postoValido(2));
        QVERIFY(!r.serie.postoValido(3));
        QVERIFY(!r.serie.postoValido(0));
    }
    void tamanhoQueNaoFechaORegistroEhErro() {
        QTemporaryDir dir;
        fs::path p = fs::path(dir.path().toStdWString()) / "vazoes.dat";
        std::ofstream(p, std::ios::binary) << "0123456789";
        ResultadoLeituraVazoes r = lerVazoesDat(p, 2, 1931);
        QVERIFY(!r.erro.empty());
        QCOMPARE(r.serie.meses(), 0);
    }
    void arquivoAusenteEhErro() {
        ResultadoLeituraVazoes r = lerVazoesDat("nao_existe/vazoes.dat", 320, 1931);
        QVERIFY(!r.erro.empty());
    }
    void incrementalSubtraiOsPostosImediatamenteAMontante() {
        std::vector<UsinaHidr> usinas = {usina("A", 1, 3), usina("B", 2, 3), usina("C", 3, 4), usina("D", 4, 0)};
        SerieVazoes s = serie(4, {{10, 20, 50, 80}, {5, 5, 40, 60}});
        Incrementais inc = calcularIncrementais(usinas, {}, s);
        QCOMPARE(inc.usinas.size(), static_cast<size_t>(4));
        QCOMPARE(achar(inc, 1)->vazoes, (std::vector<int32_t>{10, 5}));
        QCOMPARE(achar(inc, 3)->vazoes, (std::vector<int32_t>{20, 30}));
        QCOMPARE(achar(inc, 3)->postos_montante, (std::vector<int>{1, 2}));
        QCOMPARE(achar(inc, 4)->vazoes, (std::vector<int32_t>{30, 20}));
        QCOMPARE(achar(inc, 4)->postos_montante, (std::vector<int>{3}));
    }
    void negativoViraZeroEContaOMes() {
        std::vector<UsinaHidr> usinas = {usina("A", 1, 2), usina("B", 2, 0)};
        SerieVazoes s = serie(2, {{10, 30}, {50, 40}, {60, 20}});
        Incrementais inc = calcularIncrementais(usinas, {}, s);
        QCOMPARE(achar(inc, 2)->vazoes, (std::vector<int32_t>{20, 0, 0}));
        QCOMPARE(achar(inc, 2)->meses_truncados, 2);
        QCOMPARE(achar(inc, 1)->meses_truncados, 0);
    }
    void postoRepetidoAMontanteContaUmaVez() {
        std::vector<UsinaHidr> usinas = {usina("A", 1, 3), usina("B", 1, 3), usina("C", 2, 0)};
        SerieVazoes s = serie(2, {{10, 50}});
        Incrementais inc = calcularIncrementais(usinas, {}, s);
        QCOMPARE(achar(inc, 3)->postos_montante, (std::vector<int>{1}));
        QCOMPARE(achar(inc, 3)->vazoes, (std::vector<int32_t>{40}));
    }
    void confhdDefinePostoEJusanteDasUsinasDaConfiguracao() {
        std::vector<UsinaHidr> usinas = {usina("A", 1, 3), usina("B", 2, 0), usina("C", 3, 0), usina("D", 4, 3)};
        std::map<int, UsinaConfhd> confhd = {{1, {1, 2}}, {2, {2, 0}}, {3, {4, 0}}};
        SerieVazoes s = serie(4, {{10, 100, 30, 25}});
        Incrementais inc = calcularIncrementais(usinas, confhd, s);
        QCOMPARE(achar(inc, 2)->postos_montante, (std::vector<int>{1}));
        QCOMPARE(achar(inc, 2)->vazoes, (std::vector<int32_t>{90}));
        QCOMPARE(achar(inc, 3)->posto, 4);
        QVERIFY(achar(inc, 3)->postos_montante.empty());
        QCOMPARE(achar(inc, 3)->vazoes, (std::vector<int32_t>{25}));
        QCOMPARE(achar(inc, 4)->posto, 4);
        QVERIFY(achar(inc, 4)->postos_montante.empty());
    }
    void usinaForaDoConfhdUsaOHidr() {
        std::vector<UsinaHidr> usinas = {usina("A", 1, 2), usina("B", 2, 0), usina("E", 3, 2)};
        std::map<int, UsinaConfhd> confhd = {{1, {1, 2}}, {2, {2, 0}}};
        SerieVazoes s = serie(3, {{10, 100, 5}});
        Incrementais inc = calcularIncrementais(usinas, confhd, s);
        QCOMPARE(achar(inc, 2)->postos_montante, (std::vector<int>{1}));
        QCOMPARE(achar(inc, 3)->vazoes, (std::vector<int32_t>{5}));
    }
    void registroVazioFicaDeFora() {
        std::vector<UsinaHidr> usinas = {usina("A", 1, 0), UsinaHidr{}, usina("C", 2, 0)};
        SerieVazoes s = serie(2, {{10, 20}});
        Incrementais inc = calcularIncrementais(usinas, {}, s);
        QCOMPARE(inc.usinas.size(), static_cast<size_t>(2));
        QVERIFY(achar(inc, 2) == nullptr);
    }
    void postoForaDoArquivoGeraAviso() {
        std::vector<UsinaHidr> usinas = {usina("A", 9, 2), usina("B", 1, 0), usina("C", 0, 0)};
        SerieVazoes s = serie(2, {{10, 20}});
        Incrementais inc = calcularIncrementais(usinas, {}, s);
        QVERIFY(achar(inc, 1) == nullptr);
        QVERIFY(achar(inc, 3) == nullptr);
        QCOMPARE(achar(inc, 2)->vazoes, (std::vector<int32_t>{10}));
        QVERIFY(achar(inc, 2)->postos_montante.empty());
        QCOMPARE(inc.avisos.size(), static_cast<size_t>(3));
    }
    void csvTemUmaLinhaPorUsinaEAno() {
        std::vector<UsinaHidr> usinas = {usina("A,X", 1, 2), usina("B", 2, 0)};
        std::vector<std::vector<int32_t>> meses;
        for (int t = 0; t < 14; ++t) meses.push_back({t, 100});
        SerieVazoes s = serie(2, meses);
        Incrementais inc = calcularIncrementais(usinas, {}, s);
        QTemporaryDir dir;
        fs::path p = fs::path(dir.path().toStdWString()) / "inc.csv";
        QVERIFY(exportarIncrementaisCsv(inc, s, p).ok);
        std::ifstream f(p, std::ios::binary);
        std::stringstream conteudo;
        conteudo << f.rdbuf();
        std::string texto = conteudo.str();
        QVERIFY(texto.rfind("\xEF\xBB\xBF", 0) == 0);
        std::vector<std::string> linhas;
        std::stringstream ss(texto.substr(3));
        for (std::string l; std::getline(ss, l);) {
            if (!l.empty() && l.back() == '\r') l.pop_back();
            linhas.push_back(l);
        }
        QCOMPARE(linhas.size(), static_cast<size_t>(5));
        QCOMPARE(linhas[0], std::string("codigo,nome,posto,ano,jan,fev,mar,abr,mai,jun,jul,ago,set,out,nov,dez"));
        QCOMPARE(linhas[1], std::string("1,\"A,X\",1,1931,0,1,2,3,4,5,6,7,8,9,10,11"));
        QCOMPARE(linhas[2], std::string("1,\"A,X\",1,1932,12,13,,,,,,,,,,"));
        QCOMPARE(linhas[3], std::string("2,B,2,1931,100,99,98,97,96,95,94,93,92,91,90,89"));
    }
};

QTEST_GUILESS_MAIN(TestVazoes)
#include "test_vazoes.moc"
