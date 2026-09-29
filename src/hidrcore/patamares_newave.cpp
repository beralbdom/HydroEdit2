#include "patamares_newave.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include "arquivo_fixo.h"
#include "layouts_newave.h"

namespace {
std::string aparar(const std::string& s) {
    const size_t ini = s.find_first_not_of(" \t\r");
    if (ini == std::string::npos) return {};
    return s.substr(ini, s.find_last_not_of(" \t\r") - ini + 1);
}

bool numero(const std::string& texto) {
    if (texto.empty()) return false;
    try {
        size_t lidos = 0;
        static_cast<void>(std::stod(texto, &lidos));
        return lidos == texto.size();
    } catch (...) {
        return false;
    }
}

std::optional<int> inteiro(const std::string& texto) {
    try {
        size_t lidos = 0;
        const int n = std::stoi(texto, &lidos);
        if (lidos == texto.size()) return n;
    } catch (...) {
    }
    return std::nullopt;
}

// Numero real com as casas do formato Fw.d; sem casas, com o ponto no fim, como o Fortran escreve.
std::string formatar(double valor, int decimais) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", decimais, valor);
    std::string texto = buffer;
    if (decimais == 0) texto += '.';
    return texto;
}

// Posicao e tamanho do campo na linha: colunas inicio a fim ou, com separador, o campo de numero
// inicio; posicao npos se a linha de campos separados tem campos de menos.
std::pair<size_t, size_t> limites(const std::string& linha, const ColunaFixa& c, char separador) {
    if (!separador) return {static_cast<size_t>(c.inicio - 1), static_cast<size_t>(c.fim - c.inicio + 1)};
    size_t inicio = 0;
    for (int k = 1; k < c.inicio; ++k) {
        const size_t proximo = linha.find(separador, inicio);
        if (proximo == std::string::npos) return {std::string::npos, 0};
        inicio = proximo + 1;
    }
    const size_t fim = linha.find(separador, inicio);
    return {inicio, (fim == std::string::npos ? linha.size() : fim) - inicio};
}

std::string ler(const std::string& linha, const ColunaFixa& c, char separador) {
    const auto [posicao, tamanho] = limites(linha, c, separador);
    if (posicao == std::string::npos || posicao >= linha.size()) return {};
    return aparar(linha.substr(posicao, tamanho));
}

// Grava o texto alinhado a direita nas colunas do campo (vazio apaga), completando a linha com
// espacos se ela for mais curta.
void escrever(std::string& linha, const ColunaFixa& c, char separador, const std::string& texto) {
    const auto [posicao, tamanho] = limites(linha, c, separador);
    if (posicao == std::string::npos) return;
    if (linha.size() < posicao + tamanho) linha.append(posicao + tamanho - linha.size(), ' ');
    const std::string campo = texto.size() >= tamanho ? texto : std::string(tamanho - texto.size(), ' ') + texto;
    linha.replace(posicao, tamanho, campo);
}

// Arquivo relido com o layout completo (com as colunas de todos os patamares) e as alteracoes feitas
// numa copia das linhas: campos trocados no lugar e linhas inseridas ou removidas so ao montar o
// texto, para as linhas das secoes lidas continuarem valendo durante a operacao.
struct Edicao {
    std::string nome;
    ArquivoFixo arquivo;
    std::vector<std::string> linhas;
    std::multimap<int, std::string> insercoes;
    std::set<int> remocoes;
    char separador = 0;
    bool quebra_final = true;
    bool mudou = false;

    bool abrir(const std::map<std::string, std::string>& textos, const std::string& nome_padrao) {
        const auto it = textos.find(nome_padrao);
        const LayoutArquivoFixo* layout = layoutNewave(nome_padrao);
        if (it == textos.end() || !layout) return false;
        nome = nome_padrao;
        arquivo.interpretar(it->second, *layout);
        linhas = arquivo.linhas();
        separador = layout->separador;
        quebra_final = !it->second.empty() && it->second.back() == '\n';
        return true;
    }

    std::string texto() const {
        std::vector<std::string> saida;
        for (int i = 0; i < static_cast<int>(linhas.size()); ++i) {
            if (!remocoes.count(i)) saida.push_back(linhas[static_cast<size_t>(i)]);
            const auto [ini, fim] = insercoes.equal_range(i);
            for (auto it = ini; it != fim; ++it) saida.push_back(it->second);
        }
        std::string t;
        for (size_t i = 0; i < saida.size(); ++i) {
            t += saida[i];
            if (i + 1 < saida.size() || quebra_final) t += '\n';
        }
        return t;
    }
};

int indiceColuna(const SecaoFixa& d, const std::function<bool(const ColunaFixa&)>& aceita) {
    for (size_t c = 0; c < d.colunas.size(); ++c)
        if (aceita(d.colunas[c])) return static_cast<int>(c);
    return -1;
}

// Numero de patamares no registro do bloco 1 (secao 0, coluna 0).
void gravarNumero(Edicao& e, int novo) {
    const SecaoLida& s = e.arquivo.secoes()[0];
    escrever(e.linhas[static_cast<size_t>(s.linhas[0])], s.definicao.colunas[0], e.separador, std::to_string(novo));
    e.mudou = true;
}

// Secoes com uma linha por patamar (coluna Ordinal "Patamar"): em cada grupo de registros com a mesma
// linha de contexto da coluna, adicionar insere depois do ultimo uma copia dele, sem as colunas de
// contexto se ele mesmo for a linha de contexto (com um patamar so, a linha do ano), e com os valores
// zerados se zerar(secao); remover apaga o ultimo. O novo patamar comeca, assim, igual ao anterior.
// Grupo com numero de linhas diferente do numero de patamares fica como esta e entra na contagem
// devolvida.
int linhasPorPatamar(Edicao& e, int atual, int novo, const std::function<bool(const SecaoFixa&)>& zerar) {
    std::set<int> vistas;
    int fora = 0;
    for (const SecaoLida& s : e.arquivo.secoes()) {
        const SecaoFixa& d = s.definicao;
        const int ordinal = indiceColuna(d, [](const ColunaFixa& c) { return c.tipo == TipoColunaFixa::Ordinal && c.nome == "Patamar"; });
        if (ordinal < 0) continue;
        const size_t nivel = static_cast<size_t>(d.colunas[static_cast<size_t>(ordinal)].contexto);
        std::map<int, std::vector<size_t>> grupos;
        for (size_t r = 0; r < s.linhas.size(); ++r) grupos[s.linhas_contexto[r][nivel]].push_back(r);
        for (const auto& [contexto, registros] : grupos) {
            const size_t ultimo = registros.back();
            const int linha = s.linhas[ultimo];
            if (!vistas.insert(linha).second) continue;
            if (static_cast<int>(registros.size()) != atual) {
                ++fora;
                continue;
            }
            if (novo > atual) {
                std::string copia = e.linhas[static_cast<size_t>(linha)];
                for (const ColunaFixa& c : d.colunas) {
                    if (c.contexto >= 0 && ArquivoFixo::editavel(c) && s.linhas_contexto[ultimo][static_cast<size_t>(c.contexto)] == linha)
                        escrever(copia, c, e.separador, "");
                    else if (zerar(d) && c.contexto < 0 && c.tipo == TipoColunaFixa::Real && !ler(copia, c, e.separador).empty())
                        escrever(copia, c, e.separador, formatar(0, c.decimais));
                }
                e.insercoes.emplace(linha, copia);
            } else {
                e.remocoes.insert(linha);
            }
            e.mudou = true;
        }
    }
    return fora;
}

// Secoes com um campo por patamar (colunas deCarga ou deDeficit): cada coluna do patamar k recebe, em
// cada registro, o texto que valor devolve (nada se nullopt; vazio apaga, e so apaga numero, porque
// ali pode haver comentario). Texto que nao e numero no lugar de um campo novo (comentario livre,
// como no agrint.dat) vai para duas colunas depois do ultimo campo de patamar da secao. Devolve o
// numero de comentarios movidos.
using ValorCampo = std::function<std::optional<std::string>(const SecaoLida&, size_t registro, const ColunaFixa&, const std::string& linha)>;
int campoPorPatamar(Edicao& e, Patamares tipo, int k, const ValorCampo& valor) {
    int movidos = 0;
    for (const SecaoLida& s : e.arquivo.secoes()) {
        const SecaoFixa& d = s.definicao;
        int ultima = 0;
        for (const ColunaFixa& c : d.colunas)
            if (c.patamares == tipo) ultima = std::max(ultima, c.fim);
        for (size_t r = 0; r < s.linhas.size(); ++r) {
            std::string& linha = e.linhas[static_cast<size_t>(s.linhas[r])];
            for (const ColunaFixa& c : d.colunas) {
                if (c.patamares != tipo || c.patamar != k) continue;
                const std::optional<std::string> texto = valor(s, r, c, linha);
                if (!texto) continue;
                const std::string atual = ler(linha, c, e.separador);
                if (texto->empty()) {
                    if (!numero(atual)) continue;
                    escrever(linha, c, e.separador, "");
                    e.mudou = true;
                    continue;
                }
                if (!e.separador && !atual.empty() && !numero(atual)) {
                    size_t inicio = linha.find_first_not_of(' ', static_cast<size_t>(c.inicio - 1));
                    while (inicio > 0 && linha[inicio - 1] != ' ') --inicio;
                    const std::string comentario = aparar(linha.substr(inicio));
                    linha.erase(inicio);
                    escrever(linha, c, e.separador, *texto);
                    if (linha.size() < static_cast<size_t>(ultima + 1)) linha.append(static_cast<size_t>(ultima + 1) - linha.size(), ' ');
                    linha += comentario;
                    ++movidos;
                } else {
                    escrever(linha, c, e.separador, *texto);
                }
                e.mudou = true;
            }
        }
    }
    return movidos;
}

// Registros com o numero do patamar (coluna Inteiro "Patamar", em que 0 vale para todos): adicionar
// copia os do ultimo patamar para o novo, logo depois de cada um; remover apaga os do patamar
// removido. aceita restringe os registros (no penalid.dat, so os de GHMIN). Devolve quantos.
int registrosDoPatamar(Edicao& e, int atual, int novo, const std::function<bool(const SecaoLida&, size_t)>& aceita) {
    std::set<int> vistas;
    int quantos = 0;
    for (const SecaoLida& s : e.arquivo.secoes()) {
        const int coluna = indiceColuna(s.definicao, [](const ColunaFixa& c) { return c.tipo == TipoColunaFixa::Inteiro && c.nome == "Patamar"; });
        if (coluna < 0) continue;
        const ColunaFixa& c = s.definicao.colunas[static_cast<size_t>(coluna)];
        for (size_t r = 0; r < s.linhas.size(); ++r) {
            const int linha = s.linhas[r];
            if (inteiro(ler(e.linhas[static_cast<size_t>(linha)], c, e.separador)) != atual || !aceita(s, r) || !vistas.insert(linha).second) continue;
            if (novo > atual) {
                std::string copia = e.linhas[static_cast<size_t>(linha)];
                escrever(copia, c, e.separador, std::to_string(novo));
                e.insercoes.emplace(linha, copia);
            } else {
                e.remocoes.insert(linha);
            }
            ++quantos;
            e.mudou = true;
        }
    }
    return quantos;
}

// Linhas TURBMAXT, TURBMINT e VAZMAXT do modif.dat com um valor por patamar (mes, ano e os valores
// nas colunas 11 a 70; manual 3.12). O modif.dat nao tem layout de colunas e fica para o usuario.
int modificacoesPorPatamar(const std::map<std::string, std::string>& textos, int atual) {
    const auto it = textos.find("modif.dat");
    if (it == textos.end() || atual < 2) return 0;
    std::istringstream entrada(it->second);
    int quantas = 0;
    for (std::string linha; std::getline(entrada, linha);) {
        if (linha.size() < 11) continue;
        const std::string chave = aparar(linha.substr(1, 8));
        if (chave != "TURBMAXT" && chave != "TURBMINT" && chave != "VAZMAXT") continue;
        std::istringstream valores(linha.substr(10, 60));
        int n = 0;
        for (std::string v; valores >> v;) ++n;
        if (n - 2 == atual) ++quantas;
    }
    return quantas;
}

std::string ordinalPatamar(int k) { return "patamar " + std::to_string(k); }
}  // namespace

const std::vector<std::string>& arquivosComPatamares() {
    static const std::vector<std::string> nomes = {"patamar.dat", "sistema.dat", "loss.dat", "gtminpat.dat", "agrint.dat",
                                                   "adterm.dat",  "ghmin.dat",   "penalid.dat", "re.dat", "restricao-eletrica.csv",
                                                   "modif.dat"};
    return nomes;
}

// Adiciona (delta 1) ou remove (delta -1) o ultimo patamar de carga em todos os arquivos que dependem
// do numero de patamares, conforme o manual do NEWAVE 30.0.2:
// - patamar.dat (3.8): numero no bloco 1; no bloco 2 tipo 1, o campo do patamar (duracao 0); no
//   tipo 2 e nos blocos 3 a 5, uma linha por patamar em cada ano ou submercado (a nova copia a
//   anterior, com duracao 0 no bloco 2, para as duracoes continuarem somando 1 em cada mes);
// - loss.dat (3.20) e gtminpat.dat (3.23): uma linha por patamar, copiada da anterior;
// - agrint.dat (3.26, bloco 2): limite do novo patamar -1, que o manual define como sem restricao;
// - adterm.dat (3.28): geracao antecipada do novo patamar igual a do anterior, sem valor neutro;
// - ghmin.dat (3.29, campo 4), penalid.dat (3.24, campo 5, so GHMIN), re.dat (3.33, bloco 2) e
//   restricao-eletrica.csv (3.45.2, campo 5): registros do ultimo patamar copiados para o novo, ou os
//   do patamar removido apagados; patamar 0 vale para todos e nao muda;
// - modif.dat (3.12): linhas com um valor por patamar so geram aviso.
// Os limites sao 1 e 5 patamares (cinco campos de duracao no bloco 2 tipo 1). Com um patamar, o
// manual dispensa os blocos 2 a 5; sem eles nao ha o que copiar e a operacao e recusada. Nada e
// redistribuido: os avisos dizem o que o usuario precisa conferir.
Resultado mudarPatamaresDeCarga(const std::map<std::string, std::string>& textos, int delta, MudancaPatamares& saida) {
    Edicao patamar;
    if (!patamar.abrir(textos, "patamar.dat")) return Resultado::erro("patamar.dat não carregado");
    const int atual = patamaresDeCarga(patamar.arquivo);
    if (atual <= 0) return Resultado::erro("patamar.dat sem o número de patamares");
    const int novo = atual + delta;
    if (novo < 1 || novo > MAXIMO_PATAMARES_CARGA)
        return Resultado::erro("O número de patamares de carga vai de 1 a " + std::to_string(MAXIMO_PATAMARES_CARGA));
    const bool adicionar = novo > atual;
    const int k = std::max(atual, novo);
    const auto duracao = [](const SecaoFixa& d) { return d.titulo.rfind("Duração", 0) == 0; };
    bool tem_duracao = false;
    for (const SecaoLida& s : patamar.arquivo.secoes())
        if (duracao(s.definicao) && !s.linhas.empty()) tem_duracao = true;
    if (!tem_duracao) return Resultado::erro("patamar.dat sem as durações dos patamares (bloco 2); preencha-as pelo editor textual");

    std::vector<Edicao> edicoes;
    auto& avisos = saida.avisos;
    gravarNumero(patamar, novo);
    int fora = linhasPorPatamar(patamar, atual, novo, duracao);
    campoPorPatamar(patamar, Patamares::Carga, k, [&](const SecaoLida& s, size_t, const ColunaFixa& c, const std::string&) {
        return std::optional<std::string>(adicionar && duracao(s.definicao) ? formatar(0, c.decimais) : "");
    });
    if (fora) avisos.push_back("patamar.dat: " + std::to_string(fora) + " grupos sem uma linha por patamar ficaram como estavam");
    avisos.push_back(adicionar ? "patamar.dat: o " + ordinalPatamar(k) +
                                     " entra com duração 0 e com os fatores do anterior; redistribua as durações, que devem somar 1 em cada mês"
                               : "patamar.dat: sem o " + ordinalPatamar(k) + ", as durações que ficaram precisam ser redistribuídas para somar 1 em cada mês");
    edicoes.push_back(std::move(patamar));

    for (const char* nome : {"loss.dat", "gtminpat.dat"}) {
        Edicao e;
        if (!e.abrir(textos, nome)) continue;
        fora = linhasPorPatamar(e, atual, novo, [](const SecaoFixa&) { return false; });
        if (fora) avisos.push_back(std::string(nome) + ": " + std::to_string(fora) + " grupos sem uma linha por patamar ficaram como estavam");
        edicoes.push_back(std::move(e));
    }

    Edicao agrint;
    if (agrint.abrir(textos, "agrint.dat")) {
        const int movidos = campoPorPatamar(agrint, Patamares::Carga, k, [&](const SecaoLida&, size_t, const ColunaFixa& c, const std::string&) {
            return std::optional<std::string>(adicionar ? formatar(-1, c.decimais) : "");
        });
        if (adicionar && agrint.mudou) avisos.push_back("agrint.dat: o " + ordinalPatamar(k) + " entra sem limite (-1)");
        if (movidos) avisos.push_back("agrint.dat: " + std::to_string(movidos) + " comentários no lugar do novo limite foram levados para depois dos campos de patamar");
        edicoes.push_back(std::move(agrint));
    }

    Edicao adterm;
    if (adterm.abrir(textos, "adterm.dat")) {
        campoPorPatamar(adterm, Patamares::Carga, k, [&](const SecaoLida& s, size_t, const ColunaFixa& c, const std::string& linha) {
            if (!adicionar) return std::optional<std::string>("");
            const int anterior = indiceColuna(s.definicao, [&](const ColunaFixa& o) { return o.patamares == c.patamares && o.patamar == k - 1; });
            if (anterior < 0) return std::optional<std::string>();
            const std::string valor = ler(linha, s.definicao.colunas[static_cast<size_t>(anterior)], 0);
            return valor.empty() ? std::optional<std::string>() : std::optional<std::string>(valor);
        });
        if (adicionar && adterm.mudou)
            avisos.push_back("adterm.dat: a geração antecipada do " + ordinalPatamar(k) +
                             " repete a do anterior; ela deve ficar entre a geração térmica mínima e a máxima");
        edicoes.push_back(std::move(adterm));
    }

    const auto todos = [](const SecaoLida&, size_t) { return true; };
    for (const char* nome : {"ghmin.dat", "penalid.dat", "re.dat", "restricao-eletrica.csv"}) {
        Edicao e;
        if (!e.abrir(textos, nome)) continue;
        const bool penalid = std::string(nome) == "penalid.dat";
        const int quantos = registrosDoPatamar(e, atual, novo, penalid ? std::function<bool(const SecaoLida&, size_t)>([&e](const SecaoLida& s, size_t r) {
            return ler(e.linhas[static_cast<size_t>(s.linhas[r])], s.definicao.colunas[0], e.separador) == "GHMIN";
        }) : std::function<bool(const SecaoLida&, size_t)>(todos));
        if (quantos)
            avisos.push_back(std::string(nome) + ": " + std::to_string(quantos) +
                             (adicionar ? " registros do " + ordinalPatamar(atual) + " copiados para o " + ordinalPatamar(novo)
                                        : " registros do " + ordinalPatamar(atual) + " removidos"));
        edicoes.push_back(std::move(e));
    }

    if (const int modificacoes = modificacoesPorPatamar(textos, atual))
        avisos.push_back("modif.dat: " + std::to_string(modificacoes) +
                         " registros TURBMAXT, TURBMINT ou VAZMAXT têm um valor por patamar; ajuste-os pelo editor textual");

    for (const Edicao& e : edicoes)
        if (e.mudou) saida.textos[e.nome] = e.texto();
    return Resultado::sucesso();
}

// Adiciona (delta 1) ou remove (delta -1) o ultimo patamar de deficit do sistema.dat (manual 3.7):
// numero no bloco 1 e, no bloco 2, custo e profundidade do patamar em cada submercado nao ficticio
// (o manual manda o ficticio ignorar esses campos). O novo patamar repete o custo do anterior com
// profundidade 0, para a soma das profundidades continuar 1; o removido fica com 0, como os
// patamares nao usados no deck. De 1 a 4 patamares (quatro custos e quatro profundidades).
Resultado mudarPatamaresDeDeficit(const std::map<std::string, std::string>& textos, int delta, MudancaPatamares& saida) {
    Edicao sistema;
    if (!sistema.abrir(textos, "sistema.dat")) return Resultado::erro("sistema.dat não carregado");
    const int atual = patamaresDeDeficit(sistema.arquivo);
    if (atual <= 0) return Resultado::erro("sistema.dat sem o número de patamares de déficit");
    const int novo = atual + delta;
    if (novo < 1 || novo > MAXIMO_PATAMARES_DEFICIT)
        return Resultado::erro("O número de patamares de déficit vai de 1 a " + std::to_string(MAXIMO_PATAMARES_DEFICIT));
    const bool adicionar = novo > atual;
    const int k = std::max(atual, novo);
    gravarNumero(sistema, novo);
    campoPorPatamar(sistema, Patamares::Deficit, k, [&](const SecaoLida& s, size_t, const ColunaFixa& c, const std::string& linha) {
        const int ficticio = indiceColuna(s.definicao, [](const ColunaFixa& o) { return o.nome == "Fictício"; });
        if (ficticio >= 0 && ler(linha, s.definicao.colunas[static_cast<size_t>(ficticio)], 0) == "1") return std::optional<std::string>();
        const bool custo = c.nome.rfind("Custo", 0) == 0;
        if (!adicionar || !custo) return std::optional<std::string>(formatar(0, c.decimais));
        const int anterior = indiceColuna(s.definicao, [&](const ColunaFixa& o) {
            return o.patamares == Patamares::Deficit && o.patamar == k - 1 && o.nome.rfind("Custo", 0) == 0;
        });
        return std::optional<std::string>(anterior < 0 ? formatar(0, c.decimais) : ler(linha, s.definicao.colunas[static_cast<size_t>(anterior)], 0));
    });
    saida.avisos.push_back(adicionar ? "sistema.dat: o " + ordinalPatamar(k) +
                                           " de déficit repete o custo do anterior e entra com profundidade 0; ajuste custos e profundidades, que devem somar 1"
                                     : "sistema.dat: sem o " + ordinalPatamar(k) +
                                           " de déficit, ajuste as profundidades que ficaram para somarem 1");
    saida.textos["sistema.dat"] = sistema.texto();
    return Resultado::sucesso();
}
