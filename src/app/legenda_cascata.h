#pragma once
#include <QColor>
#include <QScrollArea>
#include <QString>
#include <utility>
#include <vector>

class QVBoxLayout;

class LegendaCascata : public QScrollArea {
    Q_OBJECT
public:
    explicit LegendaCascata(QWidget* parent = nullptr);
    void definirGrupos(const std::vector<std::pair<QString, QColor>>& grupos);

private:
    QWidget* conteudo_;
    QVBoxLayout* layout_;
};
