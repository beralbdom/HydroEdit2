#pragma once
#include <array>
#include "usina_hidr.h"

constexpr int TAMANHO_REGISTRO = 792;
using Registro = std::array<unsigned char, TAMANHO_REGISTRO>;

Registro serializar(const UsinaHidr& u);
UsinaHidr desserializar(const Registro& r);
