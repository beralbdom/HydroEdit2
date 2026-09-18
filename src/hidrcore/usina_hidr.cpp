#include "usina_hidr.h"
#include <cctype>
#include <string_view>

bool UsinaHidr::vazia() const {
    return nome.find_first_not_of(' ') == std::string::npos;
}

// As usinas ficticias do deck sao as que o NEWAVE usa para representar trechos sem usina real, e o
// cadastro as nomeia com o prefixo "FICT" (por exemplo "FICT.SERRA M"). Comparacao sem diferenciar
// maiusculas e ignorando o espacamento a esquerda, que o registro de 12 bytes pode trazer.
bool usinaFicticia(const UsinaHidr& usina) {
    static constexpr std::string_view kPrefixo = "FICT";
    size_t inicio = usina.nome.find_first_not_of(' ');
    if (inicio == std::string::npos) return false;

    std::string_view nome(usina.nome);
    nome.remove_prefix(inicio);
    if (nome.size() < kPrefixo.size()) return false;

    for (size_t i = 0; i < kPrefixo.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(nome[i])) != kPrefixo[i]) return false;
    }
    return true;
}
