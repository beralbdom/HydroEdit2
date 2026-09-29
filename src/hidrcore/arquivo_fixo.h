#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "resultado.h"

enum class TipoColunaFixa { Inteiro, Real, Texto, Ordinal, Grupo };
enum class Patamares { Nenhum, Carga, Deficit };
enum class Referencia { Nenhuma, Submercado, Ree, UsinaHidro, UsinaTermica, ClasseTermica, Tecnologia, Posto };

struct ColunaFixa {
    std::string nome;
    int inicio = 0;
    int fim = 0;
    TipoColunaFixa tipo = TipoColunaFixa::Texto;
    int decimais = 0;
    int contexto = -1;
    Patamares patamares = Patamares::Nenhum;
    int patamar = 0;
    Referencia referencia = Referencia::Nenhuma;
};

enum class TesteFiltro { Vazio, Preenchido, Igual, Diferente };

struct FiltroLinha {
    int inicio = 0;
    int fim = 0;
    TesteFiltro teste = TesteFiltro::Preenchido;
    std::vector<std::string> valores;
};

struct ContextoFixo {
    std::vector<FiltroLinha> filtro;
};

struct SecaoFixa {
    std::string titulo;
    int linhas_cabecalho = 0;
    std::string terminador;
    std::vector<ColunaFixa> colunas;
    int passo_repeticao = 0;
    std::vector<FiltroLinha> filtro;
    std::vector<ContextoFixo> contextos;
    bool mesma_regiao = false;
    int max_registros = 0;
    bool contigua = false;
    bool formulario = false;
};

struct LayoutArquivoFixo {
    std::string secao_manual;
    std::vector<SecaoFixa> secoes;
    bool parametros = false;
    char separador = 0;
};

struct SecaoLida {
    SecaoFixa definicao;
    std::vector<int> linhas;
    std::vector<std::vector<int>> linhas_contexto;
    int regiao_inicio = 0;
    int regiao_fim = 0;
};

struct TrechoLinhas {
    int inicio = -1;
    int fim = -1;
    bool vazio() const { return inicio < 0; }
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
    static bool editavel(const ColunaFixa& coluna) {
        return coluna.tipo != TipoColunaFixa::Ordinal && coluna.tipo != TipoColunaFixa::Grupo;
    }
    Resultado definir(int secao, int registro, int coluna, const std::string& texto);

    const std::vector<std::string>& linhas() const { return linhas_; }
    void substituirLinhas(const std::vector<std::string>& linhas, const LayoutArquivoFixo& layout);
    bool aceitaRegistrosAvulsos(int secao) const;
    bool abreBloco(int secao, int registro) const;
    TrechoLinhas trecho(int secao, int registro, int nivel) const;
    Resultado duplicar(int secao, int registro, int nivel, const LayoutArquivoFixo& layout, int* primeira_linha_nova = nullptr);
    Resultado remover(int secao, int registro, int nivel, const LayoutArquivoFixo& layout);
    int registroNaLinha(int secao, int linha) const;
    bool modificado() const { return modificado_; }

private:
    bool passa(const std::vector<FiltroLinha>& filtro, int linha) const;
    bool ignorada(int linha) const;
    std::string campo(const std::string& linha, int inicio, int fim) const;
    void apagarCampo(std::string& linha, const ColunaFixa& coluna) const;
    TrechoLinhas blocoDeContexto(const SecaoLida& secao, int nivel, int inicio) const;
    TrechoLinhas blocoAberto(int secao, int linha) const;
    int nivelProprio(int secao, int registro) const;

    std::vector<std::string> linhas_;
    std::string quebra_ = "\r\n";
    bool quebra_final_ = true;
    std::vector<SecaoLida> secoes_;
    char separador_ = 0;
    bool modificado_ = false;
};
