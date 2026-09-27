## <img width="32" height="32" alt="icon 32" src="https://github.com/user-attachments/assets/1d07387a-4f5b-4e74-9625-746050b05244" /> HydroEdit2

Editor dos dados de entrada do modelo NEWAVE. Também abre o cadastro de usinas hidráulicas (`hidr.dat`) de decks do DESSEM. Inspirado no HydroEdit clássico.

<img width="867" height="521" alt="image" src="https://github.com/user-attachments/assets/32994990-aa92-4156-9fa4-03cb43fbab92" />


## Como usar

Abra o `hidr.dat` do deck pelo menu Arquivo > Abrir. O programa lê o `arquivos.dat` da mesma pasta e carrega os demais arquivos do deck, cada um numa das abas:

| Aba | Arquivos |
| --- | --- |
| Cadastro | `hidr.dat`, `term.dat`, `clast.dat`, `postos.dat`, `ree.dat`, `tecno.dat` |
| Configuração | `confhd.dat`, `conft.dat`, `exph.dat`, `expt.dat`, `manutt.dat` |
| Modificações | `modif.dat` |
| Hidrologia | `vazoes.dat`, `vazpast.dat`, `dsvagua.dat`, `volref_saz.dat`, `polinjus.csv` |
| Sistema e mercado | `sistema.dat`, `patamar.dat`, `c_adic.dat`, `agrint.dat`, `loss.dat`, `gtminpat.dat`, `adterm.dat` |
| Restrições | `penalid.dat`, `curva.dat`, `cvar.dat`, `sar.dat`, `ghmin.dat`, `re.dat`, `restricao-eletrica.csv` |
| Dados gerais | `dger.dat`, `arquivos.dat`, `shist.dat`, `selcor.dat` |

- Usinas hidráulicas e térmicas abrem num editor com a tabela das usinas e o formulário da usina selecionada; o das térmicas reúne `term.dat`, `conft.dat`, `expt.dat`, `manutt.dat` e `clast.dat`. As hidráulicas têm também a vista da cascata.
- Os demais arquivos abrem como tabela editável. Arquivos com mais de um tipo de registro mostram cada seção na árvore; em blocos (por submercado, ano ou patamar), cada linha traz o bloco a que pertence. `dger.dat` e `selcor.dat` aparecem como lista de parâmetros. `postos.dat` e `vazoes.dat`, binários, também são editáveis.
- A aba Modificações agrupa os registros do `modif.dat` por categoria e palavra-chave.
- Ver > Editor textual troca as tabelas pelo texto de cada arquivo.
- Editar troca só as colunas do campo; o resto do arquivo é regravado igual, byte a byte. Arquivo alterado e não salvo ganha um ponto na árvore, e o programa pergunta antes de fechar.
- Ferramentas > Exportar vazões incrementais gera um CSV com a vazão de cada usina menos a dos postos imediatamente a montante, com opção de aplicar as regras do `REGRAS.DAT` do GEVAZP.

Os formatos seguem o Manual do Usuário do NEWAVE 30.0.2 (capítulo 3). Os arquivos `empresas.csv` e `turbinas.csv`, se colocados ao lado do executável, são opcionais e usados para resolver nomes adicionais; o formato de cada linha é `codigo;nome`.

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

### Build automático

O workflow `.github/workflows/build.yml` compila o executável único (Qt estático pelo vcpkg, no mesmo commit do vcpkg usado localmente) a cada push na `main` e deixa o `HydroEdit2.exe` como artefato do run. Se a versão de `src/app/main.cpp` ainda não tem release, o mesmo run cria a tag no commit compilado e publica a release com o executável: para lançar uma versão, basta mudar o número no `main.cpp`. O Qt compilado fica em cache entre os runs.

As notas da release saem dos commits desde a release anterior (`.github/scripts/notas_release.ps1`), agrupadas pela área escrita antes dos dois-pontos. Por isso a primeira linha de cada commit segue o formato `Área: descrição para quem usa o programa`, por exemplo `Cascata: legenda explica o triângulo e o círculo`. A área tem até três palavras (Cascata, Tabela, Formulário, Ferramentas, Vazões...). Commits de build, CI, testes, refatoração, troca de versão ou ajuste em algo que ainda não saiu em nenhuma release usam a área `Interno` e ficam fora das notas; commits sem área entram em "Outras mudanças".

### Visual Studio

Abra a pasta do repositório (Arquivo > Abrir > Pasta). O Visual Studio lê o `CMakePresets.json` e mostra os presets na barra de ferramentas (`msvc-debug`, `msvc-release`, `msvc-static`). Escolha `HydroEdit2.exe` como item de inicialização; a configuração de depuração em `.vs/launch.vs.json` já passa o deck de exemplo como argumento e coloca o Qt no PATH. Os testes aparecem no Gerenciador de Testes via CTest.
