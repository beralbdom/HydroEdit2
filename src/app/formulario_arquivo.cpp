#include "formulario_arquivo.h"
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <algorithm>
#include "dados_deck.h"
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

bool numerica(const ColunaFixa& c) { return c.tipo != TipoColunaFixa::Texto; }

// Um registro so: parametro de um valor, ou grupo de valores que cabe numa linha de formulario.
bool registroUnico(const SecaoFixa& s) { return s.max_registros == 1; }
}  // namespace

// Formulario das secoes de um arquivo que nao justificam tabela: parametros de um registro e
// listas curtas, como as linhas que abrem os blocos (submercados, interligacoes). Em arquivo de
// parametros (dger.dat, selcor.dat) sao todas as secoes, uma linha por parametro; nos demais, as
// secoes marcadas com formulario. Parametros de um valor ficam juntos no grupo Parametros; os de
// varios valores e as listas ganham grupo proprio, as listas com uma linha por registro. Cada campo
// grava no repositorio do deck ao terminar a edicao, com a mesma validacao das tabelas.
FormularioArquivo::FormularioArquivo(const QString& nome_padrao, const LayoutArquivoFixo& layout, DadosDeck* dados, QWidget* parent)
    : QWidget(parent), nome_(nome_padrao), layout_(layout), dados_(dados) {
    auto* externo = new QVBoxLayout(this);
    externo->setContentsMargins(0, 0, 0, 0);
    connect(dados_, &DadosDeck::recarregado, this, &FormularioArquivo::montar);
    connect(dados_, &DadosDeck::reinterpretado, this, [this](const QString& nome) {
        if (nome == nome_) montar();
    });
    connect(dados_, &DadosDeck::alterado, this, [this](const QString& nome) {
        if (nome == nome_) atualizarValores();
    });
    montar();
}

// Largura do campo pelo numero de colunas do arquivo que ele ocupa; texto pode ser mais largo.
QLineEdit* FormularioArquivo::novoCampo(const ArquivoFixo& arquivo, int secao, int registro, int coluna, QWidget* pai) {
    const ColunaFixa& c = arquivo.secoes()[static_cast<size_t>(secao)].definicao.colunas[static_cast<size_t>(coluna)];
    auto* edit = new QLineEdit(pai);
    const int caracteres = layout_.separador ? 12 : std::max(4, c.fim - c.inicio + 1);
    edit->setFixedWidth(std::clamp(edit->fontMetrics().horizontalAdvance(QString(caracteres, QLatin1Char('0'))) + 16, 56, 420));
    if (numerica(c)) edit->setAlignment(Qt::AlignRight);
    edit->setReadOnly(!ArquivoFixo::editavel(c));
    edit->setToolTip(QStringLiteral("Colunas %1 a %2").arg(c.inicio).arg(c.fim));
    campos_.push_back({edit, secao, registro, coluna});
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
        for (int s = 0; s < static_cast<int>(arquivo->secoes().size()); ++s) {
            const SecaoLida& secao = arquivo->secoes()[static_cast<size_t>(s)];
            const QString titulo = QString::fromStdString(secao.definicao.titulo);
            if (secao.linhas.empty()) {
                auto* ausente = new QLabel(QStringLiteral("ausente no arquivo"), grupo);
                ausente->setEnabled(false);
                f->addRow(titulo, ausente);
                continue;
            }
            auto* linha = new QWidget(grupo);
            auto* h = new QHBoxLayout(linha);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(6);
            const auto& colunas = secao.definicao.colunas;
            for (int c = 0; c < static_cast<int>(colunas.size()); ++c) {
                if (colunas.size() > 1) h->addWidget(new QLabel(QString::fromStdString(colunas[static_cast<size_t>(c)].nome), linha));
                h->addWidget(novoCampo(*arquivo, s, 0, c, linha));
            }
            h->addStretch(1);
            f->addRow(titulo, linha);
        }
        v->addWidget(grupo);
    } else {
        QGroupBox* parametros = nullptr;
        QFormLayout* form_parametros = nullptr;
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
                    form_parametros->addRow(titulo, novoCampo(*arquivo, s, 0, 0, parametros));
                }
                continue;
            }
            auto* grupo = new QGroupBox(titulo, interno);
            v->addWidget(grupo);
            if (secao.linhas.empty()) {
                auto* g = new QVBoxLayout(grupo);
                g->setContentsMargins(8, 6, 8, 6);
                auto* vazio = new QLabel(QStringLiteral("Sem registros no arquivo"), grupo);
                vazio->setEnabled(false);
                g->addWidget(vazio);
                continue;
            }
            if (registroUnico(secao.definicao)) {
                QFormLayout* f = novoForm(grupo);
                for (int c = 0; c < static_cast<int>(colunas.size()); ++c)
                    f->addRow(QString::fromStdString(colunas[static_cast<size_t>(c)].nome), novoCampo(*arquivo, s, 0, c, grupo));
                continue;
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
            for (int r = 0; r < static_cast<int>(secao.linhas.size()); ++r)
                for (int c = 0; c < static_cast<int>(colunas.size()); ++c) grade->addWidget(novoCampo(*arquivo, s, r, c, grupo), r + 1, c);
            grade->setColumnStretch(static_cast<int>(colunas.size()), 1);
        }
    }
    v->addStretch(1);
    FormularioUsina::alinharRotulos(interno);
    conteudo_ = FormularioUsina::paginaRolavel(interno, this);
    layout()->addWidget(conteudo_);
    atualizando_ = false;
    atualizarValores();
}

// Valores atuais do arquivo nos campos; o campo em edicao fica como esta.
void FormularioArquivo::atualizarValores() {
    const ArquivoFixo* arquivo = dados_->arquivo(nome_);
    if (!arquivo) return;
    atualizando_ = true;
    for (const Campo& c : campos_) {
        if (c.edit->hasFocus() && c.edit->isModified()) continue;
        c.edit->setText(QString::fromLatin1(arquivo->valor(c.secao, c.registro, c.coluna).c_str()));
        c.edit->setModified(false);
    }
    atualizando_ = false;
}
