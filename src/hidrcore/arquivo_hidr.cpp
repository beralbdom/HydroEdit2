#include "arquivo_hidr.h"
#include <fstream>
#include <system_error>
#include "registro.h"

namespace fs = std::filesystem;

Resultado ArquivoHidr::carregar(const fs::path& caminho) {
    std::error_code ec;
    if (!fs::is_regular_file(caminho, ec))
        return Resultado::erro("Arquivo nao encontrado: " + caminho.string());
    auto tamanho = fs::file_size(caminho, ec);
    if (ec) return Resultado::erro("Nao foi possivel ler o tamanho de " + caminho.string());
    if (tamanho == 0) return Resultado::erro("Arquivo vazio: " + caminho.string());
    if (tamanho % TAMANHO_REGISTRO != 0)
        return Resultado::erro("Tamanho " + std::to_string(tamanho) + " bytes nao e multiplo de 792 (sobram " +
                               std::to_string(tamanho % TAMANHO_REGISTRO) + " bytes)");

    std::ifstream f(caminho, std::ios::binary);
    if (!f) return Resultado::erro("Nao foi possivel abrir " + caminho.string());
    std::vector<UsinaHidr> lidas;
    lidas.reserve(tamanho / TAMANHO_REGISTRO);
    Registro r;
    while (f.read(reinterpret_cast<char*>(r.data()), TAMANHO_REGISTRO)) lidas.push_back(desserializar(r));
    if (lidas.size() != tamanho / TAMANHO_REGISTRO)
        return Resultado::erro("Leitura incompleta de " + caminho.string());
    usinas = std::move(lidas);
    return Resultado::sucesso();
}

// Grava em arquivo temporario ao lado do destino e renomeia por cima, para que uma falha
// no meio da escrita nunca deixe o hidr.dat original truncado.
Resultado ArquivoHidr::salvar(const fs::path& caminho) const {
    fs::path temporario = caminho;
    temporario += ".tmp";
    {
        std::ofstream f(temporario, std::ios::binary | std::ios::trunc);
        if (!f) return Resultado::erro("Nao foi possivel criar " + temporario.string());
        for (const UsinaHidr& u : usinas) {
            Registro r = serializar(u);
            f.write(reinterpret_cast<const char*>(r.data()), TAMANHO_REGISTRO);
        }
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
    return Resultado::sucesso();
}

int ArquivoHidr::numUsinasPreenchidas() const {
    int n = 0;
    for (const UsinaHidr& u : usinas)
        if (!u.vazia()) ++n;
    return n;
}
