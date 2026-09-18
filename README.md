# <img width="32" height="32" alt="icon copy" src="https://github.com/user-attachments/assets/2d999694-35ee-4c9c-83c9-f39248c9c6cb" /> HydroEdit 2

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

```bat
scripts\compilar.bat
```
