#pragma once
#include <QAbstractTableModel>
#include <functional>
#include <string>
#include <vector>
#include "modif_newave.h"

class DadosDeck;
class ModeloHidr;

class ModeloModif : public QAbstractTableModel {
    Q_OBJECT
public:
    ModeloModif(DadosDeck* dados, const ModeloHidr* hidr, QObject* parent = nullptr);
    void definirFiltro(const QString& chave, std::function<bool(const QString&)> aceita);
    void recarregar();
    void atualizarValores();
    int linhaDoArquivo(int registro) const;
    int usinaDoRegistro(int registro) const;
    QString chaveDoRegistro(int registro) const;
    int registros() const { return static_cast<int>(linhas_.size()); }
    int usinas() const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& ix, int role) const override;
    QVariant headerData(int secao, Qt::Orientation o, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& ix) const override;
    bool setData(const QModelIndex& ix, const QVariant& valor, int role) override;

signals:
    void valorRecusado(const QString& motivo);

private:
    struct Linha {
        int usina = 0;
        QString nome;
        QString chave;
        std::vector<std::string> tokens;
        int indice = 0;
    };

    std::vector<Linha> lerLinhas() const;
    std::vector<CampoModif> campos(const QString& chave) const;
    int campoDaColuna(const Linha& linha, int coluna) const;
    bool tipada() const { return !chave_.isEmpty(); }

    DadosDeck* dados_;
    const ModeloHidr* hidr_;
    QString chave_;
    std::function<bool(const QString&)> aceita_;
    std::vector<Linha> linhas_;
    std::vector<CampoModif> campos_;
};
