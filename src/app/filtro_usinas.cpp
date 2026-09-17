#include "filtro_usinas.h"
#include "modelo_hidr.h"

FiltroUsinas::FiltroUsinas(QObject* parent) : QSortFilterProxyModel(parent) {
    setSortRole(Qt::UserRole);
    setDynamicSortFilter(true);
}

void FiltroUsinas::definirOcultarVazias(bool ocultar) {
    beginFilterChange();
    ocultar_vazias_ = ocultar;
    endFilterChange(Direction::Rows);
}

void FiltroUsinas::definirTexto(const QString& texto) {
    beginFilterChange();
    texto_ = texto.trimmed();
    endFilterChange(Direction::Rows);
}

bool FiltroUsinas::filterAcceptsRow(int linha, const QModelIndex&) const {
    auto* m = qobject_cast<ModeloHidr*>(sourceModel());
    if (!m || linha >= m->numUsinas()) return false;
    const UsinaHidr& u = m->usina(linha);
    if (ocultar_vazias_ && u.vazia()) return false;
    if (texto_.isEmpty()) return true;
    if (QString::number(linha + 1).startsWith(texto_)) return true;
    return QString::fromLatin1(u.nome.c_str()).contains(texto_, Qt::CaseInsensitive);
}
