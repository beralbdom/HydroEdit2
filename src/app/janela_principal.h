#pragma once
#include <QMainWindow>
#include <vector>

class QCheckBox;
class QCloseEvent;
class QLabel;
class QLineEdit;
class QMenu;
class QSplitter;
class QTableView;
class QTabWidget;
class QToolButton;
class ModeloHidr;
class FiltroUsinas;
class FormularioUsina;
class PainelProblemas;
class VistaCascata;
struct ProblemaUsina;

class JanelaPrincipal : public QMainWindow {
    Q_OBJECT
public:
    explicit JanelaPrincipal(QWidget* parent = nullptr);
    void abrirCaminho(const QString& caminho);

protected:
    int linhaSelecionada() const;
    void selecionarLinha(int linha);
    bool salvarEm(const QString& caminho);
    void atualizarTitulo();
    void atualizarStatus();
    void registrarRecente(const QString& caminho);
    void atualizarRecentes();
    void closeEvent(QCloseEvent* ev) override;
    void showEvent(QShowEvent* ev) override;
    std::vector<ProblemaUsina> validarTudo() const;
    bool validarAntesDeSalvar();
    bool confirmarDescarte();
    void marcarProblemasDaLinha(int linha);

    ModeloHidr* modelo_;
    FiltroUsinas* filtro_;
    QTableView* tabela_;
    QSplitter* splitter_;
    FormularioUsina* formulario_;
    PainelProblemas* painel_problemas_;
    VistaCascata* vista_cascata_;

private slots:
    void abrir();
    void salvar();
    void salvarComo();
    void exportarCsv();
    void novaUsina();
    void novaUsinaEmCodigo();
    void duplicarUsina();
    void excluirUsina();

private:
    void criarMenus();
    void criarTabela();
    void repovoarFiltrosCascata();
    void aplicarFiltrosCascata();
    int primeiroCodigoLivre() const;

    QLineEdit* campo_filtro_;
    QCheckBox* ocultar_vazias_;
    QToolButton* ree_cascata_;
    QToolButton* submercado_cascata_;
    QCheckBox* nomes_cascata_;
    QCheckBox* ficticias_cascata_;
    QMenu* menu_ree_;
    QMenu* menu_submercado_;
    QLabel* status_arquivo_;
    QLabel* status_modelo_;
    QLabel* status_usinas_;
    QLabel* status_validacao_;
    QLabel* status_notas_;
    QAction* acao_salvar_;
    QAction* acao_salvar_como_;
    QAction* acao_exportar_;
    QMenu* menu_usina_;
    QMenu* menu_recentes_;
    bool dimensionado_ = false;
};
