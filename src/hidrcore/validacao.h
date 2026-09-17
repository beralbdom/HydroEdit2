#pragma once
#include <string>
#include <vector>
#include "usina_hidr.h"

enum class Severidade { Erro, Aviso };

struct Problema {
    std::string campo;
    Severidade severidade;
    std::string mensagem;
};

struct ContextoValidacao {
    int num_usinas = 0;
    int codigo = 0;
};

// Regras do HydroEdit 4.0a; as que o deck oficial da ONS viola sao avisos (spec, secao 5).
std::vector<Problema> validar(const UsinaHidr& u, const ContextoValidacao& ctx);
bool temErro(const std::vector<Problema>& problemas);
