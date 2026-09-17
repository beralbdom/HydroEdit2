#pragma once
#include <QSortFilterProxyModel>

class FiltroUsinas : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit FiltroUsinas(QObject* parent = nullptr);
    void definirOcultarVazias(bool ocultar);
    void definirTexto(const QString& texto);
protected:
    bool filterAcceptsRow(int linha, const QModelIndex& pai) const override;
private:
    bool ocultar_vazias_ = true;
    QString texto_;
};
