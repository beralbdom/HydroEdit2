#include "recursos_tabela.h"
#include <QAbstractItemModel>
#include <QApplication>
#include <QClipboard>
#include <QCollator>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QProxyStyle>
#include <QPushButton>
#include <QStyleFactory>
#include <QStyleOptionHeader>
#include <QTableView>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

// Texto copiado de uma planilha (Excel, LibreOffice) ou de outra tabela: linhas separadas por quebra
// (LF ou CRLF) e celulas por tabulacao; a quebra no fim, que as planilhas sempre poem, nao vira linha.
std::vector<QStringList> lerTsv(const QString& texto) {
    QString normalizado = texto;
    normalizado.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalizado.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (normalizado.endsWith(QLatin1Char('\n'))) normalizado.chop(1);
    std::vector<QStringList> linhas;
    if (normalizado.isEmpty()) return linhas;
    for (const QString& linha : normalizado.split(QLatin1Char('\n'))) linhas.push_back(linha.split(QLatin1Char('\t')));
    return linhas;
}

// Formato que as planilhas entendem ao colar: celulas separadas por tabulacao e linhas por CRLF.
QString formatarTsv(const std::vector<QStringList>& linhas) {
    QString texto;
    for (const QStringList& linha : linhas) texto += linha.join(QLatin1Char('\t')) + QStringLiteral("\r\n");
    return texto;
}

namespace {
constexpr int MAX_VALORES = 2000;
constexpr int LARGURA_MARCA = 14;
const char* const PROP_FILTRADAS = "colunas_filtradas";

// Estilo do cabecalho com uma seta pequena a direita de cada coluna, para mostrar que ela abre o
// filtro; nas colunas filtradas a seta sai na cor de destaque. A largura pedida pela secao ganha o
// espaco da seta, para o texto nao passar por cima.
class EstiloCabecalho : public QProxyStyle {
public:
    explicit EstiloCabecalho(QObject* pai) : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))) { setParent(pai); }

    void drawControl(ControlElement elemento, const QStyleOption* opcao, QPainter* pintor, const QWidget* widget) const override {
        QProxyStyle::drawControl(elemento, opcao, pintor, widget);
        const auto* cabecalho = qstyleoption_cast<const QStyleOptionHeader*>(opcao);
        if (elemento != CE_Header || !cabecalho || !widget) return;
        const bool filtrada = widget->property(PROP_FILTRADAS).toList().contains(cabecalho->section);
        const QRect r = opcao->rect;
        const QPointF centro(r.right() - LARGURA_MARCA / 2.0, r.center().y() + 0.5);
        QPainterPath seta;
        seta.moveTo(centro.x() - 3.5, centro.y() - 2.0);
        seta.lineTo(centro.x() + 3.5, centro.y() - 2.0);
        seta.lineTo(centro.x(), centro.y() + 2.0);
        seta.closeSubpath();
        pintor->save();
        pintor->setRenderHint(QPainter::Antialiasing);
        pintor->setPen(Qt::NoPen);
        pintor->setBrush(filtrada ? opcao->palette.color(QPalette::Highlight) : opcao->palette.color(QPalette::Disabled, QPalette::Text));
        pintor->drawPath(seta);
        pintor->restore();
    }

    QSize sizeFromContents(ContentsType tipo, const QStyleOption* opcao, const QSize& tamanho, const QWidget* widget) const override {
        QSize s = QProxyStyle::sizeFromContents(tipo, opcao, tamanho, widget);
        if (tipo == CT_HeaderSection && widget && qobject_cast<const QHeaderView*>(widget) &&
            static_cast<const QHeaderView*>(widget)->orientation() == Qt::Horizontal)
            s.rwidth() += LARGURA_MARCA;
        return s;
    }
};

// Filtro por coluna no estilo das planilhas, copiar e colar em texto separado por tabulacao e
// ordenacao pelo menu do cabecalho. Nas tabelas ordenaveis a ordenacao e a do modelo (sortByColumn);
// nas demais ela so move as linhas na vista (cabecalho vertical), e o modelo segue na ordem do
// arquivo. Copiar e colar andam pelas linhas na ordem da vista. O filtro esconde as linhas da vista (setRowHidden), sem modelo
// intermediario, entao a edicao continua indo direto ao modelo da tabela; um predicado externo
// (como Ocultar registros vazios) entra na mesma conta, para os dois nao se desfazerem. O clique no
// cabecalho so abre o menu: a ordenacao automatica e a selecao da coluna inteira ficam desligadas.
class RecursosTabela : public QObject {
public:
    RecursosTabela(QTableView* tabela, bool ordenavel) : QObject(tabela), tabela_(tabela), ordenavel_(ordenavel) {
        QHeaderView* cabecalho = tabela_->horizontalHeader();
        tabela_->setSortingEnabled(false);
        cabecalho->setSortIndicatorShown(false);
        cabecalho->setSectionsClickable(true);
        QObject::disconnect(cabecalho, &QHeaderView::sectionPressed, tabela_, &QTableView::selectColumn);
        QObject::disconnect(cabecalho, &QHeaderView::sectionEntered, tabela_, nullptr);
        cabecalho->setStyle(new EstiloCabecalho(cabecalho));
        connect(cabecalho, &QHeaderView::sectionClicked, this, &RecursosTabela::abrirMenu);
        tabela_->installEventFilter(this);
        tabela_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tabela_, &QWidget::customContextMenuRequested, this, [this](const QPoint& ponto) {
            QMenu menu(tabela_);
            menu.addAction(QStringLiteral("Copiar"), QKeySequence::Copy, this, &RecursosTabela::copiar);
            menu.addAction(QStringLiteral("Colar"), QKeySequence::Paste, this, &RecursosTabela::colar)
                ->setEnabled(tabela_->editTriggers() != QAbstractItemView::NoEditTriggers);
            if (extras_) {
                menu.addSeparator();
                extras_(&menu, tabela_->indexAt(ponto));
            }
            menu.exec(tabela_->viewport()->mapToGlobal(ponto));
        });
        ligarModelo();
    }

    void limpar() {
        filtros_.clear();
        aplicar();
    }

    void definirOcultas(std::function<bool(int)> oculta) {
        oculta_ = std::move(oculta);
        aplicar();
    }

    void definirExtras(std::function<void(QMenu*, const QModelIndex&)> extras) { extras_ = std::move(extras); }

    // Esconde as linhas que nao passam nos filtros das colunas ou no predicado externo e marca no
    // cabecalho as colunas filtradas. Na destruicao da tabela os cabecalhos saem antes do modelo, que
    // ainda avisa que foi reiniciado; sem cabecalho nao ha o que fazer.
    void aplicar() {
        if (!cabecalho_ || !vertical_) return;
        QAbstractItemModel* modelo = tabela_->model();
        if (!modelo) return;
        for (int r = 0; r < modelo->rowCount(); ++r) tabela_->setRowHidden(r, !visivel(r, -1));
        QVariantList filtradas;
        for (const auto& [coluna, valores] : filtros_) filtradas << coluna;
        tabela_->horizontalHeader()->setProperty(PROP_FILTRADAS, filtradas);
        tabela_->horizontalHeader()->viewport()->update();
    }

protected:
    bool eventFilter(QObject* objeto, QEvent* evento) override {
        if (objeto == tabela_ && evento->type() == QEvent::KeyPress) {
            auto* tecla = static_cast<QKeyEvent*>(evento);
            if (tecla->matches(QKeySequence::Copy)) {
                copiar();
                return true;
            }
            if (tecla->matches(QKeySequence::Paste)) {
                colar();
                return true;
            }
        }
        return QObject::eventFilter(objeto, evento);
    }

private:
    // O modelo pode ser trocado depois (setModel); os filtros valem para o modelo atual e sao
    // descartados quando ele e refeito, porque as colunas podem ser outras.
    void ligarModelo() {
        QAbstractItemModel* modelo = tabela_->model();
        if (!modelo || modelo == modelo_ligado_) return;
        modelo_ligado_ = modelo;
        connect(modelo, &QAbstractItemModel::modelReset, this, [this] {
            filtros_.clear();
            aplicar();
        });
        connect(modelo, &QAbstractItemModel::layoutChanged, this, &RecursosTabela::aplicar);
        connect(modelo, &QAbstractItemModel::rowsInserted, this, &RecursosTabela::aplicar);
        connect(modelo, &QAbstractItemModel::rowsRemoved, this, &RecursosTabela::aplicar);
    }

    // Valor para copiar e colar: o editavel (o codigo, nas colunas que mostram um nome).
    QString texto(int linha, int coluna) const {
        const QModelIndex ix = tabela_->model()->index(linha, coluna);
        const QVariant edicao = ix.data(Qt::EditRole);
        return (edicao.isValid() ? edicao : ix.data(Qt::DisplayRole)).toString().trimmed();
    }

    // Valor para o filtro: o que a celula mostra ("SUDESTE (1)").
    QString exibido(int linha, int coluna) const {
        return tabela_->model()->index(linha, coluna).data(Qt::DisplayRole).toString().trimmed();
    }

    // Linha passa nos filtros de todas as colunas, menos a ignorada (a do menu aberto, para a lista de
    // valores mostrar tambem os que o proprio filtro dela esconde), e no predicado externo.
    bool visivel(int linha, int ignorada) const {
        if (oculta_ && oculta_(linha)) return false;
        for (const auto& [coluna, valores] : filtros_)
            if (coluna != ignorada && !valores.count(exibido(linha, coluna))) return false;
        return true;
    }

    // Menu do cabecalho: ordenacao, pesquisa e lista de valores distintos da coluna com caixas de
    // marcar, como nas planilhas.
    void abrirMenu(int coluna) {
        ligarModelo();
        QAbstractItemModel* modelo = tabela_->model();
        if (!modelo) return;

        std::set<QString> vistos;
        std::vector<QString> valores;
        for (int r = 0; r < modelo->rowCount() && static_cast<int>(valores.size()) < MAX_VALORES; ++r) {
            if (!visivel(r, coluna)) continue;
            const QString v = exibido(r, coluna);
            if (vistos.insert(v).second) valores.push_back(v);
        }
        bool numericos = true;
        for (const QString& v : valores) {
            bool ok = v.isEmpty();
            if (!ok) v.toDouble(&ok);
            if (!ok) numericos = false;
        }
        QCollator colador;
        colador.setNumericMode(true);
        std::sort(valores.begin(), valores.end(), [&](const QString& a, const QString& b) {
            if (a.isEmpty() != b.isEmpty()) return b.isEmpty();
            return numericos ? a.toDouble() < b.toDouble() : colador.compare(a, b) < 0;
        });

        QMenu menu(tabela_);
        menu.addAction(QStringLiteral("Ordenar crescente"), this, [this, coluna] { ordenar(coluna, Qt::AscendingOrder); });
        menu.addAction(QStringLiteral("Ordenar decrescente"), this, [this, coluna] { ordenar(coluna, Qt::DescendingOrder); });
        if (!ordenavel_)
            menu.addAction(QStringLiteral("Ordem do arquivo"), this, [this] { ordenar(-1, Qt::AscendingOrder); })
                ->setEnabled(tabela_->verticalHeader()->sectionsMoved());
        menu.addSeparator();
        auto* painel = new QWidget(&menu);
        auto* v = new QVBoxLayout(painel);
        v->setContentsMargins(6, 4, 6, 4);
        v->setSpacing(4);
        auto* busca = new QLineEdit(painel);
        busca->setPlaceholderText(QStringLiteral("Pesquisar"));
        busca->setClearButtonEnabled(true);
        auto* lista = new QListWidget(painel);
        lista->setMinimumSize(200, 220);
        auto* todos = new QListWidgetItem(QStringLiteral("(Selecionar tudo)"), lista);
        todos->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        const auto filtro = filtros_.find(coluna);
        for (const QString& valor : valores) {
            auto* item = new QListWidgetItem(valor.isEmpty() ? QStringLiteral("(vazias)") : valor, lista);
            item->setData(Qt::UserRole, valor);
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            item->setCheckState(filtro == filtros_.end() || filtro->second.count(valor) ? Qt::Checked : Qt::Unchecked);
        }
        auto marcarTodos = [lista, todos] {
            bool todos_marcados = true;
            for (int i = 1; i < lista->count(); ++i)
                if (!lista->item(i)->isHidden() && lista->item(i)->checkState() != Qt::Checked) todos_marcados = false;
            const QSignalBlocker bloqueio(lista);
            todos->setCheckState(todos_marcados ? Qt::Checked : Qt::Unchecked);
        };
        marcarTodos();
        connect(lista, &QListWidget::itemChanged, painel, [lista, todos, marcarTodos](QListWidgetItem* item) {
            if (item == todos) {
                const QSignalBlocker bloqueio(lista);
                for (int i = 1; i < lista->count(); ++i)
                    if (!lista->item(i)->isHidden()) lista->item(i)->setCheckState(todos->checkState());
            } else {
                marcarTodos();
            }
        });
        connect(busca, &QLineEdit::textChanged, painel, [lista, marcarTodos](const QString& t) {
            for (int i = 1; i < lista->count(); ++i) lista->item(i)->setHidden(!lista->item(i)->text().contains(t, Qt::CaseInsensitive));
            marcarTodos();
        });
        auto* botoes = new QHBoxLayout;
        auto* limpar = new QPushButton(QStringLiteral("Limpar filtro"), painel);
        auto* ok = new QPushButton(QStringLiteral("OK"), painel);
        auto* cancelar = new QPushButton(QStringLiteral("Cancelar"), painel);
        ok->setDefault(true);
        botoes->addWidget(limpar);
        botoes->addStretch(1);
        botoes->addWidget(ok);
        botoes->addWidget(cancelar);
        v->addWidget(busca);
        v->addWidget(lista, 1);
        v->addLayout(botoes);
        auto* acao = new QWidgetAction(&menu);
        acao->setDefaultWidget(painel);
        menu.addAction(acao);

        connect(limpar, &QPushButton::clicked, &menu, [this, coluna, &menu] {
            filtros_.erase(coluna);
            aplicar();
            menu.close();
        });
        connect(cancelar, &QPushButton::clicked, &menu, &QMenu::close);
        connect(ok, &QPushButton::clicked, &menu, [this, coluna, lista, &menu] {
            std::set<QString> aceitos;
            bool todos_aceitos = true;
            for (int i = 1; i < lista->count(); ++i) {
                QListWidgetItem* item = lista->item(i);
                if (!item->isHidden() && item->checkState() == Qt::Checked) aceitos.insert(item->data(Qt::UserRole).toString());
                else todos_aceitos = false;
            }
            if (todos_aceitos) filtros_.erase(coluna);
            else filtros_[coluna] = std::move(aceitos);
            aplicar();
            menu.close();
        });
        busca->setFocus();
        QHeaderView* cabecalho = tabela_->horizontalHeader();
        menu.exec(cabecalho->mapToGlobal(QPoint(cabecalho->sectionViewportPosition(coluna), cabecalho->height())));
    }

public:
    // Ordena pela coluna (-1 volta a ordem do arquivo). Nas tabelas que nao sao ordenaveis pelo
    // modelo, move as linhas da vista: numeros pela ordem numerica (papel UserRole, quando o modelo o
    // da), textos pela ordem alfabetica, vazios por ultimo; empates ficam na ordem do arquivo.
    void ordenar(int coluna, Qt::SortOrder ordem) {
        if (ordenavel_) {
            if (coluna >= 0) tabela_->sortByColumn(coluna, ordem);
            return;
        }
        QAbstractItemModel* modelo = tabela_->model();
        if (!modelo) return;
        const int n = modelo->rowCount();
        std::vector<int> linhas(static_cast<size_t>(n));
        for (int r = 0; r < n; ++r) linhas[static_cast<size_t>(r)] = r;
        if (coluna >= 0) {
            QCollator colador;
            colador.setNumericMode(true);
            std::vector<QVariant> chaves;
            chaves.reserve(static_cast<size_t>(n));
            for (int r = 0; r < n; ++r) {
                const QModelIndex ix = modelo->index(r, coluna);
                const QVariant chave = ix.data(Qt::UserRole);
                chaves.push_back(chave.isValid() ? chave : ix.data(Qt::DisplayRole));
            }
            const auto numero = [](const QVariant& v) { return v.typeId() != QMetaType::QString && v.canConvert<double>(); };
            const auto vazio = [&](const QVariant& v) {
                return !v.isValid() || (numero(v) ? std::isinf(v.toDouble()) : v.toString().trimmed().isEmpty());
            };
            std::stable_sort(linhas.begin(), linhas.end(), [&](int a, int b) {
                const QVariant& va = chaves[static_cast<size_t>(a)];
                const QVariant& vb = chaves[static_cast<size_t>(b)];
                if (vazio(va) || vazio(vb)) return !vazio(va) && vazio(vb);
                const int c = numero(va) && numero(vb) ? (va.toDouble() < vb.toDouble() ? -1 : va.toDouble() > vb.toDouble() ? 1 : 0)
                                                       : colador.compare(va.toString(), vb.toString());
                return ordem == Qt::AscendingOrder ? c < 0 : c > 0;
            });
        }
        QHeaderView* vertical = tabela_->verticalHeader();
        for (int i = 0; i < n; ++i) {
            const int atual = vertical->visualIndex(linhas[static_cast<size_t>(i)]);
            if (atual != i) vertical->moveSection(atual, i);
        }
    }

private:
    // Linhas do modelo na ordem em que a vista as mostra.
    std::vector<int> linhasNaVista() const {
        QHeaderView* vertical = tabela_->verticalHeader();
        const int n = tabela_->model() ? tabela_->model()->rowCount() : 0;
        std::vector<int> linhas;
        linhas.reserve(static_cast<size_t>(n));
        for (int v = 0; v < n; ++v) linhas.push_back(vertical->logicalIndex(v));
        return linhas;
    }

    // Retangulo da selecao na ordem da vista, sem as linhas e colunas escondidas; celula fora da
    // selecao sai vazia.
    void copiar() {
        QAbstractItemModel* modelo = tabela_->model();
        const QModelIndexList selecao = tabela_->selectionModel()->selectedIndexes();
        if (!modelo || selecao.isEmpty()) return;
        QHeaderView* vertical = tabela_->verticalHeader();
        int v0 = vertical->visualIndex(selecao.front().row()), v1 = v0, c0 = selecao.front().column(), c1 = c0;
        std::set<std::pair<int, int>> marcadas;
        for (const QModelIndex& ix : selecao) {
            v0 = std::min(v0, vertical->visualIndex(ix.row()));
            v1 = std::max(v1, vertical->visualIndex(ix.row()));
            c0 = std::min(c0, ix.column());
            c1 = std::max(c1, ix.column());
            marcadas.insert({ix.row(), ix.column()});
        }
        std::vector<QStringList> linhas;
        for (int v = v0; v <= v1; ++v) {
            const int r = vertical->logicalIndex(v);
            if (tabela_->isRowHidden(r)) continue;
            QStringList linha;
            for (int c = c0; c <= c1; ++c)
                if (!tabela_->isColumnHidden(c)) linha << (marcadas.count({r, c}) ? texto(r, c) : QString());
            linhas.push_back(linha);
        }
        QApplication::clipboard()->setText(formatarTsv(linhas));
    }

    // Cola a partir do canto da selecao, descendo pelas linhas visiveis na ordem da vista; um valor so, com varias
    // celulas selecionadas, vai para todas elas. Cada valor passa pelo setData do modelo, com a mesma
    // validacao da edicao celula a celula; celulas que nao se editam ficam de fora, e tabela so de
    // leitura (sem gatilho de edicao) nao recebe nada. Em tabela que seleciona linhas inteiras, a
    // colagem comeca na coluna da celula clicada.
    void colar() {
        if (tabela_->editTriggers() == QAbstractItemView::NoEditTriggers) return;
        QAbstractItemModel* modelo = tabela_->model();
        const std::vector<QStringList> linhas = lerTsv(QApplication::clipboard()->text());
        QModelIndexList selecao = tabela_->selectionModel()->selectedIndexes();
        if (!modelo || linhas.empty()) return;
        if (selecao.isEmpty() && tabela_->currentIndex().isValid()) selecao << tabela_->currentIndex();
        if (selecao.isEmpty()) return;

        int gravados = 0;
        int recusados = 0;
        auto gravar = [&](const QModelIndex& ix, const QString& valor) {
            if (!(modelo->flags(ix) & Qt::ItemIsEditable)) return;
            if (modelo->setData(ix, valor, Qt::EditRole)) ++gravados;
            else if (valor.trimmed() != texto(ix.row(), ix.column())) ++recusados;
        };

        if (linhas.size() == 1 && linhas.front().size() == 1 && selecao.size() > 1) {
            for (const QModelIndex& ix : selecao)
                if (!tabela_->isRowHidden(ix.row())) gravar(ix, linhas.front().front());
        } else {
            QHeaderView* vertical = tabela_->verticalHeader();
            const std::vector<int> ordem = linhasNaVista();
            int v = vertical->visualIndex(selecao.front().row()), c0 = selecao.front().column();
            for (const QModelIndex& ix : selecao) {
                v = std::min(v, vertical->visualIndex(ix.row()));
                c0 = std::min(c0, ix.column());
            }
            if (tabela_->selectionBehavior() == QAbstractItemView::SelectRows && tabela_->currentIndex().isValid())
                c0 = tabela_->currentIndex().column();
            const int n = static_cast<int>(ordem.size());
            for (const QStringList& linha : linhas) {
                while (v < n && tabela_->isRowHidden(ordem[static_cast<size_t>(v)])) ++v;
                if (v >= n) break;
                const int r = ordem[static_cast<size_t>(v)];
                int c = c0;
                for (const QString& valor : linha) {
                    while (c < modelo->columnCount() && tabela_->isColumnHidden(c)) ++c;
                    if (c >= modelo->columnCount()) break;
                    gravar(modelo->index(r, c), valor);
                    ++c;
                }
                ++v;
            }
        }
        QString resumo = QStringLiteral("%1 valores colados").arg(gravados);
        if (recusados > 0) resumo += QStringLiteral(", %1 recusados").arg(recusados);
        QToolTip::showText(tabela_->viewport()->mapToGlobal(tabela_->visualRect(tabela_->currentIndex()).bottomLeft()), resumo, tabela_);
    }

    QTableView* tabela_;
    QPointer<QHeaderView> cabecalho_ = tabela_->horizontalHeader();
    QPointer<QHeaderView> vertical_ = tabela_->verticalHeader();
    bool ordenavel_;
    QAbstractItemModel* modelo_ligado_ = nullptr;
    std::map<int, std::set<QString>> filtros_;
    std::function<bool(int)> oculta_;
    std::function<void(QMenu*, const QModelIndex&)> extras_;
};

RecursosTabela* recursos(QTableView* tabela) {
    for (QObject* filho : tabela->children())
        if (auto* r = dynamic_cast<RecursosTabela*>(filho)) return r;
    return nullptr;
}
}  // namespace

// Liga a tabela ao filtro pelo cabecalho, ao copiar e colar (Ctrl+C, Ctrl+V e menu de contexto) e,
// com ordenavel, a ordenacao pelo mesmo menu do cabecalho. Chamar depois de setModel.
void habilitarRecursos(QTableView* tabela, bool ordenavel) {
    if (!recursos(tabela)) new RecursosTabela(tabela, ordenavel);
}

// Linhas que a tabela esconde alem das dos filtros, como os registros vazios; o predicado recebe a
// linha do modelo da tabela.
void definirLinhasOcultas(QTableView* tabela, std::function<bool(int)> oculta) {
    RecursosTabela* r = recursos(tabela);
    if (!r) {
        habilitarRecursos(tabela);
        r = recursos(tabela);
    }
    r->definirOcultas(std::move(oculta));
}

// Tira todos os filtros das colunas, para quando o conteudo da tabela e trocado sem refazer o modelo.
void limparFiltros(QTableView* tabela) {
    if (RecursosTabela* r = recursos(tabela)) r->limpar();
}

// Ordena a tabela pela coluna, como o menu do cabecalho; coluna -1 volta a ordem do arquivo.
void ordenarTabela(QTableView* tabela, int coluna, Qt::SortOrder ordem) {
    habilitarRecursos(tabela);
    recursos(tabela)->ordenar(coluna, ordem);
}

// Acoes que o menu de contexto da tabela acrescenta depois de copiar e colar; recebem o indice (da
// vista) sob o cursor, invalido fora das linhas.
void definirAcoesExtras(QTableView* tabela, std::function<void(QMenu*, const QModelIndex&)> acoes) {
    habilitarRecursos(tabela);
    recursos(tabela)->definirExtras(std::move(acoes));
}
