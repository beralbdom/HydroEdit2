#pragma once
#include <QAbstractTableModel>
#include "arquivo_fixo.h"
#include "recursos_tabela.h"

class DadosDeck;

constexpr int PAPEL_OPCOES = Qt::UserRole + 1;

class ModeloSecaoFixa : public QAbstractTableModel, public EdicaoEmLote {
    Q_OBJECT
public:
    ModeloSecaoFixa(DadosDeck* dados, const QString& nome_padrao, int secao, QObject* parent = nullptr);
    void definirSecao(int secao);
    int secao() const { return secao_; }

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& ix, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& ix, const QVariant& valor, int role = Qt::EditRole) override;
    QVariant headerData(int secao, Qt::Orientation o, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& ix) const override;
    void iniciarLote() override;
    void concluirLote() override;

signals:
    void valorRecusado(const QString& motivo);

private:
    const SecaoLida* secaoLida() const;

    DadosDeck* dados_;
    QString nome_;
    int secao_;
};
