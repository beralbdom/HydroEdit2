#pragma once
#include <QWidget>
#include <map>
#include <string>
#include "arquivo_fixo.h"
#include "catalogo_newave.h"

class QLabel;
class QPushButton;
class QTableView;
class ModeloSecaoFixa;

class PaginaArquivoFixo : public QWidget {
    Q_OBJECT
public:
    PaginaArquivoFixo(const ArquivoNewave& arquivo, const LayoutArquivoFixo& layout, QWidget* parent = nullptr);
    void carregar(const QString& dir_deck, const std::map<std::string, std::string>& arquivos_dat);
    void mostrarSecao(int secao);
    bool modificado() const { return arquivo_.modificado(); }
    QString nomeArquivo() const;
    bool salvar();

signals:
    void modificacaoMudou(bool modificado);

private:
    void atualizarDetalhes(const QString& aviso = {});

    ArquivoNewave info_;
    LayoutArquivoFixo layout_;
    ArquivoFixo arquivo_;
    QString caminho_;
    QString erro_;
    int secao_ = 0;
    QLabel* titulo_;
    QLabel* detalhes_;
    QPushButton* botao_salvar_;
    QTableView* tabela_;
    ModeloSecaoFixa* modelo_;
};
