#pragma once
#include <QAbstractTableModel>
#include "arquivo_fixo.h"

class ModeloSecaoFixa : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit ModeloSecaoFixa(QObject* parent = nullptr);
    void definirFonte(ArquivoFixo* arquivo, int secao);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& ix, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& ix, const QVariant& valor, int role = Qt::EditRole) override;
    QVariant headerData(int secao, Qt::Orientation o, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& ix) const override;

signals:
    void valorRecusado(const QString& motivo);
    void alterado();

private:
    const SecaoLida* secaoLida() const;

    ArquivoFixo* arquivo_ = nullptr;
    int secao_ = 0;
};
