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

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    QVBoxLayout* layout_;
};
