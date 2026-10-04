#pragma once
#include <QLayout>
#include <QList>
#include <vector>

class LayoutColunas : public QLayout {
public:
    explicit LayoutColunas(QWidget* pai = nullptr);
    ~LayoutColunas() override;

    void addItem(QLayoutItem* item) override;
    int count() const override;
    QLayoutItem* itemAt(int indice) const override;
    QLayoutItem* takeAt(int indice) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int largura) const override;
    void setGeometry(const QRect& retangulo) override;
    void invalidate() override;
    Qt::Orientations expandingDirections() const override { return Qt::Horizontal; }

    std::vector<int> colunasPara(int largura) const;

private:
    struct Arranjo {
        std::vector<int> fim_das_colunas;
        std::vector<int> larguras;
        int altura = 0;
    };
    Arranjo arranjar(int largura) const;

    QList<QLayoutItem*> itens_;
    mutable int largura_em_cache_ = -1;
    mutable Arranjo arranjo_em_cache_;
};
