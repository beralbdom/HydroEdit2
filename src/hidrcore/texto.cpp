#include "texto.h"

std::string apararDireita(std::string_view s) {
    auto fim = s.find_last_not_of(' ');
    if (fim == std::string_view::npos) return {};
    return std::string(s.substr(0, fim + 1));
}

std::string latin1ParaUtf8(std::string_view s) {
    std::string r;
    r.reserve(s.size());
    for (unsigned char c : s) {
        if (c < 0x80) {
            r.push_back(static_cast<char>(c));
        } else {
            r.push_back(static_cast<char>(0xC0 | (c >> 6)));
            r.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return r;
}
