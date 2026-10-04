#include "layouts_newave.h"
#include <map>
#include <string>

namespace {
using T = TipoColunaFixa;

ColunaFixa inteiro(const char* nome, int inicio, int fim) { return {nome, inicio, fim, T::Inteiro, 0}; }
ColunaFixa real(const char* nome, int inicio, int fim, int decimais) { return {nome, inicio, fim, T::Real, decimais}; }
ColunaFixa texto(const char* nome, int inicio, int fim) { return {nome, inicio, fim, T::Texto, 0}; }
ColunaFixa ordinal(const char* nome, int contexto) { return {nome, 0, 0, T::Ordinal, 0, contexto}; }
ColunaFixa grupo(const char* nome, int contexto) { return {nome, 0, 0, T::Grupo, 0, contexto}; }

ColunaFixa deContexto(ColunaFixa coluna, int contexto) {
    coluna.contexto = contexto;
    return coluna;
}

// Coluna do patamar k de carga (numero de patamares no bloco 1 do patamar.dat) ou de deficit (bloco 1
// do sistema.dat): some da tabela quando o deck tem menos patamares que k.
ColunaFixa doPatamar(ColunaFixa coluna, Patamares tipo, int k) {
    coluna.patamares = tipo;
    coluna.patamar = k;
    return coluna;
}
// Coluna de codigo com valores definidos no manual, mostrada e editada pela descricao de cada um.
ColunaFixa comOpcoes(ColunaFixa coluna, std::vector<OpcaoFixa> opcoes) {
    coluna.opcoes = std::move(opcoes);
    return coluna;
}

// Codigos 0 e 1 que o manual usa em muitas flags.
const std::vector<OpcaoFixa> CONSIDERA = {{"0", "Não considera"}, {"1", "Considera"}};
const std::vector<OpcaoFixa> IMPRIME = {{"0", "Não imprime"}, {"1", "Imprime"}};
const std::vector<OpcaoFixa> REPRESENTA = {{"0", "Não representa"}, {"1", "Representa"}};
const std::vector<OpcaoFixa> NAO_SIM = {{"0", "Não"}, {"1", "Sim"}};
const std::vector<OpcaoFixa> APLICA_SAR = {{"0", "Sem aplicação da SAR"}, {"1", "Com aplicação da SAR"}};

// Coluna com o codigo de um item de outro cadastro do deck (submercado, REE, usina...), mostrada
// e editada pela lista de nomes desse cadastro.
ColunaFixa ref(ColunaFixa coluna, Referencia referencia) {
    coluna.referencia = referencia;
    return coluna;
}
ColunaFixa deCarga(ColunaFixa coluna, int k) { return doPatamar(std::move(coluna), Patamares::Carga, k); }
ColunaFixa deDeficit(ColunaFixa coluna, int k) { return doPatamar(std::move(coluna), Patamares::Deficit, k); }

FiltroLinha preenchido(int inicio, int fim) { return {inicio, fim, TesteFiltro::Preenchido, {}}; }
FiltroLinha vazio(int inicio, int fim) { return {inicio, fim, TesteFiltro::Vazio, {}}; }
FiltroLinha igual(int inicio, int fim, std::vector<std::string> valores) { return {inicio, fim, TesteFiltro::Igual, std::move(valores)}; }
FiltroLinha diferente(int inicio, int fim, std::vector<std::string> valores) {
    return {inicio, fim, TesteFiltro::Diferente, std::move(valores)};
}

// conft.dat, manual do NEWAVE 30.0.2, secao 3.15: dois registros de comentario e um registro por
// usina, campos 1 a 7.
LayoutArquivoFixo conft() {
    return {"3.15",
            {{"Usinas termoelétricas na configuração",
              2,
              "",
              {inteiro("Usina", 2, 5), texto("Nome", 7, 18), ref(inteiro("Submercado", 22, 25), Referencia::Submercado),
               comOpcoes(texto("Situação", 31, 32), {{"EX", "Existente"},
                                                     {"EE", "Existente, com expansão"},
                                                     {"NE", "Não existente, com expansão"},
                                                     {"NC", "Não considerada"}}),
               ref(inteiro("Classe", 36, 39), Referencia::ClasseTermica),
               ref(inteiro("Tecnologia", 41, 43), Referencia::Tecnologia), ref(inteiro("Classe de gás", 45, 48), Referencia::ClasseGas)},
              0}}};
}

// term.dat, secao 3.16: dois registros de comentario e um registro por usina, campos 1 a 19. Os
// campos 7 a 18 sao a geracao minima de cada mes dos anos de manutencao, de 7 em 7 colunas a partir
// da 46; o 19 e a dos demais anos.
LayoutArquivoFixo term() {
    SecaoFixa usinas{"Usinas termoelétricas", 2, "",
                     {inteiro("Usina", 2, 4), texto("Nome", 6, 17), real("Capacidade (MW)", 20, 24, 0),
                      real("FC máx. (%)", 26, 29, 0), real("TEIF (%)", 32, 37, 2), real("IP demais anos (%)", 39, 44, 2)},
                     0};
    static const char* meses[] = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};
    for (int m = 0; m < 12; ++m) {
        std::string nome = std::string("GT mín. ") + meses[m] + " (MWmês)";
        usinas.colunas.push_back({nome, 46 + 7 * m, 51 + 7 * m, T::Real, 2});
    }
    usinas.colunas.push_back(real("GT mín. demais anos (MWmês)", 130, 135, 2));
    return {"3.16", {usinas}};
}

// expt.dat, secao 3.17: dois registros de comentario e um registro por modificacao, campos 1 a 7.
LayoutArquivoFixo expt() {
    return {"3.17",
            {{"Modificações por período", 2, "",
              {ref(inteiro("Usina", 1, 4), Referencia::UsinaTermica), texto("Tipo", 6, 10), real("Valor", 12, 19, 2),
               inteiro("Mês início", 21, 22), inteiro("Ano início", 24, 27), inteiro("Mês fim", 29, 30), inteiro("Ano fim", 32, 35)},
              0}}};
}

// clast.dat, secao 3.18: dois registros de comentario, os registros tipo 1 (um por classe, com um
// custo por ano de planejamento a cada 8 colunas a partir da 31) ate o 9999 no campo 1, mais dois
// registros de comentario e os registros tipo 2 (modificacoes de custo).
LayoutArquivoFixo clast() {
    return {"3.18",
            {{"Custos das classes", 2, "9999",
              {inteiro("Classe", 2, 5), texto("Nome", 7, 18), texto("Combustível", 20, 29), real("Custo ano", 31, 37, 2)},
              8},
             {"Modificações de custo", 2, "",
              {ref(inteiro("Classe", 2, 5), Referencia::ClasseTermica), real("Custo ($/MWh)", 9, 15, 2),
               inteiro("Mês início", 18, 19), inteiro("Ano início", 21, 24), inteiro("Mês fim", 27, 28), inteiro("Ano fim", 30, 33)},
              0}}};
}

// manutt.dat, secao 3.19: dois registros de comentario e um registro por manutencao; dos 13 campos
// so os 6 que o programa le tem colunas no manual (usina, data de inicio ddmmaaaa, duracao e
// potencia).
LayoutArquivoFixo manutt() {
    return {"3.19",
            {{"Manutenções programadas", 2, "",
              {ref(inteiro("Usina", 18, 20), Referencia::UsinaTermica), inteiro("Dia início", 41, 42),
               inteiro("Mês início", 43, 44), inteiro("Ano início", 45, 48), inteiro("Duração (dias)", 50, 52),
               real("Potência (MW)", 56, 62, 2)},
              0}}};
}

// arquivos.dat, manual do NEWAVE 30.0.2, secao 3.3: um nome de arquivo por registro, em ordem fixa,
// sem comentarios nem terminador; as colunas 1 a 30 sao so orientacao e o programa as ignora. O
// manual da o nome em A12 (colunas 31 a 42), mas decks atuais trazem nomes maiores
// (volref_saz.dat), lidos pelo NEWAVE; o campo vai ate a coluna 80.
LayoutArquivoFixo arquivos() {
    return {"3.3", {{.titulo = "Arquivos do caso", .colunas = {texto("Descrição", 1, 28), texto("Arquivo", 31, 80)}}}};
}

// dger.dat, manual do NEWAVE 30.0.2, secao 3.5: 102 registros em ordem fixa, um parametro por
// linha. O registro 1 e o nome do caso (colunas 1 a 80); nos demais as colunas 1 a 21 sao rotulo
// ignorado pelo programa e os valores comecam na coluna 22. O registro 23 e comentario obrigatorio
// que orienta o registro 24 (volume inicial por REE, um valor a cada 7 colunas); o registro 58 traz
// mes, ano e um volume por REE a cada 7 colunas a partir da 33. Cada parametro e uma secao de um
// registro, lida pela posicao da linha, como o programa le. No formulario, os parametros aparecem
// agrupados por tema em abas; o agrupamento e so de apresentacao.
LayoutArquivoFixo dger() {
    return {
        "3.5",
        {{.titulo = "Nome do caso", .colunas = {texto("Valor", 1, 80)}, .max_registros = 1},
         {.titulo = "Tipo de execução",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25),
                                {{"0", "Só executa a simulação final"},
                                 {"1", "Rodada completa"},
                                 {"2", "Só gera o arquivo único de cortes e/ou apaga os arquivos de cortes"}})},
          .max_registros = 1},
         {.titulo = "Duração do período (meses)", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Número de anos do estudo", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Mês de início do período pré", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Mês de início do estudo", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Ano de início do estudo", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Anos de estabilização iniciais", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Anos de estabilização finais na política", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Anos de estabilização finais na simulação final", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Imprime características das usinas",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), IMPRIME)},
          .max_registros = 1},
         {.titulo = "Imprime dados de carga", .colunas = {comOpcoes(inteiro("Valor", 22, 25), IMPRIME)}, .max_registros = 1},
         {.titulo = "Imprime energias afluentes históricas",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), IMPRIME)},
          .max_registros = 1},
         {.titulo = "Imprime parâmetros do modelo estocástico",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), IMPRIME)},
          .max_registros = 1},
         {.titulo = "Imprime parâmetros dos REEs", .colunas = {comOpcoes(inteiro("Valor", 22, 25), IMPRIME)}, .max_registros = 1},
         {.titulo = "Número máximo de iterações", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Número de simulações forward", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Aberturas da backward",
          .colunas = {inteiro("Número de aberturas", 22, 25), comOpcoes(inteiro("Variável por período", 27, 30), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Número de séries sintéticas", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Ordem máxima do PAR(p)", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Arquivo de vazões históricas",
          .colunas = {inteiro("Ano inicial", 22, 25),
                      comOpcoes(inteiro("Tamanho do registro", 29, 29), {{"0", "320 palavras"}, {"1", "600 palavras"}})},
          .max_registros = 1},
         {.titulo = "Cálculo do armazenamento inicial",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25),
                                {{"0", "Volume inicial do dger.dat"}, {"1", "Volume inicial do confhd.dat"}})},
          .max_registros = 1},
         {.titulo = "Volume inicial por REE (%)",
          .linhas_cabecalho = 1,
          .colunas = {real("REE", 22, 26, 1)},
          .passo_repeticao = 7,
          .max_registros = 1},
         {.titulo = "Probabilidade do intervalo de confiança (%)", .colunas = {real("Valor", 22, 26, 1)}, .max_registros = 1},
         {.titulo = "Taxa de desconto anual (%)", .colunas = {real("Valor", 22, 26, 1)}, .max_registros = 1},
         {.titulo = "Simulação final",
          .colunas = {comOpcoes(inteiro("Tipo", 22, 25), {{"0", "Não simula"},
                                                          {"1", "Séries sintéticas"},
                                                          {"2", "Série histórica"},
                                                          {"3", "Consistência de dados"}}),
                      comOpcoes(texto("Representação", 29, 29), {{"0", "Agregada por REE"}, {"1", "Individualizada"}})},
          .max_registros = 1},
         {.titulo = "Impressão dos resultados",
          .colunas = {comOpcoes(
              inteiro("Valor", 22, 25),
              {{"0", "Não imprime"}, {"1", "Simulação final"}, {"2", "Simulação final e cálculo da política"}})},
          .max_registros = 1},
         {.titulo = "Impressão dos riscos de déficit",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Só na convergência final"}, {"1", "Em todas as iterações"}})},
          .max_registros = 1},
         {.titulo = "Intervalo de séries com relatório detalhado", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Número mínimo de iterações",
          .colunas = {inteiro("Mínimo de iterações", 22, 25), inteiro("Iteração do teste de ZINF", 29, 29)},
          .max_registros = 1},
         {.titulo = "Corte de carga preventivo",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Não adota CCP"},
                                                           {"1", "Adota CCP na simulação final"},
                                                           {"2", "Não adota CCP e imprime novos custos de déficit"}})},
          .max_registros = 1},
         {.titulo = "Anos de manutenção térmica", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Tendência hidrológica",
          .colunas =
              {comOpcoes(inteiro("Cálculo da política", 22, 25),
                         {{"0", "Não lê tendência hidrológica"}, {"1", "Tendência por REE"}, {"2", "Tendência por posto"}}),
               comOpcoes(inteiro("Simulação final", 27, 30),
                         {{"0", "Não lê tendência hidrológica"}, {"1", "Tendência por REE"}, {"2", "Tendência por posto"}})},
          .max_registros = 1},
         {.titulo = "Restrições de Itaipu", .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)}, .max_registros = 1},
         {.titulo = "Bid de demanda", .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)}, .max_registros = 1},
         {.titulo = "Perdas na geração e transmissão",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "El Niño", .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)}, .max_registros = 1},
         {.titulo = "Índice ENSO", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Duração dos patamares",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Sazonal"}, {"1", "Variável por ano"}})},
          .max_registros = 1},
         {.titulo = "Desvio de água", .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)}, .max_registros = 1},
         {.titulo = "Energia de desvio de água",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Constante"}, {"1", "Variável com o armazenamento"}})},
          .max_registros = 1},
         {.titulo = "Curva de segurança",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25),
                                {{"0", "Não considera (usa o VMINT)"}, {"1", "Curva de aversão a risco / VMINP"}})},
          .max_registros = 1},
         {.titulo = "Geração de cenários de afluências",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Resíduos iguais, compensação na backward e forward"},
                                                           {"1", "Compensação na backward"},
                                                           {"2", "Compensação na backward e forward"}})},
          .max_registros = 1},
         {.titulo = "Profundidades do risco de déficit",
          .colunas = {real("Primeira (%)", 22, 25, 0), real("Segunda (%)", 28, 31, 0)},
          .max_registros = 1},
         {.titulo = "Iterações para a simulação final", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Agrupamento livre de intercâmbios",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Equalização de penalidades de intercâmbio", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Representação da submotorização",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Pela potência instalada"},
                                                           {"1", "Pela potência e energias afluentes médias"},
                                                           {"2", "Pela potência, energia afluente e regularização a montante"}})},
          .max_registros = 1},
         {.titulo = "Ordenação automática", .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)}, .max_registros = 1},
         {.titulo = "Cargas adicionais", .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)}, .max_registros = 1},
         {.titulo = "Delta de ZSUP (%)", .colunas = {real("Valor", 22, 25, 0)}, .max_registros = 1},
         {.titulo = "Delta de ZINF (%)", .colunas = {real("Valor", 22, 25, 0)}, .max_registros = 1},
         {.titulo = "Deltas de ZINF consecutivos", .colunas = {inteiro("Valor", 22, 25)}, .max_registros = 1},
         {.titulo = "Despacho antecipado de GNL",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Modificação automática da antecipação GNL",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Geração hidráulica mínima", .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)}, .max_registros = 1},
         {.titulo = "Simulação final com data",
          .colunas = {inteiro("Mês", 24, 25), inteiro("Ano", 27, 30), real("Volume inicial (%) REE", 33, 39, 1)},
          .passo_repeticao = 7,
          .max_registros = 1},
         {.titulo = "Processamento paralelo",
          .colunas = {comOpcoes(inteiro("Gerenciador externo", 22, 25), CONSIDERA),
                      comOpcoes(inteiro("Comunicação em dois níveis", 27, 30), CONSIDERA),
                      comOpcoes(inteiro("Armazenamento local", 32, 35),
                                {{"0", "Não considera"}, {"1", "Local por processo"}, {"2", "Local por nó"}}),
                      comOpcoes(inteiro("ENA em memória", 37, 40), CONSIDERA),
                      comOpcoes(inteiro("Cortes em memória", 42, 45), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Aversão a risco SAR", .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)}, .max_registros = 1},
         {.titulo = "Aversão a risco CVaR",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25),
                                {{"0", "Não considera"}, {"1", "Constante no tempo"}, {"2", "Variável no tempo"}})},
          .max_registros = 1},
         {.titulo = "Mínimo ZSUP na convergência",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Desconsidera vazão mínima", .colunas = {comOpcoes(inteiro("Valor", 22, 25), NAO_SIM)}, .max_registros = 1},
         {.titulo = "Restrições elétricas internas aos REEs",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Seleção de cortes",
          .colunas = {comOpcoes(inteiro("Backward", 22, 25), CONSIDERA), comOpcoes(inteiro("Forward", 27, 30), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Janela de cortes",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Não considera"}, {"1", "Janela fixa de 2 x NREE"}})},
          .max_registros = 1},
         {.titulo = "Reamostragem de cenários",
          .colunas = {comOpcoes(inteiro("Considera", 22, 25), CONSIDERA),
                      comOpcoes(inteiro("Tipo", 27, 30), {{"0", "Recombinação"}, {"1", "Plena"}}), inteiro("Passo", 32, 35)},
          .max_registros = 1},
         {.titulo = "Nó zero no cálculo de ZINF",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Consulta à FCF", .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)}, .max_registros = 1},
         {.titulo = "Impressão de cenários de ENA e ventos",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Não imprime"},
                                                           {"1", "Cenários de ENA e de ventos"},
                                                           {"2", "Só cenários de ENA"},
                                                           {"3", "Só cenários de ventos"}})},
          .max_registros = 1},
         {.titulo = "Impressão dos cortes ativos", .colunas = {comOpcoes(inteiro("Valor", 22, 25), IMPRIME)}, .max_registros = 1},
         {.titulo = "Representante da agregação",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Mais próximo"}, {"1", "Centroide"}})},
          .max_registros = 1},
         {.titulo = "Matriz de correlação espacial",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Anual"}, {"1", "Mensal"}})},
          .max_registros = 1},
         {.titulo = "Desconsidera critério estatístico",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), NAO_SIM)},
          .max_registros = 1},
         {.titulo = "Momento da reamostragem",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Backward"}, {"1", "Forward"}})},
          .max_registros = 1},
         {.titulo = "Mantém arquivos de ENA",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Apaga"}, {"1", "Mantém"}})},
          .max_registros = 1},
         {.titulo = "Teste de convergência a partir da iteração mínima",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25),
                                {{"0", "Desde a primeira iteração"}, {"1", "A partir da iteração mínima"}})},
          .max_registros = 1},
         {.titulo = "VMINT sazonal nos períodos estáticos",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "VMAXT sazonal nos períodos estáticos",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "VMINP sazonal nos períodos estáticos",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "CFUGA e CMONT sazonais nos períodos estáticos",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Restrições de emissão de GEE",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), REPRESENTA)},
          .max_registros = 1},
         {.titulo = "Afluência anual no PAR(p)",
          .colunas = {comOpcoes(inteiro("Opção", 22, 25), {{"0", "Não considera"},
                                                           {"1", "Sem termo anual nos cortes (desabilitada)"},
                                                           {"2", "Termo anual aproximado (desabilitada)"},
                                                           {"3", "Termo anual exato"}}),
                      comOpcoes(inteiro("Redução automática da ordem", 27, 30),
                                {{"0", "Considera"}, {"1", "Não considera"}, {"2", "Considera e imprime relatório"}})},
          .max_registros = 1},
         {.titulo = "Restrições de fornecimento de gás",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), REPRESENTA)},
          .max_registros = 1},
         {.titulo = "Memória de cálculo dos cortes",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), IMPRIME)},
          .max_registros = 1},
         {.titulo = "Incerteza na produção eólica",
          .colunas = {comOpcoes(
                          inteiro("Opção", 22, 25),
                          {{"0", "Não considera"}, {"1", "Weibull, método iterativo"}, {"2", "Weibull, método dos momentos"}}),
                      real("Penalidade de corte", 27, 34, 4)},
          .max_registros = 1},
         {.titulo = "Restrição de turbinamento",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Não considera"},
                                                           {"1", "Turbinamento máximo e mínimo"},
                                                           {"2", "Só turbinamento máximo"},
                                                           {"3", "Só turbinamento mínimo"}})},
          .max_registros = 1},
         {.titulo = "Restrição de defluência máxima",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Base dos subproblemas da backward",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Calcula a base"}, {"1", "Usa a da forward anterior"}})},
          .max_registros = 1},
         {.titulo = "Impressão do cortese.dat",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), {{"0", "Imprime"}, {"1", "Não imprime"}})},
          .max_registros = 1},
         {.titulo = "LPP de turbinamento máximo por REE",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "LPP de defluência máxima por REE",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "LPP de turbinamento máximo por usina",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "LPP de defluência máxima por usina",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Restrições elétricas especiais",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Função de produção hidroelétrica",
          .colunas = {comOpcoes(inteiro("Modelo", 22, 25), {{"0", "FPHA"}, {"1", "Linear"}}),
                      comOpcoes(inteiro("Imprime desvios da FPHA", 27, 30), IMPRIME)},
          .max_registros = 1},
         {.titulo = "FCF do pós-estudo", .colunas = {comOpcoes(inteiro("Valor", 22, 25), NAO_SIM)}, .max_registros = 1},
         {.titulo = "Estações de bombeamento", .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)}, .max_registros = 1},
         {.titulo = "Canais de desvio", .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)}, .max_registros = 1},
         {.titulo = "Restrições hidráulicas de vazão (RHQ)",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Restrições hidráulicas de volume (RHV)",
          .colunas = {comOpcoes(inteiro("Valor", 22, 25), CONSIDERA)},
          .max_registros = 1},
         {.titulo = "Arquivos de cortes",
          .colunas = {comOpcoes(inteiro("Gera arquivo único", 22, 25), {{"0", "Não gera"}, {"1", "Gera"}}),
                      comOpcoes(inteiro("Apaga por período", 27, 30), {{"-1", "Apaga"},
                                                                       {"0", "Não apaga"},
                                                                       {"1", "Mantém 1 arquivo"},
                                                                       {"2", "Mantém 2 arquivos"},
                                                                       {"3", "Mantém 3 arquivos"}}),
                      inteiro("Período 1", 32, 35), inteiro("Período 2", 37, 40), inteiro("Período 3", 42, 45)},
          .max_registros = 1}},
        true,
        0,
        {{"Caso",
          {{"Identificação", {"Nome do caso", "Tipo de execução"}},
           {"Horizonte",
            {"Mês de início do estudo", "Ano de início do estudo", "Número de anos do estudo", "Duração do período (meses)",
             "Mês de início do período pré", "Anos de estabilização iniciais", "Anos de estabilização finais na política",
             "Anos de estabilização finais na simulação final", "Duração dos patamares", "Anos de manutenção térmica"}},
           {"Estado inicial", {"Cálculo do armazenamento inicial", "Volume inicial por REE (%)"}},
           {"Execução", {"Processamento paralelo", "Arquivos de cortes", "Mantém arquivos de ENA"}, true}}},
         {"Política",
          {{"Iterações e convergência",
            {"Número máximo de iterações", "Número mínimo de iterações", "Probabilidade do intervalo de confiança (%)",
             "Delta de ZSUP (%)", "Delta de ZINF (%)", "Deltas de ZINF consecutivos", "Mínimo ZSUP na convergência",
             "Nó zero no cálculo de ZINF", "Teste de convergência a partir da iteração mínima"}},
           {"Forward e backward",
            {"Número de simulações forward", "Aberturas da backward", "Base dos subproblemas da backward",
             "Taxa de desconto anual (%)"}},
           {"Cortes", {"Seleção de cortes", "Janela de cortes", "Consulta à FCF", "FCF do pós-estudo"}},
           {"Aversão a risco", {"Curva de segurança", "Aversão a risco SAR", "Aversão a risco CVaR"}}}},
         {"Cenários e simulação final",
          {{"Modelo estocástico",
            {"Ordem máxima do PAR(p)", "Afluência anual no PAR(p)", "Arquivo de vazões históricas", "Tendência hidrológica",
             "Matriz de correlação espacial", "Desconsidera critério estatístico", "El Niño", "Índice ENSO"}},
           {"Geração de cenários",
            {"Número de séries sintéticas", "Geração de cenários de afluências", "Reamostragem de cenários",
             "Momento da reamostragem", "Representante da agregação", "Incerteza na produção eólica"}},
           {"Simulação final",
            {"Simulação final", "Iterações para a simulação final", "Simulação final com data",
             "Intervalo de séries com relatório detalhado", "Profundidades do risco de déficit", "Corte de carga preventivo"}}}},
         {"Representação",
          {{"Usinas hidroelétricas",
            {"Função de produção hidroelétrica", "Representação da submotorização", "Ordenação automática",
             "Geração hidráulica mínima", "Desconsidera vazão mínima", "Desvio de água", "Energia de desvio de água",
             "Estações de bombeamento", "Canais de desvio"}},
           {"Restrições hidráulicas",
            {"Restrição de turbinamento", "Restrição de defluência máxima", "LPP de turbinamento máximo por REE",
             "LPP de defluência máxima por REE", "LPP de turbinamento máximo por usina", "LPP de defluência máxima por usina",
             "Restrições hidráulicas de vazão (RHQ)", "Restrições hidráulicas de volume (RHV)"}},
           {"Rede elétrica e intercâmbio",
            {"Restrições elétricas especiais", "Restrições elétricas internas aos REEs", "Restrições de Itaipu",
             "Agrupamento livre de intercâmbios", "Equalização de penalidades de intercâmbio",
             "Perdas na geração e transmissão"}},
           {"Carga e térmicas",
            {"Cargas adicionais", "Bid de demanda", "Despacho antecipado de GNL", "Modificação automática da antecipação GNL",
             "Restrições de fornecimento de gás", "Restrições de emissão de GEE"}},
           {"Períodos estáticos",
            {"VMINT sazonal nos períodos estáticos", "VMAXT sazonal nos períodos estáticos",
             "VMINP sazonal nos períodos estáticos", "CFUGA e CMONT sazonais nos períodos estáticos"}}}},
         {"Relatórios",
          {{"Dados de entrada",
            {"Imprime características das usinas", "Imprime dados de carga", "Imprime energias afluentes históricas",
             "Imprime parâmetros do modelo estocástico", "Imprime parâmetros dos REEs"}},
           {"Resultados",
            {"Impressão dos resultados", "Impressão dos riscos de déficit", "Impressão de cenários de ENA e ventos",
             "Impressão dos cortes ativos", "Impressão do cortese.dat", "Memória de cálculo dos cortes"}}}}}};
}

// shist.dat, manual do NEWAVE 30.0.2, secao 3.6: dois registros de comentario e o registro tipo 1
// (varredura da serie historica e ano de inicio da varredura); so quando nao ha varredura, mais
// dois registros de comentario e os registros tipo 2 (um ano historico de inicio por serie
// simulada) ate o 9999 no campo 1.
LayoutArquivoFixo shist() {
    return {"3.6",
            {{.titulo = "Varredura da série histórica",
              .linhas_cabecalho = 2,
              .colunas = {comOpcoes(inteiro("Varredura", 1, 4), {{"0", "Não faz varredura"}, {"1", "Faz varredura"}}),
                          inteiro("Ano início", 5, 8)},
              .max_registros = 1,
              .formulario = true},
             {.titulo = "Séries históricas simuladas",
              .linhas_cabecalho = 2,
              .terminador = "9999",
              .colunas = {inteiro("Ano início", 1, 4)}}}};
}

// sistema.dat, manual do NEWAVE 30.0.2, secao 3.7: cinco blocos, cada um precedido de tres
// registros de comentario. Bloco 1: numero de patamares de deficit. Bloco 2: um registro por
// submercado (custo e profundidade de ate quatro patamares de deficit) ate o 999 no campo 1. Bloco
// 3: por interligacao, um registro tipo 1 (par A/B e flags) seguido de um registro por ano com o
// limite de A para B, um registro em branco e um registro por ano com o limite de B para A, ate o
// 999; o sentido e o numero do grupo separado por linha em branco desde o registro tipo 1. Bloco 4:
// por submercado, um registro com o numero seguido de um registro por ano com o mercado (e
// registros PRE/POS dos periodos estaticos, se houver), ate o 999. Bloco 5: por bloco de usinas nao
// simuladas, um registro com submercado, bloco, descricao e tecnologia seguido de um registro por
// ano com a geracao, ate o 999. Nos blocos 3 a 5 a linha que abre o bloco e a que tem o campo 1
// preenchido e as colunas de dezembro (96 a 102) vazias.
LayoutArquivoFixo sistema() {
    return {
        "3.7",
        {{.titulo = "Patamares de déficit",
          .linhas_cabecalho = 3,
          .colunas = {inteiro("Patamares de déficit", 2, 4)},
          .max_registros = 1,
          .formulario = true,
          .contagem = Patamares::Deficit},
         {.titulo = "Custo do déficit",
          .linhas_cabecalho = 3,
          .terminador = "999",
          .colunas =
              {inteiro("Submercado", 2, 4), texto("Nome", 6, 15),
               comOpcoes(inteiro("Fictício", 18, 18), {{"0", "Não fictício"}, {"1", "Fictício"}}),
               deDeficit(real("Custo pat. 1 ($/MWh)", 20, 26, 2), 1), deDeficit(real("Custo pat. 2 ($/MWh)", 28, 34, 2), 2),
               deDeficit(real("Custo pat. 3 ($/MWh)", 36, 42, 2), 3), deDeficit(real("Custo pat. 4 ($/MWh)", 44, 50, 2), 4),
               deDeficit(real("Profund. pat. 1 (p.u.)", 52, 56, 3), 1), deDeficit(real("Profund. pat. 2 (p.u.)", 58, 62, 3), 2),
               deDeficit(real("Profund. pat. 3 (p.u.)", 64, 68, 3), 3), deDeficit(real("Profund. pat. 4 (p.u.)", 70, 74, 3), 4)}},
         {.titulo = "Limites de intercâmbio",
          .linhas_cabecalho = 3,
          .terminador = "999",
          .colunas = {ref(deContexto(inteiro("Submercado A", 2, 4), 0), Referencia::Submercado),
                      ref(deContexto(inteiro("Submercado B", 6, 8), 0), Referencia::Submercado),
                      grupo("Sentido (1 = A→B, 2 = B→A)", 0), inteiro("Ano", 1, 7), real("Jan (MWmédio)", 8, 14, 0),
                      real("Fev (MWmédio)", 16, 22, 0), real("Mar (MWmédio)", 24, 30, 0), real("Abr (MWmédio)", 32, 38, 0),
                      real("Mai (MWmédio)", 40, 46, 0), real("Jun (MWmédio)", 48, 54, 0), real("Jul (MWmédio)", 56, 62, 0),
                      real("Ago (MWmédio)", 64, 70, 0), real("Set (MWmédio)", 72, 78, 0), real("Out (MWmédio)", 80, 86, 0),
                      real("Nov (MWmédio)", 88, 94, 0), real("Dez (MWmédio)", 96, 102, 0)},
          .filtro = {preenchido(96, 102)},
          .contextos = {{{preenchido(2, 4), vazio(96, 102)}}}},
         {.titulo = "Interligações",
          .colunas = {ref(inteiro("Submercado A", 2, 4), Referencia::Submercado),
                      ref(inteiro("Submercado B", 6, 8), Referencia::Submercado),
                      comOpcoes(inteiro("Tipo de limite", 24, 24),
                                {{"0", "Limite de intercâmbio"}, {"1", "Intercâmbio mínimo obrigatório"}}),
                      comOpcoes(inteiro("Penalidade interna", 32, 32),
                                {{"0", "Considera penalidade"}, {"1", "Não considera penalidade"}})},
          .filtro = {preenchido(2, 4), vazio(96, 102)},
          .mesma_regiao = true,
          .formulario = true},
         {.titulo = "Carga de energia",
          .linhas_cabecalho = 3,
          .terminador = "999",
          .colunas = {ref(deContexto(inteiro("Submercado", 2, 4), 0), Referencia::Submercado), texto("Ano", 1, 7),
                      real("Jan (MWmédio)", 8, 14, 0), real("Fev (MWmédio)", 16, 22, 0), real("Mar (MWmédio)", 24, 30, 0),
                      real("Abr (MWmédio)", 32, 38, 0), real("Mai (MWmédio)", 40, 46, 0), real("Jun (MWmédio)", 48, 54, 0),
                      real("Jul (MWmédio)", 56, 62, 0), real("Ago (MWmédio)", 64, 70, 0), real("Set (MWmédio)", 72, 78, 0),
                      real("Out (MWmédio)", 80, 86, 0), real("Nov (MWmédio)", 88, 94, 0), real("Dez (MWmédio)", 96, 102, 0)},
          .filtro = {preenchido(96, 102)},
          .contextos = {{{preenchido(2, 4), vazio(96, 102)}}}},
         {.titulo = "Submercados da carga",
          .colunas = {ref(inteiro("Submercado", 2, 4), Referencia::Submercado)},
          .filtro = {preenchido(2, 4), vazio(96, 102)},
          .mesma_regiao = true,
          .formulario = true,
          .formulario_ao_lado = "Interligações"},
         {.titulo = "Geração de usinas não simuladas",
          .linhas_cabecalho = 3,
          .terminador = "999",
          .colunas = {ref(deContexto(inteiro("Submercado", 2, 4), 0), Referencia::Submercado),
                      deContexto(inteiro("Bloco", 7, 9), 0), deContexto(texto("Descrição", 12, 31), 0), inteiro("Ano", 1, 7),
                      real("Jan (MWmédio)", 8, 14, 0), real("Fev (MWmédio)", 16, 22, 0), real("Mar (MWmédio)", 24, 30, 0),
                      real("Abr (MWmédio)", 32, 38, 0), real("Mai (MWmédio)", 40, 46, 0), real("Jun (MWmédio)", 48, 54, 0),
                      real("Jul (MWmédio)", 56, 62, 0), real("Ago (MWmédio)", 64, 70, 0), real("Set (MWmédio)", 72, 78, 0),
                      real("Out (MWmédio)", 80, 86, 0), real("Nov (MWmédio)", 88, 94, 0), real("Dez (MWmédio)", 96, 102, 0)},
          .filtro = {preenchido(96, 102)},
          .contextos = {{{preenchido(2, 4), vazio(96, 102)}}}},
         {.titulo = "Blocos de usinas não simuladas",
          .colunas = {ref(inteiro("Submercado", 2, 4), Referencia::Submercado), inteiro("Bloco", 7, 9),
                      texto("Descrição", 12, 31), ref(inteiro("Tecnologia", 34, 36), Referencia::Tecnologia)},
          .filtro = {preenchido(2, 4), vazio(96, 102)},
          .mesma_regiao = true}}};
}

// patamar.dat, manual do NEWAVE 30.0.2, secao 3.8: dois registros de comentario e o numero de
// patamares (bloco 1); tres de comentario e a duracao dos patamares (bloco 2), tipo 1 com 12
// registros por nome do mes ou tipo 2 com um registro por patamar e ano, o ano so no primeiro
// patamar, conforme o registro 40 do dger.dat; e os blocos 3 (carga por submercado, tres de
// comentario), 4 (intercambio por interligacao, cinco de comentario) e 5 (usinas nao simuladas por
// bloco, quatro de comentario), cada um ate o 9999 no campo 1, com um registro que abre o conjunto
// seguido de um registro por patamar com 12 fatores (tipo 1) ou, por ano, um registro com o ano e
// um por patamar seguinte (tipo 2). O bloco 2 nao tem terminador: cada tipo e uma regiao contigua
// que acaba no primeiro registro fora do formato (colunas 5 e 6 vazias; tipo 2 com dezembro nas
// colunas 95 a 100, tipo 1 com nome do mes e sem dezembro). O deck traz quatro registros de
// comentario antes do bloco 3, e nao tres.
LayoutArquivoFixo patamar() {
    return {"3.8",
            {{.titulo = "Patamares de carga",
              .linhas_cabecalho = 2,
              .colunas = {inteiro("Patamares de carga", 2, 3)},
              .max_registros = 1,
              .formulario = true,
              .contagem = Patamares::Carga},
             {.titulo = "Duração dos patamares por ano",
              .linhas_cabecalho = 3,
              .colunas = {deContexto(inteiro("Ano", 1, 4), 0), ordinal("Patamar", 0), real("Jan (p.u.)", 7, 12, 4),
                          real("Fev (p.u.)", 15, 20, 4), real("Mar (p.u.)", 23, 28, 4), real("Abr (p.u.)", 31, 36, 4),
                          real("Mai (p.u.)", 39, 44, 4), real("Jun (p.u.)", 47, 52, 4), real("Jul (p.u.)", 55, 60, 4),
                          real("Ago (p.u.)", 63, 68, 4), real("Set (p.u.)", 71, 76, 4), real("Out (p.u.)", 79, 84, 4),
                          real("Nov (p.u.)", 87, 92, 4), real("Dez (p.u.)", 95, 100, 4)},
              .filtro = {vazio(5, 6), preenchido(95, 100)},
              .contextos = {{{preenchido(1, 4), preenchido(95, 100)}}},
              .contigua = true},
             {.titulo = "Duração sazonal dos patamares",
              .colunas = {texto("Mês", 2, 4), deCarga(real("Patamar 1 (p.u.)", 7, 12, 4), 1),
                          deCarga(real("Patamar 2 (p.u.)", 15, 20, 4), 2), deCarga(real("Patamar 3 (p.u.)", 23, 28, 4), 3),
                          deCarga(real("Patamar 4 (p.u.)", 31, 36, 4), 4), deCarga(real("Patamar 5 (p.u.)", 39, 44, 4), 5)},
              .filtro = {preenchido(2, 4), vazio(5, 6), preenchido(7, 12), vazio(95, 100)},
              .contigua = true},
             {.titulo = "Carga por patamar e ano",
              .linhas_cabecalho = 4,
              .terminador = "9999",
              .colunas = {ref(deContexto(inteiro("Submercado", 2, 4), 0), Referencia::Submercado),
                          deContexto(inteiro("Ano", 4, 7), 1), ordinal("Patamar", 1), real("Jan (p.u.)", 9, 14, 4),
                          real("Fev (p.u.)", 16, 21, 4), real("Mar (p.u.)", 23, 28, 4), real("Abr (p.u.)", 30, 35, 4),
                          real("Mai (p.u.)", 37, 42, 4), real("Jun (p.u.)", 44, 49, 4), real("Jul (p.u.)", 51, 56, 4),
                          real("Ago (p.u.)", 58, 63, 4), real("Set (p.u.)", 65, 70, 4), real("Out (p.u.)", 72, 77, 4),
                          real("Nov (p.u.)", 79, 84, 4), real("Dez (p.u.)", 86, 91, 4)},
              .filtro = {preenchido(86, 91)},
              .contextos = {{{preenchido(2, 4), vazio(9, 91)}}, {{preenchido(4, 7), preenchido(86, 91)}}}},
             {.titulo = "Carga por patamar, sazonal",
              .colunas = {ref(deContexto(inteiro("Submercado", 2, 4), 0), Referencia::Submercado), ordinal("Patamar", 0),
                          real("Mês 1 (p.u.)", 2, 7, 4), real("Mês 2 (p.u.)", 9, 14, 4), real("Mês 3 (p.u.)", 16, 21, 4),
                          real("Mês 4 (p.u.)", 23, 28, 4), real("Mês 5 (p.u.)", 30, 35, 4), real("Mês 6 (p.u.)", 37, 42, 4),
                          real("Mês 7 (p.u.)", 44, 49, 4), real("Mês 8 (p.u.)", 51, 56, 4), real("Mês 9 (p.u.)", 58, 63, 4),
                          real("Mês 10 (p.u.)", 65, 70, 4), real("Mês 11 (p.u.)", 72, 77, 4), real("Mês 12 (p.u.)", 79, 84, 4)},
              .filtro = {preenchido(9, 14), vazio(86, 91)},
              .contextos = {{{preenchido(2, 4), vazio(9, 91)}}},
              .mesma_regiao = true},
             {.titulo = "Submercados da carga",
              .colunas = {ref(inteiro("Submercado", 2, 4), Referencia::Submercado)},
              .filtro = {preenchido(2, 4), vazio(9, 91)},
              .mesma_regiao = true,
              .formulario = true,
              .formulario_ao_lado = "Interligações"},
             {.titulo = "Intercâmbio por patamar e ano",
              .linhas_cabecalho = 5,
              .terminador = "9999",
              .colunas = {ref(deContexto(inteiro("Submercado A", 2, 4), 0), Referencia::Submercado),
                          ref(deContexto(inteiro("Submercado B", 6, 8), 0), Referencia::Submercado),
                          deContexto(inteiro("Ano", 4, 7), 1), ordinal("Patamar", 1), real("Jan (p.u.)", 9, 14, 4),
                          real("Fev (p.u.)", 16, 21, 4), real("Mar (p.u.)", 23, 28, 4), real("Abr (p.u.)", 30, 35, 4),
                          real("Mai (p.u.)", 37, 42, 4), real("Jun (p.u.)", 44, 49, 4), real("Jul (p.u.)", 51, 56, 4),
                          real("Ago (p.u.)", 58, 63, 4), real("Set (p.u.)", 65, 70, 4), real("Out (p.u.)", 72, 77, 4),
                          real("Nov (p.u.)", 79, 84, 4), real("Dez (p.u.)", 86, 91, 4)},
              .filtro = {preenchido(86, 91)},
              .contextos = {{{preenchido(2, 4), vazio(9, 91)}}, {{preenchido(4, 7), preenchido(86, 91)}}}},
             {.titulo = "Intercâmbio por patamar, sazonal",
              .colunas = {ref(deContexto(inteiro("Submercado A", 2, 4), 0), Referencia::Submercado),
                          ref(deContexto(inteiro("Submercado B", 6, 8), 0), Referencia::Submercado), ordinal("Patamar", 0),
                          real("Mês 1 (p.u.)", 2, 7, 4), real("Mês 2 (p.u.)", 9, 14, 4), real("Mês 3 (p.u.)", 16, 21, 4),
                          real("Mês 4 (p.u.)", 23, 28, 4), real("Mês 5 (p.u.)", 30, 35, 4), real("Mês 6 (p.u.)", 37, 42, 4),
                          real("Mês 7 (p.u.)", 44, 49, 4), real("Mês 8 (p.u.)", 51, 56, 4), real("Mês 9 (p.u.)", 58, 63, 4),
                          real("Mês 10 (p.u.)", 65, 70, 4), real("Mês 11 (p.u.)", 72, 77, 4), real("Mês 12 (p.u.)", 79, 84, 4)},
              .filtro = {preenchido(9, 14), vazio(86, 91)},
              .contextos = {{{preenchido(2, 4), vazio(9, 91)}}},
              .mesma_regiao = true},
             {.titulo = "Interligações",
              .colunas = {ref(inteiro("Submercado A", 2, 4), Referencia::Submercado),
                          ref(inteiro("Submercado B", 6, 8), Referencia::Submercado)},
              .filtro = {preenchido(2, 4), vazio(9, 91)},
              .mesma_regiao = true,
              .formulario = true},
             {.titulo = "Usinas não simuladas por patamar e ano",
              .linhas_cabecalho = 4,
              .terminador = "9999",
              .colunas = {ref(deContexto(inteiro("Submercado", 2, 4), 0), Referencia::Submercado),
                          deContexto(inteiro("Bloco", 6, 8), 0), deContexto(inteiro("Ano", 4, 7), 1), ordinal("Patamar", 1),
                          real("Jan (p.u.)", 9, 14, 4), real("Fev (p.u.)", 16, 21, 4), real("Mar (p.u.)", 23, 28, 4),
                          real("Abr (p.u.)", 30, 35, 4), real("Mai (p.u.)", 37, 42, 4), real("Jun (p.u.)", 44, 49, 4),
                          real("Jul (p.u.)", 51, 56, 4), real("Ago (p.u.)", 58, 63, 4), real("Set (p.u.)", 65, 70, 4),
                          real("Out (p.u.)", 72, 77, 4), real("Nov (p.u.)", 79, 84, 4), real("Dez (p.u.)", 86, 91, 4)},
              .filtro = {preenchido(86, 91)},
              .contextos = {{{preenchido(2, 4), vazio(9, 91)}}, {{preenchido(4, 7), preenchido(86, 91)}}}},
             {.titulo = "Usinas não simuladas por patamar, sazonal",
              .colunas = {ref(deContexto(inteiro("Submercado", 2, 4), 0), Referencia::Submercado),
                          deContexto(inteiro("Bloco", 6, 8), 0), ordinal("Patamar", 0), real("Mês 1 (p.u.)", 2, 7, 4),
                          real("Mês 2 (p.u.)", 9, 14, 4), real("Mês 3 (p.u.)", 16, 21, 4), real("Mês 4 (p.u.)", 23, 28, 4),
                          real("Mês 5 (p.u.)", 30, 35, 4), real("Mês 6 (p.u.)", 37, 42, 4), real("Mês 7 (p.u.)", 44, 49, 4),
                          real("Mês 8 (p.u.)", 51, 56, 4), real("Mês 9 (p.u.)", 58, 63, 4), real("Mês 10 (p.u.)", 65, 70, 4),
                          real("Mês 11 (p.u.)", 72, 77, 4), real("Mês 12 (p.u.)", 79, 84, 4)},
              .filtro = {preenchido(9, 14), vazio(86, 91)},
              .contextos = {{{preenchido(2, 4), vazio(9, 91)}}},
              .mesma_regiao = true},
             {.titulo = "Blocos de usinas não simuladas",
              .colunas = {ref(inteiro("Submercado", 2, 4), Referencia::Submercado), inteiro("Bloco", 6, 8)},
              .filtro = {preenchido(2, 4), vazio(9, 91)},
              .mesma_regiao = true}}};
}

// confhd.dat, manual do NEWAVE 30.0.2, secao 3.9: dois registros de comentario e um registro por
// usina hidroeletrica da configuracao, campos 1 a 11, sem terminador.
LayoutArquivoFixo confhd() {
    return {
        "3.9",
        {{.titulo = "Usinas hidroelétricas na configuração",
          .linhas_cabecalho = 2,
          .colunas = {
              inteiro("Usina", 2, 5), texto("Nome", 7, 18), ref(inteiro("Posto", 20, 23), Referencia::Posto),
              ref(inteiro("Usina a jusante", 26, 29), Referencia::UsinaHidro), ref(inteiro("REE", 31, 34), Referencia::Ree),
              real("Volume inicial (% vol. útil)", 36, 41, 2),
              comOpcoes(
                  texto("Situação", 45, 46),
                  {{"EX", "Existente"}, {"EE", "Existente, com expansão"}, {"NE", "Não existente"}, {"NC", "Não considerada"}}),
              comOpcoes(inteiro("Modifica cadastro", 50, 53), NAO_SIM),
              inteiro("Ano início histórico", 59, 62), inteiro("Ano fim histórico", 68, 71),
              ref(inteiro("Tecnologia", 74, 76), Referencia::Tecnologia)}}}};
}

// exph.dat, manual do NEWAVE 30.0.2, secao 3.13: tres registros de comentario e, para cada usina,
// um registro tipo 1 opcional (enchimento de volume morto) e os registros tipo 2 (entrada de
// unidades), o primeiro registro da usina com codigo e nome, ate o 9999 no campo 1 que fecha o
// cronograma da usina.
LayoutArquivoFixo exph() {
    return {"3.13",
            {{.titulo = "Entrada de unidades",
              .linhas_cabecalho = 3,
              .colunas = {ref(deContexto(inteiro("Usina", 1, 4), 0), Referencia::UsinaHidro), deContexto(texto("Nome", 6, 17), 0),
                          inteiro("Mês entrada", 45, 46), inteiro("Ano entrada", 48, 51), inteiro("Unidade", 61, 62),
                          inteiro("Conjunto", 65, 65)},
              .filtro = {preenchido(48, 51)},
              .contextos = {{{preenchido(1, 4), diferente(1, 4, {"9999"})}}}},
             {.titulo = "Usinas e enchimento de volume morto",
              .colunas = {ref(inteiro("Usina", 1, 4), Referencia::UsinaHidro), texto("Nome", 6, 17),
                          inteiro("Mês início enchimento", 19, 20), inteiro("Ano início enchimento", 22, 25),
                          inteiro("Duração enchimento (meses)", 32, 33), real("Volume morto preenchido (%)", 38, 42, 1)},
              .filtro = {preenchido(1, 4), diferente(1, 4, {"9999"})},
              .mesma_regiao = true}}};
}

// loss.dat (perda.dat no manual do NEWAVE 30.0.2, secao 3.20): quatro blocos, cada um precedido de
// dois registros de comentario: hidroeletricas, termoeletricas, submercados (nao implementado) e
// pares de submercados. Em cada bloco, o registro tipo 1 abre a usina ou submercado, o tipo 2 traz
// o ano e os fatores do primeiro patamar (um por mes a cada 6 colunas a partir da 15) e os tipos 3
// os demais patamares, sem o ano. Os blocos 1 e 3 terminam no 9999 e o bloco 4 no 999 no campo 1;
// para o bloco 2 o manual nao cita terminador nem os comentarios antes do bloco 3, e aqui se supoe
// o mesmo do bloco 1. So e lido com o registro 37 do dger.dat igual a 1.
LayoutArquivoFixo loss() {
    return {"3.20",
            {{.titulo = "Perdas das hidroelétricas",
              .linhas_cabecalho = 2,
              .terminador = "9999",
              .colunas = {ref(deContexto(inteiro("Usina", 2, 5), 0), Referencia::UsinaHidro),
                          deContexto(inteiro("Ano", 7, 10), 1), ordinal("Patamar", 1), real("Jan (p.u.)", 15, 19, 3),
                          real("Fev (p.u.)", 21, 25, 3), real("Mar (p.u.)", 27, 31, 3), real("Abr (p.u.)", 33, 37, 3),
                          real("Mai (p.u.)", 39, 43, 3), real("Jun (p.u.)", 45, 49, 3), real("Jul (p.u.)", 51, 55, 3),
                          real("Ago (p.u.)", 57, 61, 3), real("Set (p.u.)", 63, 67, 3), real("Out (p.u.)", 69, 73, 3),
                          real("Nov (p.u.)", 75, 79, 3), real("Dez (p.u.)", 81, 85, 3)},
              .filtro = {vazio(2, 5)},
              .contextos = {{{preenchido(2, 5)}}, {{vazio(2, 5), preenchido(7, 10)}}}},
             {.titulo = "Hidroelétricas",
              .colunas = {ref(inteiro("Usina", 2, 5), Referencia::UsinaHidro)},
              .filtro = {preenchido(2, 5)},
              .mesma_regiao = true,
              .formulario = true,
              .formulario_ao_lado = "Termoelétricas"},
             {.titulo = "Perdas das termoelétricas",
              .linhas_cabecalho = 2,
              .terminador = "9999",
              .colunas = {ref(deContexto(inteiro("Usina", 2, 5), 0), Referencia::UsinaTermica),
                          deContexto(inteiro("Ano", 7, 10), 1), ordinal("Patamar", 1), real("Jan (p.u.)", 15, 19, 3),
                          real("Fev (p.u.)", 21, 25, 3), real("Mar (p.u.)", 27, 31, 3), real("Abr (p.u.)", 33, 37, 3),
                          real("Mai (p.u.)", 39, 43, 3), real("Jun (p.u.)", 45, 49, 3), real("Jul (p.u.)", 51, 55, 3),
                          real("Ago (p.u.)", 57, 61, 3), real("Set (p.u.)", 63, 67, 3), real("Out (p.u.)", 69, 73, 3),
                          real("Nov (p.u.)", 75, 79, 3), real("Dez (p.u.)", 81, 85, 3)},
              .filtro = {vazio(2, 5)},
              .contextos = {{{preenchido(2, 5)}}, {{vazio(2, 5), preenchido(7, 10)}}}},
             {.titulo = "Termoelétricas",
              .colunas = {ref(inteiro("Usina", 2, 5), Referencia::UsinaTermica)},
              .filtro = {preenchido(2, 5)},
              .mesma_regiao = true,
              .formulario = true},
             {.titulo = "Perdas dos submercados (não implementado)",
              .linhas_cabecalho = 2,
              .terminador = "9999",
              .colunas = {ref(deContexto(inteiro("Submercado", 2, 5), 0), Referencia::Submercado),
                          deContexto(inteiro("Ano", 7, 10), 1), ordinal("Patamar", 1), real("Jan (p.u.)", 15, 19, 3),
                          real("Fev (p.u.)", 21, 25, 3), real("Mar (p.u.)", 27, 31, 3), real("Abr (p.u.)", 33, 37, 3),
                          real("Mai (p.u.)", 39, 43, 3), real("Jun (p.u.)", 45, 49, 3), real("Jul (p.u.)", 51, 55, 3),
                          real("Ago (p.u.)", 57, 61, 3), real("Set (p.u.)", 63, 67, 3), real("Out (p.u.)", 69, 73, 3),
                          real("Nov (p.u.)", 75, 79, 3), real("Dez (p.u.)", 81, 85, 3)},
              .filtro = {vazio(2, 5)},
              .contextos = {{{preenchido(2, 5)}}, {{vazio(2, 5), preenchido(7, 10)}}}},
             {.titulo = "Submercados (não implementado)",
              .colunas = {ref(inteiro("Submercado", 2, 5), Referencia::Submercado)},
              .filtro = {preenchido(2, 5)},
              .mesma_regiao = true,
              .formulario = true,
              .formulario_ao_lado = "Pares de submercados"},
             {.titulo = "Perdas entre submercados",
              .linhas_cabecalho = 2,
              .terminador = "999",
              .colunas = {ref(deContexto(inteiro("Submercado A", 2, 5), 0), Referencia::Submercado),
                          ref(deContexto(inteiro("Submercado B", 7, 10), 0), Referencia::Submercado),
                          deContexto(inteiro("Ano", 7, 10), 1), ordinal("Patamar", 1), real("Jan (p.u.)", 15, 19, 3),
                          real("Fev (p.u.)", 21, 25, 3), real("Mar (p.u.)", 27, 31, 3), real("Abr (p.u.)", 33, 37, 3),
                          real("Mai (p.u.)", 39, 43, 3), real("Jun (p.u.)", 45, 49, 3), real("Jul (p.u.)", 51, 55, 3),
                          real("Ago (p.u.)", 57, 61, 3), real("Set (p.u.)", 63, 67, 3), real("Out (p.u.)", 69, 73, 3),
                          real("Nov (p.u.)", 75, 79, 3), real("Dez (p.u.)", 81, 85, 3)},
              .filtro = {vazio(2, 5)},
              .contextos = {{{preenchido(2, 5)}}, {{vazio(2, 5), preenchido(7, 10)}}}},
             {.titulo = "Pares de submercados",
              .colunas = {ref(inteiro("Submercado A", 2, 5), Referencia::Submercado),
                          ref(inteiro("Submercado B", 7, 10), Referencia::Submercado)},
              .filtro = {preenchido(2, 5)},
              .mesma_regiao = true,
              .formulario = true}}};
}

// dsvagua.dat, manual do NEWAVE 30.0.2, secao 3.21: dois registros de comentario e um registro por
// usina e ano, com a vazao desviada ou adicionada de cada mes a cada 7 colunas a partir da 10 e o
// flag de usina NC na 98, ate o 9999 no campo 1.
LayoutArquivoFixo dsvagua() {
    return {"3.21",
            {{.titulo = "Outros usos da água",
              .linhas_cabecalho = 2,
              .terminador = "9999",
              .colunas = {inteiro("Ano", 1, 4), ref(inteiro("Usina", 6, 9), Referencia::UsinaHidro),
                          real("Jan (m³/s)", 10, 16, 1), real("Fev (m³/s)", 17, 23, 1), real("Mar (m³/s)", 24, 30, 1),
                          real("Abr (m³/s)", 31, 37, 1), real("Mai (m³/s)", 38, 44, 1), real("Jun (m³/s)", 45, 51, 1),
                          real("Jul (m³/s)", 52, 58, 1), real("Ago (m³/s)", 59, 65, 1), real("Set (m³/s)", 66, 72, 1),
                          real("Out (m³/s)", 73, 79, 1), real("Nov (m³/s)", 80, 86, 1), real("Dez (m³/s)", 87, 93, 1),
                          comOpcoes(inteiro("Considera se NC", 98, 101),
                                    {{"0", "Registro ignorado"}, {"1", "Passa para a usina de jusante"}})}}}};
}

// vazpast.dat, manual do NEWAVE 30.0.2, secao 3.22.3: tres registros de comentario e um registro
// por posto da configuracao, com a vazao afluente de cada mes a cada 10 colunas a partir da 19; sem
// terminador, a lista vai ate o fim do arquivo. Lido quando o registro 34 do dger.dat e 2 (secao
// 3.22.1).
LayoutArquivoFixo vazpast() {
    return {"3.22.3",
            {{.titulo = "Tendência hidrológica",
              .linhas_cabecalho = 3,
              .colunas = {ref(inteiro("Posto", 3, 5), Referencia::Posto), real("Jan (m³/s)", 19, 27, 2),
                          real("Fev (m³/s)", 29, 37, 2), real("Mar (m³/s)", 39, 47, 2), real("Abr (m³/s)", 49, 57, 2),
                          real("Mai (m³/s)", 59, 67, 2), real("Jun (m³/s)", 69, 77, 2), real("Jul (m³/s)", 79, 87, 2),
                          real("Ago (m³/s)", 89, 97, 2), real("Set (m³/s)", 99, 107, 2), real("Out (m³/s)", 109, 117, 2),
                          real("Nov (m³/s)", 119, 127, 2), real("Dez (m³/s)", 129, 137, 2)}}}};
}

// gtminpat.dat, manual do NEWAVE 30.0.2, secao 3.23: dois registros de comentario e blocos sem
// terminador ate o fim do arquivo. Cada bloco abre com o registro tipo 1 (submercado e classe
// termica). No bloco 1 seguem os registros tipo 2, um por patamar, com o fator de cada mes a cada 9
// colunas a partir da 4, valido para todos os anos. No bloco 2 seguem, por ano, o registro tipo 2
// (ano e fatores do primeiro patamar a cada 9 colunas a partir da 13) e os tipos 3 dos demais
// patamares, sem o ano. Os registros de fatores dos dois blocos se distinguem por dezembro: colunas
// 103 a 108 no bloco 1 e 112 a 117 no bloco 2.
LayoutArquivoFixo gtminpat() {
    return {"3.23",
            {{.titulo = "Fatores por patamar",
              .linhas_cabecalho = 2,
              .colunas = {ref(deContexto(inteiro("Submercado", 1, 3), 0), Referencia::Submercado),
                          ref(deContexto(inteiro("Classe", 7, 9), 0), Referencia::ClasseTermica), ordinal("Patamar", 0),
                          real("Jan (p.u.)", 4, 9, 4), real("Fev (p.u.)", 13, 18, 4), real("Mar (p.u.)", 22, 27, 4),
                          real("Abr (p.u.)", 31, 36, 4), real("Mai (p.u.)", 40, 45, 4), real("Jun (p.u.)", 49, 54, 4),
                          real("Jul (p.u.)", 58, 63, 4), real("Ago (p.u.)", 67, 72, 4), real("Set (p.u.)", 76, 81, 4),
                          real("Out (p.u.)", 85, 90, 4), real("Nov (p.u.)", 94, 99, 4), real("Dez (p.u.)", 103, 108, 4)},
              .filtro = {vazio(1, 3), vazio(112, 117)},
              .contextos = {{{preenchido(1, 3)}}}},
             {.titulo = "Fatores por ano e patamar",
              .colunas = {ref(deContexto(inteiro("Submercado", 1, 3), 0), Referencia::Submercado),
                          ref(deContexto(inteiro("Classe", 7, 9), 0), Referencia::ClasseTermica),
                          deContexto(inteiro("Ano", 5, 8), 1), ordinal("Patamar", 1), real("Jan (p.u.)", 13, 18, 4),
                          real("Fev (p.u.)", 22, 27, 4), real("Mar (p.u.)", 31, 36, 4), real("Abr (p.u.)", 40, 45, 4),
                          real("Mai (p.u.)", 49, 54, 4), real("Jun (p.u.)", 58, 63, 4), real("Jul (p.u.)", 67, 72, 4),
                          real("Ago (p.u.)", 76, 81, 4), real("Set (p.u.)", 85, 90, 4), real("Out (p.u.)", 94, 99, 4),
                          real("Nov (p.u.)", 103, 108, 4), real("Dez (p.u.)", 112, 117, 4)},
              .filtro = {vazio(1, 3), preenchido(112, 117)},
              .contextos = {{{preenchido(1, 3)}}, {{vazio(1, 3), preenchido(5, 8), preenchido(112, 117)}}},
              .mesma_regiao = true},
             {.titulo = "Submercados e classes",
              .colunas = {ref(inteiro("Submercado", 1, 3), Referencia::Submercado),
                          ref(inteiro("Classe", 7, 9), Referencia::ClasseTermica)},
              .filtro = {preenchido(1, 3)},
              .mesma_regiao = true,
              .formulario = true}}};
}

// penalid.dat, manual do NEWAVE 30.0.2, secao 3.24: dois registros de comentario e um bloco unico
// de registros, um por palavra-chave e REE (ou submercado, no INTMIN) e patamar, sem terminador.
LayoutArquivoFixo penalid() {
    return {"3.24",
            {{.titulo = "Penalidades",
              .linhas_cabecalho = 2,
              .colunas = {comOpcoes(texto("Variável", 2, 7), {{"DESVIO", "Outros usos da água"},
                                                              {"INTMIN", "Intercâmbio mínimo"},
                                                              {"VAZMIN", "Defluência mínima obrigatória"},
                                                              {"VOLMIN", "Volume mínimo operativo"},
                                                              {"GHMIN", "Geração hidráulica mínima"},
                                                              {"TURBMX", "Turbinamento máximo"},
                                                              {"TURBMN", "Turbinamento mínimo"},
                                                              {"VAZMAX", "Defluência máxima"},
                                                              {"LPPTBX", "Turbinamento máximo LPP por REE"},
                                                              {"LPPDFX", "Defluência máxima LPP por REE"},
                                                              {"LPPTBU", "Turbinamento máximo LPP por usina"},
                                                              {"LPPDFU", "Defluência máxima LPP por usina"},
                                                              {"ELETRI", "Restrição elétrica especial"},
                                                              {"RESTHQ", "Restrição hidráulica de vazão"},
                                                              {"RESTHV", "Restrição hidráulica de volume"}}),
                          real("Penalidade (R$/MWh)", 15, 22, 0), real("Penalidade 2º pat. (R$/MWh)", 25, 32, 0),
                          inteiro("REE/Submercado", 37, 39), inteiro("Patamar", 43, 44),
                          real("Penalidade ((R$/hm³)(mês/h))", 47, 54, 2),
                          real("Penalidade 2º pat. ((R$/hm³)(mês/h))", 57, 64, 2)}}}};
}

// curva.dat, manual do NEWAVE 30.0.2, secao 3.25: um registro de comentario e o registro do bloco 1
// (tipo e mes de penalizacao); dois registros de comentario e as penalidades por REE ate o 999 no
// campo 1; tres registros de comentario e, por REE, o registro com o numero do REE e um registro
// por ano com a curva de cada mes a cada 6 colunas a partir da 7, ate o 9999 no campo 1; um
// registro de comentario e os quatro parametros do processo iterativo em ordem fixa.
LayoutArquivoFixo curva() {
    return {"3.25",
            {{.titulo = "Penalização da curva",
              .linhas_cabecalho = 1,
              .colunas = {comOpcoes(inteiro("Tipo de penalização", 2, 4), {{"0", "Fixa"}, {"1", "Máxima violação"}}),
                          inteiro("Mês de penalização", 6, 8), comOpcoes(inteiro("Vminop sazonal pré/pós", 10, 12), CONSIDERA)},
              .max_registros = 1,
              .formulario = true},
             {.titulo = "Penalidades por REE",
              .linhas_cabecalho = 2,
              .terminador = "999",
              .colunas = {ref(inteiro("REE", 2, 4), Referencia::Ree), real("Penalidade ($/MWh)", 12, 18, 2)},
              .formulario = true,
              .formulario_ao_lado = "REEs da curva de segurança"},
             {.titulo = "Curva de segurança (% EARmáx)",
              .linhas_cabecalho = 3,
              .terminador = "9999",
              .colunas = {ref(deContexto(inteiro("REE", 2, 4), 0), Referencia::Ree), inteiro("Ano", 1, 4), real("Jan", 7, 11, 1),
                          real("Fev", 13, 17, 1), real("Mar", 19, 23, 1), real("Abr", 25, 29, 1), real("Mai", 31, 35, 1),
                          real("Jun", 37, 41, 1), real("Jul", 43, 47, 1), real("Ago", 49, 53, 1), real("Set", 55, 59, 1),
                          real("Out", 61, 65, 1), real("Nov", 67, 71, 1), real("Dez", 73, 77, 1)},
              .filtro = {preenchido(1, 1)},
              .contextos = {{{vazio(1, 1), preenchido(2, 4)}}}},
             {.titulo = "REEs da curva de segurança",
              .colunas = {ref(inteiro("REE", 2, 4), Referencia::Ree)},
              .filtro = {vazio(1, 1), preenchido(2, 4)},
              .mesma_regiao = true,
              .formulario = true},
             {.titulo = "Máximo de iterações",
              .linhas_cabecalho = 1,
              .colunas = {inteiro("Valor", 32, 34)},
              .max_registros = 1,
              .formulario = true},
             {.titulo = "Iteração de alteração da penalidade",
              .colunas = {inteiro("Valor", 32, 34)},
              .max_registros = 1,
              .formulario = true},
             {.titulo = "Tolerância", .colunas = {real("Valor", 30, 34, 0)}, .max_registros = 1, .formulario = true},
             {.titulo = "Relatório de convergência",
              .colunas = {comOpcoes(inteiro("Valor", 34, 34), {{"0", "Não gera relatório"}, {"1", "Gera relatório"}})},
              .max_registros = 1,
              .formulario = true}}};
}

// agrint.dat, manual do NEWAVE 30.0.2, secao 3.26: bloco 1 com tres registros de comentario e um
// registro por interligacao de cada agrupamento ate o 999 no campo 1; bloco 2 com tres registros de
// comentario e os limites de cada agrupamento por periodo, com um limite por patamar (ate cinco, de
// 8 em 8 colunas a partir da 23), ate o 999 no campo 1.
LayoutArquivoFixo agrint() {
    return {
        "3.26",
        {{.titulo = "Agrupamentos de interligações",
          .linhas_cabecalho = 3,
          .terminador = "999",
          .colunas = {inteiro("Agrupamento", 2, 4), ref(inteiro("Submercado origem", 6, 8), Referencia::Submercado),
                      ref(inteiro("Submercado destino", 10, 12), Referencia::Submercado), real("Coeficiente", 14, 20, 4)},
          .formulario = true},
         {.titulo = "Limites dos agrupamentos",
          .linhas_cabecalho = 3,
          .terminador = "999",
          .colunas = {
              ref(inteiro("Agrupamento", 2, 4), Referencia::Agrupamento), inteiro("Mês início", 7, 8), inteiro("Ano início", 10, 13),
              inteiro("Mês fim", 15, 16), inteiro("Ano fim", 18, 21), deCarga(real("Limite pat. 1 (MWmédio)", 23, 29, 0), 1),
              deCarga(real("Limite pat. 2 (MWmédio)", 31, 37, 0), 2), deCarga(real("Limite pat. 3 (MWmédio)", 39, 45, 0), 3),
              deCarga(real("Limite pat. 4 (MWmédio)", 47, 53, 0), 4), deCarga(real("Limite pat. 5 (MWmédio)", 55, 61, 0), 5)}}}};
}

// c_adic.dat, manual do NEWAVE 30.0.2, secao 3.27: dois registros de comentario e um bloco de
// conjuntos, cada um com o registro tipo 1 (submercado) seguido do tipo 3 opcional (periodo
// estatico inicial), um tipo 2 por ano de planejamento e o tipo 4 opcional (periodo estatico
// final), com um valor por mes a cada 8 colunas a partir da 8, ate o 999 no campo 1.
LayoutArquivoFixo c_adic() {
    return {"3.27",
            {{.titulo = "Carga/oferta adicional (MWmédio)",
              .linhas_cabecalho = 2,
              .terminador = "999",
              .colunas = {ref(deContexto(inteiro("Submercado", 2, 4), 0), Referencia::Submercado), texto("Ano", 1, 7),
                          real("Jan", 8, 14, 0), real("Fev", 16, 22, 0), real("Mar", 24, 30, 0), real("Abr", 32, 38, 0),
                          real("Mai", 40, 46, 0), real("Jun", 48, 54, 0), real("Jul", 56, 62, 0), real("Ago", 64, 70, 0),
                          real("Set", 72, 78, 0), real("Out", 80, 86, 0), real("Nov", 88, 94, 0), real("Dez", 96, 102, 0)},
              .filtro = {preenchido(96, 102)},
              .contextos = {{{preenchido(2, 4), vazio(96, 102)}}}},
             {.titulo = "Submercados",
              .colunas = {ref(inteiro("Submercado", 2, 4), Referencia::Submercado)},
              .filtro = {preenchido(2, 4), vazio(96, 102)},
              .mesma_regiao = true,
              .formulario = true}}};
}

// adterm.dat, manual do NEWAVE 30.0.2, secao 3.28: dois registros de comentario e, para cada usina
// GNL, um registro tipo 1 (usina e lag de antecipacao) seguido de um registro tipo 2 por lag
// (geracao antecipada de cada patamar de carga, ate cinco, a cada 12 colunas a partir da 25), ate o
// 9999 no campo 1.
LayoutArquivoFixo adterm() {
    return {
        "3.28",
        {{.titulo = "Geração antecipada por lag",
          .linhas_cabecalho = 2,
          .terminador = "9999",
          .colunas = {ref(deContexto(inteiro("Usina", 2, 5), 0), Referencia::UsinaTermica), deContexto(texto("Nome", 8, 19), 0),
                      ordinal("Lag", 0), deCarga(real("GT antecipada pat. 1 (MW)", 25, 34, 2), 1),
                      deCarga(real("GT antecipada pat. 2 (MW)", 37, 46, 2), 2),
                      deCarga(real("GT antecipada pat. 3 (MW)", 49, 58, 2), 3),
                      deCarga(real("GT antecipada pat. 4 (MW)", 61, 70, 2), 4),
                      deCarga(real("GT antecipada pat. 5 (MW)", 73, 82, 2), 5)},
          .filtro = {vazio(2, 5)},
          .contextos = {{{preenchido(2, 5)}}}},
         {.titulo = "Usinas GNL",
          .colunas = {ref(inteiro("Usina", 2, 5), Referencia::UsinaTermica), texto("Nome", 8, 19),
                      inteiro("Lag de antecipação", 22, 22)},
          .filtro = {preenchido(2, 5)},
          .mesma_regiao = true,
          .formulario = true}}};
}

// ghmin.dat, manual do NEWAVE 30.0.2, secao 3.29: dois registros de comentario e um registro por
// restricao de geracao hidraulica minima, campos 1 a 5; o ano aceita PRE e POS. O manual nao cita
// terminador, mas o 999 no campo 1 encerra a lista.
LayoutArquivoFixo ghmin() {
    return {"3.29",
            {{.titulo = "Gerações hidráulicas mínimas",
              .linhas_cabecalho = 2,
              .terminador = "999",
              .colunas = {ref(inteiro("Usina", 1, 3), Referencia::UsinaHidro), inteiro("Mês início", 6, 7),
                          texto("Ano início", 9, 12), inteiro("Patamar", 15, 15), real("GH mín. (MWmédio)", 18, 23, 1)}}}};
}

// sar.dat, manual do NEWAVE 30.0.2, secao 3.30: quatro blocos sem terminador; bloco 1 com um
// registro de comentario e dois registros em ordem fixa (mes do nivel meta e penalidade); bloco 2
// com dois registros de comentario e um registro por REE com o nivel meta de cada ano a cada 8
// colunas a partir da 17; bloco 3 com tres registros de comentario e os registros de periodo
// estatico inicial, um por ano e de periodo estatico final, com um indicador por mes a cada 4
// colunas a partir da 9; bloco 4 com dois registros de comentario, o tipo de serie hidrologica e,
// apos mais dois registros de comentario, um registro por REE com o ano do historico (tipo 2) ou
// com o percentual da media de cada mes (tipo 3). Os blocos 2 e 3 acabam na primeira linha fora do
// formato dos seus registros.
LayoutArquivoFixo sar() {
    return {"3.30",
            {{.titulo = "Mês do nível meta",
              .linhas_cabecalho = 1,
              .colunas = {inteiro("Valor", 14, 17)},
              .max_registros = 1,
              .formulario = true},
             {.titulo = "Penalidade ($/MWh)", .colunas = {real("Valor", 14, 21, 2)}, .max_registros = 1, .formulario = true},
             {.titulo = "Nível meta por REE",
              .linhas_cabecalho = 2,
              .colunas = {ref(inteiro("REE", 1, 4), Referencia::Ree), real("Nível meta ano", 17, 21, 1)},
              .passo_repeticao = 8,
              .filtro = {preenchido(1, 4), vazio(15, 16), preenchido(17, 21), vazio(22, 24)},
              .contigua = true},
             {.titulo = "Meses com aplicação da SAR",
              .linhas_cabecalho = 3,
              .colunas = {texto("Ano", 1, 7), comOpcoes(inteiro("Jan", 9, 11), APLICA_SAR),
                          comOpcoes(inteiro("Fev", 13, 15), APLICA_SAR), comOpcoes(inteiro("Mar", 17, 19), APLICA_SAR),
                          comOpcoes(inteiro("Abr", 21, 23), APLICA_SAR), comOpcoes(inteiro("Mai", 25, 27), APLICA_SAR),
                          comOpcoes(inteiro("Jun", 29, 31), APLICA_SAR), comOpcoes(inteiro("Jul", 33, 35), APLICA_SAR),
                          comOpcoes(inteiro("Ago", 37, 39), APLICA_SAR), comOpcoes(inteiro("Set", 41, 43), APLICA_SAR),
                          comOpcoes(inteiro("Out", 45, 47), APLICA_SAR), comOpcoes(inteiro("Nov", 49, 51), APLICA_SAR),
                          comOpcoes(inteiro("Dez", 53, 55), APLICA_SAR)},
              .filtro = {vazio(12, 12), vazio(16, 16), vazio(20, 20), vazio(24, 24), vazio(28, 28), vazio(32, 32), vazio(36, 36),
                         vazio(40, 40), vazio(44, 44), vazio(48, 48), vazio(52, 52), preenchido(53, 55)},
              .contigua = true},
             {.titulo = "Tipo de série hidrológica",
              .linhas_cabecalho = 2,
              .colunas = {comOpcoes(
                  inteiro("Valor", 2, 4),
                  {{"0", "Série condicionada"}, {"1", "Histórico de afluências"}, {"2", "Percentual da média mensal"}})},
              .max_registros = 1,
              .formulario = true},
             {.titulo = "Ano do histórico por REE",
              .linhas_cabecalho = 2,
              .colunas = {ref(inteiro("REE", 2, 4), Referencia::Ree), inteiro("Ano do histórico", 19, 22)},
              .filtro = {vazio(27, 32)}},
             {.titulo = "Percentual da média por REE (%)",
              .colunas = {ref(inteiro("REE", 2, 4), Referencia::Ree), real("Jan", 19, 24, 2), real("Fev", 27, 32, 2),
                          real("Mar", 35, 40, 2), real("Abr", 43, 48, 2), real("Mai", 51, 56, 2), real("Jun", 59, 64, 2),
                          real("Jul", 67, 72, 2), real("Ago", 75, 80, 2), real("Set", 83, 88, 2), real("Out", 91, 96, 2),
                          real("Nov", 99, 104, 2), real("Dez", 107, 112, 2)},
              .filtro = {preenchido(27, 32)},
              .mesma_regiao = true}}};
}

// cvar.dat, manual do NEWAVE 30.0.2, secao 3.31: tres blocos, cada um precedido de dois registros
// de comentario; o bloco 1 tem um registro com alfa e lambda constantes; os blocos 2 (alfa) e 3
// (lambda) tem o registro tipo 2 opcional (periodo estatico inicial), um tipo 1 por ano de
// planejamento e o tipo 3 opcional (periodo estatico final), com um valor por mes a cada 7 colunas
// a partir da 8; sem terminador, cada bloco acaba na primeira linha fora desse formato (separadores
// entre meses em branco e dezembro preenchido).
LayoutArquivoFixo cvar() {
    return {"3.31",
            {{.titulo = "Parâmetros constantes",
              .linhas_cabecalho = 2,
              .colunas = {real("Alfa (%)", 8, 12, 1), real("Lambda (%)", 15, 19, 1)},
              .max_registros = 1,
              .formulario = true},
             {.titulo = "Alfa variável no tempo (%)",
              .linhas_cabecalho = 2,
              .colunas = {texto("Ano", 1, 7), real("Jan", 8, 12, 1), real("Fev", 15, 19, 1), real("Mar", 22, 26, 1),
                          real("Abr", 29, 33, 1), real("Mai", 36, 40, 1), real("Jun", 43, 47, 1), real("Jul", 50, 54, 1),
                          real("Ago", 57, 61, 1), real("Set", 64, 68, 1), real("Out", 71, 75, 1), real("Nov", 78, 82, 1),
                          real("Dez", 85, 89, 1)},
              .filtro = {vazio(13, 14), vazio(20, 21), vazio(27, 28), vazio(34, 35), vazio(41, 42), vazio(48, 49), vazio(55, 56),
                         vazio(62, 63), vazio(69, 70), vazio(76, 77), vazio(83, 84), preenchido(85, 89)},
              .contigua = true},
             {.titulo = "Lambda variável no tempo (%)",
              .linhas_cabecalho = 2,
              .colunas = {texto("Ano", 1, 7), real("Jan", 8, 12, 1), real("Fev", 15, 19, 1), real("Mar", 22, 26, 1),
                          real("Abr", 29, 33, 1), real("Mai", 36, 40, 1), real("Jun", 43, 47, 1), real("Jul", 50, 54, 1),
                          real("Ago", 57, 61, 1), real("Set", 64, 68, 1), real("Out", 71, 75, 1), real("Nov", 78, 82, 1),
                          real("Dez", 85, 89, 1)},
              .filtro = {vazio(13, 14), vazio(20, 21), vazio(27, 28), vazio(34, 35), vazio(41, 42), vazio(48, 49), vazio(55, 56),
                         vazio(62, 63), vazio(69, 70), vazio(76, 77), vazio(83, 84), preenchido(85, 89)},
              .contigua = true}}};
}

// ree.dat, manual do NEWAVE 30.0.2, secao 3.32: bloco 1 com tres registros de comentario e um
// registro por REE ate o 999 no campo 1; bloco 2 com um unico registro, sem comentarios, com o flag
// de remocao das usinas ficticias nos periodos individualizados.
LayoutArquivoFixo ree() {
    return {"3.32",
            {{.titulo = "REEs",
              .linhas_cabecalho = 3,
              .terminador = "999",
              .colunas = {inteiro("REE", 2, 4), texto("Nome", 6, 15), ref(inteiro("Submercado", 19, 21), Referencia::Submercado),
                          inteiro("Mês agregação", 24, 25), inteiro("Ano agregação", 27, 30)}},
             {.titulo = "Usinas fictícias",
              .colunas = {comOpcoes(inteiro("Manter fictícias", 22, 25),
                                    {{"0", "Remove as fictícias"}, {"1", "Mantém as fictícias"}})},
              .max_registros = 1,
              .formulario = true}}};
}

// re.dat, manual do NEWAVE 30.0.2, secao 3.33: bloco 1 com dois registros de comentario e um
// registro por restricao eletrica (ate dez usinas, de 4 em 4 colunas a partir da 7) ate o 999 no
// campo 1; bloco 2 com dois registros de comentario e os limites por periodo e patamar ate o 999 no
// campo 1.
LayoutArquivoFixo re() {
    return {
        "3.33",
        {{.titulo = "Restrições elétricas",
          .linhas_cabecalho = 2,
          .terminador = "999",
          .colunas =
              {inteiro("Restrição", 1, 3), ref(inteiro("Usina 1", 7, 9), Referencia::UsinaHidro),
               ref(inteiro("Usina 2", 11, 13), Referencia::UsinaHidro), ref(inteiro("Usina 3", 15, 17), Referencia::UsinaHidro),
               ref(inteiro("Usina 4", 19, 21), Referencia::UsinaHidro), ref(inteiro("Usina 5", 23, 25), Referencia::UsinaHidro),
               ref(inteiro("Usina 6", 27, 29), Referencia::UsinaHidro), ref(inteiro("Usina 7", 31, 33), Referencia::UsinaHidro),
               ref(inteiro("Usina 8", 35, 37), Referencia::UsinaHidro), ref(inteiro("Usina 9", 39, 41), Referencia::UsinaHidro),
               ref(inteiro("Usina 10", 43, 45), Referencia::UsinaHidro)}},
         {.titulo = "Limites das restrições",
          .linhas_cabecalho = 2,
          .terminador = "999",
          .colunas = {inteiro("Restrição", 1, 3), inteiro("Mês início", 5, 6), inteiro("Ano início", 8, 11),
                      inteiro("Mês fim", 13, 14), inteiro("Ano fim", 16, 19), inteiro("Patamar", 21, 21),
                      real("Limite (MWmédio)", 23, 37, 2)}}}};
}

// selcor.dat, manual do NEWAVE 30.0.2, secao 3.34: dois registros de comentario seguidos de 7
// registros de parametros da selecao de cortes de Benders, um por linha, com rotulo livre nas
// colunas 1 a 63 e valores nas colunas 64 a 67 (e 70 a 73 nas janelas de impressao dos registros 6
// e 7). Cada parametro e uma secao de um registro, lida pela posicao da linha; o arquivo so e lido
// quando o registro 65 do dger.dat vale 1.
LayoutArquivoFixo selcor() {
    return {"3.34",
            {{.titulo = "Iteração inicial da seleção de cortes",
              .linhas_cabecalho = 2,
              .colunas = {inteiro("Valor", 64, 67)},
              .max_registros = 1},
             {.titulo = "Janela de cortes ativos (k2)", .colunas = {inteiro("Valor", 64, 67)}, .max_registros = 1},
             {.titulo = "Cortes adicionados por iteração (nadic)", .colunas = {inteiro("Valor", 64, 67)}, .max_registros = 1},
             {.titulo = "Inclui cortes da própria iteração",
              .colunas = {comOpcoes(inteiro("Valor", 64, 67), {{"0", "Não inclui"}, {"1", "Inclui"}})},
              .max_registros = 1},
             {.titulo = "Imprime relatório", .colunas = {comOpcoes(inteiro("Valor", 64, 67), IMPRIME)}, .max_registros = 1},
             {.titulo = "Períodos do relatório",
              .colunas = {inteiro("Período inicial", 64, 67), inteiro("Período final", 70, 73)},
              .max_registros = 1},
             {.titulo = "Séries do relatório",
              .colunas = {inteiro("Série inicial", 64, 67), inteiro("Série final", 70, 73)},
              .max_registros = 1}},
            true};
}

// tecno.dat, manual do NEWAVE 30.0.2, secao 3.35: dois registros de comentario e um registro por
// tecnologia ate o 999 no campo 1.
LayoutArquivoFixo tecno() {
    return {"3.35",
            {{.titulo = "Tecnologias",
              .linhas_cabecalho = 2,
              .terminador = "999",
              .colunas = {inteiro("Tecnologia", 2, 4), texto("Nome", 7, 16), real("Fator de emissão (gCO2eq/kWh)", 19, 23, 0)}}}};
}

// polinjus.csv, manual do NEWAVE 30.0.2, secoes 3.4 e 3.41.3: arquivo CSV (campos separados por
// ponto e virgula, linhas comecadas por & sao comentario) cujo primeiro campo identifica o dado:
// curva de jusante de cada usina com a altura de referencia, numero de partes do polinomio por
// partes de cada curva e cada parte com a faixa de vazao de jusante e os coeficientes de grau 0 a
// 4.
LayoutArquivoFixo polinjus() {
    return {"3.41.3",
            {{.titulo = "Curvas de jusante",
              .colunas = {ref(inteiro("Usina", 2, 2), Referencia::UsinaHidro), inteiro("Curva", 3, 3),
                          real("Altura de referência (m)", 4, 4, 0)},
              .filtro = {igual(1, 1, {"HIDRELETRICA-CURVAJUSANTE"})}},
             {.titulo = "Polinômios por partes",
              .colunas = {ref(inteiro("Usina", 2, 2), Referencia::UsinaHidro), inteiro("Curva", 3, 3), inteiro("Partes", 4, 4)},
              .filtro = {igual(1, 1, {"HIDRELETRICA-CURVAJUSANTE-POLINOMIOPORPARTES"})},
              .mesma_regiao = true},
             {.titulo = "Partes dos polinômios",
              .colunas = {ref(inteiro("Usina", 2, 2), Referencia::UsinaHidro), inteiro("Curva", 3, 3), inteiro("Parte", 4, 4),
                          real("Vazão jusante mín. (m³/s)", 5, 5, 0), real("Vazão jusante máx. (m³/s)", 6, 6, 0),
                          real("Coef. grau 0", 7, 7, 0), real("Coef. grau 1", 8, 8, 0), real("Coef. grau 2", 9, 9, 0),
                          real("Coef. grau 3", 10, 10, 0), real("Coef. grau 4", 11, 11, 0)},
              .filtro = {igual(1, 1, {"HIDRELETRICA-CURVAJUSANTE-POLINOMIOPORPARTES-SEGMENTO"})},
              .mesma_regiao = true}},
            false,
            ';'};
}

// volref_saz.dat, manual do NEWAVE 30.0.2, secao 3.42: tres registros de comentario e um registro
// por usina hidroeletrica, com o volume util de referencia de cada mes a cada 10 colunas a partir
// da 20; sem terminador, a lista vai ate o fim do arquivo.
LayoutArquivoFixo volref_saz() {
    return {"3.42",
            {{.titulo = "Volumes de referência",
              .linhas_cabecalho = 3,
              .colunas = {ref(inteiro("Usina", 1, 3), Referencia::UsinaHidro), texto("Nome", 6, 17), real("Jan (hm³)", 20, 27, 2),
                          real("Fev (hm³)", 30, 37, 2), real("Mar (hm³)", 40, 47, 2), real("Abr (hm³)", 50, 57, 2),
                          real("Mai (hm³)", 60, 67, 2), real("Jun (hm³)", 70, 77, 2), real("Jul (hm³)", 80, 87, 2),
                          real("Ago (hm³)", 90, 97, 2), real("Set (hm³)", 100, 107, 2), real("Out (hm³)", 110, 117, 2),
                          real("Nov (hm³)", 120, 127, 2), real("Dez (hm³)", 130, 137, 2)}}}};
}

// restricao-eletrica.csv, manual do NEWAVE 30.0.2, secoes 3.4 e 3.45: arquivo CSV (campos separados
// por ponto e virgula, linhas comecadas por & sao comentario) cujo primeiro campo identifica o
// dado: RE com a equacao de cada restricao eletrica especial, RE-HORIZ-PER com os periodos de
// validade e RE-LIM-FORM-PER-PAT com os limites por periodo e patamar. Os limites ficam como texto
// porque podem ser condicionais, como se(demanda(1) < 1000, 10000, 8500).
LayoutArquivoFixo restricao_eletrica() {
    return {
        "3.45",
        {{.titulo = "Equações", .colunas = {inteiro("Restrição", 2, 2), texto("Equação", 3, 3)}, .filtro = {igual(1, 1, {"RE"})}},
         {.titulo = "Períodos de validade",
          .colunas = {inteiro("Restrição", 2, 2), texto("Período inicial", 3, 3), texto("Período final", 4, 4)},
          .filtro = {igual(1, 1, {"RE-HORIZ-PER"})},
          .mesma_regiao = true},
         {.titulo = "Limites",
          .colunas = {inteiro("Restrição", 2, 2), texto("Período inicial", 3, 3), texto("Período final", 4, 4),
                      inteiro("Patamar", 5, 5), texto("Limite inferior", 6, 6), texto("Limite superior", 7, 7)},
          .filtro = {igual(1, 1, {"RE-LIM-FORM-PER-PAT"})},
          .mesma_regiao = true}},
        false,
        ';'};
}
// volumes-referencia.csv, manual do NEWAVE 30.0.2, secoes 3.4 e 3.41.4: arquivo CSV (campos
// separados por ponto e virgula, linhas comecadas por & sao comentario) com o volume de referencia
// total de cada usina por periodo de validade, com o identificador longo ou o curto
// (CADH-VOL-REF-PER) e datas no formato AAAA/MM, e o tipo de volume referencial
// (VOLUME-REFERENCIAL-TIPO-PADRAO, opcional; sem ele vale o volume inicial do confhd.dat). A tabela
// vem antes do tipo porque secao de um registro so corta a regiao das seguintes.
LayoutArquivoFixo volumes_referencia() {
    return {"3.41.4",
            {{.titulo = "Volumes de referência por período",
              .colunas = {ref(inteiro("Usina", 2, 2), Referencia::UsinaHidro), texto("Data inicial", 3, 3),
                          texto("Data final", 4, 4), real("Volume de referência total", 5, 5, 0)},
              .filtro = {igual(1, 1, {"HIDRELETRICA-CADASTRO-RESERVATORIO-VOLUME-REFERENCIA-PERIODO", "CADH-VOL-REF-PER"})}},
             {.titulo = "Tipo de volume referencial",
              .colunas = {comOpcoes(inteiro("Tipo", 2, 2), {{"0", "Volume inicial"}, {"1", "Volume de referência por período"}})},
              .filtro = {igual(1, 1, {"VOLUME-REFERENCIAL-TIPO-PADRAO"})},
              .mesma_regiao = true,
              .max_registros = 1,
              .formulario = true}},
            false,
            ';'};
}

// indices.csv, manual do NEWAVE 30.0.2, secao 3.4: arquivo CSV (campos separados por ponto e
// virgula, linhas comecadas por & sao comentario) com uma linha por funcionalidade: identificador,
// descricao (ignorada pelo programa) e nome do arquivo CSV com os dados dela. O identificador e
// livre porque o manual manda acrescentar ali as funcionalidades novas e a tabela dele esta
// incompleta.
LayoutArquivoFixo indices() {
    return {"3.4",
            {{.titulo = "Funcionalidades", .colunas = {texto("Identificador", 1, 1), texto("Descrição", 2, 2), texto("Arquivo", 3, 3)}}},
            false,
            ';'};
}

// abertura.dat, manual do NEWAVE 30.0.2, secao 3.36: tres registros de comentario e, sem
// terminador, o registro tipo 2 opcional (PRE, periodo estatico inicial), um tipo 1 por ano de
// planejamento e o tipo 3 opcional (POS, periodo estatico final), com o numero de aberturas de cada
// mes a cada 5 colunas a partir da 7 (o manual da os meses 1, 2 e 12; os do meio seguem o passo).
// Lido quando o registro 18 do dger.dat pede aberturas variaveis (colunas 27 a 30).
LayoutArquivoFixo abertura() {
    return {"3.36",
            {{.titulo = "Número de aberturas por mês",
              .linhas_cabecalho = 3,
              .colunas = {texto("Ano", 1, 6), inteiro("Jan", 7, 9), inteiro("Fev", 12, 14), inteiro("Mar", 17, 19),
                          inteiro("Abr", 22, 24), inteiro("Mai", 27, 29), inteiro("Jun", 32, 34), inteiro("Jul", 37, 39),
                          inteiro("Ago", 42, 44), inteiro("Set", 47, 49), inteiro("Out", 52, 54), inteiro("Nov", 57, 59),
                          inteiro("Dez", 62, 64)}}}};
}

// gee.dat, manual do NEWAVE 30.0.2, secao 3.37: dois registros de comentario e um unico registro
// com a penalidade, o fator de desagregacao mensal do limite anual de cada mes a cada 6 colunas a
// partir da 11 e o limite de emissao de cada ano de planejamento a cada 13 colunas a partir da 83.
// Lido quando o registro 82 do dger.dat e 1. Fica como tabela de uma linha: o numero de anos sai do
// comprimento da linha.
LayoutArquivoFixo gee() {
    return {"3.37",
            {{.titulo = "Limites de emissão de GEE",
              .linhas_cabecalho = 2,
              .colunas = {real("Penalidade", 1, 8, 2), real("Fator jan", 11, 15, 3), real("Fator fev", 17, 21, 3),
                          real("Fator mar", 23, 27, 3), real("Fator abr", 29, 33, 3), real("Fator mai", 35, 39, 3),
                          real("Fator jun", 41, 45, 3), real("Fator jul", 47, 51, 3), real("Fator ago", 53, 57, 3),
                          real("Fator set", 59, 63, 3), real("Fator out", 65, 69, 3), real("Fator nov", 71, 75, 3),
                          real("Fator dez", 77, 81, 3), real("Limite de emissão (Mton CO2eq) ano", 83, 94, 2)},
              .passo_repeticao = 13,
              .max_registros = 1}}};
}

// clasgas.dat, manual do NEWAVE 30.0.2, secao 3.38: dois registros de comentario e um registro por
// classe de gas natural, sem terminador, com heat rate, PCI, o fator de desagregacao mensal da
// disponibilidade anual de cada mes a cada 6 colunas a partir da 46 e a disponibilidade de cada ano
// de planejamento a cada 13 colunas a partir da 118. Lido quando o registro 84 do dger.dat e 1.
LayoutArquivoFixo clasgas() {
    return {"3.38",
            {{.titulo = "Classes de gás natural",
              .linhas_cabecalho = 2,
              .colunas = {inteiro("Classe", 2, 5), texto("Nome", 7, 18), real("Heat rate (BTU/kWh)", 20, 31, 2),
                          real("PCI (kcal/m³)", 33, 44, 2), real("Fator jan", 46, 50, 3), real("Fator fev", 52, 56, 3),
                          real("Fator mar", 58, 62, 3), real("Fator abr", 64, 68, 3), real("Fator mai", 70, 74, 3),
                          real("Fator jun", 76, 80, 3), real("Fator jul", 82, 86, 3), real("Fator ago", 88, 92, 3),
                          real("Fator set", 94, 98, 3), real("Fator out", 100, 104, 3), real("Fator nov", 106, 110, 3),
                          real("Fator dez", 112, 116, 3), real("Disponibilidade (milhões m³/ano) ano", 118, 129, 2)},
              .passo_repeticao = 13}}};
}
}  // namespace

// Layout dos arquivos de texto do NEWAVE que tem tabela editavel, pelo nome padrao do arquivo;
// nullptr para os que ainda so tem previa.
const LayoutArquivoFixo* layoutNewave(const std::string& nome_padrao) {
    static const std::map<std::string, LayoutArquivoFixo> layouts = {
        {"conft.dat", conft()}, {"term.dat", term()}, {"expt.dat", expt()}, {"clast.dat", clast()}, {"manutt.dat", manutt()},
        {"arquivos.dat", arquivos()}, {"dger.dat", dger()}, {"shist.dat", shist()}, {"sistema.dat", sistema()},
        {"patamar.dat", patamar()}, {"confhd.dat", confhd()}, {"exph.dat", exph()}, {"loss.dat", loss()},
        {"dsvagua.dat", dsvagua()}, {"vazpast.dat", vazpast()}, {"gtminpat.dat", gtminpat()}, {"penalid.dat", penalid()},
        {"curva.dat", curva()}, {"agrint.dat", agrint()}, {"c_adic.dat", c_adic()}, {"adterm.dat", adterm()},
        {"ghmin.dat", ghmin()}, {"sar.dat", sar()}, {"cvar.dat", cvar()}, {"ree.dat", ree()}, {"re.dat", re()},
        {"selcor.dat", selcor()}, {"tecno.dat", tecno()}, {"polinjus.csv", polinjus()}, {"volref_saz.dat", volref_saz()},
        {"restricao-eletrica.csv", restricao_eletrica()}, {"volumes-referencia.csv", volumes_referencia()},
        {"indices.csv", indices()}, {"abertura.dat", abertura()}, {"gee.dat", gee()}, {"clasgas.dat", clasgas()},
    };
    auto it = layouts.find(nome_padrao);
    return it == layouts.end() ? nullptr : &it->second;
}

namespace {
int primeiroInteiroPositivo(const ArquivoFixo& arquivo) {
    if (arquivo.secoes().empty() || arquivo.secoes()[0].linhas.empty()) return 0;
    const std::string valor = arquivo.valor(0, 0, 0);
    try {
        size_t lidos = 0;
        const int n = std::stoi(valor, &lidos);
        return lidos == valor.size() && n > 0 ? n : 0;
    } catch (...) {
        return 0;
    }
}
}  // namespace

// Numero de patamares de carga do deck: o registro do bloco 1 do patamar.dat (manual, secao 3.8),
// lido pelo layout dele; 0 se o arquivo nao traz um numero valido.
int patamaresDeCarga(const ArquivoFixo& patamar) { return primeiroInteiroPositivo(patamar); }

// Numero de patamares de deficit: o registro do bloco 1 do sistema.dat (manual, secao 3.7), lido
// pelo layout dele; 0 se o arquivo nao traz um numero valido.
int patamaresDeDeficit(const ArquivoFixo& sistema) { return primeiroInteiroPositivo(sistema); }

bool dependeDePatamares(const LayoutArquivoFixo& layout) {
    for (const SecaoFixa& secao : layout.secoes)
        for (const ColunaFixa& coluna : secao.colunas)
            if (coluna.patamares != Patamares::Nenhum) return true;
    return false;
}

// Copia do layout sem as colunas de patamares que o deck nao tem: coluna do patamar k de carga (ou de
// deficit) sai quando o numero de patamares desse tipo e conhecido e menor que k. Numero 0
// (desconhecido) mantem todas as colunas.
LayoutArquivoFixo ajustarPatamares(const LayoutArquivoFixo& layout, NumeroPatamares numero) {
    LayoutArquivoFixo ajustado = layout;
    for (SecaoFixa& secao : ajustado.secoes)
        std::erase_if(secao.colunas, [&](const ColunaFixa& c) {
            const int limite = c.patamares == Patamares::Carga ? numero.carga : c.patamares == Patamares::Deficit ? numero.deficit : 0;
            return limite > 0 && c.patamar > limite;
        });
    return ajustado;
}

// Onde estao os itens de cada cadastro referenciado: arquivo, secao e colunas do codigo e do nome. O
// posto vem do postos.dat, binario, e e tratado a parte pelo repositorio do deck.
FonteReferencia fonteReferencia(Referencia referencia) {
    switch (referencia) {
    case Referencia::Submercado: return {"sistema.dat", 1, 0, 1};
    case Referencia::Ree: return {"ree.dat", 0, 0, 1};
    case Referencia::UsinaHidro: return {"confhd.dat", 0, 0, 1};
    case Referencia::UsinaTermica: return {"conft.dat", 0, 0, 1};
    case Referencia::ClasseTermica: return {"clast.dat", 0, 0, 1};
    case Referencia::Tecnologia: return {"tecno.dat", 0, 0, 1};
    case Referencia::ClasseGas: return {"clasgas.dat", 0, 0, 1};
    default: return {"", -1, -1, -1};
    }
}
