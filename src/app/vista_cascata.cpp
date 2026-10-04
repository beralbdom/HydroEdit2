#include "vista_cascata.h"
#include <QEvent>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <map>
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
constexpr double DISTANCIA_ROTULO = 11.0;
constexpr double DISTANCIA_ROTULO_DIAGONAL = 4.0;
constexpr double RAIO_LIVRE = 7.0;
constexpr double ESPACO_CODIGO_NOME = 4.0;
constexpr double FOLGA_TRACO = 2.0;
constexpr double LARGURA_LIVRE_SETA = 4.0;
constexpr double CELULA_ROTULOS = 64.0;
constexpr double FOLGA_ROTULO = 3.0;
constexpr double MARGEM_CENA = 40.0;

QString paraTexto(const std::string& s) { return QString::fromLatin1(s.c_str()); }
}  // namespace

VistaCascata::VistaCascata(ModeloHidr* modelo, QWidget* parent) : QGraphicsView(parent), modelo_(modelo) {
    cena_ = new QGraphicsScene(this);
    setScene(cena_);
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

// O REE de cada usina, direto do confhd.dat, e o nome de cada REE, do ree.dat. O codigo 0 e
// batizado aqui porque e ele que recebe as usinas do hidr.dat que o deck nao coloca em nenhum REE.
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
        QColor cor = it_cor == cor_do_no.end() ? corSemRee() : it_cor->second;
        QString texto_codigo = QString::number(no.codigo);
        QString texto_nome = paraTexto(modelo_->usina(no.codigo - 1).nome);
        ItensNoCascata itens =
            criarPontoCascata(cena_, no, texto_codigo, texto_nome, cor,
                              usinaComReservatorio(modelo_->usina(no.codigo - 1)), palette(), font(), ao_pairar);
        QString descricao = descricaoDoNo(no.codigo);
        itens.ponto->setToolTip(descricao);
        itens.codigo->setToolTip(descricao);
        itens.nome->setToolTip(descricao);
        nos_[no.codigo] = itens;
    }

    for (const ArestaCascata& aresta : c.arestas) {
        if (aresta.rota.size() < 2 || !nos_.contains(aresta.origem) || !nos_.contains(aresta.destino)) continue;
        QPolygonF rota;
        for (const auto& [coluna, linha] : aresta.rota)
            rota << QPointF(coluna * ESPACO_COLUNA_CASCATA, linha * ESPACO_LINHA_CASCATA);

        ItensArestaCascata itens = criarArestaCascata(cena_, rota, aresta.desvio, palette());
        tracados_.push_back({rota, itens.seta != nullptr});
        arestas_.push_back({itens.traco, itens.seta, aresta.origem, aresta.destino, QLineF(rota[rota.size() - 2], rota.back()).length()});
    }
}

// O desenho nao tem faixas: empacotarBacias poe todas as bacias juntas, das maiores para as
// menores, em linhas de ate largura_maxima colunas. O REE aparece na cor do ponto e na legenda. Os
// filtros de REE e de submercado valem juntos: uma usina aparece se o REE dela esta na selecao de
// REEs (ou essa selecao esta vazia) E o submercado dela esta na selecao de submercados (ou essa
// esta vazia). Restringir e sempre a mesma coisa, uma copia do deck com as usinas de fora zeradas,
// para o layout sair pelo mesmo caminho com e sem filtro. As cores sao indexadas pela lista de REEs
// do deck inteiro, entao um REE mantem a mesma cor com e sem filtro.
void VistaCascata::reconstruir() {
    int selecionado_anterior = codigo_selecionado_;

    // Esvaziar os mapas antes de destruir os itens: apagar a cena pode disparar um hoverLeaveEvent,
    // e o retorno do PontoCascata mexe em codigo_sob_mouse_ e percorre nos_.
    nos_.clear();
    casa_filtro_.clear();
    arestas_.clear();
    tracados_.clear();
    codigo_selecionado_ = -1;
    codigo_sob_mouse_ = -1;
    cena_->clear();

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
    if (!mostrar_ficticias_) {
        for (size_t i = 0; i < usinas_deck.size(); ++i) {
            if (usinaFicticia(usinas_deck[i])) manter[i + 1] = false;
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
    Cascata c = empacotarBacias(exibidas, largura_maxima);

    std::map<int, int> indice_cor = indiceDeCorDosRees(ree_da_usina);
    auto corDoRee = [&](int codigo_ree) {
        if (codigo_ree == 0) return corSemRee();
        auto it = indice_cor.find(codigo_ree);
        return corDoIndice(it == indice_cor.end() ? 0 : it->second);
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
    atualizarSetas();
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

// So mexe na visibilidade dos nomes; os codigos continuam seguindo a regra de sempre. Como quem
// aplica e atualizarRotulos(), que roda no fim de cada reconstrucao, a escolha sobrevive a elas.
void VistaCascata::definirMostrarNomes(bool mostrar) {
    if (mostrar_nomes_ == mostrar) return;
    mostrar_nomes_ = mostrar;
    atualizarRotulos();
}

// Tirar as ficticias e uma restricao como as de REE e submercado: elas saem do conjunto antes do
// layout, entao quem ficava a montante delas passa a ser raiz da propria bacia.
void VistaCascata::definirMostrarFicticias(bool mostrar) {
    if (mostrar_ficticias_ == mostrar) return;
    mostrar_ficticias_ = mostrar;
    reconstruir();
    ajustar();
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
        if (aresta.seta) aresta.seta->setOpacity(opacidade);
    }
}

// Com 200+ pontos os rotulos so cabem quando a vista esta perto do tamanho natural; abaixo do
// limiar entram so o ponto sob o mouse, o selecionado e os que casam com o filtro de texto. Cada
// rotulo (codigo e, com os nomes ligados, o nome ao lado) vai para o primeiro lugar livre em volta do
// ponto: a direita, a esquerda, e acima e abaixo de cada lado; livre e sem tocar em usina, ligacao,
// ponta de seta ou rotulo ja posto. Sem lugar para o rotulo inteiro, tenta so o codigo; sem lugar
// nem para ele, o rotulo some neste zoom e volta ao aproximar. A conta e em pixels da vista, porque
// os rotulos tem tamanho fixo em pixels e o espaco entre as usinas cresce com o zoom. O ponto sob o
// mouse e o selecionado vem primeiro e sempre mostram o rotulo, mesmo encostando em algo.
void VistaCascata::atualizarRotulos() {
    const double escala = transform().m11();
    const bool filtro_ativo = !filtro_.trimmed().isEmpty();
    auto casaFiltro = [&](int codigo) {
        auto it = casa_filtro_.find(codigo);
        return filtro_ativo && it != casa_filtro_.end() && it->second;
    };
    std::vector<int> ordem;
    for (const auto& [codigo, itens] : nos_) {
        itens.codigo->setVisible(false);
        itens.nome->setVisible(false);
        const bool forcado = codigo == codigo_sob_mouse_ || codigo == codigo_selecionado_;
        if (escala >= LIMIAR_ROTULO || forcado || casaFiltro(codigo)) ordem.push_back(codigo);
    }
    auto prioridade = [&](int codigo) {
        if (codigo == codigo_sob_mouse_ || codigo == codigo_selecionado_) return 0;
        return casaFiltro(codigo) ? 1 : 2;
    };
    std::sort(ordem.begin(), ordem.end(), [&](int a, int b) {
        return prioridade(a) != prioridade(b) ? prioridade(a) < prioridade(b) : a < b;
    });

    std::vector<QRectF> ocupados;
    std::unordered_map<long long, std::vector<size_t>> celulas;
    auto celulasDe = [](const QRectF& r, auto&& visitar) {
        for (long long cx = static_cast<long long>(std::floor(r.left() / CELULA_ROTULOS)); cx <= std::floor(r.right() / CELULA_ROTULOS); ++cx)
            for (long long cy = static_cast<long long>(std::floor(r.top() / CELULA_ROTULOS)); cy <= std::floor(r.bottom() / CELULA_ROTULOS); ++cy)
                visitar((cx << 32) ^ (cy & 0xffffffff));
    };
    auto ocupar = [&](const QRectF& r) {
        ocupados.push_back(r);
        celulasDe(r, [&](long long celula) { celulas[celula].push_back(ocupados.size() - 1); });
    };
    auto livre = [&](const QRectF& r) {
        bool sem_conflito = true;
        celulasDe(r, [&](long long celula) {
            if (!sem_conflito) return;
            auto it = celulas.find(celula);
            if (it == celulas.end()) return;
            for (size_t i : it->second)
                if (ocupados[i].intersects(r)) {
                    sem_conflito = false;
                    return;
                }
        });
        return sem_conflito;
    };

    const QTransform vista = viewportTransform();
    for (const auto& [codigo, itens] : nos_) {
        const QPointF centro = vista.map(itens.ponto->pos());
        ocupar(QRectF(centro.x() - RAIO_LIVRE, centro.y() - RAIO_LIVRE, 2 * RAIO_LIVRE, 2 * RAIO_LIVRE));
    }
    for (const auto& [rota, seta] : tracados_) {
        for (qsizetype i = 0; i + 1 < rota.size(); ++i)
            ocupar(QRectF(vista.map(rota[i]), vista.map(rota[i + 1])).normalized().adjusted(-FOLGA_TRACO, -FOLGA_TRACO, FOLGA_TRACO, FOLGA_TRACO));
        if (!seta) continue;
        const QLineF ultimo(vista.map(rota.back()), vista.map(rota[rota.size() - 2]));
        if (ultimo.length() <= 0.0) continue;
        const QPointF base = ultimo.pointAt(std::min(1.0, comprimentoSetaCascata() / ultimo.length()));
        ocupar(QRectF(ultimo.p1(), base).normalized().adjusted(-LARGURA_LIVRE_SETA, -LARGURA_LIVRE_SETA, LARGURA_LIVRE_SETA, LARGURA_LIVRE_SETA));
    }

    for (int codigo : ordem) {
        const ItensNoCascata& itens = nos_.at(codigo);
        const QPointF centro = vista.map(itens.ponto->pos());
        const double largura_codigo = itens.codigo->boundingRect().width();
        const double altura = itens.codigo->boundingRect().height();
        const bool forcado = prioridade(codigo) == 0;
        bool posto = false;
        for (const bool com_nome : {mostrar_nomes_, false}) {
            const double largura = com_nome ? largura_codigo + ESPACO_CODIGO_NOME + itens.nome->boundingRect().width() : largura_codigo;
            const QPointF lugares[] = {
                {DISTANCIA_ROTULO, -altura / 2.0},
                {-DISTANCIA_ROTULO - largura, -altura / 2.0},
                {DISTANCIA_ROTULO_DIAGONAL, -RAIO_LIVRE - 2.0 - altura},
                {DISTANCIA_ROTULO_DIAGONAL, RAIO_LIVRE + 2.0},
                {-DISTANCIA_ROTULO_DIAGONAL - largura, -RAIO_LIVRE - 2.0 - altura},
                {-DISTANCIA_ROTULO_DIAGONAL - largura, RAIO_LIVRE + 2.0},
            };
            for (const QPointF& lugar : lugares) {
                const QRectF retangulo(centro + lugar, QSizeF(largura, altura));
                if (!forcado && !livre(retangulo.adjusted(-FOLGA_ROTULO, -1.0, FOLGA_ROTULO, 1.0))) continue;
                itens.codigo->setPos(lugar);
                itens.nome->setPos(lugar + QPointF(largura_codigo + ESPACO_CODIGO_NOME, 0.0));
                itens.codigo->setVisible(true);
                itens.nome->setVisible(com_nome);
                ocupar(retangulo);
                posto = true;
                break;
            }
            if (posto || !com_nome) break;
        }
    }
}

// A ponta de seta tem tamanho fixo em pixels, mas o ultimo trecho do desvio (meia coluna, quando
// entra pela lateral) encolhe com o zoom; abaixo do tamanho dela a seta passaria do canto da rota,
// entao encolhe junto, ate 35% do tamanho normal.
void VistaCascata::atualizarSetas() {
    const double escala = transform().m11();
    for (const ItemAresta& aresta : arestas_) {
        if (!aresta.seta) continue;
        const double disponivel = aresta.ultimo_trecho * escala - 1.0;
        aresta.seta->setScale(std::clamp(disponivel / comprimentoSetaCascata(), 0.35, 1.0));
    }
}

// Enquadra o sceneRect, e nao so os itens: e ele que carrega a margem de cima em que os titulos das
// faixas sao desenhados. fitInView pode fazer as barras de rolagem aparecerem ou sumirem, o que
// redimensiona o viewport e volta aqui pelo resizeEvent; a trava corta essa recursao no primeiro
// nivel. Quando a cena inteira so caberia abaixo de ESCALA_LEGIVEL, prefere-se cortar a mostrar
// tudo ilegivel: fixa a escala no minimo e encosta a vista no canto superior esquerdo do sceneRect,
// margem inclusa, de onde o usuario rola.
void VistaCascata::ajustar() {
    if (ajustando_ || cena_->items().isEmpty()) return;
    usuario_mexeu_zoom_ = false;
    ajustando_ = true;

    fitInView(cena_->sceneRect(), Qt::KeepAspectRatio);
    double escala = transform().m11();
    if (escala > 0.0 && escala < ESCALA_LEGIVEL) {
        scale(ESCALA_LEGIVEL / escala, ESCALA_LEGIVEL / escala);
        horizontalScrollBar()->setValue(horizontalScrollBar()->minimum());
        verticalScrollBar()->setValue(verticalScrollBar()->minimum());
    }

    ajustando_ = false;
    atualizarRotulos();
    atualizarSetas();
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

    // Se a legenda for mais alta ou mais larga do que a area visivel, encosta no canto superior
    // esquerdo em vez de sair pela borda de cima ou pela esquerda.
    QRect area = viewport()->geometry();
    legenda_->move(std::max(area.x(), area.x() + area.width() - largura - MARGEM_LEGENDA),
                   std::max(area.y(), area.y() + area.height() - altura - MARGEM_LEGENDA));
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
    atualizarSetas();
}

int VistaCascata::codigoNoPonto(const QPoint& ponto) const {
    QGraphicsItem* item = itemAt(ponto);
    if (!item) return -1;
    QVariant dado = item->data(0);
    if (!dado.isValid() && item->parentItem()) dado = item->parentItem()->data(0);
    return dado.isValid() ? dado.toInt() : -1;
}

// O arraste com o botao esquerdo rola a cena, como no modo ScrollHandDrag do QGraphicsView, mas feito
// aqui para o cursor continuar a seta padrao: aquele modo troca o cursor do viewport pela mao aberta e
// fechada a cada evento.
void VistaCascata::mousePressEvent(QMouseEvent* ev) {
    QGraphicsView::mousePressEvent(ev);
    if (ev->button() == Qt::LeftButton) {
        arrastando_ = true;
        ultimo_ponto_arraste_ = ev->position().toPoint();
    }
    int codigo = codigoNoPonto(ev->position().toPoint());
    if (codigo > 0) emit usinaEscolhida(codigo - 1);
}

void VistaCascata::mouseMoveEvent(QMouseEvent* ev) {
    QGraphicsView::mouseMoveEvent(ev);
    if (!arrastando_) return;
    QPoint ponto = ev->position().toPoint();
    QPoint delta = ponto - ultimo_ponto_arraste_;
    ultimo_ponto_arraste_ = ponto;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
}

void VistaCascata::mouseReleaseEvent(QMouseEvent* ev) {
    QGraphicsView::mouseReleaseEvent(ev);
    if (ev->button() == Qt::LeftButton) arrastando_ = false;
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
