#pragma once
#include <QWidget>
#include <map>
#include <string>
#include "catalogo_newave.h"

class QLabel;
class QPlainTextEdit;

class PaginaArquivo : public QWidget {
    Q_OBJECT
public:
    PaginaArquivo(const ArquivoNewave& arquivo, QWidget* parent = nullptr);
    void carregar(const QString& dir_deck, const std::map<std::string, std::string>& arquivos_dat);

private:
    ArquivoNewave arquivo_;
    QLabel* detalhes_;
    QPlainTextEdit* texto_;
};
