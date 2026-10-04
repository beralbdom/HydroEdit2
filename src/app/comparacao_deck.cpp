#include "comparacao_deck.h"
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>
#include <algorithm>
#include "arquivo_hidr.h"
#include "campos.h"
#include "catalogo_newave.h"
#include "dados_deck.h"
#include "diferencas.h"
#include "modelo_hidr.h"

namespace {
constexpr int MAXIMO_EDICOES = 4000;
constexpr int MAXIMO_LINHAS = 4000;
constexpr int CONTEXTO = 2;
const char* COR_REMOVIDA = "#e15759";
const char* COR_INCLUIDA = "#59a14f";
const char* COR_CABECALHO = "#8c8c8c";

QString linhaHtml(const char* cor, const QString& prefixo, const QString& texto) {
    const QString conteudo = (prefixo + texto).toHtmlEscaped();
    return cor ? QStringLiteral("<span style=\"color:%1\">%2</span>").arg(QString::fromLatin1(cor), conteudo) : conteudo;
}

QString paraTexto(const std::string& s) { return QString::fromLatin1(s.c_str()); }

QString contagem(int n, const char* singular, const char* plural) {
    return QStringLiteral("%1 %2").arg(n).arg(QString::fromUtf8(n == 1 ? singular : plural));
}

// Trechos alterados com CONTEXTO linhas iguais em volta, cada um com o cabecalho das linhas que ocupa
// em cada deck: "-" e o que so o deck aberto tem, "+" o que so o outro tem.
void compararTextos(const std::vector<std::string>& a, const std::vector<std::string>& b, DiferencaArquivo& d) {
    std::vector<LinhaDiferenca> ops;
    if (!diferencasDeLinhas(a, b, MAXIMO_EDICOES, ops)) {
        d.situacao = QStringLiteral("muito diferentes");
        d.alteracoes = MAXIMO_EDICOES;
        d.linhas << linhaHtml(COR_CABECALHO, {}, QStringLiteral("Os arquivos diferem em mais de %1 linhas; use o editor textual para conferir.").arg(MAXIMO_EDICOES));
        return;
    }
    std::vector<bool> mostrar(ops.size(), false);
    for (size_t i = 0; i < ops.size(); ++i) {
        if (ops[i].tipo == TipoDiferenca::Igual) continue;
        ++d.alteracoes;
        const size_t de = i >= CONTEXTO ? i - CONTEXTO : 0;
        for (size_t j = de; j <= std::min(ops.size() - 1, i + CONTEXTO); ++j) mostrar[j] = true;
    }
    for (size_t i = 0; i < ops.size() && d.linhas.size() < MAXIMO_LINHAS; ++i) {
        if (!mostrar[i]) continue;
        if (i == 0 || !mostrar[i - 1]) {
            int linha_a = -1;
            int linha_b = -1;
            for (size_t j = i; j < ops.size() && (linha_a < 0 || linha_b < 0); ++j) {
                if (linha_a < 0 && ops[j].linha_a >= 0) linha_a = ops[j].linha_a;
                if (linha_b < 0 && ops[j].linha_b >= 0) linha_b = ops[j].linha_b;
            }
            d.linhas << linhaHtml(COR_CABECALHO, {}, QStringLiteral("@@ linha %1 no deck aberto, %2 no outro @@").arg(linha_a + 1).arg(linha_b + 1));
        }
        const LinhaDiferenca& op = ops[i];
        if (op.tipo == TipoDiferenca::Igual) d.linhas << linhaHtml(nullptr, QStringLiteral("  "), paraTexto(a[static_cast<size_t>(op.linha_a)]));
        else if (op.tipo == TipoDiferenca::Removida)
            d.linhas << linhaHtml(COR_REMOVIDA, QStringLiteral("- "), paraTexto(a[static_cast<size_t>(op.linha_a)]));
        else d.linhas << linhaHtml(COR_INCLUIDA, QStringLiteral("+ "), paraTexto(b[static_cast<size_t>(op.linha_b)]));
    }
    d.situacao = contagem(d.alteracoes, "linha alterada", "linhas alteradas");
}

// Cadastro de usinas campo a campo: usina, campo (com o elemento, nos vetores) e os dois valores.
void compararHidr(const ArquivoHidr& a, const ArquivoHidr& b, DiferencaArquivo& d) {
    const size_t n = std::max(a.usinas.size(), b.usinas.size());
    const UsinaHidr vazia{};
    for (size_t u = 0; u < n; ++u) {
        const UsinaHidr& ua = u < a.usinas.size() ? a.usinas[u] : vazia;
        const UsinaHidr& ub = u < b.usinas.size() ? b.usinas[u] : vazia;
        const QString nome = paraTexto(!ua.nome.empty() ? ua.nome : ub.nome).trimmed();
        for (const Campo& c : campos())
            for (int i = 0; i < c.n; ++i) {
                const QString va = textoValor(c.obter(ua, i));
                const QString vb = textoValor(c.obter(ub, i));
                if (va == vb) continue;
                ++d.alteracoes;
                if (d.linhas.size() >= MAXIMO_LINHAS) continue;
                QString campo = QString::fromLatin1(c.nome.data(), static_cast<int>(c.nome.size()));
                if (!c.escalar()) campo += c.nome_elemento ? QStringLiteral(" %1").arg(QString::fromStdString(c.nome_elemento(i))) : QStringLiteral("[%1]").arg(i + 1);
                d.linhas << linhaHtml(nullptr, {}, QStringLiteral("Usina %1 %2 · %3: ").arg(u + 1).arg(nome, campo)) +
                                linhaHtml(COR_REMOVIDA, {}, va) + QStringLiteral(" → ") + linhaHtml(COR_INCLUIDA, {}, vb);
            }
    }
    d.situacao = contagem(d.alteracoes, "campo alterado", "campos alterados");
}

// Arquivos binarios registro a registro: no postos.dat, nome e anos de cada posto; no vazoes.dat,
// quantos meses mudaram em cada posto, com os primeiros exemplos.
void compararBinarios(const QString& nome, const ArquivoBinario& a, const ArquivoBinario& b, DiferencaArquivo& d) {
    if (a.tamanhoRegistro() != b.tamanhoRegistro()) {
        d.situacao = QStringLiteral("tamanhos de registro diferentes");
        d.alteracoes = 1;
        d.linhas << linhaHtml(COR_CABECALHO, {}, QStringLiteral("Registros de %1 bytes no deck aberto e de %2 no outro (número de postos diferente).")
                                                     .arg(a.tamanhoRegistro())
                                                     .arg(b.tamanhoRegistro()));
        return;
    }
    const int registros = std::max(a.registros(), b.registros());
    if (nome == QStringLiteral("postos.dat")) {
        for (int r = 0; r < registros; ++r) {
            auto descricao = [r](const ArquivoBinario& x) {
                if (r >= x.registros()) return QStringLiteral("(ausente)");
                return QStringLiteral("%1 %2-%3").arg(paraTexto(x.texto(r, 0, 12)).trimmed()).arg(x.inteiro(r, 12)).arg(x.inteiro(r, 16));
            };
            const QString va = descricao(a);
            const QString vb = descricao(b);
            if (va == vb) continue;
            ++d.alteracoes;
            d.linhas << linhaHtml(nullptr, {}, QStringLiteral("Posto %1: ").arg(r + 1)) + linhaHtml(COR_REMOVIDA, {}, va) + QStringLiteral(" → ") +
                            linhaHtml(COR_INCLUIDA, {}, vb);
        }
    } else {
        const int postos = a.tamanhoRegistro() / 4;
        for (int p = 0; p < postos && d.linhas.size() < MAXIMO_LINHAS; ++p) {
            int meses = 0;
            QStringList exemplos;
            for (int r = 0; r < registros; ++r) {
                const int va = r < a.registros() ? a.inteiro(r, 4 * p) : -1;
                const int vb = r < b.registros() ? b.inteiro(r, 4 * p) : -1;
                if (va == vb) continue;
                ++meses;
                if (exemplos.size() < 3) exemplos << QStringLiteral("registro %1: %2 → %3").arg(r + 1).arg(va).arg(vb);
            }
            if (meses == 0) continue;
            d.alteracoes += meses;
            d.linhas << linhaHtml(nullptr, {}, QStringLiteral("Posto %1: %2 (%3%4)")
                                                   .arg(p + 1)
                                                   .arg(contagem(meses, "mês diferente", "meses diferentes"),
                                                        exemplos.join(QStringLiteral("; ")), meses > 3 ? QStringLiteral("; ...") : QString()));
        }
        if (a.registros() != b.registros())
            d.linhas.prepend(linhaHtml(COR_CABECALHO, {}, QStringLiteral("%1 meses no deck aberto e %2 no outro").arg(a.registros()).arg(b.registros())));
    }
    d.situacao = contagem(d.alteracoes, "valor alterado", "valores alterados");
}
}  // namespace

// Diferencas entre o deck aberto e outro, arquivo por arquivo, na ordem do catalogo: os de texto
// linha a linha, o cadastro de usinas campo a campo e os binarios registro a registro. Arquivos iguais
// ficam de fora; arquivo que so um dos decks tem aparece como tal.
std::vector<DiferencaArquivo> compararDecks(const DadosDeck& atual, const ArquivoHidr* hidr_atual, const DadosDeck& outro,
                                            const ArquivoHidr* hidr_outro) {
    std::vector<DiferencaArquivo> resultado;
    std::vector<std::pair<QString, QString>> arquivos;
    for (const ArquivoNewave& a : catalogoNewave()) arquivos.push_back({a.nome_padrao, a.titulo});
    arquivos.push_back({QStringLiteral("modif.dat"), QStringLiteral("Modificações")});
    for (const auto& [nome, titulo] : arquivos) {
        DiferencaArquivo d{nome, titulo, {}, 0, {}};
        if (nome == QStringLiteral("hidr.dat")) {
            if (!hidr_atual || !hidr_outro) continue;
            compararHidr(*hidr_atual, *hidr_outro, d);
        } else if (DadosDeck::binario(nome)) {
            const ArquivoBinario* a = atual.arquivoBinario(nome);
            const ArquivoBinario* b = outro.arquivoBinario(nome);
            if (!a && !b) continue;
            if (!a || !b) {
                d.situacao = a ? QStringLiteral("só no deck aberto") : QStringLiteral("só no outro deck");
                d.alteracoes = 1;
            } else {
                compararBinarios(nome, *a, *b, d);
            }
        } else {
            const bool tem_a = atual.lido(nome);
            const bool tem_b = outro.lido(nome);
            if (!tem_a && !tem_b) continue;
            if (!tem_a || !tem_b) {
                d.situacao = tem_a ? QStringLiteral("só no deck aberto") : QStringLiteral("só no outro deck");
                d.alteracoes = 1;
            } else {
                compararTextos(separarLinhas(atual.texto(nome).toLatin1().toStdString()), separarLinhas(outro.texto(nome).toLatin1().toStdString()), d);
            }
        }
        if (d.alteracoes > 0) resultado.push_back(std::move(d));
    }
    return resultado;
}

// Lista dos arquivos que mudaram a esquerda e, a direita, as diferencas do arquivo escolhido.
DialogoComparacao::DialogoComparacao(const QString& pasta_atual, const QString& pasta_outra, std::vector<DiferencaArquivo> diferencas,
                                     QWidget* parent)
    : QDialog(parent), diferencas_(std::move(diferencas)) {
    setWindowTitle(QStringLiteral("Comparação de decks"));
    resize(1200, 720);
    auto* v = new QVBoxLayout(this);
    auto* cabecalho = new QLabel(QStringLiteral("<b>Deck aberto:</b> %1<br><b>Outro deck:</b> %2<br>"
                                                "<span style=\"color:%3\">- só no deck aberto</span> · <span style=\"color:%4\">+ só no outro deck</span>")
                                     .arg(pasta_atual.toHtmlEscaped(), pasta_outra.toHtmlEscaped(), QString::fromLatin1(COR_REMOVIDA),
                                          QString::fromLatin1(COR_INCLUIDA)),
                                 this);
    v->addWidget(cabecalho);
    auto* divisor = new QSplitter(Qt::Horizontal, this);
    lista_ = new QListWidget(divisor);
    detalhe_ = new QTextEdit(divisor);
    detalhe_->setReadOnly(true);
    detalhe_->setLineWrapMode(QTextEdit::NoWrap);
    detalhe_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    for (const DiferencaArquivo& d : diferencas_) lista_->addItem(QStringLiteral("%1 (%2): %3").arg(d.titulo, d.nome_padrao, d.situacao));
    if (diferencas_.empty()) lista_->addItem(QStringLiteral("Os decks são iguais"));
    divisor->addWidget(lista_);
    divisor->addWidget(detalhe_);
    divisor->setStretchFactor(1, 1);
    divisor->setSizes({380, 820});
    v->addWidget(divisor, 1);
    connect(lista_, &QListWidget::currentRowChanged, this, &DialogoComparacao::mostrar);
    if (!diferencas_.empty()) lista_->setCurrentRow(0);
}

void DialogoComparacao::mostrar(int indice) {
    if (indice < 0 || indice >= static_cast<int>(diferencas_.size())) return;
    const DiferencaArquivo& d = diferencas_[static_cast<size_t>(indice)];
    QString html = QStringLiteral("<div style=\"white-space:pre\">") + d.linhas.join(QStringLiteral("<br>"));
    if (d.linhas.size() >= MAXIMO_LINHAS) html += QStringLiteral("<br>") + linhaHtml(COR_CABECALHO, {}, QStringLiteral("(lista cortada em %1 linhas)").arg(MAXIMO_LINHAS));
    detalhe_->setHtml(html + QStringLiteral("</div>"));
}
