#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "resultado.h"

enum class TipoColunaFixa { Inteiro, Real, Texto };

struct ColunaFixa {
    std::string nome;
    int inicio = 0;
    int fim = 0;
    TipoColunaFixa tipo = TipoColunaFixa::Texto;
    int decimais = 0;
};

struct SecaoFixa {
    std::string titulo;
    int linhas_cabecalho = 0;
    std::string terminador;
    std::vector<ColunaFixa> colunas;
    int passo_repeticao = 0;
};

struct LayoutArquivoFixo {
    std::string secao_manual;
    std::vector<SecaoFixa> secoes;
};

struct SecaoLida {
    SecaoFixa definicao;
    std::vector<int> linhas;
};

class ArquivoFixo {
public:
    Resultado carregar(const std::filesystem::path& caminho, const LayoutArquivoFixo& layout);
    Resultado interpretar(const std::string& conteudo, const LayoutArquivoFixo& layout);
    Resultado salvar(const std::filesystem::path& caminho);
    std::string conteudo() const;
    std::string textoLf() const;
    void substituirTexto(const std::string& texto_lf, const LayoutArquivoFixo& layout);

    const std::vector<SecaoLida>& secoes() const { return secoes_; }
    std::string valor(int secao, int registro, int coluna) const;
    Resultado definir(int secao, int registro, int coluna, const std::string& texto);
    bool modificado() const { return modificado_; }

private:
    std::vector<std::string> linhas_;
    std::string quebra_ = "\r\n";
    bool quebra_final_ = true;
    std::vector<SecaoLida> secoes_;
    bool modificado_ = false;
};
