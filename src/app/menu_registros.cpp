#include "menu_registros.h"
#include <QMenu>
#include <QStringList>
#include "dados_deck.h"

namespace {
void aviso(QMenu* menu, const QString& texto) {
    menu->addAction(texto)->setEnabled(false);
}
}  // namespace

// Opcoes de adicionar (copias inseridas logo depois do original) ou remover para um registro de uma
// secao: o proprio registro, quando a secao aceita registros avulsos, e cada bloco de contexto a que
// ele pertence, identificado pelos valores das colunas do contexto ("Submercado A: SUDESTE (1),
// Submercado B: SUL (2)"). Depois da operacao, depois recebe a recusa (vazia se deu certo) e a
// primeira linha inserida no arquivo (-1 se nenhuma).
void preencherMenuRegistros(QMenu* menu, DadosDeck* dados, const QString& nome_padrao, int secao, int registro, bool adicionar,
                            const DepoisDaEdicao& depois) {
    const ArquivoFixo* arquivo = dados->arquivo(nome_padrao);
    if (!arquivo || secao < 0 || secao >= static_cast<int>(arquivo->secoes().size())) {
        aviso(menu, QStringLiteral("Arquivo não carregado"));
        return;
    }
    const SecaoLida& s = arquivo->secoes()[static_cast<size_t>(secao)];
    if (s.linhas.empty()) {
        aviso(menu, QStringLiteral("Seção sem registros: use o editor textual"));
        return;
    }
    if (registro < 0 || registro >= static_cast<int>(s.linhas.size())) {
        aviso(menu, QStringLiteral("Selecione um registro"));
        return;
    }
    auto executar = [dados, nome_padrao, secao, registro, adicionar, depois](int nivel) {
        int nova = -1;
        const Resultado r = adicionar ? dados->duplicar(nome_padrao, secao, registro, nivel, &nova)
                                      : dados->remover(nome_padrao, secao, registro, nivel);
        depois(r.ok ? QString() : QString::fromUtf8(r.mensagem), r.ok ? nova : -1);
    };
    if (arquivo->aceitaRegistrosAvulsos(secao)) {
        QString texto = adicionar ? QStringLiteral("Cópia do registro selecionado") : QStringLiteral("Registro selecionado");
        if (arquivo->abreBloco(secao, registro))
            texto += adicionar ? QStringLiteral(", com os registros que dependem dele") : QStringLiteral(" e os registros que dependem dele");
        menu->addAction(texto, menu, [executar] { executar(-1); });
    }
    const auto& colunas = s.definicao.colunas;
    for (int k = 0; k < static_cast<int>(s.definicao.contextos.size()); ++k) {
        if (s.linhas_contexto[static_cast<size_t>(registro)][static_cast<size_t>(k)] < 0) continue;
        QStringList partes;
        for (int c = 0; c < static_cast<int>(colunas.size()); ++c) {
            const ColunaFixa& coluna = colunas[static_cast<size_t>(c)];
            if (coluna.contexto != k || !ArquivoFixo::editavel(coluna)) continue;
            QString valor = QString::fromLatin1(arquivo->valor(secao, registro, c).c_str()).trimmed();
            if (coluna.referencia != Referencia::Nenhuma) valor = dados->rotuloReferencia(coluna.referencia, valor);
            partes << QStringLiteral("%1: %2").arg(QString::fromStdString(coluna.nome), valor.isEmpty() ? QStringLiteral("vazio") : valor);
        }
        if (partes.isEmpty()) continue;
        const QString texto = (adicionar ? QStringLiteral("Cópia do bloco ") : QStringLiteral("Bloco ")) + partes.join(QStringLiteral(", "));
        menu->addAction(texto, menu, [executar, k] { executar(k); });
    }
    if (menu->isEmpty()) aviso(menu, QStringLiteral("Esta seção não aceita novos registros"));
}

// Primeiro registro da secao na linha dada ou depois dela, para selecionar o que acabou de ser
// inserido (a copia de um bloco comeca pela linha de contexto, que pode nao ser registro); -1 se
// nenhum.
int registroAPartirDaLinha(const DadosDeck& dados, const QString& nome_padrao, int secao, int linha) {
    const ArquivoFixo* arquivo = dados.arquivo(nome_padrao);
    if (!arquivo || linha < 0) return -1;
    const std::vector<int>& linhas = arquivo->secoes()[static_cast<size_t>(secao)].linhas;
    for (size_t r = 0; r < linhas.size(); ++r)
        if (linhas[r] >= linha) return static_cast<int>(r);
    return -1;
}
