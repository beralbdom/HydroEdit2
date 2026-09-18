#include "cascata.h"
#include <algorithm>
#include <functional>
#include <map>
#include <numeric>
#include <unordered_map>
#include <utility>

namespace {

int alvoValido(const std::vector<UsinaHidr>& usinas, int codigo, int32_t alvo) {
    if (alvo < 1 || alvo > static_cast<int32_t>(usinas.size())) return 0;
    if (alvo == codigo) return 0;
    if (usinas[static_cast<size_t>(alvo - 1)].vazia()) return 0;
    return alvo;
}

}  // namespace

// Raizes explicitas (jusante invalido) sao processadas primeiro, em ordem de codigo; depois,
// qualquer no ainda nao visitado (so ocorre em ciclo) vira raiz adicional, e a aresta de
// jusante que o levaria de volta a um no ja visitado no mesmo ciclo e descartada. Um no e
// "leaf" (recebe a proxima coluna livre) quando nao sobra nenhum filho para visitar depois
// de descartar os ja visitados; um no cuja subarvore tocou um ciclo tambem recebe coluna
// propria em vez da media dos filhos, senao ele colapsaria sobre o mesmo valor do filho
// unico que restou (media de um elemento e o proprio elemento), sobrepondo colunas ao
// longo de toda a cadeia que sobra de um ciclo resolvido.
Cascata montarCascata(const std::vector<UsinaHidr>& usinas) {
    Cascata cascata;
    int n = static_cast<int>(usinas.size());

    std::unordered_map<int, size_t> indiceDoCodigo;
    for (int codigo = 1; codigo <= n; ++codigo) {
        if (usinas[static_cast<size_t>(codigo - 1)].vazia()) continue;
        indiceDoCodigo[codigo] = cascata.nos.size();
        cascata.nos.push_back(NoCascata{codigo, 0.0, 0, -1});
    }

    std::map<int, std::vector<int>> filhosDe;
    for (int codigo = 1; codigo <= n; ++codigo) {
        if (!indiceDoCodigo.contains(codigo)) continue;
        int pai = alvoValido(usinas, codigo, usinas[static_cast<size_t>(codigo - 1)].jusante);
        if (pai != 0) filhosDe[pai].push_back(codigo);
    }

    std::vector<int> raizesExplicitas;
    for (int codigo = 1; codigo <= n; ++codigo) {
        if (!indiceDoCodigo.contains(codigo)) continue;
        if (alvoValido(usinas, codigo, usinas[static_cast<size_t>(codigo - 1)].jusante) == 0) raizesExplicitas.push_back(codigo);
    }

    std::vector<bool> visitado(static_cast<size_t>(n) + 1, false);
    int proximaColuna = 0;
    int numBacias = 0;

    std::function<std::pair<double, bool>(int, int, int&, std::vector<int>&)> visitar =
        [&](int codigo, int profundidade, int& profundidadeMaxima, std::vector<int>& membros) -> std::pair<double, bool> {
        visitado[static_cast<size_t>(codigo)] = true;
        membros.push_back(codigo);
        profundidadeMaxima = std::max(profundidadeMaxima, profundidade);
        NoCascata& no = cascata.nos[indiceDoCodigo[codigo]];
        no.linha = profundidade;
        no.bacia = numBacias;

        double soma = 0.0;
        int visitados = 0;
        bool tocaCiclo = false;
        auto it = filhosDe.find(codigo);
        if (it != filhosDe.end()) {
            for (int filho : it->second) {
                if (visitado[static_cast<size_t>(filho)]) {
                    tocaCiclo = true;
                    continue;
                }
                auto [colunaFilho, filhoTocaCiclo] = visitar(filho, profundidade + 1, profundidadeMaxima, membros);
                soma += colunaFilho;
                ++visitados;
                if (filhoTocaCiclo) tocaCiclo = true;
                cascata.arestas.push_back(ArestaCascata{filho, codigo, false});
            }
        }

        if (visitados == 0 || tocaCiclo) no.coluna = static_cast<double>(proximaColuna++);
        else no.coluna = soma / static_cast<double>(visitados);
        return {no.coluna, tocaCiclo};
    };

    auto processarRaiz = [&](int raiz) {
        int profundidadeMaxima = 0;
        std::vector<int> membros;
        int colunaInicial = proximaColuna;
        visitar(raiz, 0, profundidadeMaxima, membros);
        for (int codigo : membros) {
            NoCascata& no = cascata.nos[indiceDoCodigo[codigo]];
            no.linha = profundidadeMaxima - no.linha;
        }
        cascata.bacias.push_back(BaciaCascata{numBacias, raiz, static_cast<int>(membros.size()), colunaInicial,
                                               proximaColuna - colunaInicial, profundidadeMaxima + 1});
        ++numBacias;
        ++proximaColuna;
    };

    for (int raiz : raizesExplicitas) processarRaiz(raiz);
    for (int codigo = 1; codigo <= n; ++codigo) {
        if (!indiceDoCodigo.contains(codigo)) continue;
        if (!visitado[static_cast<size_t>(codigo)]) processarRaiz(codigo);
    }
    if (numBacias > 0) --proximaColuna;

    for (int codigo = 1; codigo <= n; ++codigo) {
        if (!indiceDoCodigo.contains(codigo)) continue;
        int alvo = alvoValido(usinas, codigo, usinas[static_cast<size_t>(codigo - 1)].desvio);
        if (alvo != 0) cascata.arestas.push_back(ArestaCascata{codigo, alvo, true});
    }

    cascata.num_colunas = proximaColuna;
    int linhaMaxima = -1;
    for (const NoCascata& no : cascata.nos) linhaMaxima = std::max(linhaMaxima, no.linha);
    cascata.num_linhas = linhaMaxima + 1;

    return cascata;
}

// Reposiciona as bacias em linhas, na ordem em que ja aparecem em c.bacias. A primeira bacia de
// cada linha sempre entra, mesmo que sozinha ja estoure largura_maxima, para nao travar em bacia
// muito larga; as demais so entram se sobrar espaco (mais 1 coluna de folga entre bacias).
Cascata empacotarBacias(const Cascata& c, int largura_maxima) {
    if (largura_maxima <= 0) return c;

    Cascata resultado = c;

    std::unordered_map<int, std::vector<size_t>> nosPorBacia;
    for (size_t i = 0; i < resultado.nos.size(); ++i) nosPorBacia[resultado.nos[i].bacia].push_back(i);

    int larguraAcumulada = 0;
    int deslocamentoLinha = 0;
    int alturaMaximaLinha = 0;
    bool primeiraDaLinha = true;
    int numColunas = 0;
    int numLinhas = 0;

    for (BaciaCascata& bacia : resultado.bacias) {
        bool cabe = primeiraDaLinha || (larguraAcumulada + bacia.largura + 1 <= largura_maxima);
        if (!cabe) {
            deslocamentoLinha += alturaMaximaLinha + 1;
            larguraAcumulada = 0;
            alturaMaximaLinha = 0;
            primeiraDaLinha = true;
        }

        int novaColunaInicial = larguraAcumulada + (primeiraDaLinha ? 0 : 1);
        double deltaColuna = static_cast<double>(novaColunaInicial - bacia.coluna_inicial);

        for (size_t indice : nosPorBacia[bacia.indice]) {
            resultado.nos[indice].coluna += deltaColuna;
            resultado.nos[indice].linha += deslocamentoLinha;
        }

        bacia.coluna_inicial = novaColunaInicial;
        larguraAcumulada = novaColunaInicial + bacia.largura;
        alturaMaximaLinha = std::max(alturaMaximaLinha, bacia.altura);
        numColunas = std::max(numColunas, larguraAcumulada);
        numLinhas = std::max(numLinhas, deslocamentoLinha + bacia.altura);
        primeiraDaLinha = false;
    }

    resultado.num_colunas = numColunas;
    resultado.num_linhas = numLinhas;
    return resultado;
}

namespace {

struct FaixaEmpacotada {
    int largura;
    int altura;
};

// Empacota as bacias de uma cascata em linhas dentro de uma faixa que comeca na linha linha_base,
// com as maiores a esquerda, e devolve a largura e a altura ocupadas. Mesma regra de quebra de
// empacotarBacias: a primeira bacia da linha sempre entra, as demais so se sobrar espaco.
FaixaEmpacotada empacotarFaixa(Cascata& parcial, int largura_maxima, int linha_base) {
    std::vector<size_t> ordem(parcial.bacias.size());
    std::iota(ordem.begin(), ordem.end(), size_t(0));
    std::stable_sort(ordem.begin(), ordem.end(), [&](size_t a, size_t b) {
        return parcial.bacias[a].num_usinas > parcial.bacias[b].num_usinas;
    });

    std::unordered_map<int, std::vector<size_t>> nosPorBacia;
    for (size_t i = 0; i < parcial.nos.size(); ++i) nosPorBacia[parcial.nos[i].bacia].push_back(i);

    int larguraAcumulada = 0;
    int deslocamentoLinha = 0;
    int alturaMaximaLinha = 0;
    bool primeiraDaLinha = true;
    FaixaEmpacotada faixa{0, 0};

    for (size_t i : ordem) {
        BaciaCascata& bacia = parcial.bacias[i];
        bool cabe = primeiraDaLinha || largura_maxima <= 0 ||
                    (larguraAcumulada + bacia.largura + 1 <= largura_maxima);
        if (!cabe) {
            deslocamentoLinha += alturaMaximaLinha + 1;
            larguraAcumulada = 0;
            alturaMaximaLinha = 0;
            primeiraDaLinha = true;
        }

        int novaColunaInicial = larguraAcumulada + (primeiraDaLinha ? 0 : 1);
        double deltaColuna = static_cast<double>(novaColunaInicial - bacia.coluna_inicial);
        int deltaLinha = linha_base + deslocamentoLinha;

        for (size_t indice : nosPorBacia[bacia.indice]) {
            parcial.nos[indice].coluna += deltaColuna;
            parcial.nos[indice].linha += deltaLinha;
        }

        bacia.coluna_inicial = novaColunaInicial;
        larguraAcumulada = novaColunaInicial + bacia.largura;
        alturaMaximaLinha = std::max(alturaMaximaLinha, bacia.altura);
        faixa.largura = std::max(faixa.largura, larguraAcumulada);
        faixa.altura = std::max(faixa.altura, deslocamentoLinha + bacia.altura);
        primeiraDaLinha = false;
    }

    return faixa;
}

}  // namespace

// Uma faixa horizontal por grupo, empilhadas na ordem crescente do codigo do grupo, com 3 linhas de
// folga entre faixas: e nesse espaco que quem desenha coloca o titulo da faixa de baixo, sem
// encostar na de cima. O grupo e por usina, nao por bacia: cada faixa e montada rodando
// montarCascata so sobre as usinas daquele grupo, entao uma usina cujo jusante esta em outro grupo
// vira raiz dentro da sua faixa e os grupos de meio de cascata ganham faixa propria. As arestas
// saem do grafo completo, com entre_grupos marcando as que atravessam faixas. Usina sem entrada em
// grupo_da_usina cai no grupo 0; nome ausente ou vazio vira "Sem grupo".
Cascata empacotarPorGrupo(const std::vector<UsinaHidr>& usinas, const std::map<int, int>& grupo_da_usina,
                          const std::map<int, std::string>& nome_do_grupo, int largura_maxima) {
    auto grupoDe = [&](int codigo) {
        auto it = grupo_da_usina.find(codigo);
        return it == grupo_da_usina.end() ? 0 : it->second;
    };

    std::map<int, std::vector<int>> usinasDoGrupo;
    for (size_t i = 0; i < usinas.size(); ++i) {
        if (usinas[i].vazia()) continue;
        int codigo = static_cast<int>(i) + 1;
        usinasDoGrupo[grupoDe(codigo)].push_back(codigo);
    }

    Cascata resultado;
    int linhaBase = 0;
    int numColunas = 0;

    for (const auto& [codigo_grupo, membros] : usinasDoGrupo) {
        std::vector<bool> manter(usinas.size() + 1, false);
        for (int codigo : membros) manter[static_cast<size_t>(codigo)] = true;
        std::vector<UsinaHidr> soDoGrupo = usinas;
        for (size_t i = 0; i < soDoGrupo.size(); ++i) {
            if (!manter[i + 1]) soDoGrupo[i].nome.clear();
        }

        Cascata parcial = montarCascata(soDoGrupo);
        FaixaEmpacotada faixa = empacotarFaixa(parcial, largura_maxima, linhaBase);

        int deslocamentoIndice = static_cast<int>(resultado.bacias.size());
        for (NoCascata& no : parcial.nos) {
            no.bacia += deslocamentoIndice;
            resultado.nos.push_back(no);
        }
        for (BaciaCascata& bacia : parcial.bacias) {
            bacia.indice += deslocamentoIndice;
            resultado.bacias.push_back(bacia);
        }

        auto it_nome = nome_do_grupo.find(codigo_grupo);
        bool tem_nome = it_nome != nome_do_grupo.end() && !it_nome->second.empty();
        resultado.grupos.push_back(GrupoCascata{codigo_grupo, tem_nome ? it_nome->second : std::string("Sem grupo"),
                                                0, linhaBase, faixa.largura, faixa.altura,
                                                static_cast<int>(membros.size())});
        numColunas = std::max(numColunas, faixa.largura);
        linhaBase += faixa.altura + 3;
    }

    for (ArestaCascata aresta : montarCascata(usinas).arestas) {
        aresta.entre_grupos = grupoDe(aresta.origem) != grupoDe(aresta.destino);
        resultado.arestas.push_back(aresta);
    }

    resultado.num_colunas = numColunas;
    resultado.num_linhas = resultado.grupos.empty() ? 0 : linhaBase - 3;
    return resultado;
}

// Extrai os nos e arestas internas da bacia cuja foz e codigo_foz, deslocando as colunas para a
// bacia comecar em zero (coluna_inicial passa a 0). Cascata retornada com nos.empty() se
// nenhuma bacia tiver essa foz (por exemplo, apos a foz deixar de existir no deck).
Cascata filtrarBacia(const Cascata& c, int codigo_foz) {
    Cascata resultado;

    const BaciaCascata* bacia = nullptr;
    for (const BaciaCascata& b : c.bacias) {
        if (b.codigo_foz == codigo_foz) {
            bacia = &b;
            break;
        }
    }
    if (bacia == nullptr) return resultado;

    std::unordered_map<int, bool> membro;
    for (const NoCascata& no : c.nos) {
        if (no.bacia != bacia->indice) continue;
        NoCascata copia = no;
        copia.coluna -= bacia->coluna_inicial;
        resultado.nos.push_back(copia);
        membro[no.codigo] = true;
    }
    for (const ArestaCascata& aresta : c.arestas) {
        if (membro.count(aresta.origem) && membro.count(aresta.destino)) resultado.arestas.push_back(aresta);
    }

    resultado.bacias = {*bacia};
    resultado.bacias[0].coluna_inicial = 0;
    resultado.num_colunas = bacia->largura;
    resultado.num_linhas = bacia->altura;
    return resultado;
}

// Sobe pelos jusantes validos ate a foz (com protecao contra ciclo, via marcacao de visitado) e
// desce recursivamente por quem aponta pra cada no ja incluido, cobrindo toda a arvore de
// contribuintes a montante alem do caminho a jusante.
std::vector<int> cascataDaUsina(const std::vector<UsinaHidr>& usinas, int codigo) {
    int n = static_cast<int>(usinas.size());
    if (codigo < 1 || codigo > n) return {};
    if (usinas[static_cast<size_t>(codigo - 1)].vazia()) return {};

    std::vector<bool> incluido(static_cast<size_t>(n) + 1, false);
    std::vector<int> resultado;

    std::function<void(int)> incluirMontante = [&](int alvo) {
        incluido[static_cast<size_t>(alvo)] = true;
        resultado.push_back(alvo);
        for (int outro = 1; outro <= n; ++outro) {
            if (incluido[static_cast<size_t>(outro)]) continue;
            if (usinas[static_cast<size_t>(outro - 1)].vazia()) continue;
            if (alvoValido(usinas, outro, usinas[static_cast<size_t>(outro - 1)].jusante) == alvo) incluirMontante(outro);
        }
    };
    incluirMontante(codigo);

    int atual = codigo;
    while (true) {
        int alvo = alvoValido(usinas, atual, usinas[static_cast<size_t>(atual - 1)].jusante);
        if (alvo == 0 || incluido[static_cast<size_t>(alvo)]) break;
        incluido[static_cast<size_t>(alvo)] = true;
        resultado.push_back(alvo);
        atual = alvo;
    }

    return resultado;
}
