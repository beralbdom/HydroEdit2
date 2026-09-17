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
class ModeloHidr;
class FiltroUsinas;
class FormularioUsina;
class PainelProblemas;
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
    int primeiroCodigoLivre() const;

    QLineEdit* campo_filtro_;
    QCheckBox* ocultar_vazias_;
    QLabel* status_arquivo_;
    QLabel* status_usinas_;
    QLabel* status_notas_;
    QAction* acao_salvar_;
    QAction* acao_salvar_como_;
    QAction* acao_exportar_;
    QMenu* menu_usina_;
    QMenu* menu_recentes_;
};
