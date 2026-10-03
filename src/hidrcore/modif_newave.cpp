#include "modif_newave.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace {
std::string aparar(const std::string& s) {
    const size_t ini = s.find_first_not_of(" \t\r");
    if (ini == std::string::npos) return {};
    return s.substr(ini, s.find_last_not_of(" \t\r") - ini + 1);
}

std::string maiusculas(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::string chaveDaLinha(const std::string& linha) {
    return linha.size() < 2 ? std::string() : maiusculas(aparar(linha.substr(1, 8)));
}

int usinaDaLinha(const std::string& linha) {
    if (linha.size() < 11) return 0;
    std::istringstream valores(linha.substr(10, 20));
    int codigo = 0;
    valores >> codigo;
    return codigo;
}

bool vazia(const std::string& linha) { return aparar(linha).empty(); }

bool inteiro(const std::string& texto, long long& valor) {
    if (texto.empty()) return false;
    char* fim = nullptr;
    valor = std::strtoll(texto.c_str(), &fim, 10);
    return *fim == '\0';
}

bool real(std::string texto) {
    std::replace(texto.begin(), texto.end(), ',', '.');
    if (texto.empty()) return false;
    char* fim = nullptr;
    std::strtod(texto.c_str(), &fim);
    return *fim == '\0';
}
}  // namespace

// Campos dos valores de cada palavra-chave do modif.dat, na ordem em que o registro os traz (manual
// do NEWAVE, secao 3.12, tabela de palavras-chave e notas que a seguem): valor e unidade (H, h ou %)
// em VOLMIN e VOLMAX; um valor em NUMCNJ, NUMBAS, PRODESP, TEIF, IP, PERDHIDR e VAZMIN; valor e numero
// do conjunto em NUMMAQ e POTEFE (o deck traz "NUMMAQ 0 2", que so se le como 0 maquinas no conjunto
// 2); valor e mes em COEFEVAP; os cinco coeficientes, do grau 0 ao 4, em COTAREA e VOLCOTA; mes, ano e
// valor em CFUGA, CMONT e VAZMINT; mes, ano, valor e unidade em VMAXT, VMINT e VMINP; mes, ano e um
// valor ou um por patamar de carga em TURBMAXT, TURBMINT e VAZMAXT (um so vale para todos); usina a
// jusante e vazao maxima em CDESVIO. COEFEVAP e CDESVIO seguem so o texto do manual, sem exemplo no
// deck. Palavra-chave desconhecida nao tem campos.
std::vector<CampoModif> camposModif(const std::string& chave_original, int patamares) {
    using T = TipoCampoModif;
    const std::string chave = maiusculas(aparar(chave_original));
    const CampoModif mes{"Mês", T::Mes}, ano{"Ano", T::Ano}, unidade{"Unidade", T::Unidade};
    if (chave == "VOLMIN" || chave == "VOLMAX") return {{"Volume", T::Real}, unidade};
    if (chave == "NUMCNJ") return {{"Conjuntos", T::Inteiro}};
    if (chave == "NUMBAS") return {{"Unidades de base", T::Inteiro}};
    if (chave == "NUMMAQ") return {{"Máquinas", T::Inteiro}, {"Conjunto", T::Inteiro}};
    if (chave == "POTEFE") return {{"Potência efetiva (MW)", T::Real}, {"Conjunto", T::Inteiro}};
    if (chave == "PRODESP") return {{"Produtibilidade (MW/m³/s/m)", T::Real}};
    if (chave == "TEIF") return {{"TEIF (%)", T::Real}};
    if (chave == "IP") return {{"IP (%)", T::Real}};
    if (chave == "PERDHIDR") return {{"Perda hidráulica", T::Real}};
    if (chave == "VAZMIN") return {{"Vazão mínima (m³/s)", T::Real}};
    if (chave == "COEFEVAP") return {{"Evaporação (mm/mês)", T::Inteiro}, mes};
    if (chave == "COTAREA" || chave == "VOLCOTA")
        return {{"A0", T::Real}, {"A1", T::Real}, {"A2", T::Real}, {"A3", T::Real}, {"A4", T::Real}};
    if (chave == "CFUGA") return {mes, ano, {"Canal de fuga (m)", T::Real}};
    if (chave == "CMONT") return {mes, ano, {"Nível de montante (m)", T::Real}};
    if (chave == "VAZMINT") return {mes, ano, {"Vazão mínima (m³/s)", T::Real}};
    if (chave == "VMAXT" || chave == "VMINT" || chave == "VMINP") return {mes, ano, {"Volume", T::Real}, unidade};
    if (chave == "TURBMAXT" || chave == "TURBMINT" || chave == "VAZMAXT") {
        std::vector<CampoModif> campos{mes, ano};
        const int n = std::max(1, patamares);
        for (int k = 1; k <= n; ++k) campos.push_back({"Patamar " + std::to_string(k) + " (m³/s)", T::Real, k > 1});
        return campos;
    }
    if (chave == "CDESVIO") return {{"Usina a jusante", T::Inteiro}, {"Vazão máxima (m³/s)", T::Real}};
    return {};
}

bool chaveComData(const std::string& chave) {
    const std::vector<CampoModif> campos = camposModif(chave, 1);
    return campos.size() >= 2 && campos[0].tipo == TipoCampoModif::Mes && campos[1].tipo == TipoCampoModif::Ano;
}

// Valor como a tabela mostra: a unidade sem as aspas que o arquivo usa.
std::string valorExibidoModif(const CampoModif& campo, const std::string& token) {
    if (campo.tipo == TipoCampoModif::Unidade && token.size() >= 2 && token.front() == '\'' && token.back() == '\'')
        return token.substr(1, token.size() - 2);
    return token;
}

// Confere o texto digitado contra o tipo do campo e o devolve como vai para o arquivo: inteiro sem
// zeros a esquerda, real como digitado (virgula vira ponto), mes de 1 a 12 com duas posicoes (" 9",
// como o deck escreve), ano com quatro digitos e unidade h, H ou %, entre aspas se aspas (o deck
// usa '%' e 'h'). Vazio so passa em campo opcional e devolve vazio.
Resultado formatarCampoModif(const CampoModif& campo, const std::string& texto_original, bool aspas, std::string& token) {
    std::string texto = aparar(texto_original);
    if (campo.tipo == TipoCampoModif::Unidade && texto.size() >= 2 && texto.front() == '\'' && texto.back() == '\'')
        texto = texto.substr(1, texto.size() - 2);
    if (texto.empty()) {
        if (!campo.opcional) return Resultado::erro(campo.nome + " é obrigatório");
        token.clear();
        return Resultado::sucesso();
    }
    long long n = 0;
    switch (campo.tipo) {
    case TipoCampoModif::Inteiro:
        if (!inteiro(texto, n)) return Resultado::erro(campo.nome + ": " + texto + " não é um número inteiro");
        token = std::to_string(n);
        break;
    case TipoCampoModif::Real:
        if (!real(texto)) return Resultado::erro(campo.nome + ": " + texto + " não é um número");
        std::replace(texto.begin(), texto.end(), ',', '.');
        token = texto;
        break;
    case TipoCampoModif::Mes:
        if (!inteiro(texto, n) || n < 1 || n > 12) return Resultado::erro("Mês: " + texto + " não está entre 1 e 12");
        token = (n < 10 ? " " : "") + std::to_string(n);
        break;
    case TipoCampoModif::Ano:
        if (!inteiro(texto, n) || n < 1000 || n > 9999) return Resultado::erro("Ano: " + texto + " não tem quatro dígitos");
        token = std::to_string(n);
        break;
    case TipoCampoModif::Unidade:
        if (texto != "h" && texto != "H" && texto != "%") return Resultado::erro("Unidade: use h (hm3) ou % (%vu)");
        token = aspas ? "'" + texto + "'" : texto;
        break;
    }
    return Resultado::sucesso();
}

// Linha do registro com os valores novos, mexendo o minimo na original: as colunas 1 a 10 (a
// palavra-chave como o usuario escreveu) ficam, e cada valor que mudou toma o lugar do antigo,
// alinhado a direita no mesmo espaco; se nao couber, avanca sobre os espacos a esquerda (deixando um
// depois do valor anterior) e, em ultimo caso, empurra o resto da linha. Valores que a original nao
// tinha entram no fim, separados por um espaco; opcionais vazios do fim saem da linha. O manual
// (secao 3.12) le os valores em formato livre nas colunas 11 a 70, entao as posicoes so importam para
// a linha mudar o menos possivel. Opcional vazio antes de um preenchido (patamar 2 vazio com o 3
// preenchido) e valores alem da coluna 70 sao recusados.
Resultado montarLinhaModif(const std::string& original, std::vector<std::string> tokens, const std::vector<CampoModif>& campos,
                           std::string& linha) {
    while (!tokens.empty() && aparar(tokens.back()).empty()) tokens.pop_back();
    for (size_t i = 0; i < tokens.size(); ++i)
        if (aparar(tokens[i]).empty())
            return Resultado::erro((i < campos.size() ? campos[i].nome : "Valor " + std::to_string(i + 1)) +
                                   " está vazio antes de um valor preenchido");

    std::string valores = original.size() > 10 ? original.substr(10) : std::string();
    std::vector<std::pair<size_t, size_t>> trechos;
    for (size_t p = 0; p < valores.size();) {
        if (std::isspace(static_cast<unsigned char>(valores[p]))) {
            ++p;
            continue;
        }
        size_t q = p;
        while (q < valores.size() && !std::isspace(static_cast<unsigned char>(valores[q]))) ++q;
        trechos.push_back({p, q});
        p = q;
    }
    if (tokens.size() < trechos.size() || tokens.size() > trechos.size()) {
        const size_t mantidos = std::min(tokens.size(), trechos.size());
        valores.erase(mantidos == 0 ? 0 : trechos[mantidos - 1].second);
        trechos.resize(mantidos);
    }
    for (size_t i = trechos.size(); i < tokens.size(); ++i) valores += (valores.empty() ? "" : " ") + tokens[i];
    for (size_t i = trechos.size(); i-- > 0;) {
        const std::string novo = aparar(tokens[i]);
        auto [inicio, fim] = trechos[i];
        if (valores.compare(inicio, fim - inicio, novo) == 0) continue;
        size_t largura = fim - inicio;
        if (novo.size() > largura) {
            const size_t limite = i == 0 ? 0 : trechos[i - 1].second + 1;
            const size_t avanco = std::min(novo.size() - largura, inicio > limite ? inicio - limite : size_t(0));
            inicio -= avanco;
            largura += avanco;
        }
        valores.replace(inicio, largura, std::string(largura > novo.size() ? largura - novo.size() : 0, ' ') + novo);
    }
    const size_t ultimo = valores.find_last_not_of(' ');
    if (ultimo != std::string::npos && ultimo + 1 > 60) return Resultado::erro("Os valores passam da coluna 70 do registro");
    linha = original.substr(0, std::min<size_t>(10, original.size()));
    linha.resize(10, ' ');
    linha += valores;
    return Resultado::sucesso();
}

// Valores de um registro novo, para ele ja nascer valido: mes 1 do ano dado, numeros 0, conjunto 1 e
// unidade '%'.
std::vector<std::string> valoresPadraoModif(const std::string& chave, int ano) {
    std::vector<std::string> valores;
    for (const CampoModif& c : camposModif(chave, 1)) {
        switch (c.tipo) {
        case TipoCampoModif::Mes: valores.push_back(" 1"); break;
        case TipoCampoModif::Ano: valores.push_back(std::to_string(ano)); break;
        case TipoCampoModif::Unidade: valores.push_back("'%'"); break;
        default: valores.push_back(c.nome == "Conjunto" ? "1" : "0"); break;
        }
    }
    return valores;
}

// Insere o registro no bloco da usina, depois da ultima linha nao vazia dele, ou, se a usina nao tem
// bloco, cria um no fim do arquivo com a linha USINA (codigo na coluna 11 e o nome depois da coluna
// 30, como comentario). As duas primeiras linhas sao comentario. linha_nova recebe o indice do
// registro inserido.
std::vector<std::string> inserirModificacao(const std::vector<std::string>& linhas, int usina, const std::string& nome_usina,
                                            const std::string& registro, int& linha_nova) {
    std::vector<std::string> novas = linhas;
    int inicio = -1;
    int fim = static_cast<int>(novas.size());
    for (int i = 2; i < static_cast<int>(novas.size()); ++i) {
        if (chaveDaLinha(novas[static_cast<size_t>(i)]) != "USINA") continue;
        if (inicio >= 0) {
            fim = i;
            break;
        }
        if (usinaDaLinha(novas[static_cast<size_t>(i)]) == usina) inicio = i;
    }
    if (inicio >= 0) {
        int posicao = fim;
        while (posicao - 1 > inicio && vazia(novas[static_cast<size_t>(posicao - 1)])) --posicao;
        novas.insert(novas.begin() + posicao, registro);
        linha_nova = posicao;
        return novas;
    }
    while (novas.size() < 2) novas.push_back({});
    std::string cabecalho = " USINA    " + std::to_string(usina);
    if (!nome_usina.empty()) {
        cabecalho.resize(std::max<size_t>(cabecalho.size() + 1, 44), ' ');
        cabecalho += nome_usina;
    }
    novas.push_back(cabecalho);
    novas.push_back(registro);
    linha_nova = static_cast<int>(novas.size()) - 1;
    return novas;
}

// Apaga os registros dos indices dados; o bloco que fica sem registro perde tambem a linha USINA.
std::vector<std::string> removerModificacoes(const std::vector<std::string>& linhas, std::vector<int> indices) {
    std::vector<std::string> novas = linhas;
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    for (int indice : indices) {
        if (indice < 2 || indice >= static_cast<int>(novas.size()) || chaveDaLinha(novas[static_cast<size_t>(indice)]) == "USINA")
            continue;
        int bloco = indice - 1;
        while (bloco >= 2 && chaveDaLinha(novas[static_cast<size_t>(bloco)]) != "USINA") --bloco;
        novas.erase(novas.begin() + indice);
        if (bloco < 2) continue;
        bool vazio = true;
        for (size_t i = static_cast<size_t>(bloco) + 1; i < novas.size() && chaveDaLinha(novas[i]) != "USINA"; ++i)
            if (!vazia(novas[i])) vazio = false;
        if (vazio) novas.erase(novas.begin() + bloco);
    }
    return novas;
}
