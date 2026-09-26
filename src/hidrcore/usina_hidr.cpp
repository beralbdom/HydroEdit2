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

// Manual do DESSEM v19.0.44 (CEPEL, abr/2023): o campo 42 do cadastro (secao III.7) e o tipo de
// regularizacao da usina, mensal, semanal ou diaria, e a secao sobre os ajustes dos valores da agua
// trata a usina "de regularizacao diaria (portanto, fio d'agua no DECOMP)". Com reservatorio sao
// entao as de regularizacao mensal ou semanal; diaria ou campo vazio contam como fio d'agua.
bool usinaComReservatorio(const UsinaHidr& usina) {
    return usina.regulacao == "M" || usina.regulacao == "S";
}
