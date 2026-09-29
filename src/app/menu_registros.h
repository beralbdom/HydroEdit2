#pragma once
#include <QString>
#include <functional>

class QMenu;
class DadosDeck;

using DepoisDaEdicao = std::function<void(const QString& recusa, int primeira_linha_nova)>;

void preencherMenuRegistros(QMenu* menu, DadosDeck* dados, const QString& nome_padrao, int secao, int registro, bool adicionar,
                            const DepoisDaEdicao& depois);
int registroAPartirDaLinha(const DadosDeck& dados, const QString& nome_padrao, int secao, int linha);
