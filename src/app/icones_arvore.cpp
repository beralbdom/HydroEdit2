#include "icones_arvore.h"
#include <QApplication>
#include <QFile>
#include <QHash>
#include <QPainter>
#include <QPalette>
#include <QStandardItemModel>
#include <QSvgRenderer>
#include <map>
#include <vector>
#include <utility>

namespace {
constexpr int PAPEL_ICONE = Qt::UserRole + 20;
constexpr int PAPEL_ESMAECIDO = Qt::UserRole + 21;
constexpr char TRACO[] = "#262626";

const std::pair<const char*, const char*> DESTAQUES_ESCUROS[] = {
    {"#1f7fcf", "#5ab3f0"}, {"#d9651f", "#f08a44"}, {"#c99400", "#f0c030"}, {"#7a55d0", "#a98bf0"}};

QString procurar(const std::map<QString, QString>& mapa, const QString& chave) {
    const auto it = mapa.find(chave);
    return it == mapa.end() ? QString() : it->second;
}
}  // namespace

QString iconeDoArquivo(const QString& nome_padrao) {
    static const std::map<QString, QString> mapa = {
        {"hidr.dat", "usinas-hidr"},     {"term.dat", "usinas-term"},     {"clast.dat", "classes-term"},
        {"postos.dat", "postos"},        {"ree.dat", "rees"},             {"tecno.dat", "tecnologias"},
        {"confhd.dat", "conf-hidr"},     {"conft.dat", "conf-term"},      {"exph.dat", "exp-hidr"},
        {"expt.dat", "exp-term"},        {"manutt.dat", "manutt"},        {"vazoes.dat", "vazoes"},
        {"vazpast.dat", "vazpast"},      {"dsvagua.dat", "dsvagua"},      {"volref_saz.dat", "volref"},
        {"polinjus.csv", "polinjus"},    {"sistema.dat", "submercados"},  {"patamar.dat", "patamares"},
        {"c_adic.dat", "cadic"},         {"agrint.dat", "agrint"},        {"loss.dat", "loss"},
        {"gtminpat.dat", "gtminpat"},    {"adterm.dat", "adterm"},        {"penalid.dat", "penalid"},
        {"curva.dat", "curva"},          {"cvar.dat", "cvar"},            {"sar.dat", "sar"},
        {"ghmin.dat", "ghmin"},          {"re.dat", "re"},                {"restricao-eletrica.csv", "relet"},
        {"dger.dat", "dger"},            {"arquivos.dat", "arquivos"},    {"shist.dat", "shist"},
        {"selcor.dat", "selcor"},        {"volumes-referencia.csv", "volref"}, {"indices.csv", "arquivos"},
        {"abertura.dat", "tema-cenarios"}, {"gee.dat", "tecnologias"},  {"clasgas.dat", "classes-term"},
    };
    return procurar(mapa, nome_padrao);
}

QString iconeDoTema(const QString& tema) {
    static const std::map<QString, QString> mapa = {
        {QStringLiteral("Caso"), "tema-caso"},
        {QStringLiteral("Política"), "tema-politica"},
        {QStringLiteral("Cenários e simulação final"), "tema-cenarios"},
        {QStringLiteral("Representação"), "tema-repr"},
        {QStringLiteral("Relatórios"), "tema-rel"},
    };
    return procurar(mapa, tema);
}

QString iconeDaCategoria(const QString& categoria) {
    static const std::map<QString, QString> mapa = {
        {QStringLiteral("Volumes"), "mod-volumes"},
        {QStringLiteral("Vazões"), "mod-vazoes"},
        {QStringLiteral("Níveis"), "mod-niveis"},
        {QStringLiteral("Máquinas"), "mod-maquinas"},
        {QStringLiteral("Operação e polinômios"), "mod-operacao"},
    };
    return procurar(mapa, categoria);
}

// Icone das arvores (src/app/icones/<chave>.svg), desenhado em 24 x 24 com as cores do tema claro. O
// traco toma a cor do texto da paleta e, com fundo escuro, as cores de destaque (agua, termica, rede
// eletrica, risco) trocam pelas versoes claras. O SVG e desenhado em 16, 24 e 32 px, para escalas de
// tela de 100, 150 e 200 %, e o resultado fica guardado por chave e cores. Esmaecido e a versao
// desabilitada que o estilo gera, para item sem dados.
QIcon iconeArvore(const QString& chave, bool esmaecido) {
    if (chave.isEmpty()) return {};
    if (esmaecido) {
        const QIcon normal = iconeArvore(chave);
        QIcon apagado;
        for (int tamanho : {16, 24, 32}) apagado.addPixmap(normal.pixmap(QSize(tamanho, tamanho), QIcon::Disabled));
        return apagado;
    }
    const QPalette paleta = QApplication::palette();
    const bool escuro = paleta.color(QPalette::Base).lightness() < 128;
    const QString texto = paleta.color(QPalette::Text).name();
    const QString chave_cache = chave + (escuro ? QStringLiteral("|escuro|") : QStringLiteral("|claro|")) + texto;
    static QHash<QString, QIcon> cache;
    if (const auto it = cache.constFind(chave_cache); it != cache.constEnd()) return *it;

    QFile arquivo(QStringLiteral(":/icones/%1.svg").arg(chave));
    if (!arquivo.open(QIODevice::ReadOnly)) return {};
    QByteArray svg = arquivo.readAll();
    svg.replace(TRACO, texto.toLatin1());
    if (escuro)
        for (const auto& [claro, cor_escura] : DESTAQUES_ESCUROS) svg.replace(claro, cor_escura);
    QSvgRenderer desenho(svg);
    QIcon icone;
    for (int tamanho : {16, 24, 32}) {
        QPixmap imagem(tamanho, tamanho);
        imagem.fill(Qt::transparent);
        QPainter pintor(&imagem);
        pintor.setRenderHint(QPainter::Antialiasing);
        desenho.render(&pintor);
        pintor.end();
        icone.addPixmap(imagem);
    }
    cache.insert(chave_cache, icone);
    return icone;
}

// Poe o icone no item e guarda a chave, para atualizarIconesArvore refazer o icone quando o tema muda.
void definirIconeArvore(QStandardItem* item, const QString& chave) {
    if (chave.isEmpty()) return;
    item->setData(chave, PAPEL_ICONE);
    item->setIcon(iconeArvore(chave));
}

// Troca o icone do item pela versao esmaecida (ou volta a normal), guardando o estado para a troca de
// tema refazer o icone certo.
void esmaecerIconeArvore(QStandardItem* item, bool esmaecido) {
    const QString chave = item->data(PAPEL_ICONE).toString();
    if (chave.isEmpty() || item->data(PAPEL_ESMAECIDO).toBool() == esmaecido) return;
    item->setData(esmaecido, PAPEL_ESMAECIDO);
    item->setIcon(iconeArvore(chave, esmaecido));
}

// Refaz os icones de todos os itens do modelo com as cores da paleta atual.
void atualizarIconesArvore(QStandardItemModel* modelo) {
    std::vector<QStandardItem*> pendentes = {modelo->invisibleRootItem()};
    while (!pendentes.empty()) {
        QStandardItem* item = pendentes.back();
        pendentes.pop_back();
        for (int r = 0; r < item->rowCount(); ++r)
            if (QStandardItem* filho = item->child(r)) pendentes.push_back(filho);
        const QString chave = item->data(PAPEL_ICONE).toString();
        if (!chave.isEmpty()) item->setIcon(iconeArvore(chave, item->data(PAPEL_ESMAECIDO).toBool()));
    }
}
