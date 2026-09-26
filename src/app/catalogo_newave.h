#pragma once
#include <QString>
#include <vector>

struct ArquivoNewave {
    QString secao;
    QString titulo;
    QString nome_padrao;
    QString rotulo_arquivos;
    QString secao_manual;
};

struct PalavraChaveModif {
    QString chave;
    QString categoria;
    QString descricao;
};

const std::vector<QString>& secoesNewave();
const std::vector<ArquivoNewave>& catalogoNewave();
const std::vector<PalavraChaveModif>& palavrasChaveModif();
QString grupoNewave(const QString& nome_padrao);
