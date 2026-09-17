#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "formulario_usina.h"
#include "grade_vetor.h"
#include "modelo_hidr.h"

namespace {
QStringList coeficientes() { return {"A0", "A1", "A2", "A3", "A4"}; }
}  // namespace

QWidget* FormularioUsina::criarPaginaCadastro() {
    auto* pagina = new QWidget(this);
    auto* v = novaPagina(pagina);

    auto* linha1 = novaLinha();
    QGroupBox* identificacao = novoGrupo(pagina, QStringLiteral("Identificação"));
    auto* fi = novoForm(identificacao);
    ligarEdit(fi, kCadastro, QStringLiteral("Nome"), "nome");
    ligarEdit(fi, kCadastro, QStringLiteral("Posto"), "posto", true);
    ligarEdit(fi, kCadastro, QStringLiteral("Posto BDH"), "posto_bdh");
    QComboBox* reg = ligarCombo(fi, kCadastro, QStringLiteral("Regulação"), "regulacao");
    reg->addItem(QStringLiteral("M  Mensal"), QStringLiteral("M"));
    reg->addItem(QStringLiteral("S  Semanal"), QStringLiteral("S"));
    reg->addItem(QStringLiteral("D  Diária"), QStringLiteral("D"));
    ligarEdit(fi, kCadastro, QStringLiteral("Data"), "data");
    linha1->addWidget(identificacao);

    QGroupBox* vinculos = novoGrupo(pagina, QStringLiteral("Vínculos"));
    auto* fv = novoForm(vinculos);
    ligarCombo(fv, kCadastro, QStringLiteral("Subsistema"), "subsistema");
    ligarEdit(fv, kCadastro, QStringLiteral("Empresa"), "empresa", true);
    ligarCombo(fv, kCadastro, QStringLiteral("Jusante"), "jusante");
    ligarCombo(fv, kCadastro, QStringLiteral("Desvio"), "desvio");
    linha1->addWidget(vinculos);
    linha1->addStretch(1);
    v->addLayout(linha1);

    QGroupBox* obs = novoGrupo(pagina, QStringLiteral("Observação"));
    auto* fo = novoForm(obs);
    ligarEdit(fo, kCadastro, QStringLiteral("Observação"), "observacao");
    v->addWidget(obs);

    v->addStretch(1);
    return pagina;
}

QWidget* FormularioUsina::criarPaginaReservatorio() {
    auto* pagina = new QWidget(this);
    auto* v = novaPagina(pagina);

    auto* linha1 = novaLinha();
    QGroupBox* volumes = novoGrupo(pagina, QStringLiteral("Volumes (hm³)"));
    auto* fv = novoForm(volumes);
    ligarEdit(fv, kReservatorio, QStringLiteral("Mínimo"), "volume_minimo");
    ligarEdit(fv, kReservatorio, QStringLiteral("Máximo"), "volume_maximo");
    ligarEdit(fv, kReservatorio, QStringLiteral("Referência"), "volume_referencia");
    ligarEdit(fv, kReservatorio, QStringLiteral("Crista do vertedouro"), "volume_vertedouro");
    ligarEdit(fv, kReservatorio, QStringLiteral("Canal de desvio"), "volume_desvio");
    linha1->addWidget(volumes);

    QGroupBox* cotas = novoGrupo(pagina, QStringLiteral("Cotas (m)"));
    auto* fc = novoForm(cotas);
    ligarEdit(fc, kReservatorio, QStringLiteral("Mínima"), "cota_minima");
    ligarEdit(fc, kReservatorio, QStringLiteral("Máxima"), "cota_maxima");
    linha1->addWidget(cotas);
    linha1->addStretch(1);
    v->addLayout(linha1);

    QGroupBox* evaporacao = novoGrupo(pagina, QStringLiteral("Evaporação mensal (mm/mês)"));
    auto* ve = novoConteudo(evaporacao);
    QStringList meses = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};
    auto* evap = new GradeVetor(modelo_, 1, 12, meses, {}, evaporacao);
    for (int m = 0; m < 12; ++m) evap->definirCelula(0, m, campo("evaporacao"), m);
    ve->addWidget(evap);
    grades_.push_back(evap);
    registrarCampoGrade(kReservatorio, "evaporacao");
    v->addWidget(evaporacao);

    v->addStretch(1);
    return pagina;
}

QWidget* FormularioUsina::criarPaginaPolinomios() {
    auto* pagina = new QWidget(this);
    auto* v = novaPagina(pagina);

    QGroupBox* g = novoGrupo(pagina, QStringLiteral("Cota x Volume / Área x Cota"));
    auto* vg = novoConteudo(g);
    auto* pol = new GradeVetor(modelo_, 2, 5, coeficientes(), {QStringLiteral("Cota x Volume"), QStringLiteral("Área x Cota")}, g);
    for (int k = 0; k < 5; ++k) {
        pol->definirCelula(0, k, campo("pol_cota_volume"), k);
        pol->definirCelula(1, k, campo("pol_area_cota"), k);
    }
    vg->addWidget(pol);
    grades_.push_back(pol);
    registrarCampoGrade(kPolinomios, "pol_cota_volume");
    registrarCampoGrade(kPolinomios, "pol_area_cota");
    v->addWidget(g);

    v->addStretch(1);
    return pagina;
}

QWidget* FormularioUsina::criarPaginaConjuntos() {
    auto* pagina = new QWidget(this);
    auto* v = novaPagina(pagina);

    QGroupBox* g = novoGrupo(pagina, QStringLiteral("Configuração"));
    auto* f = novoForm(g);
    ligarEdit(f, kConjuntos, QStringLiteral("Número de conjuntos"), "num_conjuntos");
    ligarEdit(f, kConjuntos, QStringLiteral("Unidades de base"), "num_unidades_base");
    ligarEdit(f, kConjuntos, QStringLiteral("Tipo de turbina"), "tipo_turbina", true);
    QComboBox* repr = ligarCombo(f, kConjuntos, QStringLiteral("Representação do conjunto"), "representacao_conjunto");
    repr->addItem(QStringLiteral("0  Aproximada"), 0);
    repr->addItem(QStringLiteral("1  Detalhada"), 1);
    repr->addItem(QStringLiteral("2  Simplificada"), 2);
    v->addWidget(g);

    QGroupBox* maquinas = novoGrupo(pagina, QStringLiteral("Conjuntos de máquinas"));
    auto* vm = novoConteudo(maquinas);
    QStringList conj;
    for (int i = 1; i <= 5; ++i) conj << QStringLiteral("Conj. %1").arg(i);
    auto* conjuntos = new GradeVetor(modelo_, 5, 4,
                                     {QStringLiteral("Máquinas"), QStringLiteral("Pot. ef. (MW)"), QStringLiteral("Q ef. (m³/s)"), QStringLiteral("H ef. (m)")},
                                     conj, maquinas);
    for (int i = 0; i < 5; ++i) {
        conjuntos->definirCelula(i, 0, campo("num_maquinas"), i);
        conjuntos->definirCelula(i, 1, campo("potencia_efetiva"), i);
        conjuntos->definirCelula(i, 2, campo("vazao_efetiva"), i);
        conjuntos->definirCelula(i, 3, campo("altura_efetiva"), i);
    }
    vm->addWidget(conjuntos);
    grades_.push_back(conjuntos);
    registrarCampoGrade(kConjuntos, "num_maquinas");
    registrarCampoGrade(kConjuntos, "potencia_efetiva");
    registrarCampoGrade(kConjuntos, "vazao_efetiva");
    registrarCampoGrade(kConjuntos, "altura_efetiva");
    v->addWidget(maquinas);

    QGroupBox* polConjunto = novoGrupo(pagina, QStringLiteral("Polinômios turbina / gerador / potência"));
    auto* vp = novoConteudo(polConjunto);
    QStringList linhas_pc;
    static const char* pol[] = {"turbina", "gerador", "potência"};
    for (int c = 0; c < 5; ++c)
        for (int p = 0; p < 3; ++p) linhas_pc << QStringLiteral("C%1 %2").arg(c + 1).arg(QString::fromUtf8(pol[p]));
    auto* pc = new GradeVetor(modelo_, 15, 5, coeficientes(), linhas_pc, polConjunto);
    for (int i = 0; i < 75; ++i) pc->definirCelula(i / 5, i % 5, campo("pol_conjunto"), i);
    vp->addWidget(pc);
    grades_.push_back(pc);
    registrarCampoGrade(kConjuntos, "pol_conjunto");
    v->addWidget(polConjunto);

    v->addStretch(1);
    return pagina;
}

QWidget* FormularioUsina::criarPaginaJusante() {
    auto* pagina = new QWidget(this);
    auto* v = novaPagina(pagina);

    QGroupBox* g = novoGrupo(pagina, QStringLiteral("Canal de fuga"));
    auto* f = novoForm(g);
    ligarEdit(f, kJusante, QStringLiteral("Número de polinômios"), "num_pol_jusante");
    ligarEdit(f, kJusante, QStringLiteral("Canal de fuga médio (m)"), "canal_fuga_medio");
    QComboBox* infl = ligarCombo(f, kJusante, QStringLiteral("Influência do vertimento"), "influencia_vertimento");
    infl->addItem(QStringLiteral("0  Não"), 0);
    infl->addItem(QStringLiteral("1  Sim"), 1);
    v->addWidget(g);

    QGroupBox* polJusante = novoGrupo(pagina, QStringLiteral("Polinômios de jusante"));
    auto* vp = novoConteudo(polJusante);
    QStringList linhas_pj;
    for (int j = 1; j <= 6; ++j) linhas_pj << QStringLiteral("Pol. %1").arg(j);
    auto* pj = new GradeVetor(modelo_, 6, 6, coeficientes() << QStringLiteral("Ref. (m)"), linhas_pj, polJusante);
    for (int j = 0; j < 6; ++j) {
        for (int k = 0; k < 5; ++k) pj->definirCelula(j, k, campo("pol_jusante"), j * 5 + k);
        pj->definirCelula(j, 5, campo("ref_pol_jusante"), j);
    }
    vp->addWidget(pj);
    grades_.push_back(pj);
    registrarCampoGrade(kJusante, "pol_jusante");
    registrarCampoGrade(kJusante, "ref_pol_jusante");
    v->addWidget(polJusante);

    v->addStretch(1);
    return pagina;
}

QWidget* FormularioUsina::criarPaginaOperacao() {
    auto* pagina = new QWidget(this);
    auto* v = novaPagina(pagina);

    auto* linha1 = novaLinha();
    QGroupBox* producao = novoGrupo(pagina, QStringLiteral("Produção"));
    auto* fp = novoForm(producao);
    ligarEdit(fp, kOperacao, QStringLiteral("Produtibilidade específica (MW/m³/s/m)"), "produtibilidade");
    ligarEdit(fp, kOperacao, QStringLiteral("Perdas"), "perdas");
    ligarEdit(fp, kOperacao, QStringLiteral("Tipo de perda"), "tipo_perda");
    linha1->addWidget(producao);

    QGroupBox* fatores = novoGrupo(pagina, QStringLiteral("Fatores de carga (%)"));
    auto* ff = novoForm(fatores);
    ligarEdit(ff, kOperacao, QStringLiteral("Máximo"), "fator_carga_maximo");
    ligarEdit(ff, kOperacao, QStringLiteral("Mínimo"), "fator_carga_minimo");
    linha1->addWidget(fatores);
    linha1->addStretch(1);
    v->addLayout(linha1);

    QGroupBox* indisp = novoGrupo(pagina, QStringLiteral("Indisponibilidade e vazão"));
    auto* fi = novoForm(indisp);
    ligarEdit(fi, kOperacao, QStringLiteral("TEIF (%)"), "teif");
    ligarEdit(fi, kOperacao, QStringLiteral("IP (%)"), "ip");
    ligarEdit(fi, kOperacao, QStringLiteral("Vazão mínima do histórico (m³/s)"), "vazao_minima_historica");
    v->addWidget(indisp);

    v->addStretch(1);
    return pagina;
}
