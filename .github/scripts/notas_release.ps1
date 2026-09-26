# Monta as notas de uma release a partir dos commits entre a release anterior e o commit publicado.
# Cada commit no formato "Area: descricao" entra na secao da area, na ordem em que foi feito; os com
# area "Interno" (build, CI, testes, troca de versao) ficam de fora, assim como os antigos "Versao
# N". Commits sem area reconhecivel (mais de tres palavras ou mais de 30 caracteres antes dos
# dois-pontos, ou sem dois-pontos) vao para "Outras mudancas", no fim. Sem release anterior, usa o
# historico inteiro.
param(
    [string]$Anterior,
    [Parameter(Mandatory)][string]$Alvo,
    [Parameter(Mandatory)][string]$Versao,
    [Parameter(Mandatory)][string]$Repositorio,
    [Parameter(Mandatory)][string]$Saida
)

[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$intervalo = if ($Anterior) { "$Anterior..$Alvo" } else { $Alvo }
$assuntos = @(git -c i18n.logOutputEncoding=UTF-8 log --reverse --no-merges --format=%s $intervalo)
if ($LASTEXITCODE -ne 0) { throw "git log falhou para $intervalo" }

$outras = 'Outras mudanças'
$secoes = [ordered]@{}
foreach ($assunto in $assuntos) {
    $assunto = $assunto.Trim()
    if (-not $assunto -or $assunto -match '^Vers(a|ã)o \d') { continue }
    $area = $outras
    $texto = $assunto
    if ($assunto -match '^(?<area>[^:]{1,30}):\s+(?<texto>.+)$' -and ($Matches.area.Trim() -split '\s+').Count -le 3) {
        $area = $Matches.area.Trim()
        $texto = $Matches.texto.Trim()
    }
    if ($area -ieq 'Interno') { continue }
    $area = $area.Substring(0, 1).ToUpper() + $area.Substring(1)
    $texto = $texto.Substring(0, 1).ToUpper() + $texto.Substring(1)
    if (-not $secoes.Contains($area)) { $secoes[$area] = [System.Collections.Generic.List[string]]::new() }
    $secoes[$area].Add($texto)
}

$linhas = [System.Collections.Generic.List[string]]::new()
$ordem = @($secoes.Keys | Where-Object { $_ -ne $outras }) + @($secoes.Keys | Where-Object { $_ -eq $outras })
foreach ($area in $ordem) {
    $linhas.Add("### $area")
    foreach ($texto in $secoes[$area]) { $linhas.Add("- $texto") }
    $linhas.Add('')
}
if ($linhas.Count -eq 0) { $linhas.Add('Sem mudanças para quem usa o programa.'); $linhas.Add('') }
if ($Anterior) { $linhas.Add("**Comparação completa**: https://github.com/$Repositorio/compare/$Anterior...$Versao") }

[IO.File]::WriteAllText($Saida, ($linhas -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
