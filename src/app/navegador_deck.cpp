#include "navegador_deck.h"
#include <QDir>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <algorithm>
#include <filesystem>
#include "catalogo_newave.h"
#include "deck_newave.h"
#include "pagina_arquivo.h"
#include "pagina_modificacoes.h"

// Uma aba por secao do catalogo, cada uma com a lista dos arquivos a esquerda, larga o bastante para
// o titulo mais longo, e a pagina do arquivo selecionado a direita. A pagina do hidr.dat e o editor
// que ja existia, recebido pronto; a aba Modificacoes e a pagina propria do modif.dat. A primeira
// aba, com o hidr.dat, abre selecionada.
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
        lista->setRootIsDecorated(false);
        auto* paginas = new QStackedWidget(divisor);
        int largura_titulos = 0;
        for (const ArquivoNewave& arquivo : catalogoNewave()) {
            if (arquivo.secao != secao) continue;
            QWidget* pagina = nullptr;
            if (arquivo.nome_padrao == QStringLiteral("hidr.dat")) {
                pagina = editor_hidr;
            } else {
                auto* pagina_arquivo = new PaginaArquivo(arquivo, paginas);
                paginas_.push_back(pagina_arquivo);
                pagina = pagina_arquivo;
            }
            auto* item = new QTreeWidgetItem(lista, {arquivo.titulo});
            largura_titulos = std::max(largura_titulos, lista->fontMetrics().horizontalAdvance(arquivo.titulo));
            item->setToolTip(0, arquivo.nome_padrao);
            item->setData(0, Qt::UserRole, paginas->addWidget(pagina));
        }
        connect(lista, &QTreeWidget::itemSelectionChanged, paginas, [lista, paginas] {
            const QList<QTreeWidgetItem*> selecionados = lista->selectedItems();
            if (!selecionados.isEmpty()) paginas->setCurrentIndex(selecionados.first()->data(0, Qt::UserRole).toInt());
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
    auto modif = arquivos.find("ALTERACAO DADOS USINAS HIDRO");
    QString nome_modif = modif != arquivos.end() ? QString::fromStdString(modif->second) : QStringLiteral("modif.dat");
    modificacoes_->carregar(QDir(dir_deck).filePath(nome_modif));
}
