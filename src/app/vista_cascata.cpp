#include "vista_cascata.h"
#include <QEvent>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
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
// Abaixo dessa escala o espacamento de 40 unidades por coluna cai para menos de 20 px e os pontos,
// as setas e os titulos (que tem tamanho fixo em pixels) comecam a se cobrir. O ajuste automatico
// nunca desce dela; a roda do mouse ainda deixa o usuario afastar um pouco mais.
constexpr double ESCALA_LEGIVEL = 0.5;
constexpr double ESCALA_MINIMA = ESCALA_LEGIVEL * 0.5;
constexpr double ESCALA_MAXIMA = 20.0;
constexpr double FATOR_ZOOM = 1.15;
constexpr int MARGEM_LEGENDA = 8;
constexpr double MARGEM_CENA = 40.0;

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

// Indice de cor de cada REE pela posicao dele entre todos os REEs que o deck inteiro usa, e nao
// entre os que estao desenhados: assim um REE nao troca de cor quando um filtro deixa so parte
// deles na tela. O codigo 0 ("Sem REE") so entra quando existe usina do deck fora do confhd.dat.
std::map<int, int> indiceDeCorDosRees(const std::vector<UsinaHidr>& usinas,
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
    // Filha da vista, e nao do viewport: arrastar a cena rola o viewport com QWidget::scroll(), que
    // leva junto os filhos dele, e a legenda sairia de cena a cada movimento. Como irma do viewport
    // ela fica parada; quem a posiciona dentro da area visivel e posicionarLegenda().
    legenda_ = new LegendaCascata(this);
    legenda_->raise();

    // Tres gatilhos para a legenda nunca ficar ancorada por um tamanho ou por uma area visivel que
    // ja mudaram: o proprio quadro quando muda de tamanho, e as barras de rolagem quando aparecem ou
    // somem, que encolhem o viewport sem a vista receber resizeEvent.
    connect(legenda_, &LegendaCascata::tamanhoAlterado, this, &VistaCascata::posicionarLegenda);
    connect(horizontalScrollBar(), &QAbstractSlider::rangeChanged, this, &VistaCascata::posicionarLegenda);
    connect(verticalScrollBar(), &QAbstractSlider::rangeChanged, this, &VistaCascata::posicionarLegenda);

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

void VistaCascata::desenhar(const Cascata& c, const std::unordered_map<int, QColor>& cor_do_no) {
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

    // Um par de usinas pode ter mais de uma ligacao: no Tiete o bombeamento faz A apontar para B por
    // desvio enquanto B aponta para A por jusante, e ANTA tem jusante e desvio na mesma usina. Duas
    // retas iguais se sobrepoem, entao o par inteiro passa a ser desenhado em curva. A curvatura sai
    // da ordem dentro da mesma direcao: duas ligacoes de sentidos opostos ficam ambas com +1 e, como
    // a perpendicular e a direita de quem viaja, saem uma de cada lado; duas no mesmo sentido pegam
    // +1 e -1 e tambem se separam.
    std::map<std::pair<int, int>, int> ligacoes_do_par;
    for (const ArestaCascata& aresta : c.arestas) {
        ligacoes_do_par[{std::min(aresta.origem, aresta.destino), std::max(aresta.origem, aresta.destino)}]++;
    }
    std::map<std::pair<int, int>, int> desenhadas_no_sentido;

    for (const ArestaCascata& aresta : c.arestas) {
        auto it_origem = nos_.find(aresta.origem);
        auto it_destino = nos_.find(aresta.destino);
        if (it_origem == nos_.end() || it_destino == nos_.end()) continue;

        double curvatura = 0.0;
        if (ligacoes_do_par[{std::min(aresta.origem, aresta.destino), std::max(aresta.origem, aresta.destino)}] > 1) {
            int ordem = desenhadas_no_sentido[{aresta.origem, aresta.destino}]++;
            curvatura = ordem % 2 == 0 ? 1.0 : -1.0;
        }

        ItensArestaCascata itens =
            criarArestaCascata(cena_, it_origem->second.ponto->pos(), it_destino->second.ponto->pos(),
                               aresta.desvio, curvatura, palette());
        arestas_.push_back({itens.traco, itens.seta, aresta.origem, aresta.destino});
    }
}

// O desenho nao tem faixas: todas as bacias sao empacotadas juntas, das maiores para as menores,
// em linhas de ate largura_maxima colunas (empacotarPorGrupo com o mapa de grupos vazio, que e o
// caso de um grupo so). O REE aparece na cor do ponto e na legenda, nao mais em retangulos. Os
// filtros de REE e de submercado valem juntos: uma usina aparece se o REE dela esta na selecao de
// REEs (ou essa selecao esta vazia) E o submercado dela esta na selecao de submercados (ou essa
// esta vazia). Restringir e sempre a mesma coisa, uma copia do deck com as usinas de fora zeradas,
// para o layout sair pelo mesmo caminho com e sem filtro. As cores sao indexadas pela lista de REEs
// do deck inteiro, entao um REE mantem a mesma cor com e sem filtro.
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
    if (!rees_filtro_.empty() || !submercados_filtro_.empty()) {
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
    Cascata c = empacotarPorGrupo(exibidas, {}, {}, largura_maxima);

    std::map<int, int> indice_cor = indiceDeCorDosRees(usinas_deck, ree_da_usina);
    auto corDoRee = [&](int codigo_ree) {
        auto it = indice_cor.find(codigo_ree);
        return corDoGrupo(it == indice_cor.end() ? 0 : it->second);
    };
    auto reeDaUsina = [&](int codigo) {
        auto it = ree_da_usina.find(codigo);
        return it == ree_da_usina.end() ? 0 : it->second;
    };

    std::unordered_map<int, QColor> cor_do_no;
    std::set<int> rees_desenhados;
    for (const NoCascata& no : c.nos) {
        int ree = reeDaUsina(no.codigo);
        cor_do_no[no.codigo] = corDoRee(ree);
        rees_desenhados.insert(ree);
    }

    // Legenda so com os REEs que aparecem no desenho, por codigo crescente e com o "Sem REE" no fim.
    std::vector<std::pair<QString, QColor>> legenda;
    auto adicionarNaLegenda = [&](int ree) {
        auto it = nome_do_ree.find(ree);
        QString nome = it == nome_do_ree.end() || it->second.empty() ? QStringLiteral("REE %1").arg(ree)
                                                                     : paraTexto(it->second);
        legenda.push_back({nome, corDoRee(ree)});
    };
    for (int ree : rees_desenhados) {
        if (ree != 0) adicionarNaLegenda(ree);
    }
    if (rees_desenhados.contains(0)) adicionarNaLegenda(0);

    desenhar(c, cor_do_no);
    legenda_->definirGrupos(legenda);
    posicionarLegenda();
    // O enquadramento e as barras de rolagem ainda vao se acertar no resto deste giro do laco de
    // eventos; a segunda passada recalcula a posicao ja com a area visivel final.
    QTimer::singleShot(0, this, [this] { posicionarLegenda(); });

    if (!cena_->items().isEmpty()) {
        cena_->setSceneRect(
            cena_->itemsBoundingRect().adjusted(-MARGEM_CENA, -MARGEM_CENA, MARGEM_CENA, MARGEM_CENA));
    }

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
    // Mesmo submercado que o filtro usa. So quando a usina nao esta em nenhum REE e que sobra o
    // campo subsistema do proprio hidr.dat.
    std::string submercado = ree == 0 ? lookup.nomeSubsistema(usina.subsistema)
                                      : lookup.nomeSubsistema(lookup.submercadoDoRee(ree));

    QString jusante = QStringLiteral("-");
    if (usina.jusante >= 1 && usina.jusante <= modelo_->numUsinas()) {
        jusante = QStringLiteral("%1 %2").arg(usina.jusante).arg(paraTexto(modelo_->usina(usina.jusante - 1).nome));
    }

    return QStringLiteral("%1  %2\nSubmercado: %3\nREE: %4\nJusante: %5")
        .arg(codigo)
        .arg(paraTexto(usina.nome))
        .arg(paraTexto(submercado))
        .arg(paraTexto(lookup.nomeRee(ree)))
        .arg(jusante);
}

void VistaCascata::selecionar(int linha) {
    int novo_codigo = linha < 0 ? -1 : linha + 1;
    if (novo_codigo == codigo_selecionado_) return;

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

// Os dois conjuntos entram juntos para uma troca de filtro reconstruir a cena uma vez so, e nao
// duas (uma por conjunto).
void VistaCascata::definirFiltros(const std::set<int>& rees, const std::set<int>& submercados) {
    if (rees_filtro_ == rees && submercados_filtro_ == submercados) return;
    rees_filtro_ = rees;
    submercados_filtro_ = submercados;
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
        aresta.traco->setOpacity(opacidade);
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

// Enquadra o sceneRect, e nao so os itens: e ele que carrega a margem de cima em que os titulos das
// faixas sao desenhados. fitInView pode fazer as barras de rolagem aparecerem ou sumirem, o que
// redimensiona o viewport e volta aqui pelo resizeEvent; a trava corta essa recursao no primeiro
// nivel. Quando a cena inteira so caberia abaixo de ESCALA_LEGIVEL, prefere-se cortar a mostrar
// tudo ilegivel: fixa a escala no minimo e encosta a vista no canto superior esquerdo do sceneRect,
// margem inclusa, de onde o usuario rola.
void VistaCascata::ajustar() {
    usuario_mexeu_zoom_ = false;
    if (ajustando_ || cena_->items().isEmpty()) return;
    ajustando_ = true;

    QRectF alvo = cena_->sceneRect();
    fitInView(alvo, Qt::KeepAspectRatio);
    double escala = transform().m11();
    if (escala > 0.0 && escala < ESCALA_LEGIVEL) {
        scale(ESCALA_LEGIVEL / escala, ESCALA_LEGIVEL / escala);
        centerOn(alvo.left() + viewport()->width() / (2.0 * ESCALA_LEGIVEL),
                 alvo.top() + viewport()->height() / (2.0 * ESCALA_LEGIVEL));
    }

    ajustando_ = false;
    atualizarRotulos();
    posicionarLegenda();
}

// As coordenadas sao as da vista, nao as do viewport, porque a legenda e irma dele; o retangulo do
// viewport e que diz onde fica a area visivel da cena, ja descontados a moldura e as barras de
// rolagem.
void VistaCascata::posicionarLegenda() {
    if (legenda_->isHidden()) return;
    // Usa o sizeHint, e nao o tamanho atual, porque o quadro pode ainda nao ter sido redimensionado
    // pelo layout depois de trocar as linhas; o resize deixa os dois iguais antes de ancorar.
    int largura = legenda_->sizeHint().width();
    int altura = legenda_->sizeHint().height();
    legenda_->resize(largura, altura);

    QRect area = viewport()->geometry();
    legenda_->move(area.x() + area.width() - largura - MARGEM_LEGENDA,
                   area.y() + area.height() - altura - MARGEM_LEGENDA);
    legenda_->raise();
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

// So reenquadra sozinho enquanto o usuario nao tiver dado zoom: depois disso, redimensionar a
// janela mantem a escala escolhida por ele.
void VistaCascata::resizeEvent(QResizeEvent* ev) {
    QGraphicsView::resizeEvent(ev);
    if (!usuario_mexeu_zoom_) ajustar();
}

// O viewport tambem muda de tamanho sem a vista mudar, quando uma barra de rolagem aparece ou some
// depois de um zoom; e ai que a legenda ficaria pendurada no canto antigo.
bool VistaCascata::viewportEvent(QEvent* ev) {
    bool resultado = QGraphicsView::viewportEvent(ev);
    if (ev->type() == QEvent::Resize) posicionarLegenda();
    return resultado;
}
