#include "formulario_termica.h"
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>
#include "dados_deck.h"
#include "formulario_usina.h"
#include "modelo_secao_fixa.h"

namespace {
// Colunas dos layouts em layouts_newave.cpp (manual do NEWAVE 30.0.2, secoes 3.15 a 3.19).
constexpr int TERM_NOME = 1;
constexpr int TERM_CAPACIDADE = 2;
constexpr int TERM_FC_MAX = 3;
constexpr int TERM_TEIF = 4;
constexpr int TERM_IP = 5;
constexpr int TERM_GTMIN_JAN = 6;
constexpr int TERM_GTMIN_COLUNAS = 13;
constexpr int CONFT_SUBMERCADO = 2;
constexpr int CONFT_SITUACAO = 3;
constexpr int CONFT_CLASSE = 4;
constexpr int CONFT_TECNOLOGIA = 5;
constexpr int CONFT_CLASSE_GAS = 6;

const QStringList& arquivosTermicos() {
    static const QStringList nomes = {QStringLiteral("term.dat"), QStringLiteral("conft.dat"), QStringLiteral("expt.dat"),
                                      QStringLiteral("manutt.dat"), QStringLiteral("clast.dat")};
    return nomes;
}

// Filtra o campo 1 pelo valor exato; so reaplica quando o valor muda, para uma edicao na propria
// tabela filtrada nao reiniciar o modelo. Valor vazio nao casa com nenhum registro.
void filtrarPor(QSortFilterProxyModel* filtro, const QString& valor) {
    const QString padrao = QStringLiteral("^%1$").arg(QRegularExpression::escape(valor.isEmpty() ? QStringLiteral("\x01") : valor));
    if (filtro->filterRegularExpression().pattern() != padrao) filtro->setFilterRegularExpression(padrao);
}

QFormLayout* novoForm(QWidget* pai) {
    auto* f = new QFormLayout(pai);
    f->setContentsMargins(8, 6, 8, 6);
    f->setHorizontalSpacing(8);
    f->setVerticalSpacing(4);
    f->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    f->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    return f;
}

QVBoxLayout* novaColuna(QWidget* pai) {
    auto* v = new QVBoxLayout(pai);
    v->setContentsMargins(6, 4, 6, 4);
    v->setSpacing(4);
    return v;
}
}  // namespace

// Painel da usina termoeletrica selecionada, no mesmo desenho do formulario das hidroeletricas:
// titulo com codigo e nome, botao Salvar dos arquivos termicos, linha de avisos e abas com os dados
// dos cinco arquivos ligados pelo numero da usina (term.dat e conft.dat, campo 1; expt.dat e
// manutt.dat, campo 1) e pelo numero da classe (conft.dat campo 5 e clast.dat campo 1).
FormularioTermica::FormularioTermica(DadosDeck* dados, QWidget* parent) : QWidget(parent), dados_(dados) {
    auto* externo = new QVBoxLayout(this);
    externo->setContentsMargins(6, 4, 6, 4);
    externo->setSpacing(4);

    auto* cabecalho = new QHBoxLayout;
    titulo_ = new QLabel(this);
    QFont fonte = titulo_->font();
    fonte.setPointSize(fonte.pointSize() + 1);
    fonte.setBold(true);
    titulo_->setFont(fonte);
    botao_salvar_ = new QPushButton(QStringLiteral("Salvar"), this);
    cabecalho->addWidget(titulo_, 1);
    cabecalho->addWidget(botao_salvar_);
    externo->addLayout(cabecalho);
    aviso_ = new QLabel(this);
    aviso_->setEnabled(false);
    externo->addWidget(aviso_);

    abas_ = FormularioUsina::novasAbas(this);
    auto adicionar = [this](QWidget* pagina, const QString& titulo) {
        abas_->addTab(FormularioUsina::paginaRolavel(pagina, abas_), titulo);
    };
    adicionar(criarPaginaCadastro(), QStringLiteral("Cadastro"));
    adicionar(criarPaginaConfiguracao(), QStringLiteral("Configuração"));
    abas_->addTab(criarPaginaRegistros(QStringLiteral("expt.dat"), 0, filtro_expansao_,
                                       QStringLiteral("Modificações por período (expt.dat, seção 3.17)")),
                  QStringLiteral("Expansão"));
    abas_->addTab(criarPaginaRegistros(QStringLiteral("manutt.dat"), 0, filtro_manutencao_,
                                       QStringLiteral("Manutenções programadas (manutt.dat, seção 3.19)")),
                  QStringLiteral("Manutenções"));
    abas_->addTab(criarPaginaClasse(), QStringLiteral("Classe térmica"));
    externo->addWidget(abas_, 1);

    connect(botao_salvar_, &QPushButton::clicked, this, [this] {
        QStringList falhas;
        for (const QString& nome : arquivosTermicos()) {
            QString motivo;
            if (!dados_->salvar(nome, &motivo)) falhas << motivo;
        }
        atualizar(falhas.isEmpty() ? QStringLiteral("salvo") : falhas.join(QStringLiteral("; ")));
    });
    connect(dados_, &DadosDeck::alterado, this, [this](const QString& nome) {
        if (arquivosTermicos().contains(nome)) atualizar();
    });
    connect(dados_, &DadosDeck::recarregado, this, [this] { definirUsina({}); });
    connect(dados_, &DadosDeck::reinterpretado, this, [this](const QString& nome) {
        if (arquivosTermicos().contains(nome)) atualizar();
    });
    definirUsina({});
}

QLineEdit* FormularioTermica::ligarEdit(QFormLayout* form, const QString& rotulo, const QString& arquivo, int coluna, bool texto) {
    auto* edit = new QLineEdit(form->parentWidget());
    edit->setFixedWidth(texto ? 180 : 110);
    form->addRow(rotulo, edit);
    campos_.push_back({edit, nullptr, arquivo, coluna});
    connect(edit, &QLineEdit::editingFinished, this, [this, edit, arquivo, coluna] { gravar(arquivo, coluna, edit->text()); });
    return edit;
}

QWidget* FormularioTermica::criarPaginaCadastro() {
    auto* pagina = new QWidget(this);
    auto* v = novaColuna(pagina);

    auto* identificacao = new QGroupBox(QStringLiteral("Identificação"), pagina);
    auto* fi = novoForm(identificacao);
    ligarEdit(fi, QStringLiteral("Nome"), QStringLiteral("term.dat"), TERM_NOME, true);
    v->addWidget(identificacao);

    auto* capacidade = new QGroupBox(QStringLiteral("Capacidade e disponibilidade"), pagina);
    auto* fc = novoForm(capacidade);
    ligarEdit(fc, QStringLiteral("Capacidade instalada (MW)"), QStringLiteral("term.dat"), TERM_CAPACIDADE);
    ligarEdit(fc, QStringLiteral("Fator de capacidade máximo (%)"), QStringLiteral("term.dat"), TERM_FC_MAX);
    ligarEdit(fc, QStringLiteral("TEIF (%)"), QStringLiteral("term.dat"), TERM_TEIF);
    ligarEdit(fc, QStringLiteral("IP dos demais anos (%)"), QStringLiteral("term.dat"), TERM_IP);
    v->addWidget(capacidade);

    auto* geracao = new QGroupBox(QStringLiteral("Geração térmica mínima (MWmês)"), pagina);
    auto* vg = novaColuna(geracao);
    gtmin_ = new QTableWidget(TERM_GTMIN_COLUNAS, 1, geracao);
    gtmin_->setHorizontalHeaderLabels({QStringLiteral("MWmês")});
    gtmin_->setVerticalHeaderLabels({QStringLiteral("jan (anos de manutenção)"), "fev", "mar", "abr", "mai", "jun", "jul", "ago",
                                     "set", "out", "nov", "dez", QStringLiteral("demais anos")});
    gtmin_->verticalHeader()->setDefaultSectionSize(20);
    gtmin_->horizontalHeader()->setFixedHeight(22);
    gtmin_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    gtmin_->setFixedHeight(22 + TERM_GTMIN_COLUNAS * 20 + 2 * gtmin_->frameWidth() + 2);
    gtmin_->setFixedWidth(gtmin_->verticalHeader()->sizeHint().width() + 110 + 2 * gtmin_->frameWidth());
    gtmin_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gtmin_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(gtmin_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!atualizando_) gravar(QStringLiteral("term.dat"), TERM_GTMIN_JAN + item->row(), item->text());
    });
    vg->addWidget(gtmin_);
    v->addWidget(geracao);
    v->addStretch(1);
    return pagina;
}

QWidget* FormularioTermica::criarPaginaConfiguracao() {
    auto* pagina = new QWidget(this);
    auto* v = novaColuna(pagina);
    auto* grupo = new QGroupBox(QStringLiteral("Configuração termoelétrica (conft.dat)"), pagina);
    auto* f = novoForm(grupo);
    ligarEdit(f, QStringLiteral("Submercado"), QStringLiteral("conft.dat"), CONFT_SUBMERCADO);
    auto* situacao = new QComboBox(grupo);
    situacao->setMinimumWidth(180);
    situacao->addItem(QStringLiteral("EX  Existente"), QStringLiteral("EX"));
    situacao->addItem(QStringLiteral("EE  Existente, com expansão"), QStringLiteral("EE"));
    situacao->addItem(QStringLiteral("NE  Não existente, com expansão"), QStringLiteral("NE"));
    situacao->addItem(QStringLiteral("NC  Não considerada"), QStringLiteral("NC"));
    f->addRow(QStringLiteral("Situação"), situacao);
    campos_.push_back({nullptr, situacao, QStringLiteral("conft.dat"), CONFT_SITUACAO});
    connect(situacao, &QComboBox::activated, this, [this, situacao](int i) {
        gravar(QStringLiteral("conft.dat"), CONFT_SITUACAO, situacao->itemData(i).toString());
    });
    ligarEdit(f, QStringLiteral("Classe térmica"), QStringLiteral("conft.dat"), CONFT_CLASSE);
    ligarEdit(f, QStringLiteral("Tecnologia"), QStringLiteral("conft.dat"), CONFT_TECNOLOGIA);
    ligarEdit(f, QStringLiteral("Classe de gás"), QStringLiteral("conft.dat"), CONFT_CLASSE_GAS);
    v->addWidget(grupo);
    sem_configuracao_ = new QLabel(QStringLiteral("Esta usina não está no conft.dat."), pagina);
    sem_configuracao_->setEnabled(false);
    v->addWidget(sem_configuracao_);
    v->addStretch(1);
    return pagina;
}

QTableView* FormularioTermica::novaTabela(QSortFilterProxyModel* filtro, QWidget* pai) {
    auto* tabela = new QTableView(pai);
    tabela->setModel(filtro);
    tabela->setAlternatingRowColors(true);
    tabela->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    tabela->verticalHeader()->setVisible(false);
    tabela->verticalHeader()->setDefaultSectionSize(20);
    tabela->horizontalHeader()->setFixedHeight(22);
    connect(filtro, &QAbstractItemModel::modelReset, tabela, &QTableView::resizeColumnsToContents);
    connect(filtro, &QAbstractItemModel::layoutChanged, tabela, &QTableView::resizeColumnsToContents);
    connect(filtro, &QAbstractItemModel::rowsInserted, tabela, &QTableView::resizeColumnsToContents);
    return tabela;
}

// Tabela editavel so com os registros da usina, filtrados pelo campo 1 do arquivo.
QWidget* FormularioTermica::criarPaginaRegistros(const QString& arquivo, int secao, QSortFilterProxyModel*& filtro, const QString& titulo) {
    auto* pagina = new QWidget(this);
    auto* v = novaColuna(pagina);
    auto* rotulo = new QLabel(titulo, pagina);
    rotulo->setEnabled(false);
    v->addWidget(rotulo);
    auto* modelo = new ModeloSecaoFixa(dados_, arquivo, secao, pagina);
    filtro = new QSortFilterProxyModel(pagina);
    filtro->setSourceModel(modelo);
    filtro->setFilterKeyColumn(0);
    connect(modelo, &ModeloSecaoFixa::valorRecusado, this, [this](const QString& motivo) { atualizar(motivo); });
    v->addWidget(novaTabela(filtro, pagina), 1);
    return pagina;
}

// Custo da classe da usina (registro tipo 1 do clast.dat) e as modificacoes de custo dela (tipo 2).
QWidget* FormularioTermica::criarPaginaClasse() {
    auto* pagina = new QWidget(this);
    auto* v = novaColuna(pagina);
    classe_titulo_ = new QLabel(pagina);
    classe_titulo_->setEnabled(false);
    v->addWidget(classe_titulo_);
    for (int secao : {0, 1}) {
        auto* grupo = new QGroupBox(secao == 0 ? QStringLiteral("Custo de operação por ano ($/MWh)") : QStringLiteral("Modificações de custo"), pagina);
        auto* vg = novaColuna(grupo);
        auto* modelo = new ModeloSecaoFixa(dados_, QStringLiteral("clast.dat"), secao, grupo);
        auto* filtro = new QSortFilterProxyModel(grupo);
        filtro->setSourceModel(modelo);
        filtro->setFilterKeyColumn(0);
        connect(modelo, &ModeloSecaoFixa::valorRecusado, this, [this](const QString& motivo) { atualizar(motivo); });
        QTableView* tabela = novaTabela(filtro, grupo);
        if (secao == 0) {
            tabela->setFixedHeight(tabela->horizontalHeader()->height() + 20 + 2 * tabela->frameWidth() + 2);
            tabela->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            filtro_custo_ = filtro;
        } else {
            filtro_mod_custo_ = filtro;
        }
        vg->addWidget(tabela, secao == 1 ? 1 : 0);
        v->addWidget(grupo, secao == 1 ? 1 : 0);
    }
    return pagina;
}

void FormularioTermica::definirUsina(const QString& codigo) {
    usina_ = codigo.trimmed();
    atualizar();
}

int FormularioTermica::registro(const QString& arquivo) const {
    return usina_.isEmpty() ? -1 : dados_->registroPorValor(arquivo, 0, 0, usina_);
}

// Grava o campo editado na linha da usina; valor recusado volta ao que esta no arquivo e o motivo
// vai para a linha de avisos. Texto igual ao do arquivo nao gera edicao.
void FormularioTermica::gravar(const QString& arquivo, int coluna, const QString& texto) {
    if (atualizando_) return;
    const int r = registro(arquivo);
    const ArquivoFixo* a = dados_->arquivo(arquivo);
    if (r < 0 || !a) return;
    if (texto.trimmed() == QString::fromLatin1(a->valor(0, r, coluna).c_str())) return;
    Resultado res = dados_->definir(arquivo, 0, r, coluna, texto);
    if (!res.ok) atualizar(QString::fromUtf8(res.mensagem));
}

// Repreenche tudo a partir dos arquivos: campos do term.dat e do conft.dat pela linha da usina,
// grade de geracao minima e filtros das tabelas (usina em expt e manutt, classe do conft em clast).
// Campos de arquivo em que a usina nao aparece ficam vazios e desabilitados.
void FormularioTermica::atualizar(const QString& aviso) {
    atualizando_ = true;
    const int r_term = registro(QStringLiteral("term.dat"));
    const int r_conft = registro(QStringLiteral("conft.dat"));
    const ArquivoFixo* term = dados_->arquivo(QStringLiteral("term.dat"));
    const ArquivoFixo* conft = dados_->arquivo(QStringLiteral("conft.dat"));
    auto valor = [](const ArquivoFixo* a, int r, int c) { return a && r >= 0 ? QString::fromLatin1(a->valor(0, r, c).c_str()) : QString(); };

    titulo_->setText(usina_.isEmpty() ? QStringLiteral("Nenhuma usina selecionada")
                                      : QStringLiteral("Usina %1  %2").arg(usina_, valor(term, r_term, TERM_NOME)));
    for (const Campo& c : campos_) {
        const bool do_term = c.arquivo == QStringLiteral("term.dat");
        const ArquivoFixo* a = do_term ? term : conft;
        const int r = do_term ? r_term : r_conft;
        const QString texto = valor(a, r, c.coluna);
        if (c.edit) {
            c.edit->setText(texto);
            c.edit->setEnabled(r >= 0);
        } else {
            c.combo->setCurrentIndex(c.combo->findData(texto));
            c.combo->setEnabled(r >= 0);
        }
    }
    for (int i = 0; i < TERM_GTMIN_COLUNAS; ++i) {
        auto* item = new QTableWidgetItem(valor(term, r_term, TERM_GTMIN_JAN + i));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (r_term < 0) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        gtmin_->setItem(i, 0, item);
    }
    sem_configuracao_->setVisible(!usina_.isEmpty() && conft && r_conft < 0);

    const QString classe = valor(conft, r_conft, CONFT_CLASSE);
    filtrarPor(filtro_expansao_, usina_);
    filtrarPor(filtro_manutencao_, usina_);
    filtrarPor(filtro_custo_, classe);
    filtrarPor(filtro_mod_custo_, classe);
    const ArquivoFixo* clast = dados_->arquivo(QStringLiteral("clast.dat"));
    const int r_classe = classe.isEmpty() ? -1 : dados_->registroPorValor(QStringLiteral("clast.dat"), 0, 0, classe);
    classe_titulo_->setText(classe.isEmpty() ? QStringLiteral("Usina sem classe térmica no conft.dat")
                                             : QStringLiteral("Classe %1  %2  ·  combustível: %3  ·  clast.dat, seção 3.18")
                                                   .arg(classe, valor(clast, r_classe, 1), valor(clast, r_classe, 2)));

    QStringList alterados;
    for (const QString& nome : arquivosTermicos()) {
        const ArquivoFixo* a = dados_->arquivo(nome);
        if (a && a->modificado()) alterados << dados_->nomeNoDeck(nome);
    }
    QString texto = alterados.isEmpty() ? QStringLiteral("term.dat, conft.dat, expt.dat, manutt.dat e clast.dat")
                                        : QStringLiteral("alterados, não salvos: %1").arg(alterados.join(QStringLiteral(", ")));
    if (!dados_->carregado()) texto = QStringLiteral("abra o hidr.dat de um deck para editar as termoelétricas");
    if (!aviso.isEmpty()) texto += QStringLiteral("  ·  ") + aviso;
    aviso_->setText(texto);
    botao_salvar_->setEnabled(!alterados.isEmpty());
    atualizando_ = false;
}
