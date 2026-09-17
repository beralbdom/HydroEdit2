#pragma once
#include <string>

struct Resultado {
    bool ok = true;
    std::string mensagem;

    static Resultado sucesso() { return {}; }
    static Resultado erro(std::string m) { return {false, std::move(m)}; }
    explicit operator bool() const { return ok; }
};
