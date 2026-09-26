#include "ajuste_colunas.h"
#include <QEvent>
#include <QHeaderView>
#include <QTableView>
#include <algorithm>
#include <vector>

namespace {
// Guarda a largura natural de cada coluna (a do conteudo e do cabecalho) e reparte a sobra da vista
// entre elas sempre que a vista muda de tamanho.
class PreenchimentoColunas : public QObject {
public:
    explicit PreenchimentoColunas(QTableView* tabela) : QObject(tabela), tabela_(tabela) {
        tabela_->viewport()->installEventFilter(this);
    }

    void medir() {
        tabela_->resizeColumnsToContents();
        const int n = tabela_->horizontalHeader()->count();
        naturais_.assign(static_cast<size_t>(n), 0);
        for (int c = 0; c < n; ++c) naturais_[static_cast<size_t>(c)] = tabela_->columnWidth(c);
        distribuir();
    }

    // Com conteudo mais estreito que a vista, cada coluna ganha parte da sobra proporcional a sua
    // largura natural e a ultima visivel fica com o resto do arredondamento; com conteudo mais largo,
    // as colunas ficam na largura natural e a vista rola.
    void distribuir() {
        QHeaderView* cabecalho = tabela_->horizontalHeader();
        if (static_cast<int>(naturais_.size()) != cabecalho->count()) return;
        int soma = 0;
        int ultima = -1;
        for (int c = 0; c < cabecalho->count(); ++c) {
            if (cabecalho->isSectionHidden(c)) continue;
            soma += naturais_[static_cast<size_t>(c)];
            ultima = c;
        }
        const int disponivel = tabela_->viewport()->width();
        if (ultima < 0 || soma <= 0) return;
        const int sobra = std::max(0, disponivel - soma);
        int usado = 0;
        for (int c = 0; c < cabecalho->count(); ++c) {
            if (cabecalho->isSectionHidden(c)) continue;
            const int natural = naturais_[static_cast<size_t>(c)];
            int largura = natural + static_cast<int>(static_cast<long long>(sobra) * natural / soma);
            if (c == ultima && sobra > 0) largura = disponivel - usado;
            tabela_->setColumnWidth(c, largura);
            usado += largura;
        }
    }

protected:
    bool eventFilter(QObject* objeto, QEvent* evento) override {
        if (evento->type() == QEvent::Resize) distribuir();
        return QObject::eventFilter(objeto, evento);
    }

private:
    QTableView* tabela_;
    std::vector<int> naturais_;
};

PreenchimentoColunas* preenchimento(QTableView* tabela) {
    for (QObject* filho : tabela->children())
        if (auto* p = dynamic_cast<PreenchimentoColunas*>(filho)) return p;
    return nullptr;
}
}  // namespace

// Faz as colunas da tabela ocuparem toda a largura da vista, sem apertar nenhuma abaixo do seu
// conteudo. As larguras sao medidas de novo quando o modelo e refeito ou muda de forma; chamar
// depois de setModel.
void preencherLargura(QTableView* tabela) {
    auto* p = new PreenchimentoColunas(tabela);
    if (QAbstractItemModel* modelo = tabela->model()) {
        QObject::connect(modelo, &QAbstractItemModel::modelReset, p, &PreenchimentoColunas::medir);
        QObject::connect(modelo, &QAbstractItemModel::layoutChanged, p, &PreenchimentoColunas::medir);
        QObject::connect(modelo, &QAbstractItemModel::rowsInserted, p, &PreenchimentoColunas::medir);
        QObject::connect(modelo, &QAbstractItemModel::columnsInserted, p, &PreenchimentoColunas::medir);
    }
    p->medir();
}

// Mede de novo as larguras naturais e preenche a vista; no lugar de resizeColumnsToContents nas
// tabelas com preencherLargura.
void ajustarColunas(QTableView* tabela) {
    if (PreenchimentoColunas* p = preenchimento(tabela)) p->medir();
    else tabela->resizeColumnsToContents();
}
