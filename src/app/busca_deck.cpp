#include "busca_deck.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <map>
#include <set>
#include "catalogo_newave.h"
#include "dados_deck.h"
#include "deck_newave.h"
#include "layouts_newave.h"
#include "modelo_hidr.h"

namespace {
constexpr int MAXIMO_RESULTADOS = 5000;
constexpr int PAPEL_NOME = Qt::UserRole + 1;
constexpr int PAPEL_SECAO = Qt::UserRole + 2;
constexpr int PAPEL_REGISTRO = Qt::UserRole + 3;

QString paraTexto(const std::string& s) { return QString::fromLatin1(s.c_str()); }

struct Alvo {
    QString codigo;
    QString rotulo;
};
}  // namespace

// Procura a consulta (nome ou codigo) em todo o deck. Uma linha entra quando contem o texto, sem
// diferenciar maiusculas, ou quando uma coluna de referencia dela aponta para um cadastro que a
// consulta identifica: o codigo exato (6 acha a usina 6, o REE 6, o posto 6...) ou o nome exato
// ("FURNAS" acha a usina 6 tambem onde so aparece o codigo). Coluna de referencia de uma linha que
// abre bloco conta uma vez, na propria linha, e nao em cada registro do bloco. No modif.dat valem o
// texto e a usina de cada bloco; no cadastro de usinas e no postos.dat, o codigo e o nome. A lista
// para em MAXIMO_RESULTADOS.
std::vector<ResultadoBusca> procurarNoDeck(const DadosDeck& dados, const ModeloHidr* hidr, const QString& consulta) {
    std::vector<ResultadoBusca> resultados;
    const QString q = consulta.trimmed();
    if (q.isEmpty()) return resultados;
    bool numero = false;
    const int codigo_consultado = q.toInt(&numero);

    std::map<Referencia, std::map<QString, QString>> alvos;
    for (Referencia r : {Referencia::Submercado, Referencia::Ree, Referencia::UsinaHidro, Referencia::UsinaTermica,
                         Referencia::ClasseTermica, Referencia::Tecnologia, Referencia::Posto, Referencia::Agrupamento}) {
        for (const OpcaoReferencia& o : dados.opcoes(r)) {
            const QString sufixo = QStringLiteral(" (%1)").arg(o.codigo);
            const QString nome = o.rotulo.endsWith(sufixo) ? o.rotulo.chopped(sufixo.size()) : o.rotulo;
            const bool pelo_codigo = numero && DadosDeck::normalizarCodigo(o.codigo) == QString::number(codigo_consultado);
            if (pelo_codigo || nome.trimmed().compare(q, Qt::CaseInsensitive) == 0) alvos[r][DadosDeck::normalizarCodigo(o.codigo)] = o.rotulo;
        }
    }
    auto cheio = [&] { return static_cast<int>(resultados.size()) >= MAXIMO_RESULTADOS; };

    if (hidr) {
        for (int i = 0; i < hidr->numUsinas() && !cheio(); ++i) {
            const QString nome = paraTexto(hidr->usina(i).nome).trimmed();
            if (nome.isEmpty()) continue;
            const bool pelo_codigo = numero && codigo_consultado == i + 1;
            if (pelo_codigo || nome.contains(q, Qt::CaseInsensitive))
                resultados.push_back({QStringLiteral("hidr.dat"), -1, -1, i, QStringLiteral("Usina %1").arg(i + 1), nome,
                                      pelo_codigo ? QStringLiteral("código") : QStringLiteral("nome")});
        }
    }

    std::vector<QString> nomes;
    for (const ArquivoNewave& a : catalogoNewave()) nomes.push_back(a.nome_padrao);
    nomes.push_back(QStringLiteral("modif.dat"));
    for (const QString& nome : nomes) {
        if (cheio()) break;
        if (nome == QStringLiteral("postos.dat")) {
            const ArquivoBinario* postos = dados.arquivoBinario(nome);
            for (int r = 0; postos && r < postos->registros() && !cheio(); ++r) {
                const QString texto = paraTexto(postos->texto(r, postos_dat::NOME, postos_dat::TAMANHO_NOME)).trimmed();
                if (texto.isEmpty()) continue;
                const bool pelo_codigo = numero && codigo_consultado == r + 1;
                if (pelo_codigo || texto.contains(q, Qt::CaseInsensitive))
                    resultados.push_back({nome, -1, -1, r, QStringLiteral("Posto %1").arg(r + 1), texto,
                                          pelo_codigo ? QStringLiteral("código") : QStringLiteral("nome")});
            }
            continue;
        }
        const ArquivoFixo* arquivo = dados.arquivo(nome);
        if (!arquivo) continue;
        const auto& linhas = arquivo->linhas();
        std::map<int, ResultadoBusca> achados;
        std::map<int, std::pair<int, int>> registro_da_linha;
        for (int s = 0; s < static_cast<int>(arquivo->secoes().size()); ++s) {
            const SecaoLida& secao = arquivo->secoes()[static_cast<size_t>(s)];
            for (int r = 0; r < static_cast<int>(secao.linhas.size()); ++r) {
                registro_da_linha.try_emplace(secao.linhas[static_cast<size_t>(r)], s, r);
                if (static_cast<size_t>(r) < secao.linhas_contexto.size())
                    for (int linha : secao.linhas_contexto[static_cast<size_t>(r)]) registro_da_linha.try_emplace(linha, s, r);
                const auto& colunas = secao.definicao.colunas;
                for (int c = 0; c < static_cast<int>(colunas.size()); ++c) {
                    const ColunaFixa& coluna = colunas[static_cast<size_t>(c)];
                    const auto alvo = alvos.find(coluna.referencia);
                    if (alvo == alvos.end()) continue;
                    const QString valor = DadosDeck::normalizarCodigo(paraTexto(arquivo->valor(s, r, c)));
                    const auto rotulo = alvo->second.find(valor);
                    if (rotulo == alvo->second.end()) continue;
                    int linha = secao.linhas[static_cast<size_t>(r)];
                    if (coluna.contexto >= 0 && static_cast<size_t>(r) < secao.linhas_contexto.size() &&
                        static_cast<size_t>(coluna.contexto) < secao.linhas_contexto[static_cast<size_t>(r)].size())
                        linha = secao.linhas_contexto[static_cast<size_t>(r)][static_cast<size_t>(coluna.contexto)];
                    achados.try_emplace(linha, ResultadoBusca{nome, linha, s, r, QString::fromStdString(secao.definicao.titulo), {},
                                                              QStringLiteral("%1: %2").arg(QString::fromStdString(coluna.nome), rotulo->second)});
                }
            }
        }
        if (nome == QStringLiteral("modif.dat")) {
            const auto usinas = alvos.find(Referencia::UsinaHidro);
            const ResultadoModif modif = interpretarModif(arquivo->conteudo());
            for (const BlocoModif& bloco : modif.blocos)
                if (usinas != alvos.end() && usinas->second.count(QString::number(bloco.usina)))
                    achados.try_emplace(bloco.linha - 1, ResultadoBusca{nome, bloco.linha - 1, -1, -1, QStringLiteral("USINA"), {},
                                                                         QStringLiteral("Usina: %1").arg(usinas->second.at(QString::number(bloco.usina)))});
        }
        for (int i = 0; i < static_cast<int>(linhas.size()); ++i)
            if (paraTexto(linhas[static_cast<size_t>(i)]).contains(q, Qt::CaseInsensitive)) {
                const auto registro = registro_da_linha.find(i);
                const int s = registro == registro_da_linha.end() ? -1 : registro->second.first;
                const int r = registro == registro_da_linha.end() ? -1 : registro->second.second;
                const QString onde = s < 0 ? QString() : QString::fromStdString(arquivo->secoes()[static_cast<size_t>(s)].definicao.titulo);
                achados.try_emplace(i, ResultadoBusca{nome, i, s, r, onde, {}, QStringLiteral("texto")});
            }
        for (auto& [linha, achado] : achados) {
            if (cheio()) break;
            achado.texto = paraTexto(linhas[static_cast<size_t>(linha)]).trimmed();
            resultados.push_back(achado);
        }
    }
    return resultados;
}

// Busca no deck inteiro, sem bloquear a janela: os resultados ficam agrupados por arquivo, e um
// duplo clique (ou Enter) leva ao arquivo e ao registro.
DialogoBusca::DialogoBusca(const DadosDeck* dados, const ModeloHidr* hidr, QWidget* parent)
    : QDialog(parent), dados_(dados), hidr_(hidr) {
    setWindowTitle(QStringLiteral("Procurar no deck"));
    resize(900, 560);
    auto* v = new QVBoxLayout(this);
    auto* linha = new QHBoxLayout;
    consulta_ = new QLineEdit(this);
    consulta_->setPlaceholderText(QStringLiteral("Nome ou código de usina, REE, submercado, posto, classe térmica..."));
    consulta_->setClearButtonEnabled(true);
    auto* botao = new QPushButton(QStringLiteral("Procurar"), this);
    botao->setDefault(true);
    linha->addWidget(consulta_, 1);
    linha->addWidget(botao);
    v->addLayout(linha);
    resumo_ = new QLabel(this);
    resumo_->setEnabled(false);
    v->addWidget(resumo_);
    lista_ = new QTreeView(this);
    itens_ = new QStandardItemModel(this);
    itens_->setHorizontalHeaderLabels({QStringLiteral("Arquivo / linha"), QStringLiteral("Seção"), QStringLiteral("Motivo"), QStringLiteral("Conteúdo")});
    lista_->setModel(itens_);
    lista_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lista_->setUniformRowHeights(true);
    lista_->header()->setStretchLastSection(true);
    v->addWidget(lista_, 1);
    connect(botao, &QPushButton::clicked, this, &DialogoBusca::procurar);
    connect(consulta_, &QLineEdit::returnPressed, this, &DialogoBusca::procurar);
    connect(lista_, &QTreeView::activated, this, &DialogoBusca::escolher);
}

void DialogoBusca::procurar() {
    itens_->removeRows(0, itens_->rowCount());
    const std::vector<ResultadoBusca> resultados = procurarNoDeck(*dados_, hidr_, consulta_->text());
    std::map<QString, QStandardItem*> grupos;
    for (const ResultadoBusca& r : resultados) {
        QStandardItem*& grupo = grupos[r.nome_padrao];
        if (!grupo) {
            grupo = new QStandardItem(dados_->nomeNoDeck(r.nome_padrao));
            grupo->setData(r.nome_padrao, PAPEL_NOME);
            grupo->setData(-1, PAPEL_SECAO);
            grupo->setData(-1, PAPEL_REGISTRO);
            itens_->appendRow(grupo);
        }
        auto* onde = new QStandardItem(r.linha >= 0 ? QStringLiteral("Linha %1").arg(r.linha + 1) : r.onde);
        onde->setData(r.nome_padrao, PAPEL_NOME);
        onde->setData(r.secao, PAPEL_SECAO);
        onde->setData(r.registro, PAPEL_REGISTRO);
        grupo->appendRow({onde, new QStandardItem(r.linha >= 0 ? r.onde : QString()), new QStandardItem(r.motivo), new QStandardItem(r.texto)});
    }
    for (auto& [nome, grupo] : grupos) grupo->setText(QStringLiteral("%1 (%2)").arg(grupo->text()).arg(grupo->rowCount()));
    resumo_->setText(resultados.empty() ? QStringLiteral("Nada encontrado")
                                        : QStringLiteral("%1 %2 em %3 %4")
                                              .arg(resultados.size())
                                              .arg(resultados.size() == 1 ? QStringLiteral("resultado") : QStringLiteral("resultados"))
                                              .arg(grupos.size())
                                              .arg(grupos.size() == 1 ? QStringLiteral("arquivo") : QStringLiteral("arquivos")));
    lista_->expandAll();
    for (int c = 0; c < 3; ++c) lista_->resizeColumnToContents(c);
}

void DialogoBusca::escolher(const QModelIndex& indice) {
    const QModelIndex primeira = indice.siblingAtColumn(0);
    const QString nome = primeira.data(PAPEL_NOME).toString();
    if (!nome.isEmpty()) emit escolhido(nome, primeira.data(PAPEL_SECAO).toInt(), primeira.data(PAPEL_REGISTRO).toInt());
}
