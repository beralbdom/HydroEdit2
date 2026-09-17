# Analise do HydroEdit 4.0a (engenharia reversa)

Data: 2026-09-17. Fonte: `HydroEdit40a.exe` (unico artefato disponivel, sem codigo-fonte).

## Toolchain

- Delphi 5 (Borland, RTL 1999), PE32 i386, GUI Win32.
- Componentes: VCL padrao (TStringGrid, TComboBox, TEdit), NetMasters (TNMShow, componente de rede nao usado na UI principal).
- Autor: Rodrigo Carvalho Nogueira Vilanova (ONS), tela "About".
- Recursos extraidos em `rev/dfm/*.txt` (formularios TFRMEDITOR, TFRMHELPABOUT, TNMSHOW) e `rev/strings_code.txt`.

## O que o programa faz

Editor de cadastro de usinas hidraulicas do NEWAVE (`hidr.dat`), com uma unica tela principal:

- Menu File: Load, Export (CSV `CadUsH.csv`), Print (tela / usina atual / todas), Exit.
- Menu Edit: Language (Portugues / English; Espanhol e Frances desabilitados), Search (por codigo / por nome, desabilitado), Plant (Create / Change / Delete), "Gera Alter. MSD" (oculto; grava `Hidr_Modif.dat`).
- Menu Help: About.
- Combo de usinas no topo; campos exibidos como TEdit read-only; ao entrar em "Change" viram editaveis e aparecem botoes Save/Cancel.
- Procura um arquivo auxiliar `Usinas.Dat` com definicoes de turbinas, sistemas e empresas (nomes para os codigos). Se nao existe, avisa e segue.
- Ao criar usina: codigo deve ser 1..320 e nao pode colidir com existente.

## Formato do arquivo (confirmado no binario e no deck)

- Registro fixo de 792 bytes (constante `mov ecx,792` em 16 pontos do codigo), 320 registros (limite hardcoded, mensagens "entre 1 e 320").
- 320 x 792 = 253440 bytes = tamanho exato do `hidr.dat` do deck `deck_newave_2026_09_rev1`.
- Codigo da usina = posicao do registro (1-based). Registros vazios tem nome em branco.
- Little-endian, float32 IEEE, int32.
- Layout (offsets em bytes), validado com o deck e coincidente com o inewave (`inewave/newave/modelos/hidr.py`):

| Offset | Tam | Tipo | Campo |
|---|---|---|---|
| 0 | 12 | char | Nome |
| 12 | 4 | int | Posto |
| 16 | 8 | char | Posto BDH (texto; inewave le como int, errado: deck tem 8 espacos = 0x2020202020202020) |
| 24 | 4 | int | Subsistema |
| 28 | 4 | int | Empresa |
| 32 | 4 | int | Jusante (codigo da usina) |
| 36 | 4 | int | Desvio (codigo da usina) |
| 40 | 4 | float | Volume minimo (hm3) |
| 44 | 4 | float | Volume maximo (hm3) |
| 48 | 4 | float | Volume crista vertedouro (hm3) |
| 52 | 4 | float | Volume canal de desvio (hm3) |
| 56 | 4 | float | Cota minima (m) |
| 60 | 4 | float | Cota maxima (m) |
| 64 | 20 | 5 float | Polinomio cota x volume A0..A4 |
| 84 | 20 | 5 float | Polinomio area x cota A0..A4 |
| 104 | 48 | 12 int | Evaporacao mensal jan..dez (mm/mes) |
| 152 | 4 | int | Numero de conjuntos de maquinas (0..5) |
| 156 | 20 | 5 int | Numero de maquinas por conjunto |
| 176 | 20 | 5 float | Potencia efetiva por conjunto (MW) |
| 196 | 300 | 75 float | 5 conjuntos x 3 polinomios (turbina, gerador, potencia) x A0..A4. Grid "Conj. de Maquinas 2" do HydroEdit (15 linhas). Todo zero no deck atual. |
| 496 | 20 | 5 float | Altura efetiva por conjunto HEf (m) |
| 516 | 20 | 5 int | Vazao efetiva por conjunto QEf (m3/s) |
| 536 | 4 | float | Produtibilidade especifica (MW/m3/s/m) |
| 540 | 4 | float | Perdas (valor) |
| 544 | 4 | int | Numero de polinomios de jusante (1..6) |
| 548 | 120 | 6 x 5 float | Polinomios de jusante A0..A4 |
| 668 | 24 | 6 float | Referencia (m) de cada polinomio de jusante |
| 692 | 4 | float | Canal de fuga medio (m) |
| 696 | 4 | int | Influencia do vertimento no canal de fuga (0/1) |
| 700 | 4 | float | Fator de carga maximo (%) |
| 704 | 4 | float | Fator de carga minimo (%) |
| 708 | 4 | int | Vazao minima do historico (m3/s) |
| 712 | 4 | int | Numero de unidades de base |
| 716 | 4 | int | Tipo de turbina (deck: 0..3) |
| 720 | 4 | int | Representacao do conjunto (0=aprox, 1=detalhada, 2=simplificada) |
| 724 | 4 | float | TEIF (%) |
| 728 | 4 | float | IP (%) |
| 732 | 4 | int | Tipo de perda (deck: 0..2) |
| 736 | 12 | char | Data (ex. `27-08-26`) |
| 748 | 39 | char | Observacao |
| 787 | 4 | float | Volume de referencia (hm3) |
| 791 | 1 | char | Regulacao: M (mensal), S (semanal), D (diaria) |

Observacoes:

- Deck atual: 212 usinas preenchidas de 320.
- Existe variante `Patamares/dados/epe/hidr.dat` com 468867 bytes = 592 x 792 + 3 bytes extras. Mesmo layout de registro, mais registros. Um editor novo deve usar `tamanho // 792` como numero de usinas e nao fixar 320.
- Campos que o HydroEdit mostra mas nao estao na tabela do inewave: os 75 floats do bloco 196..496 (inewave ignora).

## Validacoes de consistencia do HydroEdit (mensagens extraidas)

Aplicadas ao salvar uma alteracao:

- Codigo 1..320; nome com >= 1 caractere; posto > 0; posto BDH > 0.
- Volumes nao nulos, nao negativos; Vmin <= Vmax; Vmin <= Vvert <= Vmax; Vmin <= Vdesv <= Vmax; Vmin <= Vref <= Vmax.
- Cotas nao nulas, nao negativas; volumes iguais exigem cotas iguais e vice-versa.
- A0 dos polinomios cota x volume e area x cota nao nulo e diferente de zero.
- Numero de conjuntos 0..5; dados dos grids de conjuntos nao nulos/negativos; A0 dos polinomios turbina/gerador/potencia diferente de zero.
- Numero de polinomios de jusante 1..6; A0 diferente de zero; referencia nao negativa.
- Canal de fuga medio nao nulo, nao negativo e menor que a cota minima de montante.
- Produtibilidade, perdas, fatores de carga, TEIF, IP, vazao minima historica e unidades de base nao nulos e nao negativos; FCmin <= FCmax.

Estas regras vieram do binario, nao de documentacao oficial. Devem ser confrontadas com o manual do usuario do NEWAVE antes de entrar na especificacao.

## Exportacao CSV

Cabecalhos do CSV (portugues e ingles) estao em `rev/strings_code.txt`. Um registro por usina, campos escalares mais grids achatados.
