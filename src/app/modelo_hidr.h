#pragma once
#include <QAbstractTableModel>
#include <QUndoStack>
#include <optional>
#include <vector>
#include "arquivo_hidr.h"
#include "campos.h"
#include "deck_lookup.h"

QString textoValor(const Valor& v);
std::optional<Valor> valorDeTexto(const Campo& c, const QString& texto);

class ModeloHidr : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit ModeloHidr(QObject* parent = nullptr);

    void definirArquivo(ArquivoHidr arquivo, const QString& caminho);
    const ArquivoHidr& arquivo() const { return arquivo_; }
    QString caminho() const { return caminho_; }
    void definirCaminho(const QString& c) { caminho_ = c; }
    const UsinaHidr& usina(int linha) const { return arquivo_.usinas[static_cast<size_t>(linha)]; }
    int numUsinas() const { return static_cast<int>(arquivo_.usinas.size()); }
    QUndoStack* pilhaUndo() { return &pilha_; }
    const DeckLookup& lookup() const { return lookup_; }
    void definirLookup(DeckLookup l);

    int colunaDoCampo(std::string_view nome) const;
    const Campo* campoDaColuna(int coluna) const;

    Valor valor(int linha, const Campo& c, int i) const { return c.obter(usina(linha), i); }
    void definirValor(int linha, const Campo& c, int i, const Valor& v);
    void substituirUsina(int linha, const UsinaHidr& nova, const QString& rotulo);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

signals:
    void usinaAlterada(int linha, const Campo* campo);

private:
    friend class ComandoDefinirValor;
    friend class ComandoSubstituirUsina;
    void aplicarValor(int linha, const Campo& c, int i, const Valor& v);
    void aplicarUsina(int linha, const UsinaHidr& u);
    QString nomeLookup(const Campo& c, int32_t codigo) const;

    ArquivoHidr arquivo_;
    QString caminho_;
    QUndoStack pilha_;
    DeckLookup lookup_;
    std::vector<const Campo*> colunas_;
};
