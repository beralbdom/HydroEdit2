#pragma once
#include <QMainWindow>

class QCheckBox;
class QLabel;
class QLineEdit;
class QSplitter;
class QTableView;
class ModeloHidr;
class FiltroUsinas;
class FormularioUsina;

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

    ModeloHidr* modelo_;
    FiltroUsinas* filtro_;
    QTableView* tabela_;
    QSplitter* splitter_;
    FormularioUsina* formulario_;

private slots:
    void abrir();
    void salvar();
    void salvarComo();
    void exportarCsv();

private:
    void criarMenus();
    void criarTabela();

    QLineEdit* campo_filtro_;
    QCheckBox* ocultar_vazias_;
    QLabel* status_arquivo_;
    QLabel* status_usinas_;
    QLabel* status_notas_;
    QAction* acao_salvar_;
    QAction* acao_salvar_como_;
    QAction* acao_exportar_;
};
