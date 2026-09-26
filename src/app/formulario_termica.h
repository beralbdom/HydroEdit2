#pragma once
#include <QWidget>
#include <vector>

class QComboBox;
class QFormLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QTabWidget;
class QTableView;
class QTableWidget;
class DadosDeck;

class FormularioTermica : public QWidget {
    Q_OBJECT
public:
    explicit FormularioTermica(DadosDeck* dados, QWidget* parent = nullptr);
    void definirUsina(const QString& codigo);
    QTabWidget* abas() const { return abas_; }
    void adicionarAoCabecalho(QWidget* widget);

private:
    struct Campo {
        QLineEdit* edit = nullptr;
        QComboBox* combo = nullptr;
        QString arquivo;
        int coluna = 0;
    };

    QWidget* criarPaginaCadastro();
    QWidget* criarPaginaConfiguracao();
    QWidget* criarPaginaRegistros(const QString& arquivo, int secao, QSortFilterProxyModel*& filtro, const QString& titulo);
    QWidget* criarPaginaClasse();
    QLineEdit* ligarEdit(QFormLayout* form, const QString& rotulo, const QString& arquivo, int coluna, bool texto = false);
    QTableView* novaTabela(QSortFilterProxyModel* filtro, QWidget* pai);
    void atualizar(const QString& aviso = {});
    void gravar(const QString& arquivo, int coluna, const QString& texto);
    int registro(const QString& arquivo) const;

    DadosDeck* dados_;
    QString usina_;
    bool atualizando_ = false;
    QLabel* titulo_;
    QLabel* aviso_;
    QPushButton* botao_salvar_;
    QTabWidget* abas_;
    QHBoxLayout* cabecalho_;
    std::vector<Campo> campos_;
    QTableWidget* gtmin_ = nullptr;
    QLabel* sem_configuracao_ = nullptr;
    QLabel* classe_titulo_ = nullptr;
    QSortFilterProxyModel* filtro_expansao_ = nullptr;
    QSortFilterProxyModel* filtro_manutencao_ = nullptr;
    QSortFilterProxyModel* filtro_custo_ = nullptr;
    QSortFilterProxyModel* filtro_mod_custo_ = nullptr;
};
