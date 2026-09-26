#include "arquivo_binario.h"
#include <cstring>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

// Le o arquivo inteiro, de acesso direto e nao formatado, com registros de tamanho_registro bytes; o
// tamanho do arquivo tem de ser multiplo do registro.
Resultado ArquivoBinario::carregar(const fs::path& caminho, int tamanho_registro) {
    bytes_.clear();
    tamanho_ = 0;
    modificado_ = false;
    std::ifstream f(caminho, std::ios::binary);
    if (!f) return Resultado::erro("Nao foi possivel abrir " + caminho.string());
    std::vector<char> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (tamanho_registro <= 0 || bytes.size() % static_cast<size_t>(tamanho_registro) != 0)
        return Resultado::erro("Tamanho de " + caminho.filename().string() + " (" + std::to_string(bytes.size()) +
                               " bytes) nao e multiplo do registro de " + std::to_string(tamanho_registro) + " bytes");
    bytes_ = std::move(bytes);
    tamanho_ = tamanho_registro;
    return Resultado::sucesso();
}

// Grava num temporario ao lado e so entao substitui o original, como os arquivos de texto.
Resultado ArquivoBinario::salvar(const fs::path& caminho) {
    fs::path temporario = caminho;
    temporario += ".tmp";
    {
        std::ofstream f(temporario, std::ios::binary | std::ios::trunc);
        if (!f) return Resultado::erro("Nao foi possivel criar " + temporario.string());
        f.write(bytes_.data(), static_cast<std::streamsize>(bytes_.size()));
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

size_t ArquivoBinario::posicao(int registro, int inicio) const {
    return static_cast<size_t>(registro) * static_cast<size_t>(tamanho_) + static_cast<size_t>(inicio);
}

// Texto de tamanho bytes a partir do byte inicio do registro (base 0), sem os espacos e nulos do fim.
std::string ArquivoBinario::texto(int registro, int inicio, int tamanho) const {
    std::string s(bytes_.data() + posicao(registro, inicio), static_cast<size_t>(tamanho));
    const size_t fim = s.find_last_not_of(std::string(" \0", 2));
    return fim == std::string::npos ? std::string() : s.substr(0, fim + 1);
}

// Inteiro de 4 bytes little-endian a partir do byte inicio do registro (base 0).
int32_t ArquivoBinario::inteiro(int registro, int inicio) const {
    int32_t valor = 0;
    std::memcpy(&valor, bytes_.data() + posicao(registro, inicio), sizeof valor);
    return valor;
}

// Texto maior que o campo e erro; menor e completado com espacos, como o Fortran grava.
Resultado ArquivoBinario::definirTexto(int registro, int inicio, int tamanho, const std::string& texto) {
    if (texto.size() > static_cast<size_t>(tamanho))
        return Resultado::erro(texto + " nao cabe em " + std::to_string(tamanho) + " caracteres");
    std::string campo = texto;
    campo.append(static_cast<size_t>(tamanho) - campo.size(), ' ');
    std::memcpy(bytes_.data() + posicao(registro, inicio), campo.data(), campo.size());
    modificado_ = true;
    return Resultado::sucesso();
}

void ArquivoBinario::definirInteiro(int registro, int inicio, int32_t valor) {
    std::memcpy(bytes_.data() + posicao(registro, inicio), &valor, sizeof valor);
    modificado_ = true;
}
