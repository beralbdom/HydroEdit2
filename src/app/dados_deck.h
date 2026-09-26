#pragma once
#include <QObject>
#include <QStringList>
#include <map>
#include <string>
#include "arquivo_fixo.h"

class DadosDeck : public QObject {
    Q_OBJECT
public:
    explicit DadosDeck(QObject* parent = nullptr);
    void carregar(const QString& dir_deck, const std::map<std::string, std::string>& arquivos_dat);
    bool carregado() const { return !dir_.isEmpty(); }

    const ArquivoFixo* arquivo(const QString& nome_padrao) const;
    QString nomeNoDeck(const QString& nome_padrao) const;
    QString erro(const QString& nome_padrao) const;
    int registroPorValor(const QString& nome_padrao, int secao, int coluna, const QString& valor) const;

    Resultado definir(const QString& nome_padrao, int secao, int registro, int coluna, const QString& texto);
    bool salvar(const QString& nome_padrao, QString* motivo = nullptr);
    QStringList modificados() const;

signals:
    void recarregado();
    void alterado(const QString& nome_padrao);

private:
    struct Entrada {
        ArquivoFixo arquivo;
        QString caminho;
        QString erro;
        bool lido = false;
    };
    std::map<QString, Entrada> arquivos_;
    QString dir_;
};
