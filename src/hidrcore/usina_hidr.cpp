#include "usina_hidr.h"

bool UsinaHidr::vazia() const {
    return nome.find_first_not_of(' ') == std::string::npos;
}
