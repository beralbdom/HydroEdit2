#include "modelo_hidr.h"
#include <QLocale>
#include <algorithm>
#include <map>
#include "comandos.h"
#include "texto.h"

namespace {
// Rotulo curto de cada coluna da tabela, com a unidade quando o campo tem uma (as mesmas do
// formulario). O nome do campo no arquivo continua visivel na dica do cabecalho.
QString rotuloDaColuna(std::string_view nome) {
    static const std::map<std::string_view, QString> rotulos = {
        {"codigo", QStringLiteral("Código")},
        {"nome", QStringLiteral("Nome")},
        {"posto", QStringLiteral("Posto")},
        {"posto_bdh", QStringLiteral("Posto BDH")},
        {"subsistema", QStringLiteral("Subsistema")},
        {"empresa", QStringLiteral("Empresa")},
        {"jusante", QStringLiteral("Jusante")},
        {"desvio", QStringLiteral("Desvio")},
        {"volume_minimo", QStringLiteral("Vol. mín. (hm³)")},
        {"volume_maximo", QStringLiteral("Vol. máx. (hm³)")},
        {"volume_vertedouro", QStringLiteral("Vol. vertedouro (hm³)")},
        {"volume_desvio", QStringLiteral("Vol. desvio (hm³)")},
        {"cota_minima", QStringLiteral("Cota mín. (m)")},
        {"cota_maxima", QStringLiteral("Cota máx. (m)")},
        {"num_conjuntos", QStringLiteral("Conjuntos")},
        {"produtibilidade", QStringLiteral("Produtibilidade")},
        {"perdas", QStringLiteral("Perdas")},
        {"num_pol_jusante", QStringLiteral("Pol. jusante")},
        {"canal_fuga_medio", QStringLiteral("Canal de fuga (m)")},
        {"influencia_vertimento", QStringLiteral("Infl. vertimento")},
        {"fator_carga_maximo", QStringLiteral("FC máx. (%)")},
        {"fator_carga_minimo", QStringLiteral("FC mín. (%)")},
        {"vazao_minima_historica", QStringLiteral("Vazão mín. hist. (m³/s)")},
        {"num_unidades_base", QStringLiteral("Unid. base")},
        {"tipo_turbina", QStringLiteral("Tipo de turbina")},
        {"representacao_conjunto", QStringLiteral("Repr. conjunto")},
        {"teif", QStringLiteral("TEIF (%)")},
        {"ip", QStringLiteral("IP (%)")},
        {"tipo_perda", QStringLiteral("Tipo de perda")},
        {"data", QStringLiteral("Data")},
        {"observacao", QStringLiteral("Observação")},
        {"volume_referencia", QStringLiteral("Vol. referência (hm³)")},
        {"regulacao", QStringLiteral("Regulação")},
    };
    auto it = rotulos.find(nome);
    return it != rotulos.end() ? it->second : QString::fromLatin1(nome.data(), static_cast<int>(nome.size()));
}

// Campos inteiros que a tabela mostra como "codigo nome" (os mesmos de ModeloHidr::nomeLookup). O
// texto comeca pelo codigo mas termina num nome, entao alinha a esquerda como as colunas de texto.
bool campoComNome(std::string_view nome) {
    return nome == "subsistema" || nome == "posto" || nome == "empresa" || nome == "tipo_turbina" ||
           nome == "jusante" || nome == "desvio";
}
}  // namespace

QString textoValor(const Valor& v) {
    if (const std::string* s = std::get_if<std::string>(&v)) return QString::fromLatin1(s->c_str());
    if (const int32_t* i = std::get_if<int32_t>(&v)) return QString::number(*i);
    return QString::number(std::get<float>(v), 'g', 7);
}

std::optional<Valor> valorDeTexto(const Campo& c, const QString& texto) {
    QString t = texto.trimmed();
    switch (c.tipo) {
        case TipoCampo::Texto: {
            if (t.size() > c.tamanho_elemento) return std::nullopt;
            return Valor{std::string(t.toLatin1().constData())};
        }
        case TipoCampo::Inteiro: {
            bool ok = false;
            int v = t.toInt(&ok);
            if (!ok) return std::nullopt;
            return Valor{static_cast<int32_t>(v)};
        }
        case TipoCampo::Real: {
            bool ok = false;
            double d = QLocale::c().toDouble(t.replace(',', '.'), &ok);
            if (!ok) return std::nullopt;
            return Valor{static_cast<float>(d)};
        }
    }
    return std::nullopt;
}

ModeloHidr::ModeloHidr(QObject* parent) : QAbstractTableModel(parent) {
    for (const Campo& c : campos())
        if (c.escalar()) colunas_.push_back(&c);
}

void ModeloHidr::definirArquivo(ArquivoHidr arquivo, const QString& caminho) {
    beginResetModel();
    arquivo_ = std::move(arquivo);
    caminho_ = caminho;
    pilha_.clear();
    endResetModel();
}

void ModeloHidr::definirLookup(DeckLookup l) {
    lookup_ = std::move(l);
    if (rowCount() > 0) emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::DisplayRole});
}

int ModeloHidr::colunaDoCampo(std::string_view nome) const {
    for (size_t i = 0; i < colunas_.size(); ++i)
        if (colunas_[i]->nome == nome) return static_cast<int>(i) + 1;
    return -1;
}

const Campo* ModeloHidr::campoDaColuna(int coluna) const {
    if (coluna < 1 || coluna > static_cast<int>(colunas_.size())) return nullptr;
    return colunas_[static_cast<size_t>(coluna) - 1];
}

void ModeloHidr::definirValor(int linha, const Campo& c, int i, const Valor& v) {
    Valor antigo = valor(linha, c, i);
    if (antigo == v) return;
    if (lote_) {
        new ComandoDefinirValor(this, linha, &c, i, antigo, v, lote_);
        aplicarValor(linha, c, i, v);
        return;
    }
    pilha_.push(new ComandoDefinirValor(this, linha, &c, i, antigo, v));
}

// Colagem: as edicoes entram como filhas de um so comando, que se desfaz de uma vez, e os avisos de
// usina alterada ficam para o fim (liberarAvisos).
void ModeloHidr::iniciarLote() {
    if (profundidade_lote_++ > 0) return;
    lote_ = new ComandoLote(this, QStringLiteral("Colar"));
    adiarAvisos();
}

void ModeloHidr::concluirLote() {
    if (profundidade_lote_ == 0 || --profundidade_lote_ > 0) return;
    QUndoCommand* lote = std::exchange(lote_, nullptr);
    if (lote->childCount() > 0) pilha_.push(lote);
    else delete lote;
    liberarAvisos();
}

void ModeloHidr::adiarAvisos() { ++adiando_; }

// Fim de um lote: uma edicao so sai como de costume; varias da mesma usina saem como a usina inteira
// alterada (campo nulo), e de usinas diferentes como linha -1, para quem escuta refazer o que depende
// de qualquer usina (lista de nomes, cascata, validacao) uma vez so.
void ModeloHidr::liberarAvisos() {
    if (adiando_ == 0 || --adiando_ > 0) return;
    const auto avisos = std::exchange(avisos_adiados_, {});
    if (avisos.empty()) return;
    if (avisos.size() == 1) {
        emit usinaAlterada(avisos.front().first, avisos.front().second);
        return;
    }
    const bool mesma_usina = std::all_of(avisos.begin(), avisos.end(), [&](const auto& a) { return a.first == avisos.front().first; });
    emit usinaAlterada(mesma_usina ? avisos.front().first : -1, nullptr);
}

void ModeloHidr::avisarUsina(int linha, const Campo* campo) {
    if (adiando_ > 0) avisos_adiados_.push_back({linha, campo});
    else emit usinaAlterada(linha, campo);
}

void ModeloHidr::substituirUsina(int linha, const UsinaHidr& nova, const QString& rotulo) {
    pilha_.push(new ComandoSubstituirUsina(this, linha, usina(linha), nova, rotulo));
}

void ModeloHidr::aplicarValor(int linha, const Campo& c, int i, const Valor& v) {
    c.definir(arquivo_.usinas[static_cast<size_t>(linha)], i, v);
    if (c.escalar()) {
        QModelIndex ix = index(linha, colunaDoCampo(c.nome));
        emit dataChanged(ix, ix);
    }
    avisarUsina(linha, &c);
}

void ModeloHidr::aplicarUsina(int linha, const UsinaHidr& u) {
    arquivo_.usinas[static_cast<size_t>(linha)] = u;
    emit dataChanged(index(linha, 0), index(linha, columnCount() - 1));
    avisarUsina(linha, nullptr);
}

QString ModeloHidr::nomeLookup(const Campo& c, int32_t codigo) const {
    std::string nome;
    if (c.nome == "subsistema") nome = lookup_.nomeSubsistema(codigo);
    else if (c.nome == "posto") nome = lookup_.nomePosto(codigo);
    else if (c.nome == "empresa") nome = lookup_.nomeEmpresa(codigo);
    else if (c.nome == "tipo_turbina") nome = lookup_.nomeTurbina(codigo);
    else if ((c.nome == "jusante" || c.nome == "desvio") && codigo >= 1 && codigo <= numUsinas())
        nome = usina(codigo - 1).nome;
    return QString::fromLatin1(nome.c_str());
}

int ModeloHidr::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : numUsinas(); }
int ModeloHidr::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(colunas_.size()) + 1;
}

QVariant ModeloHidr::data(const QModelIndex& ix, int role) const {
    if (!ix.isValid() || ix.row() >= rowCount() || ix.column() >= columnCount()) return {};
    if (ix.column() == 0) {
        if (role == Qt::DisplayRole || role == Qt::EditRole) return QString::number(ix.row() + 1);
        if (role == Qt::UserRole) return ix.row() + 1;
        if (role == Qt::TextAlignmentRole) return int(Qt::AlignRight | Qt::AlignVCenter);
        return {};
    }
    const Campo& c = *campoDaColuna(ix.column());
    Valor v = valor(ix.row(), c, 0);
    switch (role) {
        case Qt::EditRole: return textoValor(v);
        case Qt::DisplayRole: {
            QString t = textoValor(v);
            if (c.tipo == TipoCampo::Inteiro) {
                QString nome = nomeLookup(c, std::get<int32_t>(v));
                if (!nome.isEmpty()) t += QLatin1Char(' ') + nome;
            }
            return t;
        }
        case Qt::UserRole:
            if (c.tipo == TipoCampo::Inteiro) return std::get<int32_t>(v);
            if (c.tipo == TipoCampo::Real) return static_cast<double>(std::get<float>(v));
            return textoValor(v);
        case Qt::TextAlignmentRole:
            return c.tipo == TipoCampo::Texto || campoComNome(c.nome) ? int(Qt::AlignLeft | Qt::AlignVCenter)
                                                                       : int(Qt::AlignRight | Qt::AlignVCenter);
        default: return {};
    }
}

bool ModeloHidr::setData(const QModelIndex& ix, const QVariant& value, int role) {
    if (role != Qt::EditRole || !ix.isValid() || ix.column() == 0) return false;
    const Campo& c = *campoDaColuna(ix.column());
    std::optional<Valor> v = valorDeTexto(c, value.toString());
    if (!v) return false;
    definirValor(ix.row(), c, 0, *v);
    return true;
}

QVariant ModeloHidr::headerData(int section, Qt::Orientation o, int role) const {
    if (o == Qt::Vertical) return role == Qt::DisplayRole ? QVariant(section + 1) : QVariant();
    std::string_view nome = section == 0 ? std::string_view("codigo") : campoDaColuna(section)->nome;
    if (role == Qt::DisplayRole) return rotuloDaColuna(nome);
    if (role == Qt::ToolTipRole) return QString::fromLatin1(nome.data(), static_cast<int>(nome.size()));
    return {};
}

Qt::ItemFlags ModeloHidr::flags(const QModelIndex& ix) const {
    Qt::ItemFlags f = QAbstractTableModel::flags(ix);
    if (ix.isValid() && ix.column() > 0) f |= Qt::ItemIsEditable;
    return f;
}
