<#
  FFmpeg 7.1.5 para el port UWP (la música y las películas del juego). Solo avcodec y avutil, con los
  decodificadores que usa Yakumo (ATRAC3, ATRAC3plus y H.264), compilados con MSVC en su entorno «store» y
  enlazados con /APPCONTAINER, como pide la consola. Sale en <Trabajo>\install\ffmpeg\ (bin, lib, include);
  construir.ps1 lo toma de ahí.

  Uso:  pwsh -File ffmpeg_uwp.ps1 [-Trabajo $($env:MHP3RD_FFMPEG)] [-Hilos 16]
  Requisitos: MSYS2 en $($env:MHP3RD_MSYS) (make, tar, xz y diff) y Visual Studio con las herramientas de UWP. Las
  fuentes salen del espejo del subproyecto (xbox\espejo\ffmpeg-7.1.5.tar.xz) y se comprueba su SHA-256.
#>
param(
    [string] $Trabajo = "$($env:MHP3RD_FFMPEG)",
    [int] $Hilos = 16
)
$ErrorActionPreference = 'Stop'

$aqui = $PSScriptRoot
$sub = Split-Path -Parent (Split-Path -Parent $aqui)
$tarball = Join-Path $sub 'xbox\espejo\ffmpeg-7.1.5.tar.xz'
$sha = 'DE668509CAF9E35E3CD162473441FDB29538C6D96ED080292B3CF9E6FC5D558F'   # la que fija Yakumo en FFmpeg.cmake
$bash = "$($env:MHP3RD_MSYS)\usr\bin\bash.exe"

function ConvertTo-Posix([string] $ruta) {
    $r = $ruta -replace '\\', '/'
    if ($r -match '^([A-Za-z]):(.*)$') { return '/' + $Matches[1].ToLower() + $Matches[2] }
    return $r
}

foreach ($r in $tarball, $bash) { if (-not (Test-Path -LiteralPath $r)) { throw "falta un requisito: $r" } }
New-Item -ItemType Directory -Force -Path $Trabajo | Out-Null

# Las fuentes, del espejo y comprobadas.
if (-not (Test-Path -LiteralPath (Join-Path $Trabajo 'ffmpeg-7.1.5\configure'))) {
    $h = (Get-FileHash -LiteralPath $tarball -Algorithm SHA256).Hash
    if ($h -ne $sha) { throw "ffmpeg-7.1.5.tar.xz tiene SHA-256 $h, se esperaba $sha" }
    & $bash -lc "cd '$(ConvertTo-Posix $Trabajo)' && tar -xJf '$(ConvertTo-Posix $tarball)'"
    if ($LASTEXITCODE) { throw 'no se pudieron descomprimir las fuentes' }
}

# El entorno «store» de MSVC: cl y link de las apps, con el CRT _app y WindowsApp.lib.
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$lineas = & cmd /c "`"$(Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat')`" x64 store >nul 2>&1 && set"
foreach ($l in $lineas) {
    if ($l -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
}
# El bash de MSYS2 hereda ese PATH (MSYS2_PATH_TYPE=inherit) y se queda en la carpeta actual.
$env:MSYS2_PATH_TYPE = 'inherit'
$env:CHERE_INVOKING = '1'

# bash no admite los saltos CRLF de Windows: corre una copia del script con saltos LF.
$sh = Join-Path $Trabajo 'ffmpeg_uwp.sh'
[System.IO.File]::WriteAllText($sh, ([System.IO.File]::ReadAllText((Join-Path $aqui 'ffmpeg_uwp.sh')) -replace "`r`n", "`n"))

$log = Join-Path $Trabajo 'compilar.log'
$t0 = Get-Date
& $bash -lc "sh '$(ConvertTo-Posix $sh)' '$(ConvertTo-Posix $Trabajo)' $Hilos" 2>&1 |
    ForEach-Object { "$_" } | Tee-Object -FilePath $log | Select-Object -Last 15
if ($LASTEXITCODE) { throw "la compilación de FFmpeg falló (registro: $log)" }

# Comprobación: cada DLL con el bit APPCONTAINER y el CRT de las apps.
$bin = Join-Path $Trabajo 'install\ffmpeg\bin'
foreach ($dll in 'avcodec-61.dll', 'avutil-59.dll') {
    $ruta = Join-Path $bin $dll
    if (-not (Test-Path -LiteralPath $ruta)) { throw "no se generó $dll" }
    $b = [System.IO.File]::ReadAllBytes($ruta)
    $pe = [BitConverter]::ToInt32($b, 0x3C)
    $carac = [BitConverter]::ToUInt16($b, $pe + 24 + 70)
    if (-not ($carac -band 0x1000)) { throw "a $dll le falta el bit APPCONTAINER" }
    if (-not [System.Text.Encoding]::ASCII.GetString($b).ToLowerInvariant().Contains('vcruntime140_app.dll')) {
        throw "$dll no importa el CRT _app"
    }
    "$dll verificado: $((Get-Item -LiteralPath $ruta).Length) B, APPCONTAINER y CRT _app"
}
# La licencia (LGPL 2.1 o posterior: sin --enable-gpl ni --enable-version3), que el paquete lleva junto a las DLL.
Copy-Item -LiteralPath (Join-Path $Trabajo 'ffmpeg-7.1.5\COPYING.LGPLv2.1') -Destination (Join-Path $Trabajo 'install\ffmpeg') -Force
"FFmpeg para UWP listo en $([int]((Get-Date) - $t0).TotalSeconds) s: $(Join-Path $Trabajo 'install\ffmpeg')"
