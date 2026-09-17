#include "formulario_usina.h"
#include <QComboBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include "grade_vetor.h"
#include "modelo_hidr.h"

FormularioUsina::FormularioUsina(ModeloHidr* modelo, QWidget* parent) : QWidget(parent), modelo_(modelo) {
    auto* externo = new QVBoxLayout(this);
    configurarLayout(externo);

    titulo_ = new QLabel(this);
    QFont fonte = titulo_->font();
    fonte.setPointSize(fonte.pointSize() + 1);
    fonte.setBold(true);
    titulo_->setFont(fonte);
    externo->addWidget(titulo_);

    abas_ = new QTabWidget(this);
    abas_->setTabPosition(QTabWidget::North);
    abas_->setDocumentMode(false);

    auto adicionarPagina = [this](QWidget* conteudo, const QString& titulo) {
        auto* scroll = new QScrollArea(abas_);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(conteudo);
        abas_->addTab(scroll, titulo);
    };
    adicionarPagina(criarPaginaCadastro(), QStringLiteral("Cadastro"));
    adicionarPagina(criarPaginaReservatorio(), QStringLiteral("Reservatório"));
    adicionarPagina(criarPaginaPolinomios(), QStringLiteral("Polinômios"));
    adicionarPagina(criarPaginaConjuntos(), QStringLiteral("Conjuntos"));
    adicionarPagina(criarPaginaJusante(), QStringLiteral("Jusante"));
    adicionarPagina(criarPaginaOperacao(), QStringLiteral("Operação"));

    externo->addWidget(abas_, 1);

    connect(modelo_, &ModeloHidr::usinaAlterada, this, [this](int linha, const Campo* c) {
        if (!c || c->nome == "nome") {
            recarregarListas();
            atualizar();
        } else if (linha == linha_) {
            atualizar();
        }
    });
    connect(modelo_, &QAbstractItemModel::modelReset, this, [this] {
        recarregarListas();
        definirLinha(-1);
    });
    recarregarListas();
    definirLinha(-1);
}

void FormularioUsina::configurarLayout(QLayout* l) {
    l->setContentsMargins(6, 4, 6, 4);
    l->setSpacing(4);
}

QVBoxLayout* FormularioUsina::novaPagina(QWidget* pai) {
    auto* v = new QVBoxLayout(pai);
    configurarLayout(v);
    return v;
}

QHBoxLayout* FormularioUsina::novaLinha() {
    auto* h = new QHBoxLayout;
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    return h;
}

QGroupBox* FormularioUsina::novoGrupo(QWidget* pai, const QString& titulo) {
    auto* g = new QGroupBox(titulo, pai);
    g->setFlat(false);
    return g;
}

QVBoxLayout* FormularioUsina::novoConteudo(QWidget* pai) {
    auto* v = new QVBoxLayout(pai);
    v->setContentsMargins(8, 6, 8, 6);
    v->setSpacing(4);
    return v;
}

QFormLayout* FormularioUsina::novoForm(QWidget* pai) {
    auto* f = new QFormLayout(pai);
    f->setContentsMargins(8, 6, 8, 6);
    f->setHorizontalSpacing(8);
    f->setVerticalSpacing(4);
    f->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    f->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    f->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    f->setRowWrapPolicy(QFormLayout::DontWrapRows);
    return f;
}

QLineEdit* FormularioUsina::ligarEdit(QFormLayout* f, int pagina, const QString& rotulo, const char* nome, bool com_lookup) {
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
    if (c->tipo == TipoCampo::Texto) {
        if (std::string_view(nome) == "observacao") e->setMinimumWidth(320);
        else e->setFixedWidth(160);
    } else {
        e->setFixedWidth(110);
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
    pagina_do_campo_[nome] = pagina;
    connect(e, &QLineEdit::editingFinished, this, [this, nome] { aoEditarEdit(nome); });
    return e;
}

QComboBox* FormularioUsina::ligarCombo(QFormLayout* f, int pagina, const QString& rotulo, const char* nome) {
    auto* cb = new QComboBox(this);
    cb->setMinimumWidth(180);
    f->addRow(rotulo, cb);
    combos_[nome] = {cb, campo(nome)};
    pagina_do_campo_[nome] = pagina;
    connect(cb, &QComboBox::activated, this, [this, nome](int i) { aoEscolherCombo(nome, i); });
    return cb;
}

void FormularioUsina::registrarCampoGrade(int pagina, const char* nome) { pagina_do_campo_[nome] = pagina; }

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
    if (auto it = pagina_do_campo_.find(n); it != pagina_do_campo_.end()) abas_->setCurrentIndex(it->second);
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
