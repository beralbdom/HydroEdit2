#include "dados_deck.h"
#include <QDir>
#include <QFileInfo>
#include <filesystem>
#include "catalogo_newave.h"
#include "layouts_newave.h"

DadosDeck::DadosDeck(QObject* parent) : QObject(parent) {}

// Le todos os arquivos do catalogo que tem layout de colunas fixas, uma unica vez por deck: todas as
// vistas (as tabelas por arquivo e o editor de termicas) editam estas mesmas copias em memoria. O
// nome real vem do arquivos.dat quando o arquivo tem rotulo la. Arquivo ausente ou ilegivel fica com
// o motivo em erro() e sem dados.
void DadosDeck::carregar(const QString& dir_deck, const std::map<std::string, std::string>& arquivos_dat) {
    arquivos_.clear();
    dir_ = dir_deck;
    for (const ArquivoNewave& info : catalogoNewave()) {
        const LayoutArquivoFixo* layout = layoutNewave(info.nome_padrao.toStdString());
        if (!layout) continue;
        Entrada& entrada = arquivos_[info.nome_padrao];
        QString nome = info.nome_padrao;
        auto it = arquivos_dat.find(info.rotulo_arquivos.toStdString());
        if (!info.rotulo_arquivos.isEmpty() && it != arquivos_dat.end()) nome = QString::fromStdString(it->second);
        entrada.caminho = QDir(dir_deck).filePath(nome);
        if (dir_deck.isEmpty()) continue;
        Resultado r = entrada.arquivo.carregar(std::filesystem::path(entrada.caminho.toStdWString()), *layout);
        entrada.lido = r.ok;
        if (!r.ok) entrada.erro = QString::fromUtf8(r.mensagem);
    }
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

// Toda edicao passa por aqui, para o sinal alterado avisar as outras vistas do mesmo arquivo.
Resultado DadosDeck::definir(const QString& nome_padrao, int secao, int registro, int coluna, const QString& texto) {
    auto it = arquivos_.find(nome_padrao);
    if (it == arquivos_.end() || !it->second.lido) return Resultado::erro("Arquivo nao carregado");
    Resultado r = it->second.arquivo.definir(secao, registro, coluna, texto.toLatin1().toStdString());
    if (r.ok) emit alterado(nome_padrao);
    return r;
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
