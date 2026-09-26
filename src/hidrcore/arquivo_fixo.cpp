#include "arquivo_fixo.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

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

bool emBranco(const std::string& linha) { return linha.find_first_not_of(" \t") == std::string::npos; }

size_t comprimentoUtil(const std::string& linha) {
    size_t fim = linha.find_last_not_of(" \t");
    return fim == std::string::npos ? 0 : fim + 1;
}
}  // namespace

// Separa o conteudo em linhas, guardando o tipo de quebra (CRLF ou LF) e se o arquivo termina com
// quebra, para salvar byte a byte igual. As secoes sao lidas em ordem: cada uma pula as suas linhas
// de cabecalho e toma como registro toda linha nao vazia ate a linha cujo primeiro campo e o
// terminador (consumida, sem virar registro) ou ate o fim do arquivo. Uma secao que o arquivo nao
// chega a ter fica sem registros. Com passo_repeticao, a ultima coluna da secao se repete a cada
// passo enquanto alguma linha tiver texto naquela posicao, e as copias sao numeradas a partir de 1.
Resultado ArquivoFixo::interpretar(const std::string& conteudo, const LayoutArquivoFixo& layout) {
    linhas_.clear();
    secoes_.clear();
    modificado_ = false;
    quebra_ = conteudo.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    quebra_final_ = !conteudo.empty() && conteudo.back() == '\n';
    std::istringstream entrada(conteudo);
    for (std::string linha; std::getline(entrada, linha);) {
        if (!linha.empty() && linha.back() == '\r') linha.pop_back();
        linhas_.push_back(linha);
    }

    size_t pos = 0;
    for (const SecaoFixa& definicao : layout.secoes) {
        SecaoLida secao{definicao, {}};
        pos = std::min(linhas_.size(), pos + static_cast<size_t>(definicao.linhas_cabecalho));
        for (; pos < linhas_.size(); ++pos) {
            if (!definicao.terminador.empty() && primeiroToken(linhas_[pos]) == definicao.terminador) {
                ++pos;
                break;
            }
            if (!emBranco(linhas_[pos])) secao.linhas.push_back(static_cast<int>(pos));
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

std::string ArquivoFixo::valor(int secao, int registro, int coluna) const {
    const SecaoLida& s = secoes_[static_cast<size_t>(secao)];
    const ColunaFixa& c = s.definicao.colunas[static_cast<size_t>(coluna)];
    const std::string& linha = linhas_[static_cast<size_t>(s.linhas[static_cast<size_t>(registro)])];
    if (static_cast<int>(linha.size()) < c.inicio) return {};
    return aparar(linha.substr(static_cast<size_t>(c.inicio - 1), static_cast<size_t>(c.fim - c.inicio + 1)));
}

// Formata o texto no campo e troca so as colunas dele na linha, que e completada com espacos se for
// mais curta; o resto da linha fica intacto. Vazio apaga o campo. Inteiro e real ficam alinhados a
// direita e texto a esquerda, como nos arquivos do deck; real sai com as casas do formato Fw.d e, no
// formato Fw.0, com o ponto no fim, como o Fortran escreve. Numero invalido ou que nao cabe na
// largura e erro, e a linha nao muda.
Resultado ArquivoFixo::definir(int secao, int registro, int coluna, const std::string& texto) {
    const SecaoLida& s = secoes_[static_cast<size_t>(secao)];
    const ColunaFixa& c = s.definicao.colunas[static_cast<size_t>(coluna)];
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
                } else {
                    double v = std::stod(entrada, &lidos);
                    char buffer[64];
                    std::snprintf(buffer, sizeof buffer, "%.*f", c.decimais, v);
                    campo = buffer;
                    if (c.decimais == 0) campo += '.';
                }
            } catch (...) {
                lidos = 0;
            }
            if (lidos != entrada.size()) return Resultado::erro("Valor invalido para " + c.nome + ": " + texto);
        }
        if (campo.size() > largura)
            return Resultado::erro(c.nome + ": " + campo + " nao cabe em " + std::to_string(largura) + " colunas");
        if (c.tipo == TipoColunaFixa::Texto) campo.append(largura - campo.size(), ' ');
        else campo.insert(0, largura - campo.size(), ' ');
    } else {
        campo.assign(largura, ' ');
    }
    std::string& linha = linhas_[static_cast<size_t>(s.linhas[static_cast<size_t>(registro)])];
    if (linha.size() < static_cast<size_t>(c.fim)) linha.append(static_cast<size_t>(c.fim) - linha.size(), ' ');
    linha.replace(static_cast<size_t>(c.inicio - 1), largura, campo);
    modificado_ = true;
    return Resultado::sucesso();
}
