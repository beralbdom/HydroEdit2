#include "formulario_usina.h"
#include <QComboBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>
#include "grade_vetor.h"
#include "modelo_hidr.h"

namespace {
QStringList coeficientes() { return {"A0", "A1", "A2", "A3", "A4"}; }
}  // namespace

FormularioUsina::FormularioUsina(ModeloHidr* modelo, QWidget* parent) : QWidget(parent), modelo_(modelo) {
    auto* externo = new QVBoxLayout(this);
    externo->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    externo->addWidget(scroll);
    auto* conteudo = new QWidget(scroll);
    auto* layout = new QVBoxLayout(conteudo);
    titulo_ = new QLabel(conteudo);
    QFont f = titulo_->font();
    f.setPointSize(f.pointSize() + 3);
    f.setBold(true);
    titulo_->setFont(f);
    layout->addWidget(titulo_);
    layout->addWidget(criarCadastro());
    layout->addWidget(criarReservatorio());
    layout->addWidget(criarUsina());
    layout->addStretch(1);
    scroll->setWidget(conteudo);

    connect(modelo_, &ModeloHidr::usinaAlterada, this, [this](int linha, const Campo* c) {
        if (!c || c->nome == "nome") recarregarListas();
        if (linha == linha_) atualizar();
    });
    connect(modelo_, &QAbstractItemModel::modelReset, this, [this] {
        recarregarListas();
        definirLinha(-1);
    });
    recarregarListas();
    definirLinha(-1);
}

QLineEdit* FormularioUsina::ligarEdit(QFormLayout* f, const QString& rotulo, const char* nome, bool com_lookup) {
    const Campo* c = campo(nome);
    auto* e = new QLineEdit(this);
    if (c->tipo == TipoCampo::Inteiro) {
        e->setValidator(new QIntValidator(e));
    } else if (c->tipo == TipoCampo::Real) {
        auto* v = new QDoubleValidator(e);
        v->setNotation(QDoubleValidator::ScientificNotation);
        v->setLocale(QLocale::c());
        e->setValidator(v);
    } else {
        e->setMaxLength(c->tamanho_elemento);
    }
    QLabel* lookup = nullptr;
    if (com_lookup) {
        auto* linha = new QWidget(this);
        auto* h = new QHBoxLayout(linha);
        h->setContentsMargins(0, 0, 0, 0);
        lookup = new QLabel(linha);
        h->addWidget(e, 1);
        h->addWidget(lookup, 1);
        f->addRow(rotulo, linha);
    } else {
        f->addRow(rotulo, e);
    }
    edits_[nome] = {e, c, lookup};
    connect(e, &QLineEdit::editingFinished, this, [this, nome] { aoEditarEdit(nome); });
    return e;
}

QComboBox* FormularioUsina::ligarCombo(QFormLayout* f, const QString& rotulo, const char* nome) {
    auto* cb = new QComboBox(this);
    f->addRow(rotulo, cb);
    combos_[nome] = {cb, campo(nome)};
    connect(cb, &QComboBox::activated, this, [this, nome](int i) { aoEscolherCombo(nome, i); });
    return cb;
}

QGroupBox* FormularioUsina::criarCadastro() {
    auto* g = new QGroupBox(QStringLiteral("Cadastro"), this);
    auto* f = new QFormLayout(g);
    ligarEdit(f, QStringLiteral("Nome"), "nome");
    ligarEdit(f, QStringLiteral("Posto"), "posto", true);
    ligarEdit(f, QStringLiteral("Posto BDH"), "posto_bdh");
    ligarCombo(f, QStringLiteral("Subsistema"), "subsistema");
    ligarEdit(f, QStringLiteral("Empresa"), "empresa", true);
    ligarCombo(f, QStringLiteral("Jusante"), "jusante");
    ligarCombo(f, QStringLiteral("Desvio"), "desvio");
    ligarEdit(f, QStringLiteral("Data"), "data");
    ligarEdit(f, QStringLiteral("Observação"), "observacao");
    return g;
}

QGroupBox* FormularioUsina::criarReservatorio() {
    auto* g = new QGroupBox(QStringLiteral("Reservatório"), this);
    auto* v = new QVBoxLayout(g);
    auto* f = new QFormLayout;
    QComboBox* reg = ligarCombo(f, QStringLiteral("Regulação"), "regulacao");
    reg->addItem(QStringLiteral("M  Mensal"), QStringLiteral("M"));
    reg->addItem(QStringLiteral("S  Semanal"), QStringLiteral("S"));
    reg->addItem(QStringLiteral("D  Diária"), QStringLiteral("D"));
    ligarEdit(f, QStringLiteral("Volume mínimo (hm³)"), "volume_minimo");
    ligarEdit(f, QStringLiteral("Volume máximo (hm³)"), "volume_maximo");
    ligarEdit(f, QStringLiteral("Volume de referência (hm³)"), "volume_referencia");
    ligarEdit(f, QStringLiteral("Volume crista vertedouro (hm³)"), "volume_vertedouro");
    ligarEdit(f, QStringLiteral("Volume canal de desvio (hm³)"), "volume_desvio");
    ligarEdit(f, QStringLiteral("Cota mínima (m)"), "cota_minima");
    ligarEdit(f, QStringLiteral("Cota máxima (m)"), "cota_maxima");
    v->addLayout(f);

    auto* pol = new GradeVetor(modelo_, 2, 5, coeficientes(), {QStringLiteral("Cota x Volume"), QStringLiteral("Área x Cota")}, g);
    for (int k = 0; k < 5; ++k) {
        pol->definirCelula(0, k, campo("pol_cota_volume"), k);
        pol->definirCelula(1, k, campo("pol_area_cota"), k);
    }
    v->addWidget(new QLabel(QStringLiteral("Polinômios"), g));
    v->addWidget(pol);
    grades_.push_back(pol);

    QStringList meses = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};
    auto* evap = new GradeVetor(modelo_, 1, 12, meses, {}, g);
    for (int m = 0; m < 12; ++m) evap->definirCelula(0, m, campo("evaporacao"), m);
    v->addWidget(new QLabel(QStringLiteral("Evaporação mensal (mm/mês)"), g));
    v->addWidget(evap);
    grades_.push_back(evap);
    return g;
}

QGroupBox* FormularioUsina::criarUsina() {
    auto* g = new QGroupBox(QStringLiteral("Usina"), this);
    auto* v = new QVBoxLayout(g);
    auto* f = new QFormLayout;
    ligarEdit(f, QStringLiteral("Produtibilidade específica (MW/m³/s/m)"), "produtibilidade");
    ligarEdit(f, QStringLiteral("Perdas"), "perdas");
    ligarEdit(f, QStringLiteral("Tipo de perda"), "tipo_perda");
    ligarEdit(f, QStringLiteral("Canal de fuga médio (m)"), "canal_fuga_medio");
    QComboBox* infl = ligarCombo(f, QStringLiteral("Influência do vertimento no canal de fuga"), "influencia_vertimento");
    infl->addItem(QStringLiteral("0  Não"), 0);
    infl->addItem(QStringLiteral("1  Sim"), 1);
    ligarEdit(f, QStringLiteral("Fator de carga máximo (%)"), "fator_carga_maximo");
    ligarEdit(f, QStringLiteral("Fator de carga mínimo (%)"), "fator_carga_minimo");
    ligarEdit(f, QStringLiteral("TEIF (%)"), "teif");
    ligarEdit(f, QStringLiteral("IP (%)"), "ip");
    ligarEdit(f, QStringLiteral("Vazão mínima do histórico (m³/s)"), "vazao_minima_historica");
    ligarEdit(f, QStringLiteral("Unidades de base"), "num_unidades_base");
    ligarEdit(f, QStringLiteral("Tipo de turbina"), "tipo_turbina", true);
    QComboBox* repr = ligarCombo(f, QStringLiteral("Representação do conjunto"), "representacao_conjunto");
    repr->addItem(QStringLiteral("0  Aproximada"), 0);
    repr->addItem(QStringLiteral("1  Detalhada"), 1);
    repr->addItem(QStringLiteral("2  Simplificada"), 2);
    ligarEdit(f, QStringLiteral("Número de conjuntos"), "num_conjuntos");
    ligarEdit(f, QStringLiteral("Número de polinômios de jusante"), "num_pol_jusante");
    v->addLayout(f);

    QStringList conj;
    for (int i = 1; i <= 5; ++i) conj << QStringLiteral("Conj. %1").arg(i);
    auto* conjuntos = new GradeVetor(modelo_, 5, 4, {QStringLiteral("Máquinas"), QStringLiteral("Pot. ef. (MW)"), QStringLiteral("Q ef. (m³/s)"), QStringLiteral("H ef. (m)")}, conj, g);
    for (int i = 0; i < 5; ++i) {
        conjuntos->definirCelula(i, 0, campo("num_maquinas"), i);
        conjuntos->definirCelula(i, 1, campo("potencia_efetiva"), i);
        conjuntos->definirCelula(i, 2, campo("vazao_efetiva"), i);
        conjuntos->definirCelula(i, 3, campo("altura_efetiva"), i);
    }
    v->addWidget(new QLabel(QStringLiteral("Conjuntos de máquinas"), g));
    v->addWidget(conjuntos);
    grades_.push_back(conjuntos);

    QStringList linhas_pc;
    static const char* pol[] = {"turbina", "gerador", "potência"};
    for (int c = 0; c < 5; ++c)
        for (int p = 0; p < 3; ++p) linhas_pc << QStringLiteral("C%1 %2").arg(c + 1).arg(QString::fromUtf8(pol[p]));
    auto* pc = new GradeVetor(modelo_, 15, 5, coeficientes(), linhas_pc, g);
    for (int i = 0; i < 75; ++i) pc->definirCelula(i / 5, i % 5, campo("pol_conjunto"), i);
    v->addWidget(new QLabel(QStringLiteral("Polinômios turbina / gerador / potência por conjunto"), g));
    v->addWidget(pc);
    grades_.push_back(pc);

    QStringList linhas_pj;
    for (int j = 1; j <= 6; ++j) linhas_pj << QStringLiteral("Pol. %1").arg(j);
    auto* pj = new GradeVetor(modelo_, 6, 6, coeficientes() << QStringLiteral("Ref. (m)"), linhas_pj, g);
    for (int j = 0; j < 6; ++j) {
        for (int k = 0; k < 5; ++k) pj->definirCelula(j, k, campo("pol_jusante"), j * 5 + k);
        pj->definirCelula(j, 5, campo("ref_pol_jusante"), j);
    }
    v->addWidget(new QLabel(QStringLiteral("Polinômios de jusante"), g));
    v->addWidget(pj);
    grades_.push_back(pj);
    return g;
}

void FormularioUsina::recarregarListas() {
    atualizando_ = true;
    QComboBox* sub = combos_["subsistema"].widget;
    sub->clear();
    for (const auto& [codigo, nome] : modelo_->lookup().subsistemas)
        sub->addItem(QStringLiteral("%1  %2").arg(codigo).arg(QString::fromLatin1(nome.c_str())), codigo);
    for (const char* nome : {"jusante", "desvio"}) {
        QComboBox* cb = combos_[nome].widget;
        cb->clear();
        cb->addItem(QStringLiteral("0  (nenhuma)"), 0);
        for (int i = 0; i < modelo_->numUsinas(); ++i)
            if (!modelo_->usina(i).vazia())
                cb->addItem(QStringLiteral("%1  %2").arg(i + 1).arg(QString::fromLatin1(modelo_->usina(i).nome.c_str())), i + 1);
    }
    atualizando_ = false;
}

void FormularioUsina::definirLinha(int linha) {
    linha_ = linha;
    for (GradeVetor* g : grades_) g->definirLinha(linha);
    atualizar();
}

void FormularioUsina::atualizar() {
    atualizando_ = true;
    bool ativo = linha_ >= 0;
    titulo_->setText(ativo ? QStringLiteral("Usina %1  %2").arg(linha_ + 1).arg(QString::fromLatin1(modelo_->usina(linha_).nome.c_str()))
                           : QStringLiteral("Nenhuma usina selecionada"));
    for (auto& [nome, e] : edits_) {
        e.widget->setEnabled(ativo);
        e.widget->setText(ativo ? textoValor(modelo_->valor(linha_, *e.campo, 0)) : QString());
        if (e.nome_lookup) {
            QString n;
            if (ativo) {
                int32_t codigo = std::get<int32_t>(modelo_->valor(linha_, *e.campo, 0));
                std::string s;
                if (nome == "posto") s = modelo_->lookup().nomePosto(codigo);
                else if (nome == "empresa") s = modelo_->lookup().nomeEmpresa(codigo);
                else if (nome == "tipo_turbina") s = modelo_->lookup().nomeTurbina(codigo);
                n = QString::fromLatin1(s.c_str());
            }
            e.nome_lookup->setText(n);
        }
    }
    for (auto& [nome, c] : combos_) {
        c.widget->setEnabled(ativo);
        if (!ativo) {
            c.widget->setCurrentIndex(-1);
            continue;
        }
        Valor v = modelo_->valor(linha_, *c.campo, 0);
        QVariant chave = c.campo->tipo == TipoCampo::Texto ? QVariant(textoValor(v)) : QVariant(std::get<int32_t>(v));
        int i = c.widget->findData(chave);
        if (i < 0) {
            c.widget->addItem(QStringLiteral("%1  (?)").arg(chave.toString()), chave);
            i = c.widget->count() - 1;
        }
        c.widget->setCurrentIndex(i);
    }
    for (GradeVetor* g : grades_) g->atualizar();
    atualizando_ = false;
}

void FormularioUsina::aoEditarEdit(const char* nome) {
    if (atualizando_ || linha_ < 0) return;
    Edit& e = edits_[nome];
    std::optional<Valor> v = valorDeTexto(*e.campo, e.widget->text());
    if (v) modelo_->definirValor(linha_, *e.campo, 0, *v);
    atualizar();
}

void FormularioUsina::aoEscolherCombo(const char* nome, int indice) {
    if (atualizando_ || linha_ < 0 || indice < 0) return;
    Combo& c = combos_[nome];
    QVariant d = c.widget->itemData(indice);
    Valor v = c.campo->tipo == TipoCampo::Texto ? Valor{std::string(d.toString().toLatin1().constData())}
                                                : Valor{static_cast<int32_t>(d.toInt())};
    modelo_->definirValor(linha_, *c.campo, 0, v);
}

void FormularioUsina::focarCampo(std::string_view nome) {
    std::string n(nome);
    if (auto it = edits_.find(n); it != edits_.end()) {
        it->second.widget->setFocus();
        it->second.widget->selectAll();
        return;
    }
    if (auto it = combos_.find(n); it != combos_.end()) {
        it->second.widget->setFocus();
        return;
    }
    for (GradeVetor* g : grades_)
        if (g->contemCampo(nome)) {
            g->focarCampo(nome);
            return;
        }
}

void FormularioUsina::marcarProblemas(const std::vector<std::string>& campos_com_erro) {
    auto marcado = [&](const std::string& nome) {
        return std::find(campos_com_erro.begin(), campos_com_erro.end(), nome) != campos_com_erro.end();
    };
    static const QString estilo = QStringLiteral("background-color: #ffd6d6;");
    for (auto& [nome, e] : edits_) e.widget->setStyleSheet(marcado(nome) ? estilo : QString());
    for (auto& [nome, c] : combos_) c.widget->setStyleSheet(marcado(nome) ? estilo : QString());
}
