#include "regras_gevazp.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>

struct NoFormula {
    enum class Tipo { Numero, Vaz, Soma, Subtracao, Produto, Divisao, Negacao, Menor, MenorIgual, Maior, MaiorIgual,
                      Igual, Diferente, Se, Min, Max };
    Tipo tipo = Tipo::Numero;
    double numero = 0.0;
    int posto = 0;
    std::vector<std::shared_ptr<const NoFormula>> filhos;
};

namespace {
using No = std::shared_ptr<const NoFormula>;
using Tipo = NoFormula::Tipo;

No folha(Tipo tipo, double numero, int posto) {
    auto n = std::make_shared<NoFormula>();
    n->tipo = tipo;
    n->numero = numero;
    n->posto = posto;
    return n;
}

No ramo(Tipo tipo, std::vector<No> filhos) {
    auto n = std::make_shared<NoFormula>();
    n->tipo = tipo;
    n->filhos = std::move(filhos);
    return n;
}

// Analisador descendente recursivo da linguagem das formulas do REGRAS.DAT, deduzida do exemplo:
// numeros com ponto decimal, VAZ(n) (maiusculas ou minusculas), + - * /, parenteses, comparacoes
// < <= > >= = <>, e as funcoes SE(condicao;se_verdade;se_falso), MIN(a;b;...) e MAX(a;b;...), com
// ';' separando argumentos como numa planilha em portugues. Precedencia, da menor para a maior:
// comparacao, soma e subtracao, produto e divisao, sinal unario.
class Analisador {
public:
    explicit Analisador(const std::string& texto) : t_(texto) {}

    No analisar() {
        No raiz = comparacao();
        pularEspacos();
        if (p_ != t_.size()) falhar("texto inesperado");
        return raiz;
    }

private:
    const std::string& t_;
    size_t p_ = 0;

    [[noreturn]] void falhar(const std::string& motivo) const {
        throw std::runtime_error(motivo + " na posicao " + std::to_string(p_ + 1));
    }

    void pularEspacos() {
        while (p_ < t_.size() && std::isspace(static_cast<unsigned char>(t_[p_]))) ++p_;
    }

    bool consumir(const char* simbolo) {
        pularEspacos();
        size_t n = std::char_traits<char>::length(simbolo);
        if (t_.compare(p_, n, simbolo) != 0) return false;
        p_ += n;
        return true;
    }

    void exigir(const char* simbolo) {
        if (!consumir(simbolo)) falhar(std::string("esperado '") + simbolo + "'");
    }

    No comparacao() {
        No esq = soma();
        static const std::pair<const char*, Tipo> operadores[] = {{"<=", Tipo::MenorIgual}, {">=", Tipo::MaiorIgual},
                                                                  {"<>", Tipo::Diferente},  {"<", Tipo::Menor},
                                                                  {">", Tipo::Maior},       {"=", Tipo::Igual}};
        for (const auto& [simbolo, tipo] : operadores) {
            if (consumir(simbolo)) return ramo(tipo, {esq, soma()});
        }
        return esq;
    }

    No soma() {
        No esq = produto();
        while (true) {
            if (consumir("+")) esq = ramo(Tipo::Soma, {esq, produto()});
            else if (consumir("-")) esq = ramo(Tipo::Subtracao, {esq, produto()});
            else return esq;
        }
    }

    No produto() {
        No esq = unario();
        while (true) {
            if (consumir("*")) esq = ramo(Tipo::Produto, {esq, unario()});
            else if (consumir("/")) esq = ramo(Tipo::Divisao, {esq, unario()});
            else return esq;
        }
    }

    No unario() {
        if (consumir("-")) return ramo(Tipo::Negacao, {unario()});
        if (consumir("+")) return unario();
        return primario();
    }

    std::string identificador() {
        pularEspacos();
        size_t ini = p_;
        while (p_ < t_.size() && std::isalpha(static_cast<unsigned char>(t_[p_]))) ++p_;
        std::string nome = t_.substr(ini, p_ - ini);
        std::transform(nome.begin(), nome.end(), nome.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return nome;
    }

    std::vector<No> argumentos() {
        exigir("(");
        std::vector<No> args{comparacao()};
        while (consumir(";")) args.push_back(comparacao());
        exigir(")");
        return args;
    }

    No primario() {
        pularEspacos();
        if (p_ >= t_.size()) falhar("formula incompleta");
        char c = t_[p_];
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            size_t ini = p_;
            while (p_ < t_.size() && (std::isdigit(static_cast<unsigned char>(t_[p_])) || t_[p_] == '.')) ++p_;
            try {
                return folha(Tipo::Numero, std::stod(t_.substr(ini, p_ - ini)), 0);
            } catch (...) {
                falhar("numero invalido");
            }
        }
        if (consumir("(")) {
            No dentro = comparacao();
            exigir(")");
            return dentro;
        }
        std::string nome = identificador();
        if (nome == "VAZ") {
            std::vector<No> args = argumentos();
            if (args.size() != 1 || args[0]->tipo != Tipo::Numero) falhar("VAZ espera um numero de posto");
            return folha(Tipo::Vaz, 0.0, static_cast<int>(args[0]->numero));
        }
        if (nome == "SE") {
            std::vector<No> args = argumentos();
            if (args.size() != 3) falhar("SE espera tres argumentos");
            return ramo(Tipo::Se, std::move(args));
        }
        if (nome == "MIN" || nome == "MAX") {
            std::vector<No> args = argumentos();
            if (args.size() < 2) falhar(nome + " espera ao menos dois argumentos");
            return ramo(nome == "MIN" ? Tipo::Min : Tipo::Max, std::move(args));
        }
        falhar(nome.empty() ? std::string("simbolo inesperado") : "funcao desconhecida " + nome);
    }
};

double avaliar(const NoFormula& n, const std::function<double(int)>& vaz) {
    auto f = [&](size_t i) { return avaliar(*n.filhos[i], vaz); };
    switch (n.tipo) {
        case Tipo::Numero: return n.numero;
        case Tipo::Vaz: return vaz(n.posto);
        case Tipo::Soma: return f(0) + f(1);
        case Tipo::Subtracao: return f(0) - f(1);
        case Tipo::Produto: return f(0) * f(1);
        case Tipo::Divisao: return f(0) / f(1);
        case Tipo::Negacao: return -f(0);
        case Tipo::Menor: return f(0) < f(1) ? 1.0 : 0.0;
        case Tipo::MenorIgual: return f(0) <= f(1) ? 1.0 : 0.0;
        case Tipo::Maior: return f(0) > f(1) ? 1.0 : 0.0;
        case Tipo::MaiorIgual: return f(0) >= f(1) ? 1.0 : 0.0;
        case Tipo::Igual: return f(0) == f(1) ? 1.0 : 0.0;
        case Tipo::Diferente: return f(0) != f(1) ? 1.0 : 0.0;
        case Tipo::Se: return f(0) != 0.0 ? f(1) : f(2);
        case Tipo::Min:
        case Tipo::Max: {
            double r = f(0);
            for (size_t i = 1; i < n.filhos.size(); ++i) r = n.tipo == Tipo::Min ? std::min(r, f(i)) : std::max(r, f(i));
            return r;
        }
    }
    return 0.0;
}

std::string aparar(const std::string& s) {
    size_t ini = s.find_first_not_of(" \t\r");
    if (ini == std::string::npos) return {};
    return s.substr(ini, s.find_last_not_of(" \t\r") - ini + 1);
}
}  // namespace

// REGRAS.DAT do GEVAZP, formato deduzido do exemplo do usuario (nao ha manual do GEVAZP em docs/):
// duas linhas de cabecalho, depois uma regra por linha ate a sentinela 9999, com o posto nas colunas
// 1-5, o mes nas colunas 6-11 (0 = todos os meses) e a formula a partir da coluna 22. A coluna CONF
// (12-21) e o bloco "CONFIGURACAO DAS REGRAS UTILIZADAS" depois da sentinela nao sao usados. Um '='
// solto no fim da formula, que aparece em linhas do exemplo, e descartado.
ResultadoLeituraRegras interpretarRegras(const std::string& conteudo) {
    ResultadoLeituraRegras r;
    std::istringstream entrada(conteudo);
    std::string linha;
    int numero_linha = 0;
    while (std::getline(entrada, linha)) {
        ++numero_linha;
        if (numero_linha <= 2) continue;
        if (aparar(linha.substr(0, std::min<size_t>(5, linha.size()))) == "9999") return r;
        if (aparar(linha).empty()) continue;
        RegraPosto regra;
        try {
            regra.posto = std::stoi(linha.substr(0, 5));
            regra.mes = std::stoi(linha.substr(5, 6));
        } catch (...) {
            r.erro = "Linha " + std::to_string(numero_linha) + ": posto ou mes invalido";
            return r;
        }
        if (regra.mes < 0 || regra.mes > 12) {
            r.erro = "Linha " + std::to_string(numero_linha) + ": mes " + std::to_string(regra.mes) + " fora de 0 a 12";
            return r;
        }
        regra.formula = aparar(linha.size() > 21 ? linha.substr(21) : std::string());
        std::string expressao = regra.formula;
        while (!expressao.empty() && (expressao.back() == '=' || expressao.back() == ' ')) expressao.pop_back();
        try {
            regra.arvore = Analisador(expressao).analisar();
        } catch (const std::exception& e) {
            r.erro = "Linha " + std::to_string(numero_linha) + ", posto " + std::to_string(regra.posto) + ": " + e.what() +
                     " em \"" + regra.formula + "\"";
            return r;
        }
        r.regras.push_back(std::move(regra));
    }
    r.erro = "Sentinela 9999 nao encontrada";
    return r;
}

ResultadoLeituraRegras lerRegras(const std::filesystem::path& caminho) {
    std::ifstream f(caminho, std::ios::binary);
    if (!f) {
        ResultadoLeituraRegras r;
        r.erro = "Nao foi possivel abrir " + caminho.string();
        return r;
    }
    std::stringstream conteudo;
    conteudo << f.rdbuf();
    return interpretarRegras(conteudo.str());
}

// Substitui, mes a mes, a vazao de cada posto que tem regra ativa pelo valor da formula. Ativa no mes
// e a regra do proprio mes ou, sem ela, a de mes 0. VAZ(n) de um posto com regra ativa usa o valor ja
// calculado pela regra dele, em qualquer ordem do arquivo (no exemplo, o posto 126 usa o 127, definido
// depois); os demais usam o vazoes.dat. Com essa ordem por dependencia, o posto 303 do exemplo
// reproduz o vazoes.dat do deck de set/2026 em todos os meses. O resultado e arredondado para o inteiro
// mais proximo, como o arquivo guarda. Posto fora do arquivo, regra duplicada ou dependencia circular
// abortam sem alterar a serie.
Resultado aplicarRegras(const std::vector<RegraPosto>& regras, SerieVazoes& serie) {
    std::map<std::pair<int, int>, const RegraPosto*> por_posto_e_mes;
    for (const RegraPosto& regra : regras) {
        if (!serie.postoValido(regra.posto))
            return Resultado::erro("Regra do posto " + std::to_string(regra.posto) + " fora dos postos do vazoes.dat");
        if (!por_posto_e_mes.emplace(std::make_pair(regra.posto, regra.mes), &regra).second)
            return Resultado::erro("Regra repetida para o posto " + std::to_string(regra.posto) + " no mes " +
                                   std::to_string(regra.mes));
    }

    SerieVazoes resultado = serie;
    for (int t = 0; t < serie.meses(); ++t) {
        int mes = t % 12 + 1;
        std::map<int, const RegraPosto*> ativas;
        for (const auto& [chave, regra] : por_posto_e_mes) {
            if (chave.second == 0) ativas.emplace(chave.first, regra);
        }
        for (const auto& [chave, regra] : por_posto_e_mes) {
            if (chave.second == mes) ativas[chave.first] = regra;
        }

        std::map<int, double> calculados;
        std::vector<int> pilha;
        std::string erro;
        std::function<double(int)> vaz = [&](int posto) -> double {
            if (!serie.postoValido(posto)) {
                if (erro.empty()) erro = "VAZ(" + std::to_string(posto) + ") fora dos postos do vazoes.dat";
                return 0.0;
            }
            auto regra = ativas.find(posto);
            if (regra == ativas.end()) return serie.valor(posto, t);
            auto pronto = calculados.find(posto);
            if (pronto != calculados.end()) return pronto->second;
            if (std::find(pilha.begin(), pilha.end(), posto) != pilha.end()) {
                if (erro.empty()) erro = "Dependencia circular envolvendo o posto " + std::to_string(posto);
                return 0.0;
            }
            pilha.push_back(posto);
            double valor = avaliar(*regra->second->arvore, vaz);
            pilha.pop_back();
            calculados[posto] = valor;
            return valor;
        };
        for (const auto& [posto, regra] : ativas) {
            double valor = vaz(posto);
            if (!erro.empty()) return Resultado::erro(erro);
            resultado.valores[static_cast<size_t>(t) * static_cast<size_t>(serie.num_postos) +
                              static_cast<size_t>(posto - 1)] = static_cast<int32_t>(std::lround(valor));
        }
    }
    serie = std::move(resultado);
    return Resultado::sucesso();
}
