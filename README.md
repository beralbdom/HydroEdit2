# <img width="32" height="32" alt="icon 32" src="https://github.com/user-attachments/assets/1d07387a-4f5b-4e74-9625-746050b05244"/> HydroEdit 2

<div align="center">
  
Editor dos dados de entrada do modelo NEWAVE, escrito em C++/Qt 6. Inspirado no HydroEdit clássico.
  
[![Build](https://github.com/beralbdom/HydroEdit2/actions/workflows/build.yml/badge.svg)](https://github.com/beralbdom/HydroEdit2/actions/workflows/build.yml)
[![Versão](https://img.shields.io/github/v/release/beralbdom/HydroEdit2?label=Vers%C3%A3o)](https://github.com/beralbdom/HydroEdit2/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/beralbdom/HydroEdit2/total?label=Downloads)](https://github.com/beralbdom/HydroEdit2/releases)
[![Licença](https://img.shields.io/github/license/beralbdom/HydroEdit2?label=Licen%C3%A7a)](LICENSE)
![Windows](https://img.shields.io/badge/Windows-x64-0078D6?logo=windows)
![Qt](https://img.shields.io/badge/Qt-6.11-41CD52?logo=qt)

</div>

<img width="1211" height="706" alt="image" src="https://github.com/user-attachments/assets/b2b1c369-494d-41c1-803f-bf9a679f39f8"/>

## Como usar

Abra o deck por Arquivo > Abrir deck (pasta), ou abra o `hidr.dat`, o `caso.dat` ou o `arquivos.dat` por Arquivo > Abrir. O programa lê o `arquivos.dat` da pasta e carrega os demais arquivos do deck, cada um numa das abas:

| Aba | Arquivos |
| --- | --- |
| Cadastro | `hidr.dat`, `term.dat`, `clast.dat`, `postos.dat`, `ree.dat`, `tecno.dat`, `clasgas.dat` |
| Configuração | `confhd.dat`, `conft.dat`, `exph.dat`, `expt.dat`, `manutt.dat` |
| Modificações | `modif.dat` |
| Hidrologia | `vazoes.dat`, `vazpast.dat`, `dsvagua.dat`, `volref_saz.dat`, `polinjus.csv`, `volumes-referencia.csv` |
| Sistema e carga | `sistema.dat`, `patamar.dat`, `c_adic.dat`, `agrint.dat`, `loss.dat`, `gtminpat.dat`, `adterm.dat` |
| Restrições | `penalid.dat`, `curva.dat`, `cvar.dat`, `sar.dat`, `ghmin.dat`, `re.dat`, `restricao-eletrica.csv`, `gee.dat` |
| Dados gerais | `dger.dat`, `arquivos.dat`, `shist.dat`, `selcor.dat`, `abertura.dat`, `indices.csv` |

- Usinas hidráulicas e térmicas abrem num editor com a tabela das usinas e dados cadastrais da usina selecionada; o das térmicas reúne `term.dat`, `conft.dat`, `expt.dat`, `manutt.dat` e `clast.dat`. As hidráulicas têm também a vista da cascata, desenhada como rio: o curso principal de cada bacia segue reto, os afluentes ficam ao lado e nenhuma ligação passa sobre uma usina.
- Nos demais arquivos, o que tem poucas linhas (parâmetros, listas de submercados, interligações e blocos) abre como formulário ao clicar no arquivo, e o que é série ou cadastro extenso abre como tabela editável, uma por seção na árvore. Em blocos (por submercado, ano ou patamar), cada linha da tabela traz o bloco a que pertence. Arquivos com formulário e uma só tabela mostram os dois na mesma página. Colunas por patamar seguem o número de patamares de carga do `patamar.dat` e de déficit do `sistema.dat`. `dger.dat` e `selcor.dat` são formulários de parâmetros; os do `dger.dat` ficam agrupados por tema (caso, política, cenários e simulação final, representação e relatórios), com um item por tema na árvore. `postos.dat` e `vazoes.dat` também são editáveis.
- A aba Modificações edita o `modif.dat`: os registros ficam agrupados por categoria e modificador, com os campos de cada modificador em colunas (mês, ano, valor, unidade, conjunto, um por patamar), e os botões Adicionar, Remover e Salvar.

### Funções especiais
- Clicar no cabeçalho de uma coluna abre o filtro por valores e a ordenação, como nas planilhas. O conteúdo das tabelas pode ser copiado e colado com Ctrl+C e Ctrl+V, inclusive de e para o Excel; a cópia leva os títulos das colunas na primeira linha.
- Submercados, REEs, usinas, classes, tecnologias e postos aparecem pelo nome, com o código entre parênteses, e são escolhidos numa lista. O mesmo vale para as flags e códigos que o manual define, como as opções do `dger.dat` e a situação das usinas.
- Os botões Adicionar e Remover (também no menu do botão direito e nas listas dos formulários) inserem a cópia de um registro ou de um bloco inteiro, como uma interligação com todos os anos, ou um registro em branco, e removem registros e blocos.
- No formulário do `patamar.dat`, Adicionar patamar e Remover patamar mudam o número de patamares de carga em todos os arquivos que dependem dele (`patamar.dat`, `loss.dat`, `gtminpat.dat`, `agrint.dat`, `adterm.dat`, `ghmin.dat`, `penalid.dat`, `re.dat` e `restricao-eletrica.csv`) e avisam o que precisa ser conferido, como a soma das durações; no `sistema.dat`, o mesmo vale para os patamares de déficit.
- Editar > Desfazer e Refazer valem para o cadastro de usinas e para os arquivos do deck; uma colagem se desfaz de uma vez.
- Editar > Procurar no deck (Ctrl+Shift+F) acha uma usina, REE, submercado, posto ou classe pelo nome ou pelo código em todos os arquivos, inclusive onde só aparece o código, e leva ao registro.
- Arquivo > Salvar tudo (Ctrl+Alt+S) salva o cadastro e todos os arquivos do deck alterados; Arquivo > Salvar deck em outra pasta copia o deck para a pasta escolhida, grava nela as alterações e abre o deck novo, para criar uma revisão sem mexer na atual.
- Ferramentas > Comparar com outro deck mostra o que mudou em cada arquivo: os de texto linha a linha, o cadastro de usinas campo a campo e os binários registro a registro.
- Ferramentas > Validar deck confere o deck pelas regras do manual do NEWAVE (referências a cadastros, valores das opções, parâmetros do `dger.dat`, déficit, penalidades, agrupamentos, restrições elétricas, modificações e capacidade do programa), cada problema com a seção e a página do manual.
- Os formulários distribuem os grupos em quantas colunas couberem na largura da janela.
- Ver > Editor textual troca as tabelas pelo texto de cada arquivo.
- Ferramentas > Exportar vazões incrementais gera um CSV com a vazão de cada usina menos a dos postos imediatamente a montante, com opção de aplicar as regras do `REGRAS.DAT` do GEVAZP.
- Os arquivos `empresas.csv` e `turbinas.csv`, se colocados ao lado do executável, são opcionais e usados para resolver nomes de agentes e turbinas; o formato de cada linha é `codigo;nome`.

## Arquivos suportados

| Modelo | Arquivos |
| --- | --- |
| NEWAVE | todos os da tabela acima |
| DESSEM | `hidr.dat` |

## Compilar

- Qt 6.11.2, kit MSVC 2022 x64.
- Visual Studio 2026 Community (v18) com as ferramentas de C++.
- CMake 3.30 e Ninja, distribuídos com o Qt.

Em um prompt com o ambiente do Visual Studio carregado (`vcvars64.bat`):

```bat
cmake --preset msvc-debug
cmake --build --preset msvc-debug
ctest --preset msvc-debug
```
