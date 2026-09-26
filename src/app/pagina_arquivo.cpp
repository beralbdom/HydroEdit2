#include "pagina_arquivo.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include "dados_deck.h"

namespace {
constexpr qint64 LIMITE_PREVIA = 2 * 1024 * 1024;
constexpr qint64 AMOSTRA_BINARIO = 4096;
}  // namespace

// Pagina provisoria de um arquivo do deck que ainda nao tem editor: titulo, nome real no deck, secao
// do manual e o conteudo em texto, somente leitura. Arquivo de texto vem do repositorio do deck, e a
// previa acompanha o que for editado no editor textual; binario e lido do disco.
PaginaArquivo::PaginaArquivo(const ArquivoNewave& arquivo, DadosDeck* dados, QWidget* parent)
    : QWidget(parent), arquivo_(arquivo), dados_(dados) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    auto* titulo = new QLabel(arquivo_.titulo, this);
    QFont fonte = titulo->font();
    fonte.setPointSize(fonte.pointSize() + 1);
    fonte.setBold(true);
    titulo->setFont(fonte);
    layout->addWidget(titulo);

    detalhes_ = new QLabel(this);
    detalhes_->setEnabled(false);
    layout->addWidget(detalhes_);

    texto_ = new QPlainTextEdit(this);
    texto_->setReadOnly(true);
    texto_->setLineWrapMode(QPlainTextEdit::NoWrap);
    texto_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(texto_, 1);

    connect(dados_, &DadosDeck::reinterpretado, this, [this](const QString& nome) {
        if (nome == arquivo_.nome_padrao) mostrarDoRepositorio();
    });
    carregar({}, {});
}

void PaginaArquivo::mostrarDoRepositorio() {
    const QString secao = QStringLiteral("manual do NEWAVE, seção %1").arg(arquivo_.secao_manual);
    detalhes_->setText(QStringLiteral("%1  ·  %2  ·  somente leitura; ative Ver > Editor textual para editar")
                           .arg(dados_->nomeNoDeck(arquivo_.nome_padrao), secao));
    texto_->setPlainText(dados_->texto(arquivo_.nome_padrao));
}

// O nome real vem do arquivos.dat quando o arquivo tem rotulo la; senao e o nome padrao. Arquivos com
// byte nulo nos primeiros 4 KB sao tratados como binarios e ficam sem previa; os de texto sao lidos
// como Latin-1, a codificacao dos decks, ate 2 MB.
void PaginaArquivo::carregar(const QString& dir_deck, const std::map<std::string, std::string>& arquivos_dat) {
    texto_->clear();
    encontrado_ = true;
    const QString secao = QStringLiteral("manual do NEWAVE, seção %1").arg(arquivo_.secao_manual);
    if (dir_deck.isEmpty()) {
        detalhes_->setText(QStringLiteral("%1  ·  %2  ·  abra o hidr.dat de um deck para ver o arquivo")
                               .arg(arquivo_.nome_padrao, secao));
        return;
    }

    if (dados_->arquivo(arquivo_.nome_padrao)) {
        mostrarDoRepositorio();
        return;
    }
    QString nome = arquivo_.nome_padrao;
    auto it = arquivos_dat.find(arquivo_.rotulo_arquivos.toStdString());
    if (!arquivo_.rotulo_arquivos.isEmpty() && it != arquivos_dat.end()) nome = QString::fromStdString(it->second);

    QFile arquivo(QDir(dir_deck).filePath(nome));
    if (!arquivo.open(QIODevice::ReadOnly)) {
        encontrado_ = false;
        detalhes_->setText(QStringLiteral("%1  ·  %2  ·  não encontrado no deck").arg(nome, secao));
        return;
    }
    const qint64 tamanho = arquivo.size();
    detalhes_->setText(QStringLiteral("%1  ·  %2  ·  %3 bytes  ·  somente leitura, o editor ainda não existe")
                           .arg(nome, secao)
                           .arg(tamanho));
    QByteArray conteudo = arquivo.read(LIMITE_PREVIA);
    if (conteudo.left(AMOSTRA_BINARIO).contains('\0')) {
        texto_->setPlainText(QStringLiteral("Arquivo binário, sem prévia em texto."));
        return;
    }
    QString texto = QString::fromLatin1(conteudo);
    if (tamanho > LIMITE_PREVIA) texto += QStringLiteral("\n[prévia limitada aos primeiros 2 MB]");
    texto_->setPlainText(texto);
}
