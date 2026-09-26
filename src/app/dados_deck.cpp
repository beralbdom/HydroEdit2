#include "dados_deck.h"
#include <QDir>
#include <QFileInfo>
#include <filesystem>
#include "catalogo_newave.h"
#include "layouts_newave.h"

DadosDeck::DadosDeck(QObject* parent) : QObject(parent) {}

// Arquivos do deck que nao sao texto e ficam fora do repositorio: o hidr.dat tem editor proprio e o
// postos.dat e o vazoes.dat sao registros binarios (manual do NEWAVE, secoes 3.10 e 3.14).
bool DadosDeck::binario(const QString& nome_padrao) {
    return nome_padrao == QStringLiteral("hidr.dat") || nome_padrao == QStringLiteral("postos.dat") ||
           nome_padrao == QStringLiteral("vazoes.dat");
}

void DadosDeck::carregarArquivo(const QString& nome_padrao, const QString& rotulo, const LayoutArquivoFixo& layout,
                                const std::map<std::string, std::string>& arquivos_dat) {
    Entrada& entrada = arquivos_[nome_padrao];
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

// Le uma unica vez por deck todos os arquivos de texto do catalogo e o modif.dat: todas as vistas
// (tabelas, editor de termicas, modificacoes e editor textual) editam estas mesmas copias em memoria.
// Arquivo com layout de colunas fixas e lido pelas secoes dele; os demais, com layout vazio, ficam so
// como texto, regravado byte a byte. O nome real vem do arquivos.dat quando o arquivo tem rotulo la.
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
    emit recarregado();
}

const ArquivoFixo* DadosDeck::arquivo(const QString& nome_padrao) const {
    auto it = arquivos_.find(nome_padrao);
    return it != arquivos_.end() && it->second.lido ? &it->second.arquivo : nullptr;
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
    if (r.ok) emit alterado(nome_padrao);
    return r;
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
}

bool DadosDeck::salvar(const QString& nome_padrao, QString* motivo) {
    auto it = arquivos_.find(nome_padrao);
    if (it == arquivos_.end() || !it->second.lido || !it->second.arquivo.modificado()) return true;
    Resultado r = it->second.arquivo.salvar(std::filesystem::path(it->second.caminho.toStdWString()));
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
        if (entrada.lido && entrada.arquivo.modificado()) nomes << nome;
    return nomes;
}
