#include "dados_deck.h"
#include <QDir>
#include <QFileInfo>
#include <QUndoCommand>
#include <filesystem>
#include <algorithm>
#include <limits>
#include <utility>
#include "catalogo_newave.h"
#include "layouts_newave.h"
#include "patamares_newave.h"

// Edicao de arquivos do deck na pilha de desfazer. O push chama redo logo depois da edicao, que ja
// esta aplicada, entao o primeiro redo nao faz nada.
class ComandoDeck : public QUndoCommand {
public:
    ComandoDeck(DadosDeck* deck, const QString& rotulo, std::vector<DadosDeck::Diferenca> diferencas)
        : deck_(deck), diferencas_(std::move(diferencas)) {
        setText(rotulo);
    }
    void undo() override { deck_->restaurar(diferencas_, true); }
    void redo() override {
        if (std::exchange(aplicado_, false)) return;
        deck_->restaurar(diferencas_, false);
    }

private:
    DadosDeck* deck_;
    std::vector<DadosDeck::Diferenca> diferencas_;
    bool aplicado_ = true;
};

DadosDeck::DadosDeck(QObject* parent) : QObject(parent) {}

// Arquivos do deck que nao sao texto: o hidr.dat, que tem editor proprio e fica fora do repositorio,
// e o postos.dat e o vazoes.dat, registros binarios (manual do NEWAVE, secoes 3.10 e 3.14).
bool DadosDeck::binario(const QString& nome_padrao) {
    return nome_padrao == QStringLiteral("hidr.dat") || nome_padrao == QStringLiteral("postos.dat") ||
           nome_padrao == QStringLiteral("vazoes.dat");
}

void DadosDeck::carregarArquivo(const QString& nome_padrao, const QString& rotulo, const LayoutArquivoFixo& layout,
                                const std::map<std::string, std::string>& arquivos_dat) {
    Entrada& entrada = arquivos_[nome_padrao];
    entrada.base = layout;
    entrada.layout = layout;
    QString nome = nome_padrao;
    auto it = arquivos_dat.find(rotulo.toStdString());
    if (!rotulo.isEmpty() && it != arquivos_dat.end()) nome = QString::fromStdString(it->second);
    entrada.caminho = QDir(dir_).filePath(nome);
    if (dir_.isEmpty()) return;
    Resultado r = entrada.arquivo.carregar(std::filesystem::path(entrada.caminho.toStdWString()), layout);
    entrada.lido = r.ok;
    if (!r.ok) entrada.erro = QString::fromUtf8(r.mensagem);
    else entrada.salvo = conteudo(entrada);
}

// postos.dat e vazoes.dat: acesso direto, nao formatado, sempre com o nome padrao. O postos.dat tem
// um registro de 20 bytes por posto (nome A12, ano inicial e ano final em int32), como o DeckLookup ja
// le; o vazoes.dat, um registro por mes com um int32 por posto, entao o numero de postos, e com ele o
// tamanho do registro, vem do postos.dat (manual, secoes 3.10 e 3.14: os dois tem 320 ou 600 postos).
void DadosDeck::carregarBinario(const QString& nome_padrao, int tamanho_registro) {
    Entrada& entrada = arquivos_[nome_padrao];
    entrada.eh_binario = true;
    entrada.caminho = QDir(dir_).filePath(nome_padrao);
    if (tamanho_registro <= 0) {
        entrada.erro = QStringLiteral("sem o postos.dat não há como saber o número de postos");
        return;
    }
    Resultado r = entrada.binario.carregar(std::filesystem::path(entrada.caminho.toStdWString()), tamanho_registro);
    entrada.lido = r.ok;
    if (!r.ok) entrada.erro = QString::fromUtf8(r.mensagem);
    else entrada.salvo = conteudo(entrada);
}

// Le uma unica vez por deck todos os arquivos de texto do catalogo, o modif.dat e os binarios de
// postos e vazoes: todas as vistas (tabelas, editor de termicas, modificacoes e editor textual) editam
// estas mesmas copias em memoria. Arquivo com layout de colunas fixas e lido pelas secoes dele; os
// demais, com layout vazio, ficam so como texto, regravado byte a byte. O nome real vem do
// arquivos.dat quando o arquivo tem rotulo la.
void DadosDeck::carregar(const QString& dir_deck, const std::map<std::string, std::string>& arquivos_dat) {
    pilha_.clear();
    antes_.clear();
    linhas_pendentes_.clear();
    a_reler_.clear();
    arquivos_.clear();
    referencias_.clear();
    dir_ = dir_deck;
    for (const ArquivoNewave& info : catalogoNewave()) {
        if (binario(info.nome_padrao)) continue;
        const LayoutArquivoFixo* layout = layoutNewave(info.nome_padrao.toStdString());
        carregarArquivo(info.nome_padrao, info.rotulo_arquivos,
                        layout ? *layout : LayoutArquivoFixo{info.secao_manual.toStdString(), {}}, arquivos_dat);
    }
    carregarArquivo(QStringLiteral("modif.dat"), QStringLiteral("ALTERACAO DADOS USINAS HIDRO"), {"3.12", {}}, arquivos_dat);
    carregarBinario(QStringLiteral("postos.dat"), postos_dat::REGISTRO);
    const ArquivoBinario* postos = arquivoBinario(QStringLiteral("postos.dat"));
    carregarBinario(QStringLiteral("vazoes.dat"), postos ? 4 * postos->registros() : 0);
    aplicarPatamares(false);
    emit recarregado();
}

const ArquivoFixo* DadosDeck::arquivo(const QString& nome_padrao) const {
    auto it = arquivos_.find(nome_padrao);
    return it != arquivos_.end() && it->second.lido && !it->second.eh_binario ? &it->second.arquivo : nullptr;
}

const ArquivoBinario* DadosDeck::arquivoBinario(const QString& nome_padrao) const {
    auto it = arquivos_.find(nome_padrao);
    return it != arquivos_.end() && it->second.lido && it->second.eh_binario ? &it->second.binario : nullptr;
}

DadosDeck::Entrada* DadosDeck::binarioLido(const QString& nome_padrao) {
    auto it = arquivos_.find(nome_padrao);
    return it != arquivos_.end() && it->second.lido && it->second.eh_binario ? &it->second : nullptr;
}

bool DadosDeck::lido(const QString& nome_padrao) const {
    auto it = arquivos_.find(nome_padrao);
    return it != arquivos_.end() && it->second.lido;
}

QString DadosDeck::nomeNoDeck(const QString& nome_padrao) const {
    auto it = arquivos_.find(nome_padrao);
    return it == arquivos_.end() || it->second.caminho.isEmpty() ? nome_padrao : QFileInfo(it->second.caminho).fileName();
}

QString DadosDeck::erro(const QString& nome_padrao) const {
    auto it = arquivos_.find(nome_padrao);
    return it == arquivos_.end() ? QString() : it->second.erro;
}

// Primeiro registro da secao cuja coluna tem exatamente o valor dado (o codigo da usina ou da
// classe, por exemplo); -1 se nao houver.
int DadosDeck::registroPorValor(const QString& nome_padrao, int secao, int coluna, const QString& valor) const {
    const ArquivoFixo* a = arquivo(nome_padrao);
    if (!a || secao >= static_cast<int>(a->secoes().size())) return -1;
    const std::string procurado = valor.trimmed().toLatin1().toStdString();
    const int n = static_cast<int>(a->secoes()[static_cast<size_t>(secao)].linhas.size());
    for (int r = 0; r < n; ++r)
        if (a->valor(secao, r, coluna) == procurado) return r;
    return -1;
}

// Toda edicao de campo passa por aqui, para o sinal alterado avisar as outras vistas do mesmo arquivo.
// Depois da edicao o arquivo e relido (num lote, so no fim dele), porque o campo pode ser o que
// identifica um bloco.
Resultado DadosDeck::definir(const QString& nome_padrao, int secao, int registro, int coluna, const QString& texto) {
    auto it = arquivos_.find(nome_padrao);
    if (it == arquivos_.end() || !it->second.lido) return Resultado::erro("Arquivo nao carregado");
    aplicarPendentes(nome_padrao);
    abrirTransacao(QStringLiteral("Editar %1").arg(nome_padrao));
    registrarAntes(nome_padrao);
    Resultado r = it->second.arquivo.definir(secao, registro, coluna, texto.toLatin1().toStdString());
    if (r.ok) {
        referencias_.clear();
        if (lote_ > 0) a_reler_.insert(nome_padrao);
        else reler(nome_padrao);
        avisarAlterado(nome_padrao);
        if (lote_ == 0) aplicarPatamares(true);
    }
    fecharTransacao();
    return r;
}

// Edicoes em lote (colar um bloco na tabela): entre iniciarLote e concluirLote as gravacoes valem na
// hora, mas o aviso alterado de cada arquivo sai uma vez so, no fim, junto com a releitura pelos
// patamares, em vez de a cada celula refazer as vistas do arquivo. As linhas trocadas por
// substituirLinha e a releitura depois de definir tambem ficam para o fim, e o lote inteiro se desfaz
// de uma vez. Lotes aninhados valem como um so.
void DadosDeck::iniciarLote() {
    ++lote_;
    abrirTransacao(QStringLiteral("Colar"));
}

void DadosDeck::concluirLote() {
    if (lote_ == 0) return;
    if (--lote_ > 0) {
        fecharTransacao();
        return;
    }
    for (auto& [nome, linhas] : std::exchange(linhas_pendentes_, {})) {
        Entrada& entrada = arquivos_[nome];
        entrada.arquivo.substituirLinhas(linhas, entrada.layout);
    }
    for (const QString& nome : std::exchange(a_reler_, {})) reler(nome);
    fecharTransacao();
    const std::set<QString> alterados = std::exchange(alterados_no_lote_, {});
    for (const QString& nome : alterados) emit alterado(nome);
    if (!alterados.empty()) aplicarPatamares(true);
}

// Conteudo do arquivo como fica no disco, para comparar versoes: o texto com quebra LF ou os bytes.
std::string DadosDeck::conteudo(const Entrada& entrada) {
    if (entrada.eh_binario) return std::string(entrada.binario.bytes().begin(), entrada.binario.bytes().end());
    return entrada.arquivo.textoLf();
}

// Toda operacao que muda arquivos do deck e uma transacao: registrarAntes guarda o conteudo de cada
// arquivo na primeira vez que ela o toca, e fecharTransacao, na mais externa, poe na pilha de desfazer
// um comando com o trecho que mudou em cada um (o que fica entre o inicio e o fim iguais).
void DadosDeck::abrirTransacao(const QString& rotulo) {
    if (transacao_++ == 0) rotulo_transacao_ = rotulo;
}

void DadosDeck::registrarAntes(const QString& nome_padrao) {
    if (transacao_ == 0 || antes_.count(nome_padrao)) return;
    auto it = arquivos_.find(nome_padrao);
    if (it != arquivos_.end() && it->second.lido) antes_[nome_padrao] = conteudo(it->second);
}

void DadosDeck::fecharTransacao() {
    if (transacao_ == 0 || --transacao_ > 0) return;
    std::vector<Diferenca> diferencas;
    for (const auto& [nome, antes] : std::exchange(antes_, {})) {
        const std::string depois = conteudo(arquivos_[nome]);
        if (depois == antes) continue;
        const size_t menor = std::min(antes.size(), depois.size());
        size_t prefixo = 0;
        while (prefixo < menor && antes[prefixo] == depois[prefixo]) ++prefixo;
        size_t sufixo = 0;
        while (sufixo < menor - prefixo && antes[antes.size() - 1 - sufixo] == depois[depois.size() - 1 - sufixo]) ++sufixo;
        diferencas.push_back({nome, prefixo, sufixo, antes.substr(prefixo, antes.size() - prefixo - sufixo),
                              depois.substr(prefixo, depois.size() - prefixo - sufixo)});
    }
    if (!diferencas.empty()) pilha_.push(new ComandoDeck(this, rotulo_transacao_, std::move(diferencas)));
}

// Desfaz (ou refaz) um comando: troca em cada arquivo o trecho pelo de antes (ou de depois) e rele o
// arquivo. O arquivo volta a contar como nao alterado quando fica igual ao que esta no disco.
void DadosDeck::restaurar(const std::vector<Diferenca>& diferencas, bool desfazer) {
    for (const Diferenca& d : diferencas) {
        auto it = arquivos_.find(d.nome);
        if (it == arquivos_.end() || !it->second.lido) continue;
        Entrada& entrada = it->second;
        const std::string atual = conteudo(entrada);
        if (atual.size() < d.prefixo + d.sufixo) continue;
        const std::string novo = atual.substr(0, d.prefixo) + (desfazer ? d.antes : d.depois) + atual.substr(atual.size() - d.sufixo);
        if (entrada.eh_binario) {
            entrada.binario.substituirBytes(std::vector<char>(novo.begin(), novo.end()));
            entrada.binario.definirModificado(novo != entrada.salvo);
        } else {
            entrada.arquivo.substituirTexto(novo, entrada.layout);
            entrada.arquivo.definirModificado(novo != entrada.salvo);
        }
    }
    referencias_.clear();
    for (const Diferenca& d : diferencas) {
        if (!binario(d.nome)) emit reinterpretado(d.nome);
        emit alterado(d.nome);
    }
    aplicarPatamares(true);
}

// Linhas trocadas por substituirLinha durante um lote, que so entram no arquivo no fim dele; antes de
// qualquer outra edicao do mesmo arquivo, entram na hora.
void DadosDeck::aplicarPendentes(const QString& nome_padrao) {
    auto pendente = linhas_pendentes_.find(nome_padrao);
    if (pendente == linhas_pendentes_.end()) return;
    Entrada& entrada = arquivos_[nome_padrao];
    entrada.arquivo.substituirLinhas(pendente->second, entrada.layout);
    linhas_pendentes_.erase(pendente);
}

// Rele o arquivo pelo layout depois de uma edicao de campo: apagar ou mudar o campo que identifica um
// bloco muda a divisao em registros, e entao as vistas do arquivo se refazem (reinterpretado).
void DadosDeck::reler(const QString& nome_padrao) {
    Entrada& entrada = arquivos_[nome_padrao];
    auto estrutura = [&entrada] {
        std::vector<std::pair<std::vector<int>, std::vector<std::vector<int>>>> registros;
        for (const SecaoLida& secao : entrada.arquivo.secoes()) registros.push_back({secao.linhas, secao.linhas_contexto});
        return registros;
    };
    const auto antes = estrutura();
    entrada.arquivo.substituirTexto(entrada.arquivo.textoLf(), entrada.layout);
    if (estrutura() != antes) emit reinterpretado(nome_padrao);
}

void DadosDeck::avisarAlterado(const QString& nome_padrao) {
    if (lote_ > 0) alterados_no_lote_.insert(nome_padrao);
    else emit alterado(nome_padrao);
}

// O numero de patamares de carga (patamar.dat) e de deficit (sistema.dat) decide quantas colunas de
// patamar as tabelas mostram: limites do agrint.dat, geracao do adterm.dat, custos e profundidades
// do deficit no sistema.dat e duracao sazonal no patamar.dat. Na carga do deck, e sempre que uma
// edicao muda um desses numeros, os arquivos que dependem deles sao relidos pelo layout ajustado,
// sem mudar o texto nem a marca de alterado, e avisados por reinterpretado.
void DadosDeck::aplicarPatamares(bool avisar) {
    NumeroPatamares numero;
    if (const ArquivoFixo* patamar = arquivo(QStringLiteral("patamar.dat"))) numero.carga = patamaresDeCarga(*patamar);
    if (const ArquivoFixo* sistema = arquivo(QStringLiteral("sistema.dat"))) numero.deficit = patamaresDeDeficit(*sistema);
    if (avisar && numero == patamares_) return;
    patamares_ = numero;
    for (auto& [nome, entrada] : arquivos_) {
        if (entrada.eh_binario || !dependeDePatamares(entrada.base)) continue;
        entrada.layout = ajustarPatamares(entrada.base, numero);
        if (!entrada.lido) continue;
        entrada.arquivo.substituirTexto(entrada.arquivo.textoLf(), entrada.layout);
        if (avisar) emit reinterpretado(nome);
    }
}

// Campo de texto de um registro binario, gravado em Latin-1.
Resultado DadosDeck::definirTextoBinario(const QString& nome_padrao, int registro, int inicio, int tamanho, const QString& texto) {
    Entrada* entrada = binarioLido(nome_padrao);
    if (!entrada) return Resultado::erro("Arquivo nao carregado");
    abrirTransacao(QStringLiteral("Editar %1").arg(nome_padrao));
    registrarAntes(nome_padrao);
    Resultado r = entrada->binario.definirTexto(registro, inicio, tamanho, texto.trimmed().toLatin1().toStdString());
    if (r.ok) {
        referencias_.clear();
        avisarAlterado(nome_padrao);
    }
    fecharTransacao();
    return r;
}

// Campo inteiro de 4 bytes de um registro binario; texto que nao e inteiro de 32 bits e erro.
Resultado DadosDeck::definirInteiroBinario(const QString& nome_padrao, int registro, int inicio, const QString& texto) {
    Entrada* entrada = binarioLido(nome_padrao);
    if (!entrada) return Resultado::erro("Arquivo nao carregado");
    bool ok = false;
    const qlonglong valor = texto.trimmed().toLongLong(&ok);
    if (!ok || valor < std::numeric_limits<int32_t>::min() || valor > std::numeric_limits<int32_t>::max())
        return Resultado::erro("Valor invalido: " + texto.toStdString());
    abrirTransacao(QStringLiteral("Editar %1").arg(nome_padrao));
    registrarAntes(nome_padrao);
    entrada->binario.definirInteiro(registro, inicio, static_cast<int32_t>(valor));
    avisarAlterado(nome_padrao);
    fecharTransacao();
    return Resultado::sucesso();
}

// Codigo como numero sem zeros a esquerda ("001" e "1" sao o mesmo REE); texto que nao e numero
// fica como esta, aparado.
QString DadosDeck::normalizarCodigo(const QString& codigo) {
    bool ok = false;
    const int n = codigo.trimmed().toInt(&ok);
    return ok ? QString::number(n) : codigo.trimmed();
}

// Itens do cadastro referenciado, na ordem do arquivo, com rotulo "NOME (codigo)": submercados do
// sistema.dat, REEs do ree.dat, usinas do confhd.dat e do conft.dat, classes do clast.dat,
// tecnologias do tecno.dat e postos com nome do postos.dat. Os agrupamentos do agrint.dat tem por
// nome as interligacoes que os compoem ("SUDESTE→NORDESTE + NOFICT1→NORDESTE", com o coeficiente
// antes quando nao e 1), lidas do bloco 1 do proprio arquivo. A lista e refeita depois de qualquer
// edicao, porque um nome ou codigo pode ter mudado.
const std::vector<OpcaoReferencia>& DadosDeck::opcoes(Referencia referencia) const {
    auto it = referencias_.find(referencia);
    if (it != referencias_.end()) return it->second;
    std::vector<OpcaoReferencia> lista;
    auto rotulo = [](const QString& nome, const QString& codigo) {
        return nome.isEmpty() ? codigo : QStringLiteral("%1 (%2)").arg(nome, codigo);
    };
    if (referencia == Referencia::Agrupamento) {
        const ArquivoFixo* agrint = arquivo(QStringLiteral("agrint.dat"));
        if (agrint && !agrint->secoes().empty()) {
            auto nomeSubmercado = [this](const QString& codigo) {
                const QString rotulo = rotuloReferencia(Referencia::Submercado, codigo);
                const QString sufixo = QStringLiteral(" (%1)").arg(normalizarCodigo(codigo));
                return rotulo.endsWith(sufixo) ? rotulo.chopped(sufixo.size()) : rotulo;
            };
            std::vector<std::pair<QString, QStringList>> grupos;
            for (int r = 0; r < static_cast<int>(agrint->secoes()[0].linhas.size()); ++r) {
                const QString codigo = normalizarCodigo(QString::fromLatin1(agrint->valor(0, r, 0).c_str()));
                if (codigo.isEmpty()) continue;
                QString parte = nomeSubmercado(QString::fromLatin1(agrint->valor(0, r, 1).c_str())) + QStringLiteral("→") +
                                nomeSubmercado(QString::fromLatin1(agrint->valor(0, r, 2).c_str()));
                bool ok = false;
                const double coeficiente = QString::fromLatin1(agrint->valor(0, r, 3).c_str()).toDouble(&ok);
                if (ok && coeficiente != 1.0) parte = QString::number(coeficiente) + QStringLiteral("·") + parte;
                auto grupo = std::find_if(grupos.begin(), grupos.end(), [&](const auto& g) { return g.first == codigo; });
                if (grupo == grupos.end()) grupos.push_back({codigo, {parte}});
                else grupo->second << parte;
            }
            for (const auto& [codigo, partes] : grupos) lista.push_back({codigo, rotulo(partes.join(QStringLiteral(" + ")), codigo)});
        }
    } else if (referencia == Referencia::Posto) {
        if (const ArquivoBinario* postos = arquivoBinario(QStringLiteral("postos.dat")))
            for (int r = 0; r < postos->registros(); ++r) {
                const QString nome = QString::fromLatin1(postos->texto(r, postos_dat::NOME, postos_dat::TAMANHO_NOME).c_str()).trimmed();
                if (!nome.isEmpty()) lista.push_back({QString::number(r + 1), rotulo(nome, QString::number(r + 1))});
            }
    } else {
        const FonteReferencia fonte = fonteReferencia(referencia);
        const ArquivoFixo* a = fonte.secao >= 0 ? arquivo(QString::fromLatin1(fonte.arquivo)) : nullptr;
        if (a && fonte.secao < static_cast<int>(a->secoes().size())) {
            const int n = static_cast<int>(a->secoes()[static_cast<size_t>(fonte.secao)].linhas.size());
            for (int r = 0; r < n; ++r) {
                const QString codigo = normalizarCodigo(QString::fromLatin1(a->valor(fonte.secao, r, fonte.coluna_codigo).c_str()));
                if (codigo.isEmpty()) continue;
                const QString nome = QString::fromLatin1(a->valor(fonte.secao, r, fonte.coluna_nome).c_str()).trimmed();
                lista.push_back({codigo, rotulo(nome, codigo)});
            }
        }
    }
    return referencias_.emplace(referencia, std::move(lista)).first->second;
}

// Rotulo do codigo no cadastro referenciado; codigo que nao esta nele aparece como esta.
QString DadosDeck::rotuloReferencia(Referencia referencia, const QString& codigo) const {
    const QString chave = normalizarCodigo(codigo);
    if (chave.isEmpty()) return codigo;
    for (const OpcaoReferencia& o : opcoes(referencia))
        if (o.codigo == chave) return o.rotulo;
    return codigo;
}

// Coluna mostrada e editada por uma lista: referencia a outro cadastro ou valores fixos do manual.
bool DadosDeck::temOpcoes(const ColunaFixa& coluna) {
    return ArquivoFixo::editavel(coluna) && (coluna.referencia != Referencia::Nenhuma || !coluna.opcoes.empty());
}

// Itens da lista da coluna: os do cadastro referenciado ou os valores fixos, com rotulo
// "descricao (codigo)".
std::vector<OpcaoReferencia> DadosDeck::opcoes(const ColunaFixa& coluna) const {
    if (coluna.referencia != Referencia::Nenhuma) return opcoes(coluna.referencia);
    std::vector<OpcaoReferencia> lista;
    for (const OpcaoFixa& o : coluna.opcoes) {
        const QString codigo = QString::fromStdString(o.codigo);
        lista.push_back({codigo, QStringLiteral("%1 (%2)").arg(QString::fromStdString(o.descricao), codigo)});
    }
    return lista;
}

// Rotulo do codigo na lista da coluna; codigo que nao esta nela aparece como esta.
QString DadosDeck::rotulo(const ColunaFixa& coluna, const QString& codigo) const {
    if (coluna.referencia != Referencia::Nenhuma) return rotuloReferencia(coluna.referencia, codigo);
    const QString chave = normalizarCodigo(codigo);
    for (const OpcaoFixa& o : coluna.opcoes)
        if (QString::fromStdString(o.codigo) == chave) return QStringLiteral("%1 (%2)").arg(QString::fromStdString(o.descricao), chave);
    return codigo;
}

// Linha do arquivo (indice a partir de 0), ja com a troca de um lote em andamento; vazia fora do
// arquivo.
QString DadosDeck::linha(const QString& nome_padrao, int indice) const {
    auto pendente = linhas_pendentes_.find(nome_padrao);
    const ArquivoFixo* a = arquivo(nome_padrao);
    const std::vector<std::string>* linhas = pendente != linhas_pendentes_.end() ? &pendente->second : a ? &a->linhas() : nullptr;
    if (!linhas || indice < 0 || indice >= static_cast<int>(linhas->size())) return {};
    return QString::fromLatin1((*linhas)[static_cast<size_t>(indice)].c_str());
}

// Texto do arquivo para o editor textual: Latin-1, com quebra LF.
QString DadosDeck::texto(const QString& nome_padrao) const {
    const ArquivoFixo* a = arquivo(nome_padrao);
    return a ? QString::fromLatin1(a->textoLf().c_str()) : QString();
}

// Troca o arquivo pelo texto editado e rele as secoes; como o numero de registros pode mudar, avisa
// por reinterpretado, que faz as tabelas se refazerem, alem de alterado.
void DadosDeck::substituirTexto(const QString& nome_padrao, const QString& texto) {
    auto it = arquivos_.find(nome_padrao);
    if (it == arquivos_.end() || !it->second.lido) return;
    aplicarPendentes(nome_padrao);
    abrirTransacao(QStringLiteral("Editar texto de %1").arg(nome_padrao));
    registrarAntes(nome_padrao);
    it->second.arquivo.substituirTexto(texto.toLatin1().toStdString(), it->second.layout);
    concluirReinterpretacao(nome_padrao);
    fecharTransacao();
}

// Troca uma linha do arquivo (indice a partir de 0) sem mudar o numero de linhas; as vistas recebem
// so alterado, porque os registros continuam nas mesmas linhas. Num lote a troca fica pendente e o
// arquivo so e relido no fim, uma vez para todas as linhas; ate la, linha() ja devolve a nova.
Resultado DadosDeck::substituirLinha(const QString& nome_padrao, int indice, const QString& texto) {
    auto it = arquivos_.find(nome_padrao);
    if (it == arquivos_.end() || !it->second.lido || it->second.eh_binario) return Resultado::erro("Arquivo não carregado");
    if (indice < 0 || indice >= static_cast<int>(it->second.arquivo.linhas().size())) return Resultado::erro("Linha fora do arquivo");
    abrirTransacao(QStringLiteral("Editar %1").arg(nome_padrao));
    registrarAntes(nome_padrao);
    if (lote_ > 0) {
        auto pendente = linhas_pendentes_.try_emplace(nome_padrao, it->second.arquivo.linhas()).first;
        pendente->second[static_cast<size_t>(indice)] = texto.toLatin1().toStdString();
    } else {
        std::vector<std::string> linhas = it->second.arquivo.linhas();
        linhas[static_cast<size_t>(indice)] = texto.toLatin1().toStdString();
        it->second.arquivo.substituirLinhas(linhas, it->second.layout);
    }
    referencias_.clear();
    avisarAlterado(nome_padrao);
    fecharTransacao();
    return Resultado::sucesso();
}

// Avisa as vistas de um arquivo relido com outras linhas; se o numero de patamares mudou, os
// arquivos que dependem dele sao relidos tambem.
void DadosDeck::concluirReinterpretacao(const QString& nome_padrao) {
    referencias_.clear();
    emit reinterpretado(nome_padrao);
    emit alterado(nome_padrao);
    aplicarPatamares(true);
}

// Copia de um registro ou do bloco de contexto dele, inserida logo depois (ArquivoFixo::duplicar).
Resultado DadosDeck::duplicar(const QString& nome_padrao, int secao, int registro, int nivel, int* primeira_linha_nova,
                              bool em_branco) {
    auto it = arquivos_.find(nome_padrao);
    if (it == arquivos_.end() || !it->second.lido || it->second.eh_binario) return Resultado::erro("Arquivo nao carregado");
    aplicarPendentes(nome_padrao);
    abrirTransacao(QStringLiteral("Adicionar registro em %1").arg(nome_padrao));
    registrarAntes(nome_padrao);
    Resultado r = it->second.arquivo.duplicar(secao, registro, nivel, it->second.layout, primeira_linha_nova, em_branco);
    if (r.ok) concluirReinterpretacao(nome_padrao);
    fecharTransacao();
    return r;
}

// Adiciona (delta 1) ou remove (delta -1) o ultimo patamar de carga ou de deficit em todos os arquivos
// do deck que dependem dele (mudarPatamaresDeCarga, mudarPatamaresDeDeficit). Os arquivos alterados
// sao relidos e ficam por salvar; avisos recebe a lista deles e o que o usuario precisa conferir.
Resultado DadosDeck::mudarPatamares(Patamares tipo, int delta, QStringList* avisos) {
    std::map<std::string, std::string> textos;
    for (const std::string& nome : arquivosComPatamares()) {
        const auto it = arquivos_.find(QString::fromStdString(nome));
        if (it != arquivos_.end() && it->second.lido && !it->second.eh_binario) textos[nome] = it->second.arquivo.textoLf();
    }
    MudancaPatamares mudanca;
    const Resultado r = tipo == Patamares::Deficit ? mudarPatamaresDeDeficit(textos, delta, mudanca) : mudarPatamaresDeCarga(textos, delta, mudanca);
    if (!r.ok) return r;
    abrirTransacao(delta > 0 ? QStringLiteral("Adicionar patamar") : QStringLiteral("Remover patamar"));
    QStringList alterados;
    for (const auto& [nome, texto] : mudanca.textos) {
        Entrada& entrada = arquivos_[QString::fromStdString(nome)];
        registrarAntes(QString::fromStdString(nome));
        entrada.arquivo.substituirTexto(texto, entrada.layout);
        alterados << QString::fromStdString(nome);
    }
    referencias_.clear();
    for (const QString& nome : alterados) {
        emit reinterpretado(nome);
        emit alterado(nome);
    }
    aplicarPatamares(true);
    fecharTransacao();
    if (avisos) {
        *avisos << QStringLiteral("Arquivos alterados: %1. Salve todos eles, para o deck não ficar inconsistente.")
                       .arg(alterados.join(QStringLiteral(", ")));
        for (const std::string& aviso : mudanca.avisos) *avisos << QString::fromUtf8(aviso);
    }
    return r;
}

// Remove um registro ou o bloco de contexto dele (ArquivoFixo::remover).
Resultado DadosDeck::remover(const QString& nome_padrao, int secao, int registro, int nivel) {
    auto it = arquivos_.find(nome_padrao);
    if (it == arquivos_.end() || !it->second.lido || it->second.eh_binario) return Resultado::erro("Arquivo nao carregado");
    aplicarPendentes(nome_padrao);
    abrirTransacao(QStringLiteral("Remover registro de %1").arg(nome_padrao));
    registrarAntes(nome_padrao);
    Resultado r = it->second.arquivo.remover(secao, registro, nivel, it->second.layout);
    if (r.ok) concluirReinterpretacao(nome_padrao);
    fecharTransacao();
    return r;
}

bool DadosDeck::salvar(const QString& nome_padrao, QString* motivo) {
    auto it = arquivos_.find(nome_padrao);
    if (it == arquivos_.end() || !it->second.lido) return true;
    Entrada& e = it->second;
    if (!(e.eh_binario ? e.binario.modificado() : e.arquivo.modificado())) return true;
    const std::filesystem::path caminho(e.caminho.toStdWString());
    Resultado r = e.eh_binario ? e.binario.salvar(caminho) : e.arquivo.salvar(caminho);
    if (!r.ok) {
        if (motivo) *motivo = QString::fromUtf8(r.mensagem);
        return false;
    }
    e.salvo = conteudo(e);
    emit alterado(nome_padrao);
    return true;
}

// Grava o arquivo em outra pasta, com o nome que ele tem no deck, sem mudar a pasta do deck aberto nem
// a marca de alterado.
bool DadosDeck::salvarEm(const QString& nome_padrao, const QString& pasta, QString* motivo) {
    auto it = arquivos_.find(nome_padrao);
    if (it == arquivos_.end() || !it->second.lido) return true;
    aplicarPendentes(nome_padrao);
    Entrada& e = it->second;
    const std::filesystem::path caminho(QDir(pasta).filePath(nomeNoDeck(nome_padrao)).toStdWString());
    const bool modificado = e.eh_binario ? e.binario.modificado() : e.arquivo.modificado();
    Resultado r = e.eh_binario ? e.binario.salvar(caminho) : e.arquivo.salvar(caminho);
    if (e.eh_binario) e.binario.definirModificado(modificado);
    else e.arquivo.definirModificado(modificado);
    if (!r.ok) {
        if (motivo) *motivo = QString::fromUtf8(r.mensagem);
        return false;
    }
    return true;
}

QStringList DadosDeck::modificados() const {
    QStringList nomes;
    for (const auto& [nome, entrada] : arquivos_)
        if (entrada.lido && (entrada.eh_binario ? entrada.binario.modificado() : entrada.arquivo.modificado())) nomes << nome;
    return nomes;
}
