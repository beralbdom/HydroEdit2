#include "navegador_deck.h"
#include <QDir>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTreeView>
#include <algorithm>
#include <filesystem>
#include "catalogo_newave.h"
#include "dados_deck.h"
#include "deck_newave.h"
#include "editor_termicas.h"
#include "editor_texto_arquivo.h"
#include "estilo_arvore.h"
#include "layouts_newave.h"
#include "pagina_binaria.h"
#include "pagina_arquivo_fixo.h"
#include "pagina_modificacoes.h"

namespace {
constexpr int PAPEL_PAGINA = Qt::UserRole;
constexpr int PAPEL_SECAO = Qt::UserRole + 1;
constexpr int PAPEL_TEXTO = Qt::UserRole + 2;
}  // namespace

// Uma aba por secao do catalogo, cada uma com a arvore dos arquivos a esquerda e a pagina do item
// selecionado a direita. Na arvore, os arquivos ficam sob cabecalhos de grupo colapsaveis (na ordem
// em que o grupo aparece no catalogo), e arquivo de colunas fixas com mais de uma secao ganha um
// filho por secao. Cada arquivo de texto tem duas paginas: a normal (editor ou tabela) e a do editor
// textual, mostrada quando o modo textual esta ligado; os binarios so tem a normal, e um arquivo de
// texto sem layout so tem a do editor textual. A aba
// Modificacoes alterna do mesmo jeito entre a arvore do modif.dat e o texto dele. Todas as vistas
// editaveis usam o mesmo repositorio.
NavegadorDeck::NavegadorDeck(QWidget* editor_hidr, const ModeloHidr* modelo, QWidget* parent) : QTabWidget(parent) {
    setDocumentMode(true);
    dados_ = new DadosDeck(this);
    for (const QString& secao : secoesNewave()) {
        if (secao == QStringLiteral("Modificações")) {
            pilha_modificacoes_ = new QStackedWidget(this);
            modificacoes_ = new PaginaModificacoes(modelo, dados_, pilha_modificacoes_);
            auto* texto_modif = new EditorTextoArquivo(QStringLiteral("Modificações"), QStringLiteral("modif.dat"),
                                                       QStringLiteral("3.12"), dados_, pilha_modificacoes_);
            editores_texto_.push_back(texto_modif);
            pilha_modificacoes_->addWidget(modificacoes_);
            pilha_modificacoes_->addWidget(texto_modif);
            addTab(pilha_modificacoes_, secao);
            continue;
        }
        auto* divisor = new QSplitter(Qt::Horizontal, this);
        divisor->setHandleWidth(4);
        auto* lista = new QTreeView(divisor);
        auto* itens = new QStandardItemModel(lista);
        lista->setModel(itens);
        lista->setHeaderHidden(true);
        estilizarArvore(lista);
        auto* paginas = new QStackedWidget(divisor);

        std::vector<QString> grupos;
        for (const ArquivoNewave& arquivo : catalogoNewave())
            if (arquivo.secao == secao && std::find(grupos.begin(), grupos.end(), grupoNewave(arquivo.nome_padrao)) == grupos.end())
                grupos.push_back(grupoNewave(arquivo.nome_padrao));

        int largura = 0;
        QStandardItem* primeiro = nullptr;
        for (const QString& grupo : grupos) {
            QStandardItem* cabecalho = grupo.isEmpty() ? itens->invisibleRootItem() : novoGrupoArvore(itens, grupo);
            const int recuo = (grupo.isEmpty() ? 1 : 2) * lista->indentation();
            for (const ArquivoNewave& arquivo : catalogoNewave()) {
                if (arquivo.secao != secao || grupoNewave(arquivo.nome_padrao) != grupo) continue;
                const LayoutArquivoFixo* layout = layoutNewave(arquivo.nome_padrao.toStdString());
                const bool termicas = arquivo.nome_padrao == QStringLiteral("term.dat");
                QWidget* pagina = nullptr;
                if (arquivo.nome_padrao == QStringLiteral("hidr.dat")) pagina = editor_hidr;
                else if (arquivo.nome_padrao == QStringLiteral("postos.dat")) pagina = postos_ = new PaginaPostos(dados_, paginas);
                else if (arquivo.nome_padrao == QStringLiteral("vazoes.dat")) pagina = vazoes_ = new PaginaVazoes(dados_, paginas);
                else if (termicas) pagina = new EditorTermicas(dados_, paginas);
                else if (layout) pagina = new PaginaArquivoFixo(arquivo, *layout, dados_, paginas);
                int indice = pagina ? paginas->addWidget(pagina) : -1;
                int indice_texto = -1;
                if (!DadosDeck::binario(arquivo.nome_padrao)) {
                    auto* editor = new EditorTextoArquivo(arquivo.titulo, arquivo.nome_padrao, arquivo.secao_manual, dados_, paginas);
                    editores_texto_.push_back(editor);
                    indice_texto = paginas->addWidget(editor);
                }
                if (indice < 0) indice = indice_texto;
                auto* item = new QStandardItem(arquivo.titulo);
                cabecalho->appendRow(item);
                item->setToolTip(arquivo.nome_padrao);
                item->setData(indice, PAPEL_PAGINA);
                item->setData(0, PAPEL_SECAO);
                item->setData(indice_texto, PAPEL_TEXTO);
                itens_.push_back({lista, item, arquivo.titulo, arquivo.nome_padrao});
                if (!primeiro) primeiro = item;
                largura = std::max(largura, recuo + lista->fontMetrics().horizontalAdvance(arquivo.titulo));
                if (layout && !termicas && !layout->parametros && layout->secoes.size() > 1) {
                    for (size_t s = 0; s < layout->secoes.size(); ++s) {
                        const QString titulo = QString::fromStdString(layout->secoes[s].titulo);
                        auto* filho = new QStandardItem(titulo);
                        item->appendRow(filho);
                        filho->setData(indice, PAPEL_PAGINA);
                        filho->setData(static_cast<int>(s), PAPEL_SECAO);
                        filho->setData(indice_texto, PAPEL_TEXTO);
                        largura = std::max(largura, recuo + lista->indentation() + lista->fontMetrics().horizontalAdvance(titulo));
                    }
                }
            }
        }
        lista->expandAll();
        auto mostrar = [this, lista, paginas] {
            const QModelIndexList selecionados = lista->selectionModel()->selectedIndexes();
            if (selecionados.isEmpty() || !(selecionados.first().flags() & Qt::ItemIsSelectable)) return;
            const QModelIndex item = selecionados.first();
            const int indice_texto = item.data(PAPEL_TEXTO).toInt();
            if (modo_texto_ && indice_texto >= 0) {
                paginas->setCurrentIndex(indice_texto);
                return;
            }
            paginas->setCurrentIndex(item.data(PAPEL_PAGINA).toInt());
            if (auto* pagina_fixa = qobject_cast<PaginaArquivoFixo*>(paginas->currentWidget()))
                pagina_fixa->mostrarSecao(item.data(PAPEL_SECAO).toInt());
        };
        mostrar_selecionados_.push_back(mostrar);
        connect(lista->selectionModel(), &QItemSelectionModel::selectionChanged, paginas, mostrar);
        lista->setCurrentIndex(primeiro->index());
        divisor->addWidget(lista);
        divisor->addWidget(paginas);
        divisor->setStretchFactor(1, 1);
        divisor->setSizes({largura + 2 * lista->frameWidth() + 24, 800});
        addTab(divisor, secao);
    }
    connect(dados_, &DadosDeck::alterado, this, &NavegadorDeck::atualizarItens);
    connect(dados_, &DadosDeck::recarregado, this, &NavegadorDeck::atualizarItens);
}

// Le o arquivos.dat da pasta do deck e repassa ao repositorio, que resolve por ele o nome real dos
// arquivos.
void NavegadorDeck::carregarDeck(const QString& dir_deck) {
    const auto arquivos = lerArquivosDat(std::filesystem::path(QDir(dir_deck).filePath(QStringLiteral("arquivos.dat")).toStdWString()));
    dados_->carregar(dir_deck, arquivos);
}

// Liga ou desliga o editor textual: antes de trocar, aplica o que estiver pendente nos editores de
// texto, para as tabelas ja mostrarem a edicao; depois reescolhe a pagina do item selecionado em
// cada aba.
void NavegadorDeck::definirModoTexto(bool ativo) {
    aplicarEdicoesPendentes();
    modo_texto_ = ativo;
    for (const auto& mostrar : mostrar_selecionados_) mostrar();
    pilha_modificacoes_->setCurrentIndex(ativo ? 1 : 0);
}

// Ver > Ocultar registros vazios vale tambem para os postos sem nome, na tabela do postos.dat e na
// lista de postos das vazoes.
void NavegadorDeck::definirOcultarVazios(bool ocultar) {
    if (postos_) postos_->definirOcultarVazios(ocultar);
    if (vazoes_) vazoes_->definirOcultarVazios(ocultar);
}

void NavegadorDeck::aplicarEdicoesPendentes() {
    for (EditorTextoArquivo* editor : editores_texto_) editor->aplicarPendente();
}

// Estado de cada item: com um deck aberto, arquivo que nao esta nele fica esmaecido e em italico;
// arquivo alterado e nao salvo ganha um ponto depois do titulo. O hidr.dat, aberto pelo proprio
// editor, nunca aparece como ausente.
void NavegadorDeck::atualizarItens() {
    const QStringList alterados = dados_->modificados();
    for (const ItemArquivo& i : itens_) {
        QTreeView* arvore = i.arvore;
        bool ausente = false;
        if (dados_->carregado() && i.nome_padrao != QStringLiteral("hidr.dat"))
            ausente = !dados_->lido(i.nome_padrao);
        const QPalette& paleta = arvore->palette();
        const QColor cor = ausente ? paleta.color(QPalette::Disabled, QPalette::Text) : paleta.color(QPalette::Text);
        QFont fonte = arvore->font();
        fonte.setItalic(ausente);
        i.item->setFont(fonte);
        i.item->setForeground(cor);
        for (int f = 0; f < i.item->rowCount(); ++f) i.item->child(f)->setForeground(cor);
        i.item->setText(alterados.contains(i.nome_padrao) ? i.titulo + QStringLiteral("  •") : i.titulo);
    }
}

QStringList NavegadorDeck::arquivosModificados() const {
    QStringList nomes;
    for (const QString& nome : dados_->modificados()) nomes << dados_->nomeNoDeck(nome);
    return nomes;
}

// Salva todos os arquivos alterados, com o que estiver pendente nos editores de texto; para no
// primeiro que falhar.
bool NavegadorDeck::salvarTodos() {
    aplicarEdicoesPendentes();
    for (const QString& nome : dados_->modificados())
        if (!dados_->salvar(nome)) return false;
    return true;
}
