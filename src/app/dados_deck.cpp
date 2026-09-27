#include "dados_deck.h"
#include <QDir>
#include <QFileInfo>
#include <filesystem>
#include <limits>
#include "catalogo_newave.h"
#include "layouts_newave.h"

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
}

// Le uma unica vez por deck todos os arquivos de texto do catalogo, o modif.dat e os binarios de
// postos e vazoes: todas as vistas (tabelas, editor de termicas, modificacoes e editor textual) editam
// estas mesmas copias em memoria. Arquivo com layout de colunas fixas e lido pelas secoes dele; os
// demais, com layout vazio, ficam so como texto, regravado byte a byte. O nome real vem do
// arquivos.dat quando o arquivo tem rotulo la.
void DadosDeck::carregar(const QString& dir_deck, const std::map<std::string, std::string>& arquivos_dat) {
    arquivos_.clear();
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
Resultado DadosDeck::definir(const QString& nome_padrao, int secao, int registro, int coluna, const QString& texto) {
    auto it = arquivos_.find(nome_padrao);
    if (it == arquivos_.end() || !it->second.lido) return Resultado::erro("Arquivo nao carregado");
    Resultado r = it->second.arquivo.definir(secao, registro, coluna, texto.toLatin1().toStdString());
    if (r.ok) {
        emit alterado(nome_padrao);
        aplicarPatamares(true);
    }
    return r;
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
    Resultado r = entrada->binario.definirTexto(registro, inicio, tamanho, texto.trimmed().toLatin1().toStdString());
    if (r.ok) emit alterado(nome_padrao);
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
    entrada->binario.definirInteiro(registro, inicio, static_cast<int32_t>(valor));
    emit alterado(nome_padrao);
    return Resultado::sucesso();
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
    it->second.arquivo.substituirTexto(texto.toLatin1().toStdString(), it->second.layout);
    emit reinterpretado(nome_padrao);
    emit alterado(nome_padrao);
    aplicarPatamares(true);
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
    emit alterado(nome_padrao);
    return true;
}

QStringList DadosDeck::modificados() const {
    QStringList nomes;
    for (const auto& [nome, entrada] : arquivos_)
        if (entrada.lido && (entrada.eh_binario ? entrada.binario.modificado() : entrada.arquivo.modificado())) nomes << nome;
    return nomes;
}
