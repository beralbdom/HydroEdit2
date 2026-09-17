#include "registro.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include "campos.h"
#include "texto.h"

static_assert(std::endian::native == std::endian::little, "hidr.dat e little-endian");

Registro serializar(const UsinaHidr& u) {
    Registro r;
    r.fill(' ');
    for (const Campo& c : campos()) {
        for (int i = 0; i < c.n; ++i) {
            unsigned char* p = r.data() + c.offset + i * c.tamanho_elemento;
            Valor v = c.obter(u, i);
            switch (c.tipo) {
                case TipoCampo::Texto: {
                    const std::string& s = std::get<std::string>(v);
                    std::memset(p, ' ', static_cast<size_t>(c.tamanho_elemento));
                    std::memcpy(p, s.data(), std::min(s.size(), static_cast<size_t>(c.tamanho_elemento)));
                    break;
                }
                case TipoCampo::Inteiro: {
                    int32_t x = std::get<int32_t>(v);
                    std::memcpy(p, &x, 4);
                    break;
                }
                case TipoCampo::Real: {
                    float x = std::get<float>(v);
                    std::memcpy(p, &x, 4);
                    break;
                }
            }
        }
    }
    return r;
}

UsinaHidr desserializar(const Registro& r) {
    UsinaHidr u;
    for (const Campo& c : campos()) {
        for (int i = 0; i < c.n; ++i) {
            const unsigned char* p = r.data() + c.offset + i * c.tamanho_elemento;
            switch (c.tipo) {
                case TipoCampo::Texto: {
                    std::string_view s(reinterpret_cast<const char*>(p), static_cast<size_t>(c.tamanho_elemento));
                    c.definir(u, i, Valor{apararDireita(s)});
                    break;
                }
                case TipoCampo::Inteiro: {
                    int32_t x;
                    std::memcpy(&x, p, 4);
                    c.definir(u, i, Valor{x});
                    break;
                }
                case TipoCampo::Real: {
                    float x;
                    std::memcpy(&x, p, 4);
                    c.definir(u, i, Valor{x});
                    break;
                }
            }
        }
    }
    return u;
}
