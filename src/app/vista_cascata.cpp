#include "vista_cascata.h"
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include "legenda_cascata.h"
#include "modelo_hidr.h"

namespace {
constexpr double LIMIAR_ROTULO = 0.9;
constexpr double ESCALA_MINIMA = 0.05;
constexpr double ESCALA_MAXIMA = 20.0;
constexpr double FATOR_ZOOM = 1.15;
constexpr double OPACIDADE_ENTRE_GRUPOS = 0.5;
constexpr int MARGEM_LEGENDA = 8;

QColor corDoGrupo(int indice) {
    static const std::array<QColor, 12> cores = {
        QColor(0x4e, 0x79, 0xa7), QColor(0xf2, 0x8e, 0x2b), QColor(0xe1, 0x57, 0x59),
        QColor(0x76, 0xb7, 0xb2), QColor(0x59, 0xa1, 0x4f), QColor(0xed, 0xc9, 0x48),
        QColor(0xb0, 0x7a, 0xa1), QColor(0xff, 0x9d, 0xa7), QColor(0x9c, 0x75, 0x5f),
        QColor(0xba, 0xb0, 0xac), QColor(0x86, 0xbc, 0xb6), QColor(0xd3, 0x72, 0x95),
    };
    return cores[static_cast<size_t>(((indice % 12) + 12) % 12)];
}

QString paraTexto(const std::string& s) { return QString::fromLatin1(s.c_str()); }

// Indice de cor de cada codigo de grupo pela posicao dele entre todos os codigos que o deck inteiro
// produz, e nao entre os grupos desenhados: assim um grupo nao troca de cor quando um filtro deixa
// so parte dos grupos na tela. O grupo 0 ("Sem grupo") so entra quando existe usina do deck fora do
// mapa de agrupamento.
std::map<int, int> indiceDeCorDosGrupos(const std::vector<UsinaHidr>& usinas,
                                        const std::map<int, int>& grupo_da_usina) {
    std::set<int> codigos;
    for (const auto& [codigo_usina, grupo] : grupo_da_usina) codigos.insert(grupo);
    for (size_t i = 0; i < usinas.size(); ++i) {
        if (usinas[i].vazia()) continue;
        if (!grupo_da_usina.contains(static_cast<int>(i) + 1)) {
            codigos.insert(0);
            break;
        }
    }

    std::map<int, int> indice;
    int proximo = 0;
    for (int codigo : codigos) indice[codigo] = proximo++;
    return indice;
}
}  // namespace

VistaCascata::VistaCascata(ModeloHidr* modelo, QWidget* parent) : QGraphicsView(parent), modelo_(modelo) {
    cena_ = new QGraphicsScene(this);
    setScene(cena_);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    legenda_ = new LegendaCascata(viewport());

    connect(modelo_, &QAbstractItemModel::modelReset, this, &VistaCascata::aoResetarModelo);
}

// Conexao unica com o reset do modelo: liga a flag de ajuste automatico e reconstroi a cena
// aqui mesmo, sem depender da ordem de conexao com nenhum sinal externo.
void VistaCascata::aoResetarModelo() {
    ajustar_no_proximo_ = true;
    reconstruir();
}

// Monta os dois mapas que empacotarPorGrupo espera: o REE de cada usina, direto do confhd.dat, e o
// nome de cada REE, do ree.dat. O codigo 0 e batizado aqui porque e ele que recebe as usinas do
// hidr.dat que o deck nao coloca em nenhum REE.
void VistaCascata::mapasDeRee(std::map<int, int>& ree_da_usina, std::map<int, std::string>& nome_do_ree) const {
    const DeckLookup& lookup = modelo_->lookup();
    ree_da_usina = lookup.ree_da_usina;
    nome_do_ree.clear();
    nome_do_ree[0] = "Sem REE";
    for (const auto& [codigo, ree] : lookup.rees) nome_do_ree[codigo] = ree.nome;
}

void VistaCascata::desenhar(const Cascata& c, const std::unordered_map<int, QColor>& cor_do_no,
                            const std::vector<QColor>& cor_do_grupo,
                            const std::vector<std::pair<QString, QColor>>& legenda) {
    for (size_t i = 0; i < c.grupos.size() && i < cor_do_grupo.size(); ++i) {
        const GrupoCascata& grupo = c.grupos[i];
        QString titulo = QStringLiteral("%1 (%2 usinas)").arg(paraTexto(grupo.nome)).arg(grupo.num_usinas);
        criarFaixaCascata(cena_, grupo, titulo, cor_do_grupo[i], palette(), font());
    }
    legenda_->definirGrupos(legenda);
    posicionarLegenda();

    auto ao_pairar = [this](int codigo, bool entrou) {
        if (entrou) codigo_sob_mouse_ = codigo;
        else if (codigo_sob_mouse_ == codigo) codigo_sob_mouse_ = -1;
        atualizarRotulos();
    };

    for (const NoCascata& no : c.nos) {
        auto it_cor = cor_do_no.find(no.codigo);
        QColor cor = it_cor == cor_do_no.end() ? corDoGrupo(0) : it_cor->second;
        QString rotulo = QStringLiteral("%1  %2").arg(no.codigo).arg(paraTexto(modelo_->usina(no.codigo - 1).nome));
        ItensNoCascata itens = criarPontoCascata(cena_, no, rotulo, cor, palette(), font(), ao_pairar);
        QString descricao = descricaoDoNo(no.codigo);
        itens.ponto->setToolTip(descricao);
        itens.rotulo->setToolTip(descricao);
        nos_[no.codigo] = itens;
    }

    for (const ArestaCascata& aresta : c.arestas) {
        auto it_origem = nos_.find(aresta.origem);
        auto it_destino = nos_.find(aresta.destino);
        if (it_origem == nos_.end() || it_destino == nos_.end()) continue;
        ItensArestaCascata itens = criarArestaCascata(cena_, it_origem->second.ponto->pos(),
                                                      it_destino->second.ponto->pos(), aresta.desvio, palette());
        double opacidade_base = aresta.entre_grupos ? OPACIDADE_ENTRE_GRUPOS : 1.0;
        itens.linha->setOpacity(opacidade_base);
        itens.seta->setOpacity(opacidade_base);
        arestas_.push_back({itens.linha, itens.seta, aresta.origem, aresta.destino, opacidade_base});
    }
}

// As faixas sao sempre por REE, uma por REE presente no que esta sendo exibido. Ordem de restricao:
// o modo "so a cascata da usina selecionada" tem prioridade sobre os filtros de REE e submercado,
// que valem juntos (uma usina aparece se o REE dela esta na selecao de REEs, ou a selecao esta
// vazia, E o submercado dela esta na de submercados, ou ela esta vazia). Restringir e sempre a
// mesma coisa: uma copia do deck com as usinas de fora zeradas, para o layout sair pelo mesmo
// caminho em todos os casos. As cores sao indexadas pela lista de REEs do deck inteiro, entao um
// REE mantem a mesma cor com e sem filtro.
void VistaCascata::reconstruir() {
    int selecionado_anterior = codigo_selecionado_;

    cena_->clear();
    nos_.clear();
    casa_filtro_.clear();
    arestas_.clear();
    codigo_selecionado_ = -1;
    codigo_sob_mouse_ = -1;

    const std::vector<UsinaHidr>& usinas_deck = modelo_->arquivo().usinas;
    const DeckLookup& lookup = modelo_->lookup();

    std::vector<bool> manter(usinas_deck.size() + 1, true);
    bool restrito = false;
    if (so_selecionada_ && selecionado_anterior > 0) {
        manter.assign(usinas_deck.size() + 1, false);
        for (int codigo : cascataDaUsina(usinas_deck, selecionado_anterior)) manter[static_cast<size_t>(codigo)] = true;
        restrito = true;
    } else if (!rees_filtro_.empty() || !submercados_filtro_.empty()) {
        for (size_t i = 0; i < usinas_deck.size(); ++i) {
            int ree = lookup.reeDaUsina(static_cast<int>(i) + 1);
            bool casa_ree = rees_filtro_.empty() || rees_filtro_.contains(ree);
            bool casa_submercado =
                submercados_filtro_.empty() || submercados_filtro_.contains(lookup.submercadoDoRee(ree));
            manter[i + 1] = casa_ree && casa_submercado;
        }
        restrito = true;
    }

    std::vector<UsinaHidr> exibidas = usinas_deck;
    if (restrito) {
        for (size_t i = 0; i < exibidas.size(); ++i) {
            if (!manter[i + 1]) exibidas[i].nome.clear();
        }
    }

    int num_exibidas = 0;
    for (const UsinaHidr& usina : exibidas) {
        if (!usina.vazia()) ++num_exibidas;
    }
    int largura_maxima =
        std::max(8, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(num_exibidas) * 2.5))));

    std::map<int, int> ree_da_usina;
    std::map<int, std::string> nome_do_ree;
    mapasDeRee(ree_da_usina, nome_do_ree);
    Cascata c = empacotarPorGrupo(exibidas, ree_da_usina, nome_do_ree, largura_maxima);

    std::map<int, int> indice_cor = indiceDeCorDosGrupos(usinas_deck, ree_da_usina);
    auto corDoCodigo = [&](int codigo_ree) {
        auto it = indice_cor.find(codigo_ree);
        return corDoGrupo(it == indice_cor.end() ? 0 : it->second);
    };

    std::vector<QColor> cor_do_grupo;
    std::vector<std::pair<QString, QColor>> legenda;
    for (const GrupoCascata& grupo : c.grupos) {
        QColor cor = corDoCodigo(grupo.codigo);
        cor_do_grupo.push_back(cor);
        legenda.push_back({paraTexto(grupo.nome), cor});
    }

    std::unordered_map<int, QColor> cor_do_no;
    for (const NoCascata& no : c.nos) {
        auto it = ree_da_usina.find(no.codigo);
        cor_do_no[no.codigo] = corDoCodigo(it == ree_da_usina.end() ? 0 : it->second);
    }

    desenhar(c, cor_do_no, cor_do_grupo, legenda);

    auto it = nos_.find(selecionado_anterior);
    if (it != nos_.end()) {
        codigo_selecionado_ = selecionado_anterior;
        aplicarEstiloPonto(it->second.ponto, true, palette());
    }
    aplicarFiltro();

    if (ajustar_no_proximo_) {
        ajustar();
        ajustar_no_proximo_ = false;
    }
    atualizarRotulos();
}

QString VistaCascata::descricaoDoNo(int codigo) const {
    const UsinaHidr& usina = modelo_->usina(codigo - 1);
    const DeckLookup& lookup = modelo_->lookup();
    int ree = lookup.reeDaUsina(codigo);

    QString jusante = QStringLiteral("-");
    if (usina.jusante >= 1 && usina.jusante <= modelo_->numUsinas()) {
        jusante = QStringLiteral("%1 %2").arg(usina.jusante).arg(paraTexto(modelo_->usina(usina.jusante - 1).nome));
    }

    return QStringLiteral("%1  %2\nSubmercado: %3\nREE: %4\nJusante: %5")
        .arg(codigo)
        .arg(paraTexto(usina.nome))
        .arg(paraTexto(lookup.nomeSubsistema(usina.subsistema)))
        .arg(paraTexto(lookup.nomeRee(ree)))
        .arg(jusante);
}

// Com o modo "so a cascata da usina" ligado, trocar a selecao muda o conjunto de nos exibidos,
// entao reconstroi a cena inteira em vez de so trocar o estilo do ponto antigo pelo novo.
void VistaCascata::selecionar(int linha) {
    int novo_codigo = linha < 0 ? -1 : linha + 1;
    if (novo_codigo == codigo_selecionado_) return;

    if (so_selecionada_) {
        codigo_selecionado_ = novo_codigo;
        reconstruir();
        ajustar();
        return;
    }

    auto it_antigo = nos_.find(codigo_selecionado_);
    if (it_antigo != nos_.end()) aplicarEstiloPonto(it_antigo->second.ponto, false, palette());

    codigo_selecionado_ = novo_codigo;
    auto it_novo = nos_.find(codigo_selecionado_);
    if (it_novo != nos_.end()) {
        aplicarEstiloPonto(it_novo->second.ponto, true, palette());
        ensureVisible(it_novo->second.ponto);
    }
    atualizarRotulos();
}

void VistaCascata::definirFiltro(const QString& texto) {
    filtro_ = texto;
    aplicarFiltro();
    atualizarRotulos();
}

void VistaCascata::definirRees(const std::set<int>& rees) {
    if (rees_filtro_ == rees) return;
    rees_filtro_ = rees;
    reconstruir();
    ajustar();
}

void VistaCascata::definirSubmercados(const std::set<int>& submercados) {
    if (submercados_filtro_ == submercados) return;
    submercados_filtro_ = submercados;
    reconstruir();
    ajustar();
}

void VistaCascata::definirSoSelecionada(bool ligado) {
    if (so_selecionada_ == ligado) return;
    so_selecionada_ = ligado;
    reconstruir();
    ajustar();
}

void VistaCascata::aplicarFiltro() {
    QString texto = filtro_.trimmed();
    for (const auto& par : nos_) {
        int codigo = par.first;
        bool mostrar = true;
        if (!texto.isEmpty()) {
            const UsinaHidr& usina = modelo_->usina(codigo - 1);
            bool bate_codigo = QString::number(codigo).startsWith(texto);
            bool bate_nome = paraTexto(usina.nome).contains(texto, Qt::CaseInsensitive);
            mostrar = bate_codigo || bate_nome;
        }
        par.second.ponto->setOpacity(mostrar ? 1.0 : 0.25);
        casa_filtro_[codigo] = mostrar;
    }
    for (ItemAresta& aresta : arestas_) {
        bool ambos_visiveis = casa_filtro_[aresta.origem] && casa_filtro_[aresta.destino];
        double opacidade = aresta.opacidade_base * (ambos_visiveis ? 1.0 : 0.25);
        aresta.linha->setOpacity(opacidade);
        aresta.seta->setOpacity(opacidade);
    }
}

// Com 200+ pontos os rotulos so cabem quando a vista esta perto do tamanho natural; abaixo do
// limiar ficam so o ponto sob o mouse, o selecionado e os que casam com o filtro de texto.
void VistaCascata::atualizarRotulos() {
    double escala = transform().m11();
    bool filtro_ativo = !filtro_.trimmed().isEmpty();
    for (const auto& par : nos_) {
        int codigo = par.first;
        bool visivel = escala >= LIMIAR_ROTULO || codigo == codigo_sob_mouse_ || codigo == codigo_selecionado_;
        if (!visivel && filtro_ativo) {
            auto it = casa_filtro_.find(codigo);
            visivel = it != casa_filtro_.end() && it->second;
        }
        par.second.rotulo->setVisible(visivel);
    }
}

void VistaCascata::posicionarLegenda() {
    if (legenda_->isHidden()) return;
    legenda_->move(viewport()->width() - legenda_->width() - MARGEM_LEGENDA, MARGEM_LEGENDA);
}

// fitInView pode fazer as barras de rolagem aparecerem ou sumirem, o que redimensiona o viewport e
// volta aqui pelo resizeEvent; a trava corta essa recursao no primeiro nivel.
void VistaCascata::ajustar() {
    usuario_mexeu_zoom_ = false;
    if (ajustando_ || cena_->items().isEmpty()) return;
    ajustando_ = true;
    fitInView(cena_->itemsBoundingRect().adjusted(-30, -30, 30, 30), Qt::KeepAspectRatio);
    ajustando_ = false;
    atualizarRotulos();
}

void VistaCascata::wheelEvent(QWheelEvent* ev) {
    double escala_atual = transform().m11();
    double alvo = escala_atual * (ev->angleDelta().y() > 0 ? FATOR_ZOOM : 1.0 / FATOR_ZOOM);
    double nova = std::clamp(alvo, ESCALA_MINIMA, ESCALA_MAXIMA);
    double fator = nova / escala_atual;
    if (fator <= 0.0) return;
    scale(fator, fator);
    usuario_mexeu_zoom_ = true;
    atualizarRotulos();
}

int VistaCascata::codigoNoPonto(const QPoint& ponto) const {
    QGraphicsItem* item = itemAt(ponto);
    if (!item) return -1;
    QVariant dado = item->data(0);
    if (!dado.isValid() && item->parentItem()) dado = item->parentItem()->data(0);
    return dado.isValid() ? dado.toInt() : -1;
}

void VistaCascata::mousePressEvent(QMouseEvent* ev) {
    QGraphicsView::mousePressEvent(ev);
    int codigo = codigoNoPonto(ev->position().toPoint());
    if (codigo > 0) emit usinaEscolhida(codigo - 1);
}

void VistaCascata::mouseDoubleClickEvent(QMouseEvent* ev) {
    QGraphicsView::mouseDoubleClickEvent(ev);
    int codigo = codigoNoPonto(ev->position().toPoint());
    if (codigo > 0) emit focarCascataDe(codigo - 1);
}

// So reenquadra sozinho enquanto o usuario nao tiver dado zoom: depois disso, redimensionar a
// janela mantem a escala escolhida por ele.
void VistaCascata::resizeEvent(QResizeEvent* ev) {
    QGraphicsView::resizeEvent(ev);
    posicionarLegenda();
    if (!usuario_mexeu_zoom_) ajustar();
}
