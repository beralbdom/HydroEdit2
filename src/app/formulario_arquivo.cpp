#include "formulario_arquivo.h"
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QAbstractItemView>
#include <QPushButton>
#include <QStyle>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <map>
#include <set>
#include "dados_deck.h"
#include "delegate_referencia.h"
#include "formulario_usina.h"

namespace {
QFormLayout* novoForm(QWidget* pai) {
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

constexpr int LARGURA_MAXIMA_LISTA = 280;

// Grupos de um formulario em duas colunas quando cabem lado a lado, na largura natural, dentro da
// largura disponivel (a da area de rolagem); senao, um embaixo do outro. A largura minima pedida e a
// do grupo mais largo, para a area de rolagem poder estreitar a pagina e a grade trocar para uma
// coluna. A escolha e refeita quando a largura muda e quando o conteudo muda de tamanho (as listas
// crescem ao receber os itens). Os rotulos ficam com a mesma largura em cada coluna.
class GradeGrupos : public QWidget {
public:
    GradeGrupos() : grade_(new QGridLayout(this)) {
        grade_->setContentsMargins(6, 4, 6, 4);
        grade_->setSpacing(6);
    }

    void adicionar(QGroupBox* grupo, QFormLayout* form) {
        grupos_.push_back(grupo);
        forms_.push_back(form);
        colunas_ = 0;
        distribuir(2);
    }

    QSize minimumSizeHint() const override {
        int largura = 0;
        for (QGroupBox* g : grupos_) largura = std::max(largura, g->minimumSizeHint().width());
        const QMargins m = grade_->contentsMargins();
        return {largura + m.left() + m.right(), grade_->minimumSize().height()};
    }

protected:
    void resizeEvent(QResizeEvent* evento) override {
        QWidget::resizeEvent(evento);
        decidir();
    }

    bool event(QEvent* evento) override {
        const bool tratado = QWidget::event(evento);
        if (evento->type() == QEvent::LayoutRequest) decidir();
        return tratado;
    }

private:
    void decidir() {
        int esquerda = 0;
        int direita = 0;
        for (size_t i = 0; i < grupos_.size(); ++i) (i % 2 ? direita : esquerda) = std::max(i % 2 ? direita : esquerda, grupos_[i]->sizeHint().width());
        const QMargins m = grade_->contentsMargins();
        distribuir(width() >= esquerda + direita + grade_->horizontalSpacing() + m.left() + m.right() ? 2 : 1);
        alinhar();
    }

    void distribuir(int colunas) {
        if (colunas == colunas_) return;
        colunas_ = colunas;
        for (int r = 0; r < grade_->rowCount(); ++r) grade_->setRowStretch(r, 0);
        for (QGroupBox* g : grupos_) grade_->removeWidget(g);
        for (size_t i = 0; i < grupos_.size(); ++i) grade_->addWidget(grupos_[i], static_cast<int>(i) / colunas, static_cast<int>(i) % colunas);
        grade_->setColumnStretch(0, 1);
        grade_->setColumnStretch(1, colunas == 2 ? 1 : 0);
        grade_->setRowStretch((static_cast<int>(grupos_.size()) + colunas - 1) / colunas, 1);
        updateGeometry();
    }

    void alinhar() {
        for (int coluna = 0; coluna < colunas_; ++coluna) {
            std::vector<QWidget*> rotulos;
            int largura = 0;
            for (size_t i = static_cast<size_t>(coluna); i < forms_.size(); i += static_cast<size_t>(colunas_))
                for (int r = 0; r < forms_[i]->rowCount(); ++r)
                    if (QLayoutItem* item = forms_[i]->itemAt(r, QFormLayout::LabelRole); item && item->widget()) {
                        rotulos.push_back(item->widget());
                        largura = std::max(largura, item->widget()->sizeHint().width());
                    }
            for (QWidget* rotulo : rotulos) rotulo->setMinimumWidth(largura);
        }
    }

    QGridLayout* grade_;
    std::vector<QGroupBox*> grupos_;
    std::vector<QFormLayout*> forms_;
    int colunas_ = 0;
};

bool numerica(const ColunaFixa& c) { return c.tipo != TipoColunaFixa::Texto && !DadosDeck::temOpcoes(c); }

// Um registro so: parametro de um valor, ou grupo de valores que cabe numa linha de formulario.
bool registroUnico(const SecaoFixa& s) { return s.max_registros == 1; }
}  // namespace

// Formulario das secoes de um arquivo que nao justificam tabela: parametros de um registro e
// listas curtas, como as linhas que abrem os blocos (submercados, interligacoes). Em arquivo de
// parametros (dger.dat, selcor.dat) sao todas as secoes, uma linha por parametro; nos demais, as
// secoes marcadas com formulario. Parametros de um valor ficam juntos no grupo Parametros; os de
// varios valores e as listas ganham grupo proprio, as listas com uma linha por registro, um botao
// para remover cada uma e um para adicionar a copia da ultima. Cada campo grava no repositorio do
// deck ao terminar a edicao, com a mesma validacao das tabelas. Os rotulos das listas de cadastro
// sao refeitos a cada alteracao do deck, porque o nome pode ter mudado em outro arquivo.
FormularioArquivo::FormularioArquivo(const QString& nome_padrao, const LayoutArquivoFixo& layout, DadosDeck* dados, QWidget* parent)
    : QWidget(parent), nome_(nome_padrao), layout_(layout), dados_(dados) {
    auto* externo = new QVBoxLayout(this);
    externo->setContentsMargins(0, 0, 0, 0);
    connect(dados_, &DadosDeck::recarregado, this, &FormularioArquivo::montar);
    connect(dados_, &DadosDeck::reinterpretado, this, [this](const QString& nome) {
        if (nome == nome_) montar();
    });
    connect(dados_, &DadosDeck::alterado, this, &FormularioArquivo::atualizarValores);
    montar();
}

// Campo do numero de patamares com os botoes que adicionam ou removem o ultimo patamar em todos os
// arquivos do deck; digitar o numero so muda o numero.
QWidget* FormularioArquivo::campoComPatamares(QWidget* campo, Patamares tipo, QWidget* pai) {
    auto* linha = new QWidget(pai);
    auto* h = new QHBoxLayout(linha);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    campo->setParent(linha);
    h->addWidget(campo);
    auto* adicionar = new QPushButton(QStringLiteral("Adicionar patamar"), linha);
    auto* remover = new QPushButton(QStringLiteral("Remover patamar"), linha);
    const QString arquivos = tipo == Patamares::Deficit ? QStringLiteral("no sistema.dat")
                                                        : QStringLiteral("no patamar.dat e nos arquivos que têm valores por patamar");
    adicionar->setToolTip(QStringLiteral("Acrescenta um patamar depois do último, %1").arg(arquivos));
    remover->setToolTip(QStringLiteral("Tira o último patamar, %1").arg(arquivos));
    connect(adicionar, &QPushButton::clicked, this, [this, tipo] { mudarPatamares(tipo, 1); });
    connect(remover, &QPushButton::clicked, this, [this, tipo] { mudarPatamares(tipo, -1); });
    h->addWidget(adicionar);
    h->addWidget(remover);
    h->addStretch(1);
    return linha;
}

// Muda o numero de patamares pelo repositorio do deck e mostra os arquivos alterados e o que conferir.
void FormularioArquivo::mudarPatamares(Patamares tipo, int delta) {
    QStringList avisos;
    const Resultado r = dados_->mudarPatamares(tipo, delta, &avisos);
    if (!r.ok) {
        emit valorRecusado(QString::fromUtf8(r.mensagem));
        return;
    }
    QMessageBox::information(this, delta > 0 ? QStringLiteral("Patamar adicionado") : QStringLiteral("Patamar removido"),
                             avisos.join(QStringLiteral("\n\n")));
}

// Copia do registro inserida logo depois dele, ou o registro removido, com o bloco que ele abre em
// outra secao; o formulario e refeito pela releitura do arquivo.
void FormularioArquivo::editarRegistros(int secao, int registro, bool adicionar) {
    const Resultado r = adicionar ? dados_->duplicar(nome_, secao, registro, -1) : dados_->remover(nome_, secao, registro, -1);
    if (!r.ok) emit valorRecusado(QString::fromUtf8(r.mensagem));
}

// Largura do campo pelo numero de colunas do arquivo que ele ocupa; texto pode ser mais largo. Coluna
// com lista (outro cadastro, como submercado, REE e usina, ou valores fixos do manual) vira lista com
// "NOME (codigo)".
QWidget* FormularioArquivo::novoCampo(const ArquivoFixo& arquivo, int secao, int registro, int coluna, QWidget* pai) {
    const ColunaFixa& c = arquivo.secoes()[static_cast<size_t>(secao)].definicao.colunas[static_cast<size_t>(coluna)];
    if (DadosDeck::temOpcoes(c)) {
        auto* lista = new QComboBox(pai);
        lista->setMaxVisibleItems(20);
        lista->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        campos_.push_back({nullptr, lista, secao, registro, coluna});
        connect(lista, &QComboBox::activated, this, [this, lista, secao, registro, coluna, c] {
            if (atualizando_) return;
            const ArquivoFixo* a = dados_->arquivo(nome_);
            if (!a) return;
            const QString atual = QString::fromLatin1(a->valor(secao, registro, coluna).c_str());
            const QString novo = lista->currentData().toString();
            if (DadosDeck::normalizarCodigo(atual) == novo) return;
            Resultado r = dados_->definir(nome_, secao, registro, coluna, novo);
            if (!r.ok) {
                DelegateReferencia::preencher(lista, dados_->opcoes(c), atual);
                emit valorRecusado(QString::fromUtf8(r.mensagem));
            }
        });
        return lista;
    }
    auto* edit = new QLineEdit(pai);
    const int caracteres = layout_.separador ? 12 : std::max(4, c.fim - c.inicio + 1);
    edit->setFixedWidth(std::clamp(edit->fontMetrics().horizontalAdvance(QString(caracteres, QLatin1Char('0'))) + 16, 56, 420));
    if (numerica(c)) edit->setAlignment(Qt::AlignRight);
    edit->setReadOnly(!ArquivoFixo::editavel(c));
    edit->setToolTip(QStringLiteral("Colunas %1 a %2").arg(c.inicio).arg(c.fim));
    campos_.push_back({edit, nullptr, secao, registro, coluna});
    connect(edit, &QLineEdit::editingFinished, this, [this, edit, secao, registro, coluna] {
        if (atualizando_) return;
        const ArquivoFixo* a = dados_->arquivo(nome_);
        if (!a) return;
        const QString atual = QString::fromLatin1(a->valor(secao, registro, coluna).c_str());
        if (edit->text().trimmed() == atual) return;
        Resultado r = dados_->definir(nome_, secao, registro, coluna, edit->text());
        if (!r.ok) {
            edit->setText(atual);
            emit valorRecusado(QString::fromUtf8(r.mensagem));
        }
    });
    return edit;
}

// Refaz os campos (o numero de registros pode ter mudado). O conteudo antigo sai com deleteLater,
// porque a reconstrucao pode vir de dentro do sinal de um campo dele (um numero de patamares
// editado rele o proprio arquivo).
void FormularioArquivo::montar() {
    atualizando_ = true;
    campos_.clear();
    if (conteudo_) {
        conteudo_->hide();
        conteudo_->deleteLater();
        conteudo_ = nullptr;
    }
    const ArquivoFixo* arquivo = dados_->arquivo(nome_);
    if (arquivo && layout_.parametros && !layout_.abas.empty()) {
        conteudo_ = novosTemas(*arquivo);
        layout()->addWidget(conteudo_);
        atualizando_ = false;
        atualizarValores();
        return;
    }
    auto* interno = new QWidget;
    auto* v = new QVBoxLayout(interno);
    v->setContentsMargins(6, 4, 6, 4);
    v->setSpacing(4);

    if (!arquivo) {
        auto* aviso = new QLabel(QStringLiteral("Arquivo não carregado"), interno);
        aviso->setEnabled(false);
        v->addWidget(aviso);
    } else if (layout_.parametros) {
        auto* grupo = new QGroupBox(QStringLiteral("Parâmetros"), interno);
        QFormLayout* f = novoForm(grupo);
        for (int s = 0; s < static_cast<int>(arquivo->secoes().size()); ++s)
            f->addRow(QString::fromStdString(arquivo->secoes()[static_cast<size_t>(s)].definicao.titulo), linhaParametro(*arquivo, s, grupo));
        v->addWidget(grupo);
    } else {
        QGroupBox* parametros = nullptr;
        QFormLayout* form_parametros = nullptr;
        std::set<std::string> ao_lado;
        for (const SecaoLida& secao : arquivo->secoes())
            if (secao.definicao.formulario && !secao.definicao.formulario_ao_lado.empty()) ao_lado.insert(secao.definicao.formulario_ao_lado);
        for (int s = 0; s < static_cast<int>(arquivo->secoes().size()); ++s) {
            const SecaoLida& secao = arquivo->secoes()[static_cast<size_t>(s)];
            if (!secao.definicao.formulario) continue;
            const QString titulo = QString::fromStdString(secao.definicao.titulo);
            const auto& colunas = secao.definicao.colunas;
            if (registroUnico(secao.definicao) && colunas.size() == 1) {
                if (!parametros) {
                    parametros = new QGroupBox(QStringLiteral("Parâmetros"), interno);
                    form_parametros = novoForm(parametros);
                    v->addWidget(parametros);
                }
                if (secao.linhas.empty()) {
                    auto* ausente = new QLabel(QStringLiteral("ausente no arquivo"), parametros);
                    ausente->setEnabled(false);
                    form_parametros->addRow(titulo, ausente);
                } else {
                    QWidget* campo = novoCampo(*arquivo, s, 0, 0, parametros);
                    if (secao.definicao.contagem != Patamares::Nenhum) campo = campoComPatamares(campo, secao.definicao.contagem, parametros);
                    form_parametros->addRow(titulo, campo);
                }
                continue;
            }
            if (ao_lado.count(secao.definicao.titulo)) continue;
            QGroupBox* grupo = novoGrupo(*arquivo, s, interno);
            const auto vizinha = std::find_if(arquivo->secoes().begin(), arquivo->secoes().end(), [&](const SecaoLida& o) {
                return !secao.definicao.formulario_ao_lado.empty() && o.definicao.titulo == secao.definicao.formulario_ao_lado;
            });
            if (vizinha == arquivo->secoes().end()) {
                v->addWidget(grupo);
                continue;
            }
            auto* lado_a_lado = new QHBoxLayout;
            lado_a_lado->setSpacing(v->spacing());
            lado_a_lado->addWidget(grupo);
            lado_a_lado->addWidget(novoGrupo(*arquivo, static_cast<int>(vizinha - arquivo->secoes().begin()), interno), 1);
            v->addLayout(lado_a_lado);
        }
    }
    v->addStretch(1);
    FormularioUsina::alinharRotulos(interno);
    conteudo_ = FormularioUsina::paginaRolavel(interno, this);
    layout()->addWidget(conteudo_);
    atualizando_ = false;
    atualizarValores();
}

// Campo de um parametro (secao de um registro) ou, com varias colunas, os campos com o nome de cada
// coluna antes, um por linha quando algum tem lista e quatro por linha quando sao so numeros (o
// volume inicial por REE tem um campo por REE); parametro que o arquivo nao tem aparece como ausente.
QWidget* FormularioArquivo::linhaParametro(const ArquivoFixo& arquivo, int s, QWidget* pai) {
    const SecaoLida& secao = arquivo.secoes()[static_cast<size_t>(s)];
    if (secao.linhas.empty()) {
        auto* ausente = new QLabel(QStringLiteral("ausente no arquivo"), pai);
        ausente->setEnabled(false);
        return ausente;
    }
    auto* linha = new QWidget(pai);
    auto* g = new QGridLayout(linha);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(6);
    g->setVerticalSpacing(4);
    const auto& colunas = secao.definicao.colunas;
    const int n = static_cast<int>(colunas.size());
    const bool listas = std::any_of(colunas.begin(), colunas.end(), [](const ColunaFixa& c) { return DadosDeck::temOpcoes(c); });
    const int por_linha = listas ? 1 : std::min(n, 4);
    for (int c = 0; c < n; ++c) {
        const int r = c / por_linha;
        const int k = (c % por_linha) * 2;
        if (n > 1) g->addWidget(new QLabel(QString::fromStdString(colunas[static_cast<size_t>(c)].nome), linha), r, k);
        g->addWidget(novoCampo(arquivo, s, 0, c, linha), r, k + 1, Qt::AlignLeft);
    }
    g->setColumnStretch(por_linha * 2, 1);
    return linha;
}

// Parametros agrupados por tema (LayoutArquivoFixo::abas): uma pagina por tema, escolhida pela
// arvore do navegador (mostrarTema), com os grupos lado a lado quando cabem (GradeGrupos), a altura
// da linha igualada e os rotulos alinhados em cada coluna. Parametro que nenhum grupo cita vai para o
// tema Outros, para nada sumir do formulario.
QWidget* FormularioArquivo::novosTemas(const ArquivoFixo& arquivo) {
    std::map<std::string, int> por_titulo;
    for (int s = 0; s < static_cast<int>(arquivo.secoes().size()); ++s) por_titulo[arquivo.secoes()[static_cast<size_t>(s)].definicao.titulo] = s;
    std::vector<AbaFormulario> abas = layout_.abas;
    std::set<std::string> citados;
    for (const AbaFormulario& aba : abas)
        for (const GrupoFormulario& g : aba.grupos) citados.insert(g.secoes.begin(), g.secoes.end());
    GrupoFormulario outros{"Outros parâmetros", {}};
    for (const SecaoLida& secao : arquivo.secoes())
        if (!citados.count(secao.definicao.titulo)) outros.secoes.push_back(secao.definicao.titulo);
    if (!outros.secoes.empty()) abas.push_back({"Outros", {outros}});

    auto* painel = new QStackedWidget(this);
    for (const AbaFormulario& aba : abas) {
        auto* pagina = new GradeGrupos;
        for (const GrupoFormulario& g : aba.grupos) {
            auto* grupo = new QGroupBox(QString::fromStdString(g.titulo), pagina);
            QFormLayout* f = novoForm(grupo);
            for (const std::string& titulo : g.secoes) {
                const auto it = por_titulo.find(titulo);
                if (it != por_titulo.end()) f->addRow(QString::fromStdString(titulo), linhaParametro(arquivo, it->second, grupo));
            }
            pagina->adicionar(grupo, f);
        }
        painel->addWidget(FormularioUsina::paginaRolavel(pagina, painel));
    }
    painel->setCurrentIndex(std::min(tema_, painel->count() - 1));
    return painel;
}

// Pagina do tema (indice em LayoutArquivoFixo::abas) no formulario de parametros agrupados; o tema
// escolhido continua o mesmo quando o formulario e refeito.
void FormularioArquivo::mostrarTema(int tema) {
    tema_ = tema;
    if (auto* painel = qobject_cast<QStackedWidget*>(conteudo_)) painel->setCurrentIndex(std::min(tema_, painel->count() - 1));
}

// Grupo de uma secao do formulario: aviso se o arquivo nao tem registros dela, pares rotulo e campo
// se ela tem um registro so e, nas listas, uma linha de campos por registro, com o botao de remover
// cada uma e o de adicionar a copia da ultima quando a secao aceita registros avulsos.
QGroupBox* FormularioArquivo::novoGrupo(const ArquivoFixo& arquivo, int s, QWidget* pai) {
    const SecaoLida& secao = arquivo.secoes()[static_cast<size_t>(s)];
    const auto& colunas = secao.definicao.colunas;
    auto* grupo = new QGroupBox(QString::fromStdString(secao.definicao.titulo), pai);
    if (secao.linhas.empty()) {
        auto* g = new QVBoxLayout(grupo);
        g->setContentsMargins(8, 6, 8, 6);
        auto* vazio = new QLabel(QStringLiteral("Sem registros no arquivo"), grupo);
        vazio->setEnabled(false);
        g->addWidget(vazio);
        return grupo;
    }
    if (registroUnico(secao.definicao)) {
        QFormLayout* f = novoForm(grupo);
        for (int c = 0; c < static_cast<int>(colunas.size()); ++c)
            f->addRow(QString::fromStdString(colunas[static_cast<size_t>(c)].nome), novoCampo(arquivo, s, 0, c, grupo));
        return grupo;
    }
    auto* grade = new QGridLayout(grupo);
    grade->setContentsMargins(8, 6, 8, 6);
    grade->setHorizontalSpacing(6);
    grade->setVerticalSpacing(4);
    for (int c = 0; c < static_cast<int>(colunas.size()); ++c) {
        auto* rotulo = new QLabel(QString::fromStdString(colunas[static_cast<size_t>(c)].nome), grupo);
        rotulo->setEnabled(false);
        grade->addWidget(rotulo, 0, c, numerica(colunas[static_cast<size_t>(c)]) ? Qt::AlignRight : Qt::AlignLeft);
    }
    const int n_colunas = static_cast<int>(colunas.size());
    const int n_registros = static_cast<int>(secao.linhas.size());
    const bool avulsos = arquivo.aceitaRegistrosAvulsos(s);
    for (int r = 0; r < n_registros; ++r) {
        for (int c = 0; c < n_colunas; ++c) grade->addWidget(novoCampo(arquivo, s, r, c, grupo), r + 1, c);
        if (!avulsos) continue;
        auto* remover = new QToolButton(grupo);
        remover->setIcon(style()->standardIcon(QStyle::SP_LineEditClearButton));
        remover->setAutoRaise(true);
        remover->setToolTip(arquivo.abreBloco(s, r) ? QStringLiteral("Remover este registro e os que dependem dele")
                                                     : QStringLiteral("Remover este registro"));
        connect(remover, &QToolButton::clicked, this, [this, s, r] { editarRegistros(s, r, false); });
        grade->addWidget(remover, r + 1, n_colunas);
    }
    grade->setColumnStretch(n_colunas + (avulsos ? 1 : 0), 1);
    if (avulsos) {
        auto* adicionar = new QPushButton(QStringLiteral("Adicionar"), grupo);
        adicionar->setToolTip(QStringLiteral("Insere uma cópia do último registro, para editar"));
        connect(adicionar, &QPushButton::clicked, this, [this, s, n_registros] { editarRegistros(s, n_registros - 1, true); });
        grade->addWidget(adicionar, n_registros + 1, 0, 1, std::min(2, n_colunas), Qt::AlignLeft);
    }
    grade->setRowStretch(n_registros + 2, 1);
    return grupo;
}

// Valores atuais do arquivo nos campos; o campo em edicao fica como esta.
void FormularioArquivo::atualizarValores() {
    const ArquivoFixo* arquivo = dados_->arquivo(nome_);
    if (!arquivo) return;
    atualizando_ = true;
    for (const Campo& c : campos_) {
        const QString valor = QString::fromLatin1(arquivo->valor(c.secao, c.registro, c.coluna).c_str());
        if (c.lista) {
            const ColunaFixa& coluna = arquivo->secoes()[static_cast<size_t>(c.secao)].definicao.colunas[static_cast<size_t>(c.coluna)];
            DelegateReferencia::preencher(c.lista, dados_->opcoes(coluna), valor);
            c.lista->setMinimumWidth(std::min(c.lista->sizeHint().width(), LARGURA_MAXIMA_LISTA));
            c.lista->view()->setMinimumWidth(c.lista->view()->sizeHintForColumn(0) + 24);
            c.lista->setToolTip(c.lista->currentText());
            continue;
        }
        if (c.edit->hasFocus() && c.edit->isModified()) continue;
        c.edit->setText(valor);
        c.edit->setModified(false);
    }
    atualizando_ = false;
}
