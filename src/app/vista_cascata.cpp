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
#include <string>
#include <utility>
#include "cascata.h"
#include "legenda_cascata.h"
#include "modelo_hidr.h"

namespace {
constexpr double LIMIAR_ROTULO = 0.9;
constexpr double ESCALA_MINIMA = 0.05;
constexpr double ESCALA_MAXIMA = 20.0;
constexpr double FATOR_ZOOM = 1.15;
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

// Traduz o agrupamento escolhido nos dois mapas que empacotarPorGrupo espera e devolve, alem da
// cascata empacotada, a que grupo cada bacia foi parar (chave: BaciaCascata::indice), porque a cor
// de cada ponto vem do grupo da sua bacia. A largura maxima aproxima a faixa de um formato quadrado.
Cascata VistaCascata::montarLayout(const Cascata& base, std::unordered_map<int, int>& grupo_da_bacia) const {
    const DeckLookup& lookup = modelo_->lookup();
    std::map<int, int> grupo_da_usina;
    std::map<int, std::string> nome_do_grupo;

    switch (agrupamento_) {
        case Agrupamento::Ree:
            grupo_da_usina = lookup.ree_da_usina;
            for (const auto& [codigo, ree] : lookup.rees) nome_do_grupo[codigo] = ree.nome;
            break;
        case Agrupamento::Submercado:
            for (const auto& [codigo, ree] : lookup.ree_da_usina) {
                int submercado = lookup.submercadoDoRee(ree);
                if (submercado != 0) grupo_da_usina[codigo] = submercado;
            }
            nome_do_grupo = lookup.subsistemas;
            break;
        case Agrupamento::Bacia:
            for (const BaciaCascata& bacia : base.bacias) {
                grupo_da_usina[bacia.codigo_foz] = bacia.codigo_foz;
                nome_do_grupo[bacia.codigo_foz] = modelo_->usina(bacia.codigo_foz - 1).nome;
            }
            break;
    }

    grupo_da_bacia.clear();
    for (const BaciaCascata& bacia : base.bacias) {
        auto it = grupo_da_usina.find(bacia.codigo_foz);
        grupo_da_bacia[bacia.indice] = it == grupo_da_usina.end() ? 0 : it->second;
    }

    int largura_maxima =
        std::max(8, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(base.nos.size()) * 2.5))));
    return empacotarPorGrupo(base, grupo_da_usina, nome_do_grupo, largura_maxima);
}

void VistaCascata::desenhar(const Cascata& c, const std::unordered_map<int, int>& grupo_da_bacia,
                            const std::unordered_map<int, int>& indice_cor) {
    auto corDoCodigoDeGrupo = [&](int codigo_grupo) {
        auto it = indice_cor.find(codigo_grupo);
        return corDoGrupo(it == indice_cor.end() ? 0 : it->second);
    };

    std::vector<std::pair<QString, QColor>> itens_legenda;
    for (const GrupoCascata& grupo : c.grupos) {
        QColor cor = corDoCodigoDeGrupo(grupo.codigo);
        QString nome = paraTexto(grupo.nome);
        criarFaixaCascata(cena_, grupo, QStringLiteral("%1 (%2 usinas)").arg(nome).arg(grupo.num_usinas), cor,
                          palette(), font());
        itens_legenda.push_back({nome, cor});
    }
    legenda_->definirGrupos(itens_legenda);
    posicionarLegenda();

    auto ao_pairar = [this](int codigo, bool entrou) {
        if (entrou) codigo_sob_mouse_ = codigo;
        else if (codigo_sob_mouse_ == codigo) codigo_sob_mouse_ = -1;
        atualizarRotulos();
    };

    for (const NoCascata& no : c.nos) {
        auto it_grupo = grupo_da_bacia.find(no.bacia);
        QColor cor = corDoCodigoDeGrupo(it_grupo == grupo_da_bacia.end() ? 0 : it_grupo->second);
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
        arestas_.push_back({itens.linha, itens.seta, aresta.origem, aresta.destino});
    }
}

// Ordem de restricao: modo "so a cascata da usina selecionada", filtro por bacia, filtro por REE e,
// sem nenhum deles, o deck inteiro. baciasAtualizadas e gruposAtualizados sempre carregam as listas
// do deck completo, independente do que esta na tela, para os combos da janela continuarem
// oferecendo todas as opcoes; as cores dos grupos tambem saem dessa lista completa, entao um grupo
// mantem a mesma cor com e sem filtro.
void VistaCascata::reconstruir() {
    int selecionado_anterior = codigo_selecionado_;

    cena_->clear();
    nos_.clear();
    casa_filtro_.clear();
    arestas_.clear();
    codigo_selecionado_ = -1;
    codigo_sob_mouse_ = -1;

    const std::vector<UsinaHidr>& usinas = modelo_->arquivo().usinas;
    Cascata completa = montarCascata(usinas);

    std::unordered_map<int, int> grupo_da_bacia;
    Cascata layout_completo = montarLayout(completa, grupo_da_bacia);

    std::unordered_map<int, int> indice_cor;
    for (size_t i = 0; i < layout_completo.grupos.size(); ++i)
        indice_cor[layout_completo.grupos[i].codigo] = static_cast<int>(i);

    auto restringir = [&](const std::vector<bool>& manter) {
        std::vector<UsinaHidr> copia = usinas;
        for (size_t i = 0; i < copia.size(); ++i)
            if (!manter[i + 1]) copia[i].nome.clear();
        return montarCascata(copia);
    };

    Cascata c = layout_completo;
    if (so_selecionada_ && selecionado_anterior > 0) {
        std::vector<bool> manter(usinas.size() + 1, false);
        for (int codigo : cascataDaUsina(usinas, selecionado_anterior)) manter[static_cast<size_t>(codigo)] = true;
        c = montarLayout(restringir(manter), grupo_da_bacia);
    } else if (bacia_filtro_ != 0) {
        Cascata filtrada = filtrarBacia(completa, bacia_filtro_);
        // Bacia sumiu do deck (foz zerada ou jusante trocado): volta para "Todas" em vez de deixar
        // a cena vazia com o combo tambem tentando resincronizar sozinho.
        if (filtrada.nos.empty()) bacia_filtro_ = 0;
        else c = montarLayout(filtrada, grupo_da_bacia);
    } else if (ree_filtro_ != 0) {
        const DeckLookup& lookup = modelo_->lookup();
        std::vector<bool> manter(usinas.size() + 1, false);
        for (size_t i = 0; i < usinas.size(); ++i)
            manter[i + 1] = lookup.reeDaUsina(static_cast<int>(i) + 1) == ree_filtro_;
        c = montarLayout(restringir(manter), grupo_da_bacia);
    }

    desenhar(c, grupo_da_bacia, indice_cor);

    auto it = nos_.find(selecionado_anterior);
    if (it != nos_.end()) {
        codigo_selecionado_ = selecionado_anterior;
        aplicarEstiloPonto(it->second.ponto, true, palette());
    }
    aplicarFiltro();

    emit baciasAtualizadas(completa.bacias);
    emit gruposAtualizados(layout_completo.grupos);

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

void VistaCascata::definirBacia(int codigo_foz) {
    if (bacia_filtro_ == codigo_foz) return;
    bacia_filtro_ = codigo_foz;
    reconstruir();
    ajustar();
}

void VistaCascata::definirRee(int codigo_ree) {
    if (ree_filtro_ == codigo_ree) return;
    ree_filtro_ = codigo_ree;
    reconstruir();
    ajustar();
}

void VistaCascata::definirAgrupamento(Agrupamento agrupamento) {
    if (agrupamento_ == agrupamento) return;
    agrupamento_ = agrupamento;
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
        double opacidade = ambos_visiveis ? 1.0 : 0.25;
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
