#pragma once
#include <QObject>
#include <QStringList>
#include <map>
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
    const std::vector<OpcaoReferencia>& opcoes(Referencia referencia) const;
    QString rotuloReferencia(Referencia referencia, const QString& codigo) const;
    static QString normalizarCodigo(const QString& codigo);
    void substituirTexto(const QString& nome_padrao, const QString& texto);
    Resultado duplicar(const QString& nome_padrao, int secao, int registro, int nivel, int* primeira_linha_nova = nullptr);
    Resultado remover(const QString& nome_padrao, int secao, int registro, int nivel);
    bool salvar(const QString& nome_padrao, QString* motivo = nullptr);
    QStringList modificados() const;

signals:
    void recarregado();
    void alterado(const QString& nome_padrao);
    void reinterpretado(const QString& nome_padrao);

private:
    struct Entrada {
        ArquivoFixo arquivo;
        ArquivoBinario binario;
        bool eh_binario = false;
        LayoutArquivoFixo base;
        LayoutArquivoFixo layout;
        QString caminho;
        QString erro;
        bool lido = false;
    };
    void carregarArquivo(const QString& nome_padrao, const QString& rotulo, const LayoutArquivoFixo& layout,
                         const std::map<std::string, std::string>& arquivos_dat);
    void carregarBinario(const QString& nome_padrao, int tamanho_registro);
    Entrada* binarioLido(const QString& nome_padrao);
    void aplicarPatamares(bool avisar);
    void concluirReinterpretacao(const QString& nome_padrao);

    std::map<QString, Entrada> arquivos_;
    QString dir_;
    NumeroPatamares patamares_;
    mutable std::map<Referencia, std::vector<OpcaoReferencia>> referencias_;
};
