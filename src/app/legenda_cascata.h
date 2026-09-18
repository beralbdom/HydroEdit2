#pragma once
#include <QColor>
#include <QFrame>
#include <QString>
#include <utility>
#include <vector>

class QVBoxLayout;

class LegendaCascata : public QFrame {
    Q_OBJECT
public:
    explicit LegendaCascata(QWidget* parent = nullptr);
    void definirGrupos(const std::vector<std::pair<QString, QColor>>& grupos);

signals:
    void tamanhoAlterado();

protected:
    void paintEvent(QPaintEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    QVBoxLayout* layout_;
};
