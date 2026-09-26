# <img width="32" height="32" alt="icon 32" src="https://github.com/user-attachments/assets/1d07387a-4f5b-4e74-9625-746050b05244" /> HydroEdit 2

Editor de dados de entrada do modelo NEWAVE. Inspirado no HydroEdit clássico.

<img width="840" height="532" alt="image" src="https://github.com/user-attachments/assets/abacd15d-7ef1-403a-8c67-efe7fa1935a4" />

## Como usar

Abra um arquivo `hidr.dat` pelo menu Arquivo > Abrir. Se `sistema.dat` e `postos.dat` existirem no mesmo diretório do `hidr.dat`, seus nomes de usina e posto são usados para completar a tabela. Os arquivos `empresas.csv` e `turbinas.csv`, se colocados ao lado do executável, são opcionais e usados para resolver nomes adicionais; o formato de cada linha é `codigo;nome`.

## Arquivos suportados
| Modelo    | Arquivo |
| -------- | ------- |
| NEWAVE  | hidr    |

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

As notas da release saem dos commits desde a release anterior (`.github/scripts/notas_release.ps1`), agrupadas pela área escrita antes dos dois-pontos. Por isso a primeira linha de cada commit segue o formato `Área: descrição para quem usa o programa`, por exemplo `Cascata: legenda explica o triângulo e o círculo`. A área tem até três palavras (Cascata, Tabela, Formulário, Ferramentas, Vazões...). Commits de build, CI, testes, refatoração ou troca de versão usam a área `Interno` e ficam fora das notas; commits sem área entram em "Outras mudanças".

### Visual Studio

Abra a pasta do repositório (Arquivo > Abrir > Pasta). O Visual Studio lê o `CMakePresets.json` e mostra os presets na barra de ferramentas (`msvc-debug`, `msvc-release`, `msvc-static`). Escolha `HydroEdit2.exe` como item de inicialização; a configuração de depuração em `.vs/launch.vs.json` já passa o deck de exemplo como argumento e coloca o Qt no PATH. Os testes aparecem no Gerenciador de Testes via CTest.
