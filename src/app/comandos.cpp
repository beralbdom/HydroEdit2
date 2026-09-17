#include "comandos.h"
#include "modelo_hidr.h"

ComandoDefinirValor::ComandoDefinirValor(ModeloHidr* m, int linha, const Campo* c, int i, Valor antigo, Valor novo)
    : m_(m), linha_(linha), campo_(c), indice_(i), antigo_(std::move(antigo)), novo_(std::move(novo)) {
    QString nome = QString::fromLatin1(c->nome.data(), static_cast<int>(c->nome.size()));
    setText(QStringLiteral("Editar %1 da usina %2").arg(nome).arg(linha + 1));
}
void ComandoDefinirValor::undo() { m_->aplicarValor(linha_, *campo_, indice_, antigo_); }
void ComandoDefinirValor::redo() { m_->aplicarValor(linha_, *campo_, indice_, novo_); }

ComandoSubstituirUsina::ComandoSubstituirUsina(ModeloHidr* m, int linha, UsinaHidr antiga, UsinaHidr nova, const QString& rotulo)
    : m_(m), linha_(linha), antiga_(std::move(antiga)), nova_(std::move(nova)) {
    setText(rotulo);
}
void ComandoSubstituirUsina::undo() { m_->aplicarUsina(linha_, antiga_); }
void ComandoSubstituirUsina::redo() { m_->aplicarUsina(linha_, nova_); }
