#include "validacao_deck.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include "arquivo_hidr.h"
#include "catalogo_newave.h"
#include "dados_deck.h"
#include "deck_newave.h"
#include "layouts_newave.h"

namespace {
constexpr double TOLERANCIA = 1e-3;
constexpr int PAPEL_NOME = Qt::UserRole + 1;
constexpr int PAPEL_SECAO = Qt::UserRole + 2;
constexpr int PAPEL_REGISTRO = Qt::UserRole + 3;

QString nomeDoCadastro(Referencia r) {
    switch (r) {
    case Referencia::Submercado: return QStringLiteral("submercado");
    case Referencia::Ree: return QStringLiteral("REE");
    case Referencia::UsinaHidro: return QStringLiteral("usina hidráulica");
    case Referencia::UsinaTermica: return QStringLiteral("usina térmica");
    case Referencia::ClasseTermica: return QStringLiteral("classe térmica");
    case Referencia::Tecnologia: return QStringLiteral("tecnologia");
    case Referencia::Posto: return QStringLiteral("posto");
    case Referencia::Agrupamento: return QStringLiteral("agrupamento");
    case Referencia::ClasseGas: return QStringLiteral("classe de gás");
    default: return {};
    }
}

QString arquivoDoCadastro(Referencia r) {
    if (r == Referencia::Posto) return QStringLiteral("postos.dat");
    if (r == Referencia::Agrupamento) return QStringLiteral("agrint.dat");
    return QString::fromLatin1(fonteReferencia(r).arquivo);
}

std::optional<double> numero(const QString& texto) {
    bool ok = false;
    const double v = QString(texto).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    return ok ? std::optional<double>(v) : std::nullopt;
}

class Validacao {
public:
    Validacao(const DadosDeck& dados, const ArquivoHidr* hidr) : dados_(dados), hidr_(hidr) {}

    std::vector<ProblemaDeck> problemas;

    void problema(bool erro, const QString& nome, int s, int r, const QString& mensagem, const QString& regra) {
        problemas.push_back({erro, nome, s, r, mensagem, regra});
    }

    const ArquivoFixo* arquivo(const char* nome) const { return dados_.arquivo(QString::fromLatin1(nome)); }

    static int secao(const ArquivoFixo* a, const char* titulo) {
        if (!a) return -1;
        for (size_t s = 0; s < a->secoes().size(); ++s)
            if (a->secoes()[s].definicao.titulo == titulo) return static_cast<int>(s);
        return -1;
    }

    static int coluna(const ArquivoFixo* a, int s, const char* nome) {
        if (!a || s < 0) return -1;
        const auto& colunas = a->secoes()[static_cast<size_t>(s)].definicao.colunas;
        for (size_t c = 0; c < colunas.size(); ++c)
            if (colunas[c].nome == nome) return static_cast<int>(c);
        return -1;
    }

    static int registros(const ArquivoFixo* a, int s) { return a && s >= 0 ? static_cast<int>(a->secoes()[static_cast<size_t>(s)].linhas.size()) : 0; }

    static QString valor(const ArquivoFixo* a, int s, int r, int c) {
        return a && s >= 0 && c >= 0 ? QString::fromLatin1(a->valor(s, r, c).c_str()).trimmed() : QString();
    }

    std::optional<double> parametro(const char* titulo, const char* nome_coluna = nullptr) const {
        const ArquivoFixo* dger = arquivo("dger.dat");
        const int s = secao(dger, titulo);
        if (registros(dger, s) == 0) return std::nullopt;
        return numero(valor(dger, s, 0, nome_coluna ? coluna(dger, s, nome_coluna) : 0));
    }

    const std::set<QString>& codigos(Referencia r) {
        auto it = codigos_.find(r);
        if (it != codigos_.end()) return it->second;
        std::set<QString>& conjunto = codigos_[r];
        for (const OpcaoReferencia& o : dados_.opcoes(r)) conjunto.insert(DadosDeck::normalizarCodigo(o.codigo));
        return conjunto;
    }

    std::set<QString> valoresDaColuna(const char* nome, const char* titulo, const char* nome_coluna) const {
        std::set<QString> valores;
        const ArquivoFixo* a = arquivo(nome);
        const int s = secao(a, titulo);
        const int c = coluna(a, s, nome_coluna);
        for (int r = 0; r < registros(a, s); ++r) valores.insert(DadosDeck::normalizarCodigo(valor(a, s, r, c)));
        valores.erase(QString());
        return valores;
    }

    void regrasGerais();
    void configuracao();
    void dadosGerais();
    void sistema();
    void restricoes();
    void modificacoes();
    void capacidade();

private:
    const DadosDeck& dados_;
    const ArquivoHidr* hidr_;
    std::map<Referencia, std::set<QString>> codigos_;
};

// Todo campo numero deve ser numero; campo com valores listados no manual deve ter um deles; e codigo
// que aponta para outro cadastro (usina, REE, submercado, classe, tecnologia, posto, agrupamento,
// classe de gas) deve existir nele, como as definicoes dos campos indicam ("Numero do REE a que
// pertence a usina", manual 3.9, p. 62, e equivalentes). Codigo 0 e vazio sao "nenhum". A usina
// hidraulica do confhd.dat (jusante), do exph.dat e do re.dat precisa estar na configuracao (manual
// 3.9, p. 63; 3.13, p. 69; 3.33, p. 106); no ghmin.dat e no dsvagua.dat, fora dela e aviso, porque os
// dados so sao ignorados. Nos demais arquivos (polinomios, volumes de referencia), que trazem o
// cadastro inteiro, basta a usina existir no hidr.dat. Campo de uma linha que abre bloco conta uma
// vez.
void Validacao::regrasGerais() {
    const std::set<QString> configuracao_hidraulica = {QStringLiteral("confhd.dat"), QStringLiteral("exph.dat"), QStringLiteral("ghmin.dat"),
                                                       QStringLiteral("dsvagua.dat"), QStringLiteral("re.dat")};
    for (const ArquivoNewave& info : catalogoNewave()) {
        const ArquivoFixo* a = dados_.arquivo(info.nome_padrao);
        if (!a) continue;
        const QString regra = QStringLiteral("Manual %1").arg(info.secao_manual);
        std::set<std::pair<int, int>> vistos;
        for (int s = 0; s < static_cast<int>(a->secoes().size()); ++s) {
            const SecaoLida& sec = a->secoes()[static_cast<size_t>(s)];
            for (int c = 0; c < static_cast<int>(sec.definicao.colunas.size()); ++c) {
                const ColunaFixa& col = sec.definicao.colunas[static_cast<size_t>(c)];
                if (!ArquivoFixo::editavel(col)) continue;
                for (int r = 0; r < static_cast<int>(sec.linhas.size()); ++r) {
                    int linha = sec.linhas[static_cast<size_t>(r)];
                    if (col.contexto >= 0 && static_cast<size_t>(r) < sec.linhas_contexto.size() &&
                        static_cast<size_t>(col.contexto) < sec.linhas_contexto[static_cast<size_t>(r)].size())
                        linha = sec.linhas_contexto[static_cast<size_t>(r)][static_cast<size_t>(col.contexto)];
                    if (!vistos.insert({linha, s * 1000 + c}).second) continue;
                    const QString v = valor(a, s, r, c);
                    if (v.isEmpty()) continue;
                    const QString campo = QString::fromStdString(col.nome);
                    if ((col.tipo == TipoColunaFixa::Inteiro || col.tipo == TipoColunaFixa::Real) && !numero(v)) {
                        problema(true, info.nome_padrao, s, r, QStringLiteral("%1: \"%2\" não é número").arg(campo, v), regra);
                        continue;
                    }
                    const QString codigo = DadosDeck::normalizarCodigo(v);
                    if (!col.opcoes.empty() &&
                        std::none_of(col.opcoes.begin(), col.opcoes.end(), [&](const OpcaoFixa& o) { return QString::fromStdString(o.codigo) == codigo; }))
                        problema(true, info.nome_padrao, s, r, QStringLiteral("%1: %2 não é um dos valores do manual").arg(campo, v), regra);
                    if (col.referencia == Referencia::Nenhuma || codigo == QStringLiteral("0")) continue;
                    const bool da_configuracao = configuracao_hidraulica.count(info.nome_padrao) > 0;
                    if (col.referencia == Referencia::UsinaHidro && !da_configuracao) {
                        const auto n = numero(codigo);
                        const bool cadastrada = !hidr_ || (n && *n >= 1 && *n <= static_cast<double>(hidr_->usinas.size()) &&
                                                           !hidr_->usinas[static_cast<size_t>(*n - 1)].vazia());
                        if (!cadastrada)
                            problema(true, info.nome_padrao, s, r, QStringLiteral("%1: usina hidráulica %2 não está cadastrada no hidr.dat").arg(campo, codigo), regra);
                        continue;
                    }
                    if (codigos(col.referencia).count(codigo)) continue;
                    const bool erro = col.referencia != Referencia::UsinaHidro ||
                                      (info.nome_padrao != QStringLiteral("ghmin.dat") && info.nome_padrao != QStringLiteral("dsvagua.dat"));
                    problema(erro, info.nome_padrao, s, r,
                             QStringLiteral("%1: %2 %3 não está em %4").arg(campo, nomeDoCadastro(col.referencia), codigo, arquivoDoCadastro(col.referencia)),
                             regra);
                }
            }
        }
    }
}

// Configuracao hidraulica e termica: codigo da usina hidraulica igual ao registro dela no hidr.dat
// (manual 3.9, p. 62, e Anexo 2, pergunta 17, p. 277); usina EE ou NE sem cronograma de expansao fica
// sem maquinas ou sem potencia (3.9, p. 63; 3.15, p. 71); o term.dat tem um registro para cada usina
// do conft.dat (3.16, p. 71); conjunto adicionado no exph.dat no maximo igual ao numero de conjuntos do
// hidr.dat (3.13, p. 69).
void Validacao::configuracao() {
    const ArquivoFixo* confhd = arquivo("confhd.dat");
    const int sc = secao(confhd, "Usinas hidroelétricas na configuração");
    const std::set<QString> com_expansao_hidr = valoresDaColuna("exph.dat", "Entrada de unidades", "Usina");
    std::set<QString> exph = valoresDaColuna("exph.dat", "Usinas e enchimento de volume morto", "Usina");
    exph.insert(com_expansao_hidr.begin(), com_expansao_hidr.end());
    for (int r = 0; r < registros(confhd, sc); ++r) {
        const QString codigo = DadosDeck::normalizarCodigo(valor(confhd, sc, r, coluna(confhd, sc, "Usina")));
        const QString situacao = valor(confhd, sc, r, coluna(confhd, sc, "Situação"));
        const auto n = numero(codigo);
        if (hidr_ && n) {
            const int i = static_cast<int>(*n);
            if (i < 1 || i > static_cast<int>(hidr_->usinas.size()) || hidr_->usinas[static_cast<size_t>(i - 1)].vazia())
                problema(true, QStringLiteral("confhd.dat"), sc, r, QStringLiteral("Usina %1 não está cadastrada no hidr.dat").arg(codigo),
                         QStringLiteral("Manual 3.9, p. 62"));
        }
        if ((situacao == QStringLiteral("EE") || situacao == QStringLiteral("NE")) && !exph.count(codigo))
            problema(false, QStringLiteral("confhd.dat"), sc, r,
                     QStringLiteral("Usina %1 é %2 e não tem cronograma no exph.dat: fica sem máquinas").arg(codigo, situacao),
                     QStringLiteral("Manual 3.9, p. 63"));
    }

    const ArquivoFixo* conft = arquivo("conft.dat");
    const int st = secao(conft, "Usinas termoelétricas na configuração");
    const std::set<QString> expt = valoresDaColuna("expt.dat", "Modificações por período", "Usina");
    const std::set<QString> term = valoresDaColuna("term.dat", "Usinas termoelétricas", "Usina");
    std::set<QString> configuradas;
    for (int r = 0; r < registros(conft, st); ++r) {
        const QString codigo = DadosDeck::normalizarCodigo(valor(conft, st, r, coluna(conft, st, "Usina")));
        const QString situacao = valor(conft, st, r, coluna(conft, st, "Situação"));
        configuradas.insert(codigo);
        if ((situacao == QStringLiteral("EE") || situacao == QStringLiteral("NE")) && !expt.count(codigo))
            problema(false, QStringLiteral("conft.dat"), st, r,
                     QStringLiteral("Usina %1 é %2 e não tem dados no expt.dat: fica com potência e geração mínima zero").arg(codigo, situacao),
                     QStringLiteral("Manual 3.15, p. 71"));
        if (dados_.lido(QStringLiteral("term.dat")) && !term.count(codigo))
            problema(true, QStringLiteral("conft.dat"), st, r, QStringLiteral("Usina %1 não tem registro no term.dat").arg(codigo),
                     QStringLiteral("Manual 3.16, p. 71"));
    }
    const ArquivoFixo* termicas = arquivo("term.dat");
    const int sterm = secao(termicas, "Usinas termoelétricas");
    for (int r = 0; conft && r < registros(termicas, sterm); ++r) {
        const QString codigo = DadosDeck::normalizarCodigo(valor(termicas, sterm, r, coluna(termicas, sterm, "Usina")));
        if (!configuradas.count(codigo))
            problema(true, QStringLiteral("term.dat"), sterm, r, QStringLiteral("Usina %1 não está no conft.dat").arg(codigo),
                     QStringLiteral("Manual 3.16, p. 71"));
    }

    const ArquivoFixo* exph_arquivo = arquivo("exph.dat");
    const int se = secao(exph_arquivo, "Entrada de unidades");
    for (int r = 0; hidr_ && r < registros(exph_arquivo, se); ++r) {
        const auto usina = numero(valor(exph_arquivo, se, r, coluna(exph_arquivo, se, "Usina")));
        const auto conjunto = numero(valor(exph_arquivo, se, r, coluna(exph_arquivo, se, "Conjunto")));
        if (!usina || !conjunto || *usina < 1 || *usina > static_cast<double>(hidr_->usinas.size())) continue;
        const int conjuntos = hidr_->usinas[static_cast<size_t>(*usina - 1)].num_conjuntos;
        if (*conjunto > conjuntos)
            problema(true, QStringLiteral("exph.dat"), se, r,
                     QStringLiteral("Conjunto %1 maior que os %2 conjuntos da usina no hidr.dat").arg(*conjunto).arg(conjuntos),
                     QStringLiteral("Manual 3.13, p. 69"));
    }
}

// Parametros do dger.dat (manual 3.5): deltas de ZSUP e ZINF em [0;100] e numero de deltas de ZINF
// consecutivos entre zero e o maximo de iteracoes (p. 48); anos finais de estabilizacao da simulacao
// final entre zero e os da politica (p. 46); iteracao do teste de ZINF, se informada, nao menor que o
// minimo de iteracoes (p. 35); passo da reamostragem entre 1 e o maximo de iteracoes (p. 40);
// tendencia hidrologica sem misturar REE e posto entre politica e simulacao final (p. 48); e o
// tamanho do registro do historico (320 ou 600) igual ao do hidr.dat, do postos.dat e do vazoes.dat
// (p. 33, 3.10, 3.11 e 3.14).
void Validacao::dadosGerais() {
    if (!arquivo("dger.dat")) return;
    const QString nome = QStringLiteral("dger.dat");
    const ArquivoFixo* dger = arquivo("dger.dat");
    auto onde = [&](const char* titulo) { return secao(dger, titulo); };
    const auto maximo = parametro("Número máximo de iterações");
    for (const char* titulo : {"Delta de ZSUP (%)", "Delta de ZINF (%)"})
        if (const auto v = parametro(titulo); v && (*v < 0 || *v > 100))
            problema(true, nome, onde(titulo), 0, QStringLiteral("%1 deve estar entre 0 e 100").arg(QString::fromUtf8(titulo)),
                     QStringLiteral("Manual 3.5, p. 48"));
    if (const auto v = parametro("Deltas de ZINF consecutivos"); v && maximo && (*v < 0 || *v >= *maximo))
        problema(true, nome, onde("Deltas de ZINF consecutivos"), 0,
                 QStringLiteral("Deltas de ZINF consecutivos deve ser maior ou igual a zero e menor que o número máximo de iterações (%1)").arg(*maximo),
                 QStringLiteral("Manual 3.5, p. 48"));
    const auto finais_sf = parametro("Anos de estabilização finais na simulação final");
    const auto finais = parametro("Anos de estabilização finais na política");
    if (finais_sf && finais && (*finais_sf < 0 || *finais_sf > *finais))
        problema(true, nome, onde("Anos de estabilização finais na simulação final"), 0,
                 QStringLiteral("Os anos finais de estabilização na simulação final devem ficar entre 0 e os da política (%1)").arg(*finais),
                 QStringLiteral("Manual 3.5, p. 46"));
    const auto minimo = parametro("Número mínimo de iterações", "Mínimo de iterações");
    if (const auto teste = parametro("Número mínimo de iterações", "Iteração do teste de ZINF"); teste && minimo && *teste != 0 && *teste < *minimo)
        problema(true, nome, onde("Número mínimo de iterações"), 0,
                 QStringLiteral("A iteração do teste de ZINF deve ser maior ou igual ao número mínimo de iterações (%1)").arg(*minimo),
                 QStringLiteral("Manual 3.5, p. 35"));
    if (const auto passo = parametro("Reamostragem de cenários", "Passo"); passo && maximo && (*passo < 1 || *passo > *maximo))
        problema(true, nome, onde("Reamostragem de cenários"), 0,
                 QStringLiteral("O passo da reamostragem deve ficar entre 1 e o número máximo de iterações (%1)").arg(*maximo),
                 QStringLiteral("Manual 3.5, p. 40"));
    const auto politica = parametro("Tendência hidrológica", "Cálculo da política");
    const auto simulacao = parametro("Tendência hidrológica", "Simulação final");
    if (politica && simulacao && ((*politica == 1 && *simulacao == 2) || (*politica == 2 && *simulacao == 1)))
        problema(true, nome, onde("Tendência hidrológica"), 0,
                 QStringLiteral("A tendência hidrológica não pode ser por REE na política e por posto na simulação final, nem o contrário"),
                 QStringLiteral("Manual 3.5, p. 48"));
    if (const auto tamanho = parametro("Arquivo de vazões históricas", "Tamanho do registro")) {
        const int esperado = *tamanho == 1 ? 600 : 320;
        const QString regra = QStringLiteral("Manual 3.5, p. 33; 3.10, 3.11 e 3.14");
        if (hidr_ && static_cast<int>(hidr_->usinas.size()) != esperado)
            problema(true, QStringLiteral("hidr.dat"), -1, -1,
                     QStringLiteral("O hidr.dat tem %1 registros e o dger.dat indica %2").arg(hidr_->usinas.size()).arg(esperado), regra);
        if (const ArquivoBinario* postos = dados_.arquivoBinario(QStringLiteral("postos.dat")); postos && postos->registros() != esperado)
            problema(true, QStringLiteral("postos.dat"), -1, -1,
                     QStringLiteral("O postos.dat tem %1 postos e o dger.dat indica %2").arg(postos->registros()).arg(esperado), regra);
        if (const ArquivoBinario* vazoes = dados_.arquivoBinario(QStringLiteral("vazoes.dat")); vazoes && vazoes->tamanhoRegistro() / 4 != esperado)
            problema(true, QStringLiteral("vazoes.dat"), -1, -1,
                     QStringLiteral("O vazoes.dat tem %1 postos por mês e o dger.dat indica %2").arg(vazoes->tamanhoRegistro() / 4).arg(esperado), regra);
    }
}

// sistema.dat: a soma das profundidades dos patamares de deficit de cada submercado nao ficticio
// deve ser 1 (manual 3.7, p. 51; nos ficticios os campos sao ignorados, p. 52).
void Validacao::sistema() {
    const ArquivoFixo* a = arquivo("sistema.dat");
    const int sp = secao(a, "Patamares de déficit");
    const int sd = secao(a, "Custo do déficit");
    if (registros(a, sp) == 0) return;
    const auto patamares = numero(valor(a, sp, 0, 0));
    if (!patamares || *patamares < 1) return;
    for (int r = 0; r < registros(a, sd); ++r) {
        if (valor(a, sd, r, coluna(a, sd, "Fictício")) == QStringLiteral("1")) continue;
        double soma = 0.0;
        for (int p = 1; p <= std::min(4, static_cast<int>(*patamares)); ++p)
            soma += numero(valor(a, sd, r, coluna(a, sd, QStringLiteral("Profund. pat. %1 (p.u.)").arg(p).toUtf8().constData()))).value_or(0.0);
        if (std::abs(soma - 1.0) > TOLERANCIA)
            problema(true, QStringLiteral("sistema.dat"), sd, r,
                     QStringLiteral("As profundidades dos patamares de déficit do submercado %1 somam %2, e não 1").arg(valor(a, sd, r, 0)).arg(soma, 0, 'f', 3),
                     QStringLiteral("Manual 3.7, p. 51"));
    }
}

// Penalidades e restricoes: penalidade zero nao e permitida (manual 3.24, p. 82); coeficiente de
// agrupamento maior que zero, data inicial nao posterior a final, datas em branco interrompem a
// execucao e limite negativo so -1 (3.26, p. 91 a 93); geracao hidraulica minima maior que zero
// (3.29, p. 97); restricao eletrica com 1 a 10 usinas do mesmo REE, cada usina em uma restricao so,
// limite de restricao declarada, com datas e valor maior que zero (3.33, p. 106 a 108).
void Validacao::restricoes() {
    const ArquivoFixo* penalid = arquivo("penalid.dat");
    const int sp = secao(penalid, "Penalidades");
    for (int r = 0; r < registros(penalid, sp); ++r)
        for (const char* nome : {"Penalidade (R$/MWh)", "Penalidade 2º pat. (R$/MWh)", "Penalidade ((R$/hm³)(mês/h))", "Penalidade 2º pat. ((R$/hm³)(mês/h))"})
            if (const auto v = numero(valor(penalid, sp, r, coluna(penalid, sp, nome))); v && *v == 0.0)
                problema(true, QStringLiteral("penalid.dat"), sp, r, QStringLiteral("Penalidade igual a zero não é permitida"), QStringLiteral("Manual 3.24, p. 82"));

    auto data = [](const QString& mes, const QString& ano) -> std::optional<int> {
        const auto m = numero(mes);
        const auto a = numero(ano);
        if (!m || !a) return std::nullopt;
        return static_cast<int>(*a) * 12 + static_cast<int>(*m);
    };
    auto conferirPeriodo = [&](const char* nome_arquivo, const ArquivoFixo* a, int s, int r, const QString& regra) {
        const QString mi = valor(a, s, r, coluna(a, s, "Mês início"));
        const QString ai = valor(a, s, r, coluna(a, s, "Ano início"));
        const QString mf = valor(a, s, r, coluna(a, s, "Mês fim"));
        const QString af = valor(a, s, r, coluna(a, s, "Ano fim"));
        const QString arquivo = QString::fromLatin1(nome_arquivo);
        if (mi.isEmpty() && ai.isEmpty() && mf.isEmpty() && af.isEmpty()) {
            problema(true, arquivo, s, r, QStringLiteral("Datas inicial e final em branco interrompem a execução"), regra);
            return;
        }
        const auto inicio = data(mi, ai);
        const auto fim = data(mf, af);
        if (inicio && fim && *inicio > *fim) problema(true, arquivo, s, r, QStringLiteral("A data inicial é posterior à final"), regra);
    };

    const ArquivoFixo* agrint = arquivo("agrint.dat");
    const int sa = secao(agrint, "Agrupamentos de interligações");
    for (int r = 0; r < registros(agrint, sa); ++r)
        if (const auto v = numero(valor(agrint, sa, r, coluna(agrint, sa, "Coeficiente"))); v && *v <= 0)
            problema(true, QStringLiteral("agrint.dat"), sa, r, QStringLiteral("O coeficiente da interligação deve ser maior que zero"),
                     QStringLiteral("Manual 3.26, p. 91"));
    const int sl = secao(agrint, "Limites dos agrupamentos");
    for (int r = 0; r < registros(agrint, sl); ++r) {
        conferirPeriodo("agrint.dat", agrint, sl, r, QStringLiteral("Manual 3.26, p. 92 e 93"));
        const auto& colunas = agrint->secoes()[static_cast<size_t>(sl)].definicao.colunas;
        for (int c = 0; c < static_cast<int>(colunas.size()); ++c) {
            if (colunas[static_cast<size_t>(c)].nome.rfind("Limite pat.", 0) != 0) continue;
            if (const auto v = numero(valor(agrint, sl, r, c)); v && *v < 0 && *v != -1)
                problema(true, QStringLiteral("agrint.dat"), sl, r,
                         QStringLiteral("%1: limite negativo só pode ser -1 (sem restrição)").arg(QString::fromStdString(colunas[static_cast<size_t>(c)].nome)),
                         QStringLiteral("Manual 3.26, p. 93"));
        }
    }

    const ArquivoFixo* ghmin = arquivo("ghmin.dat");
    const int sg = secao(ghmin, "Gerações hidráulicas mínimas");
    for (int r = 0; r < registros(ghmin, sg); ++r)
        if (const auto v = numero(valor(ghmin, sg, r, coluna(ghmin, sg, "GH mín. (MWmédio)"))); v && *v <= 0)
            problema(true, QStringLiteral("ghmin.dat"), sg, r, QStringLiteral("A geração hidráulica mínima deve ser maior que zero"),
                     QStringLiteral("Manual 3.29, p. 97"));

    const ArquivoFixo* re = arquivo("re.dat");
    const int sr = secao(re, "Restrições elétricas");
    const ArquivoFixo* confhd = arquivo("confhd.dat");
    const int sc = secao(confhd, "Usinas hidroelétricas na configuração");
    std::map<QString, QString> ree_da_usina;
    for (int r = 0; r < registros(confhd, sc); ++r)
        ree_da_usina[DadosDeck::normalizarCodigo(valor(confhd, sc, r, coluna(confhd, sc, "Usina")))] =
            DadosDeck::normalizarCodigo(valor(confhd, sc, r, coluna(confhd, sc, "REE")));
    std::set<QString> restricoes_declaradas;
    std::map<QString, QString> restricao_da_usina;
    const QString regra_re = QStringLiteral("Manual 3.33, p. 106");
    for (int r = 0; r < registros(re, sr); ++r) {
        const QString restricao = DadosDeck::normalizarCodigo(valor(re, sr, r, coluna(re, sr, "Restrição")));
        restricoes_declaradas.insert(restricao);
        std::set<QString> rees;
        int usinas = 0;
        for (int k = 1; k <= 10; ++k) {
            const QString usina = DadosDeck::normalizarCodigo(valor(re, sr, r, coluna(re, sr, QStringLiteral("Usina %1").arg(k).toUtf8().constData())));
            if (usina.isEmpty() || usina == QStringLiteral("0")) continue;
            ++usinas;
            if (ree_da_usina.count(usina)) rees.insert(ree_da_usina[usina]);
            const auto [it, nova] = restricao_da_usina.try_emplace(usina, restricao);
            if (!nova)
                problema(true, QStringLiteral("re.dat"), sr, r,
                         QStringLiteral("A usina %1 já está na restrição %2: cada usina só pode estar em uma").arg(usina, it->second), regra_re);
        }
        if (usinas == 0) problema(true, QStringLiteral("re.dat"), sr, r, QStringLiteral("A restrição não tem usinas"), regra_re);
        if (rees.size() > 1) problema(true, QStringLiteral("re.dat"), sr, r, QStringLiteral("As usinas da restrição são de REEs diferentes"), regra_re);
    }
    const int slim = secao(re, "Limites das restrições");
    for (int r = 0; r < registros(re, slim); ++r) {
        const QString restricao = DadosDeck::normalizarCodigo(valor(re, slim, r, coluna(re, slim, "Restrição")));
        if (!restricoes_declaradas.count(restricao))
            problema(true, QStringLiteral("re.dat"), slim, r, QStringLiteral("A restrição %1 não foi declarada no primeiro bloco").arg(restricao),
                     QStringLiteral("Manual 3.33, p. 107"));
        conferirPeriodo("re.dat", re, slim, r, QStringLiteral("Manual 3.33, p. 107"));
        if (const auto v = numero(valor(re, slim, r, coluna(re, slim, "Limite (MWmédio)"))); v && *v <= 0)
            problema(true, QStringLiteral("re.dat"), slim, r, QStringLiteral("O limite deve ser maior que zero"), QStringLiteral("Manual 3.33, p. 108"));
    }
}

// modif.dat: os avisos da leitura (bloco repetido de uma usina, registro fora de bloco; manual 3.12,
// p. 64), usina fora da configuracao (codigo do cadastro, 3.12, p. 64), TURBMAXT, TURBMINT e VAZMAXT
// com um valor ou um por patamar de carga (p. 67) e VAZMIN com dois valores com o segundo menor que o
// primeiro (p. 68).
void Validacao::modificacoes() {
    const QString nome = QStringLiteral("modif.dat");
    if (!dados_.lido(nome)) return;
    const ResultadoModif modif = interpretarModif(dados_.texto(nome).toLatin1().toStdString());
    for (const std::string& aviso : modif.avisos) problema(false, nome, -1, -1, QString::fromStdString(aviso), QStringLiteral("Manual 3.12, p. 64"));
    const int patamares = dados_.numeroPatamaresDeCarga();
    const std::set<QString>& configuradas = codigos(Referencia::UsinaHidro);
    for (const BlocoModif& bloco : modif.blocos) {
        if (!configuradas.empty() && !configuradas.count(QString::number(bloco.usina)))
            problema(true, nome, -1, -1, QStringLiteral("Linha %1: a usina %2 não está no confhd.dat").arg(bloco.linha).arg(bloco.usina),
                     QStringLiteral("Manual 3.12, p. 64"));
        for (const RegistroModif& registro : bloco.registros) {
            const QString chave = QString::fromStdString(registro.palavra_chave).toUpper();
            const int valores = static_cast<int>(registro.valores.size());
            if ((chave == QStringLiteral("TURBMAXT") || chave == QStringLiteral("TURBMINT") || chave == QStringLiteral("VAZMAXT")) && patamares > 0 &&
                valores - 2 != 1 && valores - 2 != patamares)
                problema(true, nome, -1, -1,
                         QStringLiteral("Linha %1: %2 da usina %3 deve ter 1 valor ou %4 (um por patamar), e tem %5")
                             .arg(registro.linha)
                             .arg(chave)
                             .arg(bloco.usina)
                             .arg(patamares)
                             .arg(valores - 2),
                         QStringLiteral("Manual 3.12, p. 67"));
            if (chave == QStringLiteral("VAZMIN") && valores == 2) {
                const auto primeiro = numero(QString::fromStdString(registro.valores[0]));
                const auto segundo = numero(QString::fromStdString(registro.valores[1]));
                if (primeiro && segundo && *segundo >= *primeiro)
                    problema(true, nome, -1, -1,
                             QStringLiteral("Linha %1: no VAZMIN da usina %2, o segundo valor deve ser menor que o primeiro").arg(registro.linha).arg(bloco.usina),
                             QStringLiteral("Manual 3.12, p. 68"));
            }
        }
    }
}

// Capacidade do programa (manual, capitulo 9, p. 266 e 267): ate 30 anos de planejamento, 15 REEs, 15
// submercados (4 ficticios), 330 usinas hidraulicas, 300 termicas, 5 patamares de carga e 4 de
// deficit.
void Validacao::capacidade() {
    const QString regra = QStringLiteral("Manual, capacidade do programa, p. 266 e 267");
    auto limite = [&](const QString& nome, int quantidade, int maximo, const QString& o_que) {
        if (quantidade > maximo) problema(true, nome, -1, -1, QStringLiteral("%1: %2, acima do máximo de %3").arg(o_que).arg(quantidade).arg(maximo), regra);
    };
    if (const auto anos = parametro("Número de anos do estudo")) limite(QStringLiteral("dger.dat"), static_cast<int>(*anos), 30, QStringLiteral("Anos de planejamento"));
    const ArquivoFixo* ree = arquivo("ree.dat");
    limite(QStringLiteral("ree.dat"), registros(ree, secao(ree, "REEs")), 15, QStringLiteral("REEs"));
    const ArquivoFixo* sist = arquivo("sistema.dat");
    const int sd = secao(sist, "Custo do déficit");
    int ficticios = 0;
    for (int r = 0; r < registros(sist, sd); ++r) ficticios += valor(sist, sd, r, coluna(sist, sd, "Fictício")) == QStringLiteral("1");
    limite(QStringLiteral("sistema.dat"), registros(sist, sd) - ficticios, 15, QStringLiteral("Submercados"));
    limite(QStringLiteral("sistema.dat"), ficticios, 4, QStringLiteral("Submercados fictícios"));
    if (registros(sist, 0) > 0)
        if (const auto deficit = numero(valor(sist, 0, 0, 0))) limite(QStringLiteral("sistema.dat"), static_cast<int>(*deficit), 4, QStringLiteral("Patamares de déficit"));
    const ArquivoFixo* confhd = arquivo("confhd.dat");
    limite(QStringLiteral("confhd.dat"), registros(confhd, 0), 330, QStringLiteral("Usinas hidráulicas"));
    const ArquivoFixo* conft = arquivo("conft.dat");
    limite(QStringLiteral("conft.dat"), registros(conft, 0), 300, QStringLiteral("Usinas térmicas"));
    limite(QStringLiteral("patamar.dat"), dados_.numeroPatamaresDeCarga(), 5, QStringLiteral("Patamares de carga"));
}
}  // namespace

// Conferencia do deck pelas regras do manual do NEWAVE, cada uma com a secao e a pagina de onde vem.
// Erro e o que o manual diz que nao e permitido ou que e obrigatorio; aviso, o que o NEWAVE ignora ou
// trata de outro jeito. Os arquivos que o deck nao tem ficam de fora.
std::vector<ProblemaDeck> validarDeck(const DadosDeck& dados, const ArquivoHidr* hidr) {
    Validacao v(dados, hidr);
    v.regrasGerais();
    v.configuracao();
    v.dadosGerais();
    v.sistema();
    v.restricoes();
    v.modificacoes();
    v.capacidade();
    return std::move(v.problemas);
}

// Lista dos problemas do deck, com o tipo, o arquivo, o registro e a regra do manual; um duplo clique
// leva ao registro.
DialogoValidacao::DialogoValidacao(const DadosDeck* dados, const ArquivoHidr* hidr, QWidget* parent)
    : QDialog(parent), dados_(dados), hidr_(hidr) {
    setWindowTitle(QStringLiteral("Validação do deck"));
    resize(1000, 560);
    auto* v = new QVBoxLayout(this);
    auto* linha = new QHBoxLayout;
    resumo_ = new QLabel(this);
    auto* botao = new QPushButton(QStringLiteral("Validar de novo"), this);
    linha->addWidget(resumo_, 1);
    linha->addWidget(botao);
    v->addLayout(linha);
    lista_ = new QTreeView(this);
    itens_ = new QStandardItemModel(this);
    lista_->setModel(itens_);
    lista_->setRootIsDecorated(false);
    lista_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lista_->setUniformRowHeights(true);
    lista_->setSortingEnabled(true);
    v->addWidget(lista_, 1);
    connect(botao, &QPushButton::clicked, this, &DialogoValidacao::validar);
    connect(lista_, &QTreeView::activated, this, &DialogoValidacao::escolher);
    validar();
}

void DialogoValidacao::validar() {
    itens_->clear();
    itens_->setHorizontalHeaderLabels({QStringLiteral("Tipo"), QStringLiteral("Arquivo"), QStringLiteral("Onde"), QStringLiteral("Problema"), QStringLiteral("Regra")});
    const std::vector<ProblemaDeck> problemas = validarDeck(*dados_, hidr_);
    int erros = 0;
    for (const ProblemaDeck& p : problemas) {
        erros += p.erro;
        auto* tipo = new QStandardItem(p.erro ? QStringLiteral("Erro") : QStringLiteral("Aviso"));
        tipo->setData(p.nome_padrao, PAPEL_NOME);
        tipo->setData(p.secao, PAPEL_SECAO);
        tipo->setData(p.registro, PAPEL_REGISTRO);
        QString onde;
        if (const ArquivoFixo* a = dados_->arquivo(p.nome_padrao); a && p.secao >= 0 && p.secao < static_cast<int>(a->secoes().size()))
            onde = QStringLiteral("%1 · registro %2").arg(QString::fromStdString(a->secoes()[static_cast<size_t>(p.secao)].definicao.titulo)).arg(p.registro + 1);
        itens_->appendRow({tipo, new QStandardItem(dados_->nomeNoDeck(p.nome_padrao)), new QStandardItem(onde), new QStandardItem(p.mensagem),
                           new QStandardItem(p.regra)});
    }
    resumo_->setText(problemas.empty() ? QStringLiteral("Nenhum problema encontrado pelas regras do manual")
                                       : QStringLiteral("%1 erros e %2 avisos").arg(erros).arg(static_cast<int>(problemas.size()) - erros));
    for (int c = 0; c < 3; ++c) lista_->resizeColumnToContents(c);
    lista_->setColumnWidth(3, 480);
}

void DialogoValidacao::escolher(const QModelIndex& indice) {
    const QModelIndex primeira = indice.siblingAtColumn(0);
    const QString nome = primeira.data(PAPEL_NOME).toString();
    if (!nome.isEmpty()) emit escolhido(nome, primeira.data(PAPEL_SECAO).toInt(), primeira.data(PAPEL_REGISTRO).toInt());
}
