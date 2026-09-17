#include "painel_problemas.h"
#include <QListWidget>

PainelProblemas::PainelProblemas(QWidget* parent) : QDockWidget(QStringLiteral("Problemas"), parent) {
    lista_ = new QListWidget(this);
    setWidget(lista_);
    setAllowedAreas(Qt::BottomDockWidgetArea);
    connect(lista_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        emit problemaEscolhido(item->data(Qt::UserRole).toInt(), item->data(Qt::UserRole + 1).toString());
    });
}

void PainelProblemas::definirProblemas(const std::vector<ProblemaUsina>& problemas) {
    lista_->clear();
    for (const ProblemaUsina& p : problemas) {
        QString sev = p.problema.severidade == Severidade::Erro ? QStringLiteral("ERRO") : QStringLiteral("aviso");
        auto* item = new QListWidgetItem(QStringLiteral("[%1] usina %2, %3: %4")
                                             .arg(sev)
                                             .arg(p.linha + 1)
                                             .arg(QString::fromLatin1(p.problema.campo.c_str()),
                                                  QString::fromUtf8(p.problema.mensagem)));
        item->setData(Qt::UserRole, p.linha);
        item->setData(Qt::UserRole + 1, QString::fromLatin1(p.problema.campo.c_str()));
        if (p.problema.severidade == Severidade::Erro) item->setForeground(Qt::red);
        lista_->addItem(item);
    }
    setVisible(!problemas.empty());
}
