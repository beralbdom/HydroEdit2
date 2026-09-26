#pragma once
#include <QStyleOptionComboBox>
#include <QToolButton>

class BotaoCombo : public QToolButton {
    Q_OBJECT
public:
    explicit BotaoCombo(QWidget* parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    QStyleOptionComboBox opcaoCombo() const;
};
