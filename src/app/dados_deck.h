#pragma once
#include <QObject>
#include <QStringList>
#include <QUndoStack>
#include <map>
#include <set>
#include <vector>
#include <string>
#include "arquivo_binario.h"
#include "arquivo_fixo.h"
#include "layouts_newave.h"

struct OpcaoReferencia {
    QString codigo;
    QString rotulo;
};

class DadosDeck : public QObject {
    Q_OBJECT
public:
    explicit DadosDeck(QObject* parent = nullptr);
    void carregar(const QString& dir_deck, const std::map<std::string, std::string>& arquivos_dat);
    bool carregado() const { return !dir_.isEmpty(); }
    static bool binario(const QString& nome_padrao);

    const ArquivoFixo* arquivo(const QString& nome_padrao) const;
    const ArquivoBinario* arquivoBinario(const QString& nome_padrao) const;
    bool lido(const QString& nome_padrao) const;
    QString nomeNoDeck(const QString& nome_padrao) const;
    QString erro(const QString& nome_padrao) const;
    int registroPorValor(const QString& nome_padrao, int secao, int coluna, const QString& valor) const;

    Resultado definir(const QString& nome_padrao, int secao, int registro, int coluna, const QString& texto);
    Resultado definirTextoBinario(const QString& nome_padrao, int registro, int inicio, int tamanho, const QString& texto);
    Resultado definirInteiroBinario(const QString& nome_padrao, int registro, int inicio, const QString& texto);
    QString texto(const QString& nome_padrao) const;
    QString linha(const QString& nome_padrao, int indice) const;
    const std::vector<OpcaoReferencia>& opcoes(Referencia referencia) const;
    QString rotuloReferencia(Referencia referencia, const QString& codigo) const;
    static bool temOpcoes(const ColunaFixa& coluna);
    std::vector<OpcaoReferencia> opcoes(const ColunaFixa& coluna) const;
    QString rotulo(const ColunaFixa& coluna, const QString& codigo) const;
    static QString normalizarCodigo(const QString& codigo);
    void substituirTexto(const QString& nome_padrao, const QString& texto);
    Resultado substituirLinha(const QString& nome_padrao, int indice, const QString& texto);
    int numeroPatamaresDeCarga() const { return patamares_.carga; }
    Resultado duplicar(const QString& nome_padrao, int secao, int registro, int nivel, int* primeira_linha_nova = nullptr,
                       bool em_branco = false);
    Resultado remover(const QString& nome_padrao, int secao, int registro, int nivel);
    Resultado mudarPatamares(Patamares tipo, int delta, QStringList* avisos = nullptr);
    bool salvar(const QString& nome_padrao, QString* motivo = nullptr);
    bool salvarEm(const QString& nome_padrao, const QString& pasta, QString* motivo = nullptr);
    QStringList modificados() const;
    void iniciarLote();
    void concluirLote();
    QUndoStack* pilhaUndo() { return &pilha_; }

signals:
    void recarregado();
    void alterado(const QString& nome_padrao);
    void reinterpretado(const QString& nome_padrao);

private:
    friend class ComandoDeck;
    struct Diferenca {
        QString nome;
        size_t prefixo = 0;
        size_t sufixo = 0;
        std::string antes;
        std::string depois;
    };
    struct Entrada {
        ArquivoFixo arquivo;
        ArquivoBinario binario;
        bool eh_binario = false;
        LayoutArquivoFixo base;
        LayoutArquivoFixo layout;
        QString caminho;
        QString erro;
        bool lido = false;
        std::string salvo;
    };
    void carregarArquivo(const QString& nome_padrao, const QString& rotulo, const LayoutArquivoFixo& layout,
                         const std::map<std::string, std::string>& arquivos_dat);
    void carregarBinario(const QString& nome_padrao, int tamanho_registro);
    Entrada* binarioLido(const QString& nome_padrao);
    void aplicarPatamares(bool avisar);
    void concluirReinterpretacao(const QString& nome_padrao);
    void avisarAlterado(const QString& nome_padrao);
    static std::string conteudo(const Entrada& entrada);
    void abrirTransacao(const QString& rotulo);
    void registrarAntes(const QString& nome_padrao);
    void fecharTransacao();
    void restaurar(const std::vector<Diferenca>& diferencas, bool desfazer);
    void aplicarPendentes(const QString& nome_padrao);
    void reler(const QString& nome_padrao);

    std::map<QString, Entrada> arquivos_;
    QString dir_;
    NumeroPatamares patamares_;
    mutable std::map<Referencia, std::vector<OpcaoReferencia>> referencias_;
    int lote_ = 0;
    std::set<QString> alterados_no_lote_;
    std::map<QString, std::vector<std::string>> linhas_pendentes_;
    std::set<QString> a_reler_;
    QUndoStack pilha_;
    int transacao_ = 0;
    QString rotulo_transacao_;
    std::map<QString, std::string> antes_;
};
