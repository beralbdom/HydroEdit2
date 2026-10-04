#include "arquivo_fixo.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <tuple>
#include <utility>

namespace fs = std::filesystem;

namespace {
std::string aparar(const std::string& s) {
    size_t ini = s.find_first_not_of(" \t\r");
    if (ini == std::string::npos) return {};
    return s.substr(ini, s.find_last_not_of(" \t\r") - ini + 1);
}

std::string primeiroToken(const std::string& linha) {
    std::istringstream entrada(linha);
    std::string token;
    entrada >> token;
    return token;
}

size_t comprimentoUtil(const std::string& linha) {
    size_t fim = linha.find_last_not_of(" \t");
    return fim == std::string::npos ? 0 : fim + 1;
}

// Posicao e tamanho do campo de numero n (a partir de 1) numa linha de campos separados; posicao npos
// se a linha tem menos campos.
std::pair<size_t, size_t> limitesCampo(const std::string& linha, char separador, int n) {
    size_t inicio = 0;
    for (int k = 1; k < n; ++k) {
        const size_t proximo = linha.find(separador, inicio);
        if (proximo == std::string::npos) return {std::string::npos, 0};
        inicio = proximo + 1;
    }
    const size_t fim = linha.find(separador, inicio);
    return {inicio, (fim == std::string::npos ? linha.size() : fim) - inicio};
}
}  // namespace

// Texto aparado das colunas inicio a fim; em arquivo de campos separados, do campo de numero inicio.
std::string ArquivoFixo::campo(const std::string& linha, int inicio, int fim) const {
    if (separador_) {
        const auto [posicao, tamanho] = limitesCampo(linha, separador_, inicio);
        return posicao == std::string::npos ? std::string() : aparar(linha.substr(posicao, tamanho));
    }
    if (static_cast<int>(linha.size()) < inicio) return {};
    return aparar(linha.substr(static_cast<size_t>(inicio - 1), static_cast<size_t>(fim - inicio + 1)));
}

// Linha que nunca e registro nem contexto: em branco ou, em arquivo de campos separados, comentario
// comecado por &.
bool ArquivoFixo::ignorada(int linha) const {
    const std::string& texto = linhas_[static_cast<size_t>(linha)];
    const size_t primeiro = texto.find_first_not_of(" \t");
    return primeiro == std::string::npos || (separador_ && texto[primeiro] == '&');
}

// Todos os testes do filtro valem para a linha (filtro vazio aceita qualquer uma). O texto testado e
// o do campo, aparado.
bool ArquivoFixo::passa(const std::vector<FiltroLinha>& filtro, int linha) const {
    const std::string& texto = linhas_[static_cast<size_t>(linha)];
    for (const FiltroLinha& f : filtro) {
        const std::string valor = campo(texto, f.inicio, f.fim);
        const bool listado = std::find(f.valores.begin(), f.valores.end(), valor) != f.valores.end();
        switch (f.teste) {
        case TesteFiltro::Vazio: if (!valor.empty()) return false; break;
        case TesteFiltro::Preenchido: if (valor.empty()) return false; break;
        case TesteFiltro::Igual: if (!listado) return false; break;
        case TesteFiltro::Diferente: if (listado) return false; break;
        }
    }
    return true;
}

// Separa o conteudo em linhas, guardando o tipo de quebra (CRLF ou LF) e se o arquivo termina com
// quebra, para salvar byte a byte igual. As secoes sao lidas em ordem: cada uma pula as suas linhas
// de cabecalho e sua regiao vai ate a linha cujo primeiro campo e o terminador (consumida), ate o
// fim do arquivo, com max_registros ate o ultimo registro permitido ou, se contigua, ate a primeira
// linha nao vazia que nao passa no filtro (que fica para a secao seguinte). Registro e toda linha nao
// vazia da regiao que passa no filtro. Secao com mesma_regiao le de novo a regiao da anterior, sem
// cabecalho e sem avancar, para mostrar outro tipo de registro das mesmas linhas. Para cada contexto
// da secao, cada registro guarda a linha de contexto: a mais proxima, no proprio registro ou antes
// dele dentro da regiao, que passa no filtro do contexto (-1 se nenhuma). Uma secao que o arquivo
// nao chega a ter fica sem registros. Com passo_repeticao, a ultima coluna da secao se repete a cada
// passo enquanto alguma linha tiver texto naquela posicao, e as copias sao numeradas a partir de 1.
// Com separador, as colunas sao numeros de campo e as linhas comecadas por & sao comentario.
Resultado ArquivoFixo::interpretar(const std::string& conteudo, const LayoutArquivoFixo& layout) {
    linhas_.clear();
    secoes_.clear();
    modificado_ = false;
    separador_ = layout.separador;
    quebra_ = conteudo.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    quebra_final_ = !conteudo.empty() && conteudo.back() == '\n';
    std::istringstream entrada(conteudo);
    for (std::string linha; std::getline(entrada, linha);) {
        if (!linha.empty() && linha.back() == '\r') linha.pop_back();
        linhas_.push_back(linha);
    }

    size_t pos = 0;
    size_t regiao_inicio = 0;
    size_t regiao_fim = 0;
    for (const SecaoFixa& definicao : layout.secoes) {
        SecaoLida secao{definicao, {}, {}};
        const bool repetir = definicao.mesma_regiao && !secoes_.empty();
        if (!repetir) {
            regiao_inicio = std::min(linhas_.size(), pos + static_cast<size_t>(definicao.linhas_cabecalho));
            regiao_fim = linhas_.size();
            pos = linhas_.size();
        }
        const size_t limite = repetir ? regiao_fim : linhas_.size();
        for (size_t p = regiao_inicio; p < limite; ++p) {
            if (!repetir && !definicao.terminador.empty() && primeiroToken(linhas_[p]) == definicao.terminador) {
                regiao_fim = p;
                pos = p + 1;
                break;
            }
            if (definicao.max_registros > 0 && static_cast<int>(secao.linhas.size()) == definicao.max_registros) {
                if (!repetir) regiao_fim = pos = p;
                break;
            }
            if (!repetir && definicao.contigua && !ignorada(static_cast<int>(p)) && !passa(definicao.filtro, static_cast<int>(p))) {
                regiao_fim = pos = p;
                break;
            }
            if (!ignorada(static_cast<int>(p)) && passa(definicao.filtro, static_cast<int>(p))) secao.linhas.push_back(static_cast<int>(p));
        }
        secao.regiao_inicio = static_cast<int>(regiao_inicio);
        secao.regiao_fim = static_cast<int>(regiao_fim);
        for (int linha : secao.linhas) {
            std::vector<int> contexto;
            for (const ContextoFixo& c : definicao.contextos) {
                int achada = -1;
                for (int q = linha; q >= static_cast<int>(regiao_inicio) && achada < 0; --q)
                    if (!ignorada(q) && passa(c.filtro, q)) achada = q;
                contexto.push_back(achada);
            }
            secao.linhas_contexto.push_back(std::move(contexto));
        }
        if (definicao.passo_repeticao > 0 && !secao.definicao.colunas.empty()) {
            size_t maior = 0;
            for (int i : secao.linhas) maior = std::max(maior, comprimentoUtil(linhas_[static_cast<size_t>(i)]));
            ColunaFixa base = secao.definicao.colunas.back();
            secao.definicao.colunas.pop_back();
            for (int k = 0; base.inicio + k * definicao.passo_repeticao <= static_cast<int>(maior); ++k) {
                ColunaFixa copia = base;
                copia.nome = base.nome + " " + std::to_string(k + 1);
                copia.inicio += k * definicao.passo_repeticao;
                copia.fim += k * definicao.passo_repeticao;
                secao.definicao.colunas.push_back(copia);
            }
        }
        secoes_.push_back(std::move(secao));
    }
    return Resultado::sucesso();
}

Resultado ArquivoFixo::carregar(const fs::path& caminho, const LayoutArquivoFixo& layout) {
    std::ifstream f(caminho, std::ios::binary);
    if (!f) return Resultado::erro("Nao foi possivel abrir " + caminho.string());
    std::stringstream conteudo;
    conteudo << f.rdbuf();
    return interpretar(conteudo.str(), layout);
}

std::string ArquivoFixo::conteudo() const {
    std::string texto;
    for (size_t i = 0; i < linhas_.size(); ++i) {
        texto += linhas_[i];
        if (i + 1 < linhas_.size() || quebra_final_) texto += quebra_;
    }
    return texto;
}

// Texto do arquivo com quebra LF, o formato de um editor de texto; termina com quebra se o arquivo
// terminava.
std::string ArquivoFixo::textoLf() const {
    std::string texto;
    for (size_t i = 0; i < linhas_.size(); ++i) {
        texto += linhas_[i];
        if (i + 1 < linhas_.size() || quebra_final_) texto += '\n';
    }
    return texto;
}

// Troca o arquivo inteiro pelo texto editado, relendo as secoes pelo layout. O tipo de quebra
// original (CRLF ou LF) continua valendo para salvar, e o arquivo passa a contar como alterado se o
// texto for diferente do que havia.
void ArquivoFixo::substituirTexto(const std::string& texto_lf, const LayoutArquivoFixo& layout) {
    const std::string anterior = textoLf();
    const std::string quebra = quebra_;
    const bool ja_modificado = modificado_;
    interpretar(texto_lf, layout);
    quebra_ = quebra;
    modificado_ = ja_modificado || texto_lf != anterior;
}

// Grava num temporario ao lado e so entao substitui o original, como o hidr.dat.
Resultado ArquivoFixo::salvar(const fs::path& caminho) {
    fs::path temporario = caminho;
    temporario += ".tmp";
    {
        std::ofstream f(temporario, std::ios::binary | std::ios::trunc);
        if (!f) return Resultado::erro("Nao foi possivel criar " + temporario.string());
        f << conteudo();
        if (!f) {
            f.close();
            std::error_code ec;
            fs::remove(temporario, ec);
            return Resultado::erro("Falha ao escrever " + temporario.string());
        }
    }
    std::error_code ec;
    fs::rename(temporario, caminho, ec);
    if (ec) {
        std::error_code ec2;
        fs::remove(temporario, ec2);
        return Resultado::erro("Falha ao substituir " + caminho.string() + ": " + ec.message());
    }
    modificado_ = false;
    return Resultado::sucesso();
}

// Coluna de contexto le da linha de contexto do registro; coluna Ordinal da a posicao do registro
// entre os da secao desde a sua linha de contexto, e coluna Grupo o numero do grupo de linhas
// separado por linha em branco em que o registro esta, contado desde a linha de contexto; ambas a
// partir de 1.
std::string ArquivoFixo::valor(int secao, int registro, int coluna) const {
    const SecaoLida& s = secoes_[static_cast<size_t>(secao)];
    const ColunaFixa& c = s.definicao.colunas[static_cast<size_t>(coluna)];
    const size_t r = static_cast<size_t>(registro);
    int indice = s.linhas[r];
    if (c.contexto >= 0) {
        indice = s.linhas_contexto[r][static_cast<size_t>(c.contexto)];
        if (indice < 0) return {};
    }
    if (c.tipo == TipoColunaFixa::Ordinal) {
        int posicao = 0;
        for (int q = registro; q >= 0 && s.linhas[static_cast<size_t>(q)] >= indice; --q) ++posicao;
        return std::to_string(posicao);
    }
    if (c.tipo == TipoColunaFixa::Grupo) {
        const auto vazia = [this](int q) { return linhas_[static_cast<size_t>(q)].find_first_not_of(" \t") == std::string::npos; };
        int grupo = 1;
        for (int q = indice + 1; q <= s.linhas[r]; ++q)
            if (!vazia(q) && vazia(q - 1)) ++grupo;
        return std::to_string(grupo);
    }
    return campo(linhas_[static_cast<size_t>(indice)], c.inicio, c.fim);
}

// Formata o texto no campo e troca so as colunas dele na linha, que e completada com espacos se for
// mais curta; o resto da linha fica intacto. Vazio apaga o campo. Inteiro e real ficam alinhados a
// direita e texto a esquerda, como nos arquivos do deck; real sai com as casas do formato Fw.d, ou com
// as casas digitadas se forem mais e couberem na largura (o Fortran le o ponto explicito), e, sem
// casas, com o ponto no fim, como o Fortran escreve. Numero invalido ou que nao cabe na
// largura e erro, e a linha nao muda. Coluna de contexto grava na linha de contexto do registro,
// valendo para todo o bloco; Ordinal e Grupo nao se editam, e secao, registro ou coluna fora do
// arquivo sao erro. Em arquivo de
// campos separados, real fica como foi digitado, o campo mantem a largura que tinha quando o valor
// cabe nela e a linha ganha separadores se tiver campos de menos.
Resultado ArquivoFixo::definir(int secao, int registro, int coluna, const std::string& texto) {
    if (secao < 0 || secao >= static_cast<int>(secoes_.size())) return Resultado::erro("Secao fora do arquivo");
    const SecaoLida& s = secoes_[static_cast<size_t>(secao)];
    if (registro < 0 || registro >= static_cast<int>(s.linhas.size()) || coluna < 0 || coluna >= static_cast<int>(s.definicao.colunas.size()))
        return Resultado::erro("Campo fora da secao");
    const ColunaFixa& c = s.definicao.colunas[static_cast<size_t>(coluna)];
    if (!editavel(c)) return Resultado::erro(c.nome + " e somente leitura");
    const size_t largura = static_cast<size_t>(c.fim - c.inicio + 1);
    std::string entrada = aparar(texto);
    std::string campo;
    if (!entrada.empty()) {
        if (c.tipo == TipoColunaFixa::Texto) {
            campo = entrada;
        } else {
            std::replace(entrada.begin(), entrada.end(), ',', '.');
            size_t lidos = 0;
            try {
                if (c.tipo == TipoColunaFixa::Inteiro) {
                    campo = std::to_string(std::stoll(entrada, &lidos));
                } else if (separador_) {
                    static_cast<void>(std::stod(entrada, &lidos));
                    campo = entrada;
                } else {
                    const double v = std::stod(entrada, &lidos);
                    const size_t ponto = entrada.find('.');
                    int casas = c.decimais;
                    if (ponto != std::string::npos) {
                        const size_t fim = entrada.find_first_not_of("0123456789", ponto + 1);
                        casas = std::max(casas, static_cast<int>((fim == std::string::npos ? entrada.size() : fim) - ponto - 1));
                    }
                    for (;; --casas) {
                        char buffer[64];
                        std::snprintf(buffer, sizeof buffer, "%.*f", casas, v);
                        campo = buffer;
                        if (casas == 0) campo += '.';
                        if (casas <= c.decimais || campo.size() <= largura) break;
                    }
                }
            } catch (...) {
                lidos = 0;
            }
            if (lidos != entrada.size()) return Resultado::erro("Valor invalido para " + c.nome + ": " + texto);
        }
    }
    const int indice = c.contexto < 0 ? s.linhas[static_cast<size_t>(registro)]
                                      : s.linhas_contexto[static_cast<size_t>(registro)][static_cast<size_t>(c.contexto)];
    if (indice < 0) return Resultado::erro(c.nome + ": o registro nao tem linha de contexto");
    std::string& linha = linhas_[static_cast<size_t>(indice)];
    if (separador_) {
        auto [posicao, tamanho] = limitesCampo(linha, separador_, c.inicio);
        while (posicao == std::string::npos) {
            linha += separador_;
            std::tie(posicao, tamanho) = limitesCampo(linha, separador_, c.inicio);
        }
        if (campo.size() < tamanho) {
            if (c.tipo == TipoColunaFixa::Texto) campo.append(tamanho - campo.size(), ' ');
            else campo.insert(0, tamanho - campo.size(), ' ');
        }
        linha.replace(posicao, tamanho, campo);
        modificado_ = true;
        return Resultado::sucesso();
    }
    if (!campo.empty()) {
        if (campo.size() > largura)
            return Resultado::erro(c.nome + ": " + campo + " nao cabe em " + std::to_string(largura) + " colunas");
        if (c.tipo == TipoColunaFixa::Texto) campo.append(largura - campo.size(), ' ');
        else campo.insert(0, largura - campo.size(), ' ');
    } else {
        campo.assign(largura, ' ');
    }
    if (linha.size() < static_cast<size_t>(c.fim)) linha.append(static_cast<size_t>(c.fim) - linha.size(), ' ');
    linha.replace(static_cast<size_t>(c.inicio - 1), largura, campo);
    modificado_ = true;
    return Resultado::sucesso();
}

// Troca as linhas do arquivo e rele as secoes, como substituirTexto.
void ArquivoFixo::substituirLinhas(const std::vector<std::string>& linhas, const LayoutArquivoFixo& layout) {
    std::string texto;
    for (size_t i = 0; i < linhas.size(); ++i) {
        texto += linhas[i];
        if (i + 1 < linhas.size() || quebra_final_) texto += '\n';
    }
    substituirTexto(texto, layout);
}

// Deixa em branco as colunas do campo na linha, sem mudar o comprimento dela.
void ArquivoFixo::apagarCampo(std::string& linha, const ColunaFixa& c) const {
    if (separador_) {
        const auto [posicao, tamanho] = limitesCampo(linha, separador_, c.inicio);
        if (posicao != std::string::npos) linha.replace(posicao, tamanho, tamanho, ' ');
        return;
    }
    const size_t inicio = static_cast<size_t>(c.inicio - 1);
    if (linha.size() <= inicio) return;
    const size_t tamanho = std::min(static_cast<size_t>(c.fim - c.inicio + 1), linha.size() - inicio);
    linha.replace(inicio, tamanho, tamanho, ' ');
}

// Registros avulsos podem ser inseridos e removidos quando a secao nao tem numero fixo de registros
// nem coluna Ordinal: nas secoes com uma linha por patamar, a quantidade de linhas acompanha o numero
// de patamares e so o bloco inteiro se duplica ou remove.
bool ArquivoFixo::aceitaRegistrosAvulsos(int secao) const {
    const SecaoFixa& d = secoes_[static_cast<size_t>(secao)].definicao;
    if (d.max_registros > 0) return false;
    return std::none_of(d.colunas.begin(), d.colunas.end(), [](const ColunaFixa& c) { return c.tipo == TipoColunaFixa::Ordinal; });
}

// Linhas do bloco que comeca na linha de contexto inicio, no nivel dado da secao: vao ate a proxima
// linha da regiao que abre um contexto desse nivel ou de um mais externo, ou ate o fim da regiao
// (antes do terminador).
TrechoLinhas ArquivoFixo::blocoDeContexto(const SecaoLida& s, int nivel, int inicio) const {
    if (inicio < 0) return {};
    for (int p = inicio + 1; p < s.regiao_fim; ++p) {
        if (ignorada(p)) continue;
        for (int k = 0; k <= nivel; ++k)
            if (passa(s.definicao.contextos[static_cast<size_t>(k)].filtro, p)) return {inicio, p};
    }
    return {inicio, s.regiao_fim};
}

// Bloco que a linha abre numa outra secao da mesma regiao, no nivel de contexto mais externo cujo
// filtro ela passa (a linha de uma interligacao abre o bloco dos seus limites por ano); vazio se
// nenhum.
TrechoLinhas ArquivoFixo::blocoAberto(int secao, int linha) const {
    TrechoLinhas melhor;
    int melhor_nivel = -1;
    for (size_t t = 0; t < secoes_.size(); ++t) {
        const SecaoLida& s = secoes_[t];
        if (static_cast<int>(t) == secao || linha < s.regiao_inicio || linha >= s.regiao_fim) continue;
        for (int k = 0; k < static_cast<int>(s.definicao.contextos.size()); ++k) {
            if (melhor_nivel >= 0 && k >= melhor_nivel) break;
            if (!passa(s.definicao.contextos[static_cast<size_t>(k)].filtro, linha)) continue;
            melhor = blocoDeContexto(s, k, linha);
            melhor_nivel = k;
            break;
        }
    }
    return melhor;
}

// Nivel de contexto que a linha do proprio registro abre (o primeiro registro de cada usina do
// exph.dat traz o codigo e o nome), ou -1.
int ArquivoFixo::nivelProprio(int secao, int registro) const {
    const SecaoLida& s = secoes_[static_cast<size_t>(secao)];
    const std::vector<int>& contexto = s.linhas_contexto[static_cast<size_t>(registro)];
    for (size_t k = 0; k < contexto.size(); ++k)
        if (contexto[k] == s.linhas[static_cast<size_t>(registro)]) return static_cast<int>(k);
    return -1;
}

// O registro traz consigo um bloco de outra secao (a interligacao e os seus limites por ano), que
// duplicar e remover levam junto.
bool ArquivoFixo::abreBloco(int secao, int registro) const {
    return nivelProprio(secao, registro) < 0 && !blocoAberto(secao, secoes_[static_cast<size_t>(secao)].linhas[static_cast<size_t>(registro)]).vazio();
}

int ArquivoFixo::registroNaLinha(int secao, int linha) const {
    const std::vector<int>& linhas = secoes_[static_cast<size_t>(secao)].linhas;
    const auto it = std::find(linhas.begin(), linhas.end(), linha);
    return it == linhas.end() ? -1 : static_cast<int>(it - linhas.begin());
}

// Insere uma copia logo depois do original. Nivel >= 0 copia o bloco do contexto desse nivel do
// registro (uma interligacao com todos os anos, um ano com todos os patamares). Nivel -1 copia o
// registro: se a linha dele abre o proprio contexto, so a linha, com as colunas do contexto em
// branco, para continuar o mesmo bloco; se abre bloco de outra secao, o bloco inteiro; senao a
// linha. Em branco (so para registro avulso que nao abre bloco) a copia fica so com os campos que o
// filtro da secao exige preenchidos ou iguais a um valor, os que a fazem ser lida como registro da
// secao. A copia de um registro que deixaria de ser registro da secao e recusada, e o arquivo fica
// como estava.
Resultado ArquivoFixo::duplicar(int secao, int registro, int nivel, const LayoutArquivoFixo& layout, int* primeira_linha_nova,
                                bool em_branco) {
    const SecaoLida& s = secoes_[static_cast<size_t>(secao)];
    if (s.definicao.max_registros > 0) return Resultado::erro("A seção tem número fixo de registros");
    TrechoLinhas origem;
    std::vector<std::string> copia;
    bool avulso = false;
    if (nivel >= 0) {
        origem = blocoDeContexto(s, nivel, s.linhas_contexto[static_cast<size_t>(registro)][static_cast<size_t>(nivel)]);
        if (origem.vazio()) return Resultado::erro("O registro não pertence a um bloco desse nível");
    } else {
        if (!aceitaRegistrosAvulsos(secao)) return Resultado::erro("Os registros desta seção acompanham os patamares; duplique o bloco");
        const int linha = s.linhas[static_cast<size_t>(registro)];
        if (nivelProprio(secao, registro) < 0) origem = blocoAberto(secao, linha);
        if (em_branco && !origem.vazio()) return Resultado::erro("O registro abre um bloco; adicione uma cópia dele");
        if (origem.vazio()) {
            origem = {linha, linha + 1};
            avulso = true;
            std::string texto = linhas_[static_cast<size_t>(linha)];
            const std::vector<int>& contexto = s.linhas_contexto[static_cast<size_t>(registro)];
            for (const ColunaFixa& c : s.definicao.colunas)
                if (c.contexto >= 0 && contexto[static_cast<size_t>(c.contexto)] == linha && editavel(c)) apagarCampo(texto, c);
            if (em_branco) {
                const auto exigido = [&](const ColunaFixa& c) {
                    return std::any_of(s.definicao.filtro.begin(), s.definicao.filtro.end(), [&](const FiltroLinha& f) {
                        return (f.teste == TesteFiltro::Preenchido || f.teste == TesteFiltro::Igual) && f.inicio <= c.fim && c.inicio <= f.fim;
                    });
                };
                for (const ColunaFixa& c : s.definicao.colunas)
                    if (c.contexto < 0 && editavel(c) && !exigido(c)) apagarCampo(texto, c);
            }
            copia.push_back(texto);
        }
    }
    if (copia.empty()) copia.assign(linhas_.begin() + origem.inicio, linhas_.begin() + origem.fim);
    const std::vector<std::string> antes = linhas_;
    const bool modificado_antes = modificado_;
    std::vector<std::string> novas = linhas_;
    novas.insert(novas.begin() + origem.fim, copia.begin(), copia.end());
    substituirLinhas(novas, layout);
    if (avulso && registroNaLinha(secao, origem.fim) < 0) {
        substituirLinhas(antes, layout);
        modificado_ = modificado_antes;
        return Resultado::erro("A cópia do registro não seria lida como registro da seção");
    }
    if (primeira_linha_nova) *primeira_linha_nova = origem.fim;
    return Resultado::sucesso();
}

// Apaga do arquivo o que duplicar copiaria: o bloco do nivel dado ou o registro (com o bloco que a
// linha dele abre em outra secao). Registro que abre o proprio contexto so sai se for o unico do
// bloco, e entao sai o bloco inteiro (a usina do exph.dat com o seu 9999).
Resultado ArquivoFixo::remover(int secao, int registro, int nivel, const LayoutArquivoFixo& layout) {
    const SecaoLida& s = secoes_[static_cast<size_t>(secao)];
    if (s.definicao.max_registros > 0) return Resultado::erro("A seção tem número fixo de registros");
    TrechoLinhas alvo;
    if (nivel >= 0) {
        alvo = blocoDeContexto(s, nivel, s.linhas_contexto[static_cast<size_t>(registro)][static_cast<size_t>(nivel)]);
        if (alvo.vazio()) return Resultado::erro("O registro não pertence a um bloco desse nível");
    } else {
        if (!aceitaRegistrosAvulsos(secao)) return Resultado::erro("Os registros desta seção acompanham os patamares; remova o bloco");
        const int linha = s.linhas[static_cast<size_t>(registro)];
        const int proprio = nivelProprio(secao, registro);
        if (proprio >= 0) {
            alvo = blocoDeContexto(s, proprio, linha);
            const auto no_bloco = std::count_if(s.linhas.begin(), s.linhas.end(), [&](int q) { return q >= alvo.inicio && q < alvo.fim; });
            if (no_bloco > 1) return Resultado::erro("O registro abre o bloco dos seguintes; remova-os antes ou remova o bloco inteiro");
        } else {
            alvo = blocoAberto(secao, linha);
            if (alvo.vazio()) alvo = {linha, linha + 1};
        }
    }
    std::vector<std::string> novas = linhas_;
    novas.erase(novas.begin() + alvo.inicio, novas.begin() + alvo.fim);
    substituirLinhas(novas, layout);
    return Resultado::sucesso();
}
