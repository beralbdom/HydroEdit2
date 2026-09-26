#include "navegador_deck.h"
#include <QDir>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <algorithm>
#include <filesystem>
#include "catalogo_newave.h"
#include "deck_newave.h"
#include "layouts_newave.h"
#include "pagina_arquivo.h"
#include "pagina_arquivo_fixo.h"
#include "pagina_modificacoes.h"

// Uma aba por secao do catalogo, cada uma com a arvore dos arquivos a esquerda, larga o bastante para
// o titulo mais longo, e a pagina do item selecionado a direita. A pagina do hidr.dat e o editor que
// ja existia, recebido pronto; arquivos com layout de colunas fixas abrem a tabela editavel, e os
// que tem mais de uma secao ganham um filho por secao na arvore; os demais mostram a previa. A aba
// Modificacoes e a pagina propria do modif.dat. Arquivo alterado e nao salvo aparece com asterisco.
NavegadorDeck::NavegadorDeck(QWidget* editor_hidr, const ModeloHidr* modelo, QWidget* parent) : QTabWidget(parent) {
    setDocumentMode(true);
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
        bool tem_filhos = false;
        auto* paginas = new QStackedWidget(divisor);
        int largura_titulos = 0;
        for (const ArquivoNewave& arquivo : catalogoNewave()) {
            if (arquivo.secao != secao) continue;
            QWidget* pagina = nullptr;
            const LayoutArquivoFixo* layout = layoutNewave(arquivo.nome_padrao.toStdString());
            if (arquivo.nome_padrao == QStringLiteral("hidr.dat")) {
                pagina = editor_hidr;
            } else if (layout) {
                auto* pagina_fixa = new PaginaArquivoFixo(arquivo, *layout, paginas);
                paginas_fixas_.push_back(pagina_fixa);
                pagina = pagina_fixa;
            } else {
                auto* pagina_arquivo = new PaginaArquivo(arquivo, paginas);
                paginas_.push_back(pagina_arquivo);
                pagina = pagina_arquivo;
            }
            const int indice = paginas->addWidget(pagina);
            auto* item = new QTreeWidgetItem(lista, {arquivo.titulo});
            largura_titulos = std::max(largura_titulos, lista->fontMetrics().horizontalAdvance(arquivo.titulo));
            item->setToolTip(0, arquivo.nome_padrao);
            item->setData(0, Qt::UserRole, indice);
            item->setData(0, Qt::UserRole + 1, 0);
            if (layout && layout->secoes.size() > 1) {
                tem_filhos = true;
                for (size_t s = 0; s < layout->secoes.size(); ++s) {
                    const QString titulo = QString::fromStdString(layout->secoes[s].titulo);
                    auto* filho = new QTreeWidgetItem(item, {titulo});
                    filho->setData(0, Qt::UserRole, indice);
                    filho->setData(0, Qt::UserRole + 1, static_cast<int>(s));
                    largura_titulos = std::max(largura_titulos, lista->indentation() + lista->fontMetrics().horizontalAdvance(titulo));
                }
            }
            if (auto* pagina_fixa = qobject_cast<PaginaArquivoFixo*>(pagina)) {
                connect(pagina_fixa, &PaginaArquivoFixo::modificacaoMudou, lista, [item, titulo = arquivo.titulo](bool modificado) {
                    item->setText(0, modificado ? titulo + QStringLiteral(" *") : titulo);
                });
            }
        }
        lista->setRootIsDecorated(tem_filhos);
        lista->expandAll();
        if (tem_filhos) largura_titulos += lista->indentation();
        connect(lista, &QTreeWidget::itemSelectionChanged, paginas, [lista, paginas] {
            const QList<QTreeWidgetItem*> selecionados = lista->selectedItems();
            if (selecionados.isEmpty()) return;
            paginas->setCurrentIndex(selecionados.first()->data(0, Qt::UserRole).toInt());
            if (auto* pagina_fixa = qobject_cast<PaginaArquivoFixo*>(paginas->currentWidget()))
                pagina_fixa->mostrarSecao(selecionados.first()->data(0, Qt::UserRole + 1).toInt());
        });
        lista->setCurrentItem(lista->topLevelItem(0));
        divisor->addWidget(lista);
        divisor->addWidget(paginas);
        divisor->setStretchFactor(1, 1);
        divisor->setSizes({largura_titulos + 2 * lista->frameWidth() + 24, 800});
        addTab(divisor, secao);
    }
}

// Le o arquivos.dat da pasta do deck uma vez e repassa a todas as paginas, que resolvem o nome real
// dos seus arquivos por ele; o modif.dat vem do rotulo "ALTERACAO DADOS USINAS HIDRO".
void NavegadorDeck::carregarDeck(const QString& dir_deck) {
    const auto arquivos = lerArquivosDat(std::filesystem::path(QDir(dir_deck).filePath(QStringLiteral("arquivos.dat")).toStdWString()));
    for (PaginaArquivo* pagina : paginas_) pagina->carregar(dir_deck, arquivos);
    for (PaginaArquivoFixo* pagina : paginas_fixas_) pagina->carregar(dir_deck, arquivos);
    auto modif = arquivos.find("ALTERACAO DADOS USINAS HIDRO");
    QString nome_modif = modif != arquivos.end() ? QString::fromStdString(modif->second) : QStringLiteral("modif.dat");
    modificacoes_->carregar(QDir(dir_deck).filePath(nome_modif));
}

QStringList NavegadorDeck::arquivosModificados() const {
    QStringList nomes;
    for (const PaginaArquivoFixo* pagina : paginas_fixas_)
        if (pagina->modificado()) nomes << pagina->nomeArquivo();
    return nomes;
}

// Salva todos os arquivos alterados; para no primeiro que falhar, que fica com o motivo na pagina.
bool NavegadorDeck::salvarTodos() {
    for (PaginaArquivoFixo* pagina : paginas_fixas_)
        if (!pagina->salvar()) return false;
    return true;
}
