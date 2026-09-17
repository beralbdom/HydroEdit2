#pragma once
#include <QTableWidget>
#include <string_view>
#include <vector>
#include "campos.h"

class ModeloHidr;

// Grade fixa cujas celulas apontam para elementos de campos vetoriais de uma usina do modelo.
class GradeVetor : public QTableWidget {
    Q_OBJECT
public:
    GradeVetor(ModeloHidr* modelo, int linhas, int colunas, const QStringList& cab_h, const QStringList& cab_v,
               QWidget* parent = nullptr);
    void definirCelula(int l, int c, const Campo* campo, int indice);
    void definirLinha(int linha_modelo);
    void atualizar();
    bool contemCampo(std::string_view nome) const;
    void focarCampo(std::string_view nome);

private:
    struct Celula { const Campo* campo = nullptr; int indice = 0; };
    void aoEditar(int l, int c);
    Celula& celula(int l, int c) { return celulas_[static_cast<size_t>(l * columnCount() + c)]; }

    ModeloHidr* modelo_;
    int linha_modelo_ = -1;
    std::vector<Celula> celulas_;
    bool atualizando_ = false;
};
