# HydroEdit2

Editor do cadastro de usinas hidráulicas do NEWAVE (`hidr.dat`). Inspirado no HydroEdit clássico.

<img width="861" height="520" alt="image" src="https://github.com/user-attachments/assets/419d7e77-e07e-437a-9698-9b8a6c9ff919" />

## Requisitos

- Qt 6.11.2, kit MSVC 2022 x64.
- Visual Studio 2026 Community (v18) com as ferramentas de C++.
- CMake 3.30 e Ninja, distribuídos com o Qt.

## Compilar

Em um prompt com o ambiente do Visual Studio carregado (`vcvars64.bat`):

```bat
cmake --preset msvc-debug
cmake --build --preset msvc-debug
ctest --preset msvc-debug
```

## Distribuição

```bat
scripts\empacotar.bat
```

O script compila o preset `msvc-release`, copia `HydroEdit2.exe` para `dist\` e roda o `windeployqt` para trazer as DLLs do Qt necessárias. O pacote resultante em `dist\` roda em uma máquina sem o Qt no PATH.

## Como usar

Abra um arquivo `hidr.dat` pelo menu Arquivo > Abrir. Se `sistema.dat` e `postos.dat` existirem no mesmo diretório do `hidr.dat`, seus nomes de usina e posto são usados para completar a tabela. Os arquivos `empresas.csv` e `turbinas.csv`, se colocados ao lado do executável, são opcionais e usados para resolver nomes adicionais; o formato de cada linha é `codigo;nome`.
