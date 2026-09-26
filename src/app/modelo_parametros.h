#pragma once
#include <QAbstractTableModel>
#include <utility>
#include <vector>

class DadosDeck;
class ArquivoFixo;

class ModeloParametros : public QAbstractTableModel {
    Q_OBJECT
public:
    ModeloParametros(DadosDeck* dados, const QString& nome_padrao, QObject* parent = nullptr);
    int parametros() const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& ix, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& ix, const QVariant& valor, int role = Qt::EditRole) override;
    QVariant headerData(int secao, Qt::Orientation o, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& ix) const override;

signals:
    void valorRecusado(const QString& motivo);

private:
    void refazer();
    const ArquivoFixo* arquivo() const;
    bool presente(int linha) const;

    DadosDeck* dados_;
    QString nome_;
    std::vector<std::pair<int, int>> campos_;
};
