#include "navegador_deck.h"
#include <QDir>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <algorithm>
#include <filesystem>
#include "catalogo_newave.h"
#include "dados_deck.h"
#include "estilo_arvore.h"
#include "deck_newave.h"
#include "editor_termicas.h"
#include "layouts_newave.h"
#include "pagina_arquivo.h"
#include "pagina_arquivo_fixo.h"
#include "pagina_modificacoes.h"

// Uma aba por secao do catalogo, cada uma com a arvore dos arquivos a esquerda e a pagina do item
// selecionado a direita. Na arvore, os arquivos ficam sob cabecalhos de grupo colapsaveis (na ordem
// em que o grupo aparece no catalogo). Arquivo de colunas fixas com mais de uma secao ganha um filho
// por secao. A aba
// Modificacoes e a pagina propria do modif.dat. Todas as vistas editaveis usam o mesmo repositorio.
NavegadorDeck::NavegadorDeck(QWidget* editor_hidr, const ModeloHidr* modelo, QWidget* parent) : QTabWidget(parent) {
    setDocumentMode(true);
    dados_ = new DadosDeck(this);
    modificacoes_ = new PaginaModificacoes(modelo, this);
    for (const QString& secao : secoesNewave()) {
        if (secao == QStringLiteral("Modificações")) {
            addTab(modificacoes_, secao);
            continue;
        }
        auto* divisor = new QSplitter(Qt::Horizontal, this);
        divisor->setHandleWidth(4);
        auto* lista = new QTreeWidget(divisor);
        lista->setHeaderHidden(true);
        estilizarArvore(lista);
        auto* paginas = new QStackedWidget(divisor);

        std::vector<QString> grupos;
        for (const ArquivoNewave& arquivo : catalogoNewave())
            if (arquivo.secao == secao && std::find(grupos.begin(), grupos.end(), grupoNewave(arquivo.nome_padrao)) == grupos.end())
                grupos.push_back(grupoNewave(arquivo.nome_padrao));

        int largura = 0;
        QTreeWidgetItem* primeiro = nullptr;
        for (const QString& grupo : grupos) {
            QTreeWidgetItem* cabecalho = grupo.isEmpty() ? nullptr : novoGrupoArvore(lista, grupo);
            const int recuo = (cabecalho ? 2 : 1) * lista->indentation();
            for (const ArquivoNewave& arquivo : catalogoNewave()) {
                if (arquivo.secao != secao || grupoNewave(arquivo.nome_padrao) != grupo) continue;
                const LayoutArquivoFixo* layout = layoutNewave(arquivo.nome_padrao.toStdString());
                const bool hidro = arquivo.nome_padrao == QStringLiteral("hidr.dat");
                const bool termicas = arquivo.nome_padrao == QStringLiteral("term.dat");
                QWidget* pagina = nullptr;
                PaginaArquivo* previa = nullptr;
                if (hidro) {
                    pagina = editor_hidr;
                } else if (termicas) {
                    pagina = new EditorTermicas(dados_, paginas);
                } else if (layout) {
                    pagina = new PaginaArquivoFixo(arquivo, *layout, dados_, paginas);
                } else {
                    previa = new PaginaArquivo(arquivo, paginas);
                    paginas_.push_back(previa);
                    pagina = previa;
                }
                const int indice = paginas->addWidget(pagina);
                auto* item = cabecalho ? new QTreeWidgetItem(cabecalho, {arquivo.titulo}) : new QTreeWidgetItem(lista, {arquivo.titulo});
                item->setToolTip(0, previa ? QStringLiteral("%1  ·  somente prévia, o editor ainda não existe").arg(arquivo.nome_padrao)
                                           : arquivo.nome_padrao);
                item->setData(0, Qt::UserRole, indice);
                item->setData(0, Qt::UserRole + 1, 0);
                itens_.push_back({item, arquivo.titulo, arquivo.nome_padrao, previa});
                if (!primeiro) primeiro = item;
                largura = std::max(largura, recuo + lista->fontMetrics().horizontalAdvance(arquivo.titulo));
                if (layout && !termicas && layout->secoes.size() > 1) {
                    for (size_t s = 0; s < layout->secoes.size(); ++s) {
                        const QString titulo = QString::fromStdString(layout->secoes[s].titulo);
                        auto* filho = new QTreeWidgetItem(item, {titulo});
                        filho->setData(0, Qt::UserRole, indice);
                        filho->setData(0, Qt::UserRole + 1, static_cast<int>(s));
                        largura = std::max(largura, recuo + lista->indentation() + lista->fontMetrics().horizontalAdvance(titulo));
                    }
                }
            }
        }
        lista->expandAll();
        connect(lista, &QTreeWidget::itemSelectionChanged, paginas, [lista, paginas] {
            const QList<QTreeWidgetItem*> selecionados = lista->selectedItems();
            if (selecionados.isEmpty() || !(selecionados.first()->flags() & Qt::ItemIsSelectable)) return;
            paginas->setCurrentIndex(selecionados.first()->data(0, Qt::UserRole).toInt());
            if (auto* pagina_fixa = qobject_cast<PaginaArquivoFixo*>(paginas->currentWidget()))
                pagina_fixa->mostrarSecao(selecionados.first()->data(0, Qt::UserRole + 1).toInt());
        });
        lista->setCurrentItem(primeiro);
        divisor->addWidget(lista);
        divisor->addWidget(paginas);
        divisor->setStretchFactor(1, 1);
        divisor->setSizes({largura + 2 * lista->frameWidth() + 24, 800});
        addTab(divisor, secao);
    }
    connect(dados_, &DadosDeck::alterado, this, &NavegadorDeck::atualizarItens);
    connect(dados_, &DadosDeck::recarregado, this, &NavegadorDeck::atualizarItens);
}

// Le o arquivos.dat da pasta do deck uma vez e repassa ao repositorio e as paginas de previa, que
// resolvem o nome real dos seus arquivos por ele; o modif.dat vem do rotulo "ALTERACAO DADOS USINAS
// HIDRO".
void NavegadorDeck::carregarDeck(const QString& dir_deck) {
    const auto arquivos = lerArquivosDat(std::filesystem::path(QDir(dir_deck).filePath(QStringLiteral("arquivos.dat")).toStdWString()));
    for (PaginaArquivo* pagina : paginas_) pagina->carregar(dir_deck, arquivos);
    dados_->carregar(dir_deck, arquivos);
    auto modif = arquivos.find("ALTERACAO DADOS USINAS HIDRO");
    QString nome_modif = modif != arquivos.end() ? QString::fromStdString(modif->second) : QStringLiteral("modif.dat");
    modificacoes_->carregar(QDir(dir_deck).filePath(nome_modif));
}

// Estado de cada item: com um deck aberto, arquivo que nao esta nele fica esmaecido e em italico;
// arquivo alterado e nao salvo ganha um ponto depois do titulo. O hidr.dat,
// aberto pelo proprio editor, nunca aparece como ausente.
void NavegadorDeck::atualizarItens() {
    const QStringList alterados = dados_->modificados();
    for (const ItemArquivo& i : itens_) {
        QTreeWidget* arvore = i.item->treeWidget();
        bool ausente = false;
        if (dados_->carregado() && i.nome_padrao != QStringLiteral("hidr.dat"))
            ausente = i.previa ? !i.previa->encontrado() : dados_->arquivo(i.nome_padrao) == nullptr;
        const QPalette& paleta = arvore->palette();
        const QColor cor = ausente ? paleta.color(QPalette::Disabled, QPalette::Text) : paleta.color(QPalette::Text);
        QFont fonte = arvore->font();
        fonte.setItalic(ausente);
        i.item->setFont(0, fonte);
        i.item->setForeground(0, cor);
        for (int f = 0; f < i.item->childCount(); ++f) i.item->child(f)->setForeground(0, cor);
        i.item->setText(0, alterados.contains(i.nome_padrao) ? i.titulo + QStringLiteral("  •") : i.titulo);
    }
}

QStringList NavegadorDeck::arquivosModificados() const {
    QStringList nomes;
    for (const QString& nome : dados_->modificados()) nomes << dados_->nomeNoDeck(nome);
    return nomes;
}

// Salva todos os arquivos alterados; para no primeiro que falhar.
bool NavegadorDeck::salvarTodos() {
    for (const QString& nome : dados_->modificados())
        if (!dados_->salvar(nome)) return false;
    return true;
}
