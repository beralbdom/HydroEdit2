#include "grade_vetor.h"
#include <QFrame>
#include <QHeaderView>
#include "modelo_hidr.h"

GradeVetor::GradeVetor(ModeloHidr* modelo, int linhas, int colunas, const QStringList& cab_h, const QStringList& cab_v,
                       QWidget* parent)
    : QTableWidget(linhas, colunas, parent), modelo_(modelo), celulas_(static_cast<size_t>(linhas * colunas)) {
    setHorizontalHeaderLabels(cab_h);
    if (cab_v.isEmpty()) verticalHeader()->setVisible(false);
    else setVerticalHeaderLabels(cab_v);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    verticalHeader()->setDefaultSectionSize(20);
    setShowGrid(true);
    setAlternatingRowColors(true);
    setFrameShape(QFrame::StyledPanel);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    for (int l = 0; l < linhas; ++l)
        for (int c = 0; c < colunas; ++c) {
            auto* item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            item->setFlags(Qt::ItemIsSelectable);
            setItem(l, c, item);
        }
    connect(this, &QTableWidget::cellChanged, this, &GradeVetor::aoEditar);
}

void GradeVetor::definirCelula(int l, int c, const Campo* campo, int indice) {
    celula(l, c) = {campo, indice};
    item(l, c)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
}

void GradeVetor::definirLinha(int linha_modelo) {
    linha_modelo_ = linha_modelo;
    atualizar();
}

void GradeVetor::atualizar() {
    atualizando_ = true;
    for (int l = 0; l < rowCount(); ++l)
        for (int c = 0; c < columnCount(); ++c) {
            const Celula& ce = celula(l, c);
            QString t = (ce.campo && linha_modelo_ >= 0) ? textoValor(modelo_->valor(linha_modelo_, *ce.campo, ce.indice)) : QString();
            item(l, c)->setText(t);
        }
    setEnabled(linha_modelo_ >= 0);
    atualizando_ = false;
}

void GradeVetor::aoEditar(int l, int c) {
    if (atualizando_ || linha_modelo_ < 0) return;
    const Celula& ce = celula(l, c);
    if (!ce.campo) return;
    std::optional<Valor> v = valorDeTexto(*ce.campo, item(l, c)->text());
    if (v) modelo_->definirValor(linha_modelo_, *ce.campo, ce.indice, *v);
    atualizar();
}

QSize GradeVetor::sizeHint() const {
    return QSize(QTableWidget::sizeHint().width(),
                horizontalHeader()->sizeHint().height() + rowCount() * verticalHeader()->defaultSectionSize() + 2 * frameWidth() + 2);
}

QSize GradeVetor::minimumSizeHint() const { return sizeHint(); }

bool GradeVetor::contemCampo(std::string_view nome) const {
    for (const Celula& ce : celulas_)
        if (ce.campo && ce.campo->nome == nome) return true;
    return false;
}

void GradeVetor::focarCampo(std::string_view nome) {
    for (int l = 0; l < rowCount(); ++l)
        for (int c = 0; c < columnCount(); ++c)
            if (celula(l, c).campo && celula(l, c).campo->nome == nome) {
                setFocus();
                setCurrentCell(l, c);
                return;
            }
}
