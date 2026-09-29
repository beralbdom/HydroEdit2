#include "catalogo_newave.h"
#include <map>

// Abas do editor, na ordem em que aparecem. O agrupamento dos arquivos em abas e proposta do app; os
// arquivos e titulos vem do capitulo 3 do manual do NEWAVE 30.0.2.
const std::vector<QString>& secoesNewave() {
    static const std::vector<QString> secoes = {
        QStringLiteral("Cadastro"),          QStringLiteral("Configuração"), QStringLiteral("Modificações"),
        QStringLiteral("Hidrologia"),        QStringLiteral("Sistema e carga"), QStringLiteral("Restrições"),
        QStringLiteral("Dados gerais"),
    };
    return secoes;
}

// Arquivos de entrada do NEWAVE por aba. O titulo e a secao vem do indice do capitulo 3 do manual;
// o rotulo e a descricao do arquivo no arquivos.dat (secao 3.3), usada para achar o nome real no
// deck. Arquivos sem rotulo (hidr.dat, postos.dat, vazoes.dat, os CSV) usam sempre o nome padrao.
// modif.dat nao entra aqui: a aba Modificacoes tem pagina propria.
const std::vector<ArquivoNewave>& catalogoNewave() {
    static const std::vector<ArquivoNewave> arquivos = {
        {QStringLiteral("Cadastro"), QStringLiteral("Usinas hidráulicas"), QStringLiteral("hidr.dat"), {}, QStringLiteral("3.11")},
        {QStringLiteral("Cadastro"), QStringLiteral("Usinas térmicas"), QStringLiteral("term.dat"), QStringLiteral("DADOS DAS USINAS TERMICAS"), QStringLiteral("3.16")},
        {QStringLiteral("Cadastro"), QStringLiteral("Classes térmicas"), QStringLiteral("clast.dat"), QStringLiteral("DADOS DAS CLASSES TERMICAS"), QStringLiteral("3.18")},
        {QStringLiteral("Cadastro"), QStringLiteral("Postos fluviométricos"), QStringLiteral("postos.dat"), {}, QStringLiteral("3.10")},
        {QStringLiteral("Cadastro"), QStringLiteral("REEs"), QStringLiteral("ree.dat"), QStringLiteral("DADOS DOS RESER.EQ.ENERGIA"), QStringLiteral("3.32")},
        {QStringLiteral("Cadastro"), QStringLiteral("Tecnologias"), QStringLiteral("tecno.dat"), QStringLiteral("ARQUIVO DE TECNOLOGIAS"), QStringLiteral("3.35")},

        {QStringLiteral("Configuração"), QStringLiteral("Configuração hidráulica"), QStringLiteral("confhd.dat"), QStringLiteral("CONFIGURACAO HIDRAULICA"), QStringLiteral("3.9")},
        {QStringLiteral("Configuração"), QStringLiteral("Configuração térmica"), QStringLiteral("conft.dat"), QStringLiteral("CONFIGURACAO TERMICA"), QStringLiteral("3.15")},
        {QStringLiteral("Configuração"), QStringLiteral("Expansão hidráulica"), QStringLiteral("exph.dat"), QStringLiteral("DADOS DE EXPANSAO HIDRAULICA"), QStringLiteral("3.13")},
        {QStringLiteral("Configuração"), QStringLiteral("Expansão térmica"), QStringLiteral("expt.dat"), QStringLiteral("ARQUIVO DE EXPANSAO TERMICA"), QStringLiteral("3.17")},
        {QStringLiteral("Configuração"), QStringLiteral("Manutenções programadas"), QStringLiteral("manutt.dat"), QStringLiteral("ARQUIVO DE MANUT.PROG. UTE'S"), QStringLiteral("3.19")},

        {QStringLiteral("Hidrologia"), QStringLiteral("Vazões históricas"), QStringLiteral("vazoes.dat"), {}, QStringLiteral("3.14")},
        {QStringLiteral("Hidrologia"), QStringLiteral("Tendência hidrológica por posto"), QStringLiteral("vazpast.dat"), QStringLiteral("ARQUIVO C/TEND. HIDROLOGICA"), QStringLiteral("3.22.3")},
        {QStringLiteral("Hidrologia"), QStringLiteral("Outros usos da água"), QStringLiteral("dsvagua.dat"), QStringLiteral("ARQUIVO DSVAGUA"), QStringLiteral("3.21")},
        {QStringLiteral("Hidrologia"), QStringLiteral("Volume de referência sazonal"), QStringLiteral("volref_saz.dat"), QStringLiteral("ARQ. C/ VOLUME REF. SAZONAL"), QStringLiteral("3.42")},
        {QStringLiteral("Hidrologia"), QStringLiteral("Polinômios de jusante por partes"), QStringLiteral("polinjus.csv"), {}, QStringLiteral("3.41.3")},

        {QStringLiteral("Sistema e carga"), QStringLiteral("Submercados"), QStringLiteral("sistema.dat"), QStringLiteral("DADOS DOS SUBSISTEMAS"), QStringLiteral("3.7")},
        {QStringLiteral("Sistema e carga"), QStringLiteral("Patamares de carga"), QStringLiteral("patamar.dat"), QStringLiteral("ARQUIVO DE PATAMARES MERCADO"), QStringLiteral("3.8")},
        {QStringLiteral("Sistema e carga"), QStringLiteral("Carga e oferta adicionais"), QStringLiteral("c_adic.dat"), QStringLiteral("ARQUIVO C/CARGAS ADICIONAIS"), QStringLiteral("3.27")},
        {QStringLiteral("Sistema e carga"), QStringLiteral("Agrupamento livre de interligações"), QStringLiteral("agrint.dat"), QStringLiteral("ARQUIVO AGRUPAMENTO LIVRE"), QStringLiteral("3.26")},
        {QStringLiteral("Sistema e carga"), QStringLiteral("Perdas na rede de transmissão"), QStringLiteral("loss.dat"), QStringLiteral("ARQUIVO C/FATORES DE PERDAS"), QStringLiteral("3.20")},
        {QStringLiteral("Sistema e carga"), QStringLiteral("Patamares de geração térmica mínima"), QStringLiteral("gtminpat.dat"), QStringLiteral("ARQUIVO C/PATAMARES GTMIN"), QStringLiteral("3.23")},
        {QStringLiteral("Sistema e carga"), QStringLiteral("Despacho antecipado de térmicas GNL"), QStringLiteral("adterm.dat"), QStringLiteral("ARQUIVO DESP. ANTEC. GNL"), QStringLiteral("3.28")},

        {QStringLiteral("Restrições"), QStringLiteral("Penalidades"), QStringLiteral("penalid.dat"), QStringLiteral("ARQUIVO P/PENALID. POR DESV."), QStringLiteral("3.24")},
        {QStringLiteral("Restrições"), QStringLiteral("Curva de aversão a risco"), QStringLiteral("curva.dat"), QStringLiteral("ARQUIVO C.GUIA / PENAL.VMINT"), QStringLiteral("3.25")},
        {QStringLiteral("Restrições"), QStringLiteral("Aversão a risco: CVaR"), QStringLiteral("cvar.dat"), QStringLiteral("ARQUIVO AVERSAO RISCO - CVAR"), QStringLiteral("3.31")},
        {QStringLiteral("Restrições"), QStringLiteral("Aversão a risco: SAR"), QStringLiteral("sar.dat"), QStringLiteral("ARQUIVO AVERSAO RISCO - SAR"), QStringLiteral("3.30")},
        {QStringLiteral("Restrições"), QStringLiteral("Geração hidráulica mínima"), QStringLiteral("ghmin.dat"), QStringLiteral("ARQUIVO GER. HIDR. MIN"), QStringLiteral("3.29")},
        {QStringLiteral("Restrições"), QStringLiteral("Restrições elétricas internas aos REEs"), QStringLiteral("re.dat"), QStringLiteral("ARQUIVO RESTRICOES ELETRICAS"), QStringLiteral("3.33")},
        {QStringLiteral("Restrições"), QStringLiteral("Restrições elétricas especiais"), QStringLiteral("restricao-eletrica.csv"), {}, QStringLiteral("3.45")},

        {QStringLiteral("Dados gerais"), QStringLiteral("Dados gerais"), QStringLiteral("dger.dat"), QStringLiteral("DADOS GERAIS"), QStringLiteral("3.5")},
        {QStringLiteral("Dados gerais"), QStringLiteral("Nomes dos arquivos"), QStringLiteral("arquivos.dat"), {}, QStringLiteral("3.3")},
        {QStringLiteral("Dados gerais"), QStringLiteral("Séries históricas da simulação final"), QStringLiteral("shist.dat"), QStringLiteral("ARQUIVO DE S.HISTORICAS S.F."), QStringLiteral("3.6")},
        {QStringLiteral("Dados gerais"), QStringLiteral("Seleção de cortes"), QStringLiteral("selcor.dat"), {}, QStringLiteral("3.34")},
    };
    return arquivos;
}

// Palavras-chave do modif.dat e a descricao de cada uma, da tabela da secao 3.12 do manual do
// NEWAVE 30.0.2. A categoria, que agrupa a arvore da aba Modificacoes, e proposta do app.
const std::vector<PalavraChaveModif>& palavrasChaveModif() {
    static const std::vector<PalavraChaveModif> chaves = {
        {QStringLiteral("VOLMIN"), QStringLiteral("Volumes"), QStringLiteral("Volume mínimo operativo")},
        {QStringLiteral("VOLMAX"), QStringLiteral("Volumes"), QStringLiteral("Volume máximo operativo")},
        {QStringLiteral("VMAXT"), QStringLiteral("Volumes"), QStringLiteral("Volume máximo, com data")},
        {QStringLiteral("VMINT"), QStringLiteral("Volumes"), QStringLiteral("Volume mínimo, com data")},
        {QStringLiteral("VMINP"), QStringLiteral("Volumes"), QStringLiteral("Volume mínimo com penalidade, com data")},
        {QStringLiteral("VAZMIN"), QStringLiteral("Vazões"), QStringLiteral("Vazão mínima")},
        {QStringLiteral("VAZMINT"), QStringLiteral("Vazões"), QStringLiteral("Vazão mínima, com data")},
        {QStringLiteral("VAZMAXT"), QStringLiteral("Vazões"), QStringLiteral("Defluência máxima com penalidade, com data e por patamar")},
        {QStringLiteral("TURBMAXT"), QStringLiteral("Vazões"), QStringLiteral("Turbinamento máximo com penalidade, com data e por patamar")},
        {QStringLiteral("TURBMINT"), QStringLiteral("Vazões"), QStringLiteral("Turbinamento mínimo com penalidade, com data e por patamar")},
        {QStringLiteral("CFUGA"), QStringLiteral("Níveis"), QStringLiteral("Canal de fuga, com data")},
        {QStringLiteral("CMONT"), QStringLiteral("Níveis"), QStringLiteral("Nível de montante, com data")},
        {QStringLiteral("NUMCNJ"), QStringLiteral("Máquinas"), QStringLiteral("Total de conjuntos de máquinas")},
        {QStringLiteral("NUMMAQ"), QStringLiteral("Máquinas"), QStringLiteral("Número de máquinas de um conjunto")},
        {QStringLiteral("POTEFE"), QStringLiteral("Máquinas"), QStringLiteral("Potência efetiva de um conjunto")},
        {QStringLiteral("NUMBAS"), QStringLiteral("Máquinas"), QStringLiteral("Número de unidades de base")},
        {QStringLiteral("PRODESP"), QStringLiteral("Operação e polinômios"), QStringLiteral("Produtibilidade específica")},
        {QStringLiteral("TEIF"), QStringLiteral("Operação e polinômios"), QStringLiteral("Taxa esperada de indisponibilidade forçada")},
        {QStringLiteral("IP"), QStringLiteral("Operação e polinômios"), QStringLiteral("Indisponibilidade programada")},
        {QStringLiteral("PERDHIDR"), QStringLiteral("Operação e polinômios"), QStringLiteral("Perda hidráulica")},
        {QStringLiteral("COEFEVAP"), QStringLiteral("Operação e polinômios"), QStringLiteral("Coeficiente de evaporação mensal")},
        {QStringLiteral("COTAREA"), QStringLiteral("Operação e polinômios"), QStringLiteral("Polinômio cota-área")},
        {QStringLiteral("VOLCOTA"), QStringLiteral("Operação e polinômios"), QStringLiteral("Polinômio volume-cota")},
        {QStringLiteral("CDESVIO"), QStringLiteral("Operação e polinômios"), QStringLiteral("Canal de desvio: usina a jusante e vazão máxima")},
    };
    return chaves;
}

// Grupo de cada arquivo dentro da sua aba, para os cabecalhos da arvore; proposta do app, como as
// abas. Arquivo sem grupo fica sem cabecalho.
QString grupoNewave(const QString& nome_padrao) {
    static const std::map<QString, QString> grupos = {
        {QStringLiteral("hidr.dat"), QStringLiteral("Usinas")},
        {QStringLiteral("term.dat"), QStringLiteral("Usinas")},
        {QStringLiteral("clast.dat"), QStringLiteral("Referências")},
        {QStringLiteral("postos.dat"), QStringLiteral("Referências")},
        {QStringLiteral("ree.dat"), QStringLiteral("Referências")},
        {QStringLiteral("tecno.dat"), QStringLiteral("Referências")},
        {QStringLiteral("confhd.dat"), QStringLiteral("Hidroelétricas")},
        {QStringLiteral("exph.dat"), QStringLiteral("Hidroelétricas")},
        {QStringLiteral("conft.dat"), QStringLiteral("Termoelétricas")},
        {QStringLiteral("expt.dat"), QStringLiteral("Termoelétricas")},
        {QStringLiteral("manutt.dat"), QStringLiteral("Termoelétricas")},
        {QStringLiteral("vazoes.dat"), QStringLiteral("Vazões")},
        {QStringLiteral("vazpast.dat"), QStringLiteral("Vazões")},
        {QStringLiteral("dsvagua.dat"), QStringLiteral("Operação hidráulica")},
        {QStringLiteral("volref_saz.dat"), QStringLiteral("Operação hidráulica")},
        {QStringLiteral("polinjus.csv"), QStringLiteral("Operação hidráulica")},
        {QStringLiteral("sistema.dat"), QStringLiteral("Carga")},
        {QStringLiteral("patamar.dat"), QStringLiteral("Carga")},
        {QStringLiteral("c_adic.dat"), QStringLiteral("Carga")},
        {QStringLiteral("agrint.dat"), QStringLiteral("Interligações")},
        {QStringLiteral("loss.dat"), QStringLiteral("Interligações")},
        {QStringLiteral("gtminpat.dat"), QStringLiteral("Termoelétricas")},
        {QStringLiteral("adterm.dat"), QStringLiteral("Termoelétricas")},
        {QStringLiteral("penalid.dat"), QStringLiteral("Penalidades e risco")},
        {QStringLiteral("curva.dat"), QStringLiteral("Penalidades e risco")},
        {QStringLiteral("cvar.dat"), QStringLiteral("Penalidades e risco")},
        {QStringLiteral("sar.dat"), QStringLiteral("Penalidades e risco")},
        {QStringLiteral("ghmin.dat"), QStringLiteral("Operativas")},
        {QStringLiteral("re.dat"), QStringLiteral("Operativas")},
        {QStringLiteral("restricao-eletrica.csv"), QStringLiteral("Operativas")},
    };
    auto it = grupos.find(nome_padrao);
    return it == grupos.end() ? QString() : it->second;
}
