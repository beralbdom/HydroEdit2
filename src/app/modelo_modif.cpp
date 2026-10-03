#include "modelo_modif.h"
#include <QStringList>
#include <limits>
#include <set>
#include "dados_deck.h"
#include "deck_newave.h"
#include "modelo_hidr.h"
#include "modelo_secao_fixa.h"

namespace {
const QString MODIF = QStringLiteral("modif.dat");

QString rotuloUnidade(const QString& codigo) {
    if (codigo.compare(QStringLiteral("h"), Qt::CaseInsensitive) == 0) return QStringLiteral("hm3");
    if (codigo == QStringLiteral("%")) return QStringLiteral("%vu");
    return codigo;
}

bool numerico(TipoCampoModif tipo) { return tipo != TipoCampoModif::Unidade; }
}  // namespace

// Registros do modif.dat numa tabela editavel. Com uma palavra-chave escolhida, cada campo dela vira
// uma coluna (camposModif, com um patamar por coluna conforme o numero de patamares de carga do
// deck); com uma categoria, as colunas sao as comuns: modificador (palavra-chave), mes e ano (das
// palavras-chave com data) e os demais valores juntos, somente para leitura. Usina e nome nao se
// editam. Cada edicao reescreve so a linha do registro no repositorio do deck.
ModeloModif::ModeloModif(DadosDeck* dados, const ModeloHidr* hidr, QObject* parent)
    : QAbstractTableModel(parent), dados_(dados), hidr_(hidr), aceita_([](const QString&) { return true; }) {}

void ModeloModif::definirFiltro(const QString& chave, std::function<bool(const QString&)> aceita) {
    chave_ = chave;
    aceita_ = std::move(aceita);
    recarregar();
}

std::vector<CampoModif> ModeloModif::campos(const QString& chave) const {
    return camposModif(chave.toStdString(), dados_->numeroPatamaresDeCarga());
}

// Registros que passam no filtro, relidos do texto atual do modif.dat; o nome vem do cadastro de
// usinas e, sem ele, do comentario da linha USINA.
std::vector<ModeloModif::Linha> ModeloModif::lerLinhas() const {
    std::vector<Linha> linhas;
    if (!dados_->arquivo(MODIF)) return linhas;
    const ResultadoModif modif = interpretarModif(dados_->texto(MODIF).toLatin1().toStdString());
    for (const BlocoModif& bloco : modif.blocos) {
        QString nome = QString::fromLatin1(bloco.comentario.c_str());
        if (hidr_ && bloco.usina >= 1 && bloco.usina <= hidr_->numUsinas()) {
            const QString do_cadastro = QString::fromLatin1(hidr_->usina(bloco.usina - 1).nome.c_str()).trimmed();
            if (!do_cadastro.isEmpty()) nome = do_cadastro;
        }
        for (const RegistroModif& registro : bloco.registros) {
            const QString chave = QString::fromStdString(registro.palavra_chave);
            if (tipada() ? chave != chave_ : !aceita_(chave)) continue;
            linhas.push_back({bloco.usina, nome, chave, registro.valores, registro.linha - 1});
        }
    }
    return linhas;
}

void ModeloModif::recarregar() {
    beginResetModel();
    campos_ = tipada() ? campos(chave_) : std::vector<CampoModif>{};
    linhas_ = lerLinhas();
    endResetModel();
}

// Releitura depois de uma edicao que nao muda o numero de registros: a selecao e a celula atual da
// vista continuam onde estavam.
void ModeloModif::atualizarValores() {
    std::vector<Linha> novas = lerLinhas();
    if (novas.size() != linhas_.size()) {
        recarregar();
        return;
    }
    linhas_ = std::move(novas);
    if (!linhas_.empty()) emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

int ModeloModif::linhaDoArquivo(int registro) const { return linhas_[static_cast<size_t>(registro)].indice; }
int ModeloModif::usinaDoRegistro(int registro) const { return linhas_[static_cast<size_t>(registro)].usina; }
QString ModeloModif::chaveDoRegistro(int registro) const { return linhas_[static_cast<size_t>(registro)].chave; }

int ModeloModif::usinas() const {
    std::set<int> codigos;
    for (const Linha& l : linhas_) codigos.insert(l.usina);
    return static_cast<int>(codigos.size());
}

int ModeloModif::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(linhas_.size()); }

int ModeloModif::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return tipada() ? 2 + static_cast<int>(campos_.size()) : 6;
}

// Indice do campo da palavra-chave do registro mostrado na coluna, ou -1: na vista de uma
// palavra-chave, todos os campos; na vista geral, so mes e ano das palavras-chave com data.
int ModeloModif::campoDaColuna(const Linha& linha, int coluna) const {
    if (tipada()) return coluna >= 2 && coluna < 2 + static_cast<int>(campos_.size()) ? coluna - 2 : -1;
    if ((coluna == 3 || coluna == 4) && chaveComData(linha.chave.toStdString())) return coluna - 3;
    return -1;
}

QVariant ModeloModif::headerData(int secao, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    if (secao == 0) return QStringLiteral("Usina");
    if (secao == 1) return QStringLiteral("Nome");
    if (tipada()) return QString::fromStdString(campos_[static_cast<size_t>(secao - 2)].nome);
    static const QStringList gerais = {QStringLiteral("Modificador"), QStringLiteral("Mês"), QStringLiteral("Ano"), QStringLiteral("Valores")};
    return gerais.value(secao - 2);
}

QVariant ModeloModif::data(const QModelIndex& ix, int role) const {
    if (!ix.isValid()) return {};
    const Linha& linha = linhas_[static_cast<size_t>(ix.row())];
    const int coluna = ix.column();
    const std::vector<CampoModif> campos_linha = campos(linha.chave);
    const int campo = campoDaColuna(linha, coluna);
    auto texto = [&]() -> QString {
        if (coluna == 0) return QString::number(linha.usina);
        if (coluna == 1) return linha.nome;
        if (campo >= 0) {
            if (campo >= static_cast<int>(linha.tokens.size()) || campo >= static_cast<int>(campos_linha.size())) return {};
            return QString::fromLatin1(valorExibidoModif(campos_linha[static_cast<size_t>(campo)], linha.tokens[static_cast<size_t>(campo)]).c_str()).trimmed();
        }
        if (!tipada() && coluna == 2) return linha.chave;
        if (!tipada() && coluna == 5) {
            QStringList resto;
            for (size_t i = chaveComData(linha.chave.toStdString()) ? 2 : 0; i < linha.tokens.size(); ++i)
                resto << QString::fromLatin1(linha.tokens[i].c_str());
            return resto.join(QStringLiteral("  "));
        }
        return {};
    };
    const bool unidade = campo >= 0 && campo < static_cast<int>(campos_linha.size()) &&
                         campos_linha[static_cast<size_t>(campo)].tipo == TipoCampoModif::Unidade;
    if (role == Qt::EditRole) return texto();
    if (role == Qt::DisplayRole) return unidade ? rotuloUnidade(texto()) : texto();
    if (role == PAPEL_OPCOES && unidade)
        return QVariantList{QStringList{QStringLiteral("h"), rotuloUnidade(QStringLiteral("h"))},
                            QStringList{QStringLiteral("%"), rotuloUnidade(QStringLiteral("%"))}};
    const bool numero = coluna == 0 ||
                        (campo >= 0 && campo < static_cast<int>(campos_linha.size()) && numerico(campos_linha[static_cast<size_t>(campo)].tipo));
    if (role == Qt::TextAlignmentRole) return int((numero ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    if (role == Qt::UserRole) {
        bool ok = false;
        const double v = texto().toDouble(&ok);
        if (numero) return ok ? QVariant(v) : QVariant(-std::numeric_limits<double>::infinity());
        return texto();
    }
    if (role == Qt::ToolTipRole && !tipada() && coluna == 5) return QStringLiteral("Escolha o modificador na árvore para editar estes valores");
    return {};
}

Qt::ItemFlags ModeloModif::flags(const QModelIndex& ix) const {
    Qt::ItemFlags f = QAbstractTableModel::flags(ix);
    if (!ix.isValid()) return f;
    const Linha& linha = linhas_[static_cast<size_t>(ix.row())];
    const int campo = campoDaColuna(linha, ix.column());
    if (campo >= 0 && campo < static_cast<int>(campos(linha.chave).size())) f |= Qt::ItemIsEditable;
    return f;
}

void ModeloModif::iniciarLote() { dados_->iniciarLote(); }
void ModeloModif::concluirLote() { dados_->concluirLote(); }

// Valida o valor contra o campo (formatarCampoModif), remonta a linha do registro e a grava no
// repositorio do deck; a unidade conserva as aspas que a linha ja usava (e ganha aspas num campo novo).
// Valor igual ao atual nao regrava a linha, para colar o que ja esta la nao mudar o espacamento. O
// registro guarda os campos novos na hora: num lote o aviso do repositorio so vem no fim, e a proxima
// celula da mesma linha parte deles.
bool ModeloModif::setData(const QModelIndex& ix, const QVariant& valor, int role) {
    if (!ix.isValid() || role != Qt::EditRole) return false;
    if (valor.toString().trimmed() == data(ix, Qt::EditRole).toString()) return false;
    const Linha& linha = linhas_[static_cast<size_t>(ix.row())];
    const int campo = campoDaColuna(linha, ix.column());
    const std::vector<CampoModif> campos_linha = campos(linha.chave);
    if (campo < 0 || campo >= static_cast<int>(campos_linha.size())) return false;
    std::vector<std::string> tokens = linha.tokens;
    if (tokens.size() <= static_cast<size_t>(campo)) tokens.resize(static_cast<size_t>(campo) + 1);
    const std::string& anterior = tokens[static_cast<size_t>(campo)];
    const bool aspas = anterior.empty() || anterior.front() == '\'';
    std::string token;
    Resultado r = formatarCampoModif(campos_linha[static_cast<size_t>(campo)], valor.toString().toLatin1().toStdString(), aspas, token);
    if (r.ok) {
        tokens[static_cast<size_t>(campo)] = token;
        const std::string original = dados_->linha(MODIF, linha.indice).toLatin1().toStdString();
        std::string nova;
        r = montarLinhaModif(original, tokens, campos_linha, nova);
        const int indice = linha.indice;
        if (r.ok && nova != original) r = dados_->substituirLinha(MODIF, indice, QString::fromLatin1(nova.c_str()));
        if (r.ok && ix.row() < rowCount() && linhas_[static_cast<size_t>(ix.row())].indice == indice)
            linhas_[static_cast<size_t>(ix.row())].tokens = tokens;
    }
    if (!r.ok) {
        emit valorRecusado(QString::fromUtf8(r.mensagem));
        return false;
    }
    return true;
}
