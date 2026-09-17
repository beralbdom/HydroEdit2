#pragma once
#include <QWidget>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "campos.h"

class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLayout;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QVBoxLayout;
class ModeloHidr;
class GradeVetor;

class FormularioUsina : public QWidget {
    Q_OBJECT
public:
    explicit FormularioUsina(ModeloHidr* modelo, QWidget* parent = nullptr);
    void definirLinha(int linha);
    int linha() const { return linha_; }
    void focarCampo(std::string_view nome);
    void marcarProblemas(const std::vector<std::string>& campos_com_erro);

private:
    enum Pagina { kCadastro = 0, kReservatorio, kPolinomios, kConjuntos, kJusante, kOperacao };

    struct Combo { QComboBox* widget; const Campo* campo; };
    struct Edit { QLineEdit* widget; const Campo* campo; QLabel* nome_lookup; };

    QWidget* criarPaginaCadastro();
    QWidget* criarPaginaReservatorio();
    QWidget* criarPaginaPolinomios();
    QWidget* criarPaginaConjuntos();
    QWidget* criarPaginaJusante();
    QWidget* criarPaginaOperacao();

    static void configurarLayout(QLayout* l);
    static QVBoxLayout* novaPagina(QWidget* pai);
    static QGroupBox* novoGrupo(QWidget* pai, const QString& titulo);
    static QFormLayout* novoForm(QWidget* pai);
    QLineEdit* ligarEdit(QFormLayout* f, int pagina, const QString& rotulo, const char* campo, bool com_lookup = false);
    QComboBox* ligarCombo(QFormLayout* f, int pagina, const QString& rotulo, const char* campo);
    void registrarCampoGrade(int pagina, const char* nome);
    void recarregarListas();
    void atualizar();
    void aoEditarEdit(const char* nome);
    void aoEscolherCombo(const char* nome, int indice);

    ModeloHidr* modelo_;
    int linha_ = -1;
    bool atualizando_ = false;
    std::map<std::string, Edit> edits_;
    std::map<std::string, Combo> combos_;
    std::vector<GradeVetor*> grades_;
    std::map<std::string, int> pagina_do_campo_;
    QLabel* titulo_;
    QListWidget* menu_;
    QStackedWidget* paginas_;
};
