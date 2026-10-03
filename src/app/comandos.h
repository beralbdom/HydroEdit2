#pragma once
#include <QUndoCommand>
#include "campos.h"
#include "usina_hidr.h"

class ModeloHidr;

class ComandoDefinirValor : public QUndoCommand {
public:
    ComandoDefinirValor(ModeloHidr* m, int linha, const Campo* c, int i, Valor antigo, Valor novo, QUndoCommand* pai = nullptr);
    void undo() override;
    void redo() override;
private:
    ModeloHidr* m_;
    int linha_;
    const Campo* campo_;
    int indice_;
    Valor antigo_;
    Valor novo_;
};

class ComandoLote : public QUndoCommand {
public:
    ComandoLote(ModeloHidr* m, const QString& rotulo);
    void undo() override;
    void redo() override;
private:
    ModeloHidr* m_;
    bool aplicado_ = true;
};

class ComandoSubstituirUsina : public QUndoCommand {
public:
    ComandoSubstituirUsina(ModeloHidr* m, int linha, UsinaHidr antiga, UsinaHidr nova, const QString& rotulo);
    void undo() override;
    void redo() override;
private:
    ModeloHidr* m_;
    int linha_;
    UsinaHidr antiga_;
    UsinaHidr nova_;
};
