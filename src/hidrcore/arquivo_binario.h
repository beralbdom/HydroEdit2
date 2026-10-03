#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>
#include "resultado.h"

class ArquivoBinario {
public:
    Resultado carregar(const std::filesystem::path& caminho, int tamanho_registro);
    Resultado salvar(const std::filesystem::path& caminho);

    int registros() const { return tamanho_ > 0 ? static_cast<int>(bytes_.size() / static_cast<size_t>(tamanho_)) : 0; }
    int tamanhoRegistro() const { return tamanho_; }
    std::string texto(int registro, int inicio, int tamanho) const;
    int32_t inteiro(int registro, int inicio) const;
    Resultado definirTexto(int registro, int inicio, int tamanho, const std::string& texto);
    void definirInteiro(int registro, int inicio, int32_t valor);
    bool modificado() const { return modificado_; }
    void definirModificado(bool modificado) { modificado_ = modificado; }
    const std::vector<char>& bytes() const { return bytes_; }
    void substituirBytes(std::vector<char> bytes) { bytes_ = std::move(bytes); }

private:
    size_t posicao(int registro, int inicio) const;

    std::vector<char> bytes_;
    int tamanho_ = 0;
    bool modificado_ = false;
};
