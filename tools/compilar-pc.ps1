<#
  Etapa 3 del camino a la Xbox: compilar Yakumo v0.6.0-alpha.4 desde su código, con la misma
  configuración que la release de Windows según su BUILDINFO.txt (Release, MHP3RD_RELEASE=ON,
  MSVC 19.44, SDL3 3.4.16, FFmpeg 7.1.5 de BtbN, 355 overlays).

  Pasos, en este orden (-Paso todo los encadena):
    arbol       extrae el código del espejo (pc\espejo\Yakumo-v0.6.0-alpha.4-codigo.zip) en el árbol
    preparar    prepare_game.sh: copia la ISO del usuario y el EBOOT.ELF preparado a profiles\mhp3rd\game
    configurar  cmake -G Ninja con la configuración de la release
    generar     generate.sh: psp_analyze y psp_recomp escriben las unidades de C++ en profiles\mhp3rd\generated
    compilar    reconfigura, ya con las unidades, y compila Yakumo.exe
    overlays    build_overlays.sh: extrae las 355 overlays de DATA.BIN y compila una DLL por overlay
    armar       copia SDL3.dll junto a Yakumo.exe y cuenta lo que quedó en out\mhp3rd\bin

  Cada paso deja su registro en <árbol>\_registros\<paso>.log. Los requisitos están en el técnico
  §13.3: Visual Studio 2022 con C++ de escritorio, el Vulkan SDK 1.4.357.0, Git for Windows, SDL3
  3.4.16 en $($env:MHP3RD_SDL3) y el entorno de Python $($env:MHP3RD_PY), cuyo python3.exe sustituye al atajo de la
  Microsoft Store que Windows trae con ese nombre.
#>
param(
    # Uno o varios pasos separados por comas («-Paso generar,compilar»), o «todo». Con «pwsh -File»
    # una lista llega como texto suelto, por eso se separa aquí y no con ValidateSet.
    [string[]] $Paso = @('todo'),
    [string] $Arbol = "$($env:MHP3RD_ARBOL)",
    # Unidades grandes compilando a la vez. Yakumo pone una por cada 4 GiB de memoria (15 aquí), pero
    # con lo que la PC tiene abierto quedan unos 40 GB libres: 8 deja margen.
    [int] $UnidadesALaVez = 8,
    [int] $Hilos = [Environment]::ProcessorCount
)
$ErrorActionPreference = 'Stop'

$sub = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$zipCodigo = Join-Path $sub 'pc\espejo\Yakumo-v0.6.0-alpha.4-codigo.zip'
$elf = Join-Path $sub 'pc\datos\EBOOT.ELF'
$vulkan = 'C:\Program Files\VulkanSDK\1.4.357.0'
$sdl = "$($env:MHP3RD_SDL3)"
$py = "$($env:MHP3RD_PY)\Scripts"
$bash = 'C:\Program Files\Git\bin\bash.exe'
$build = Join-Path $Arbol 'out\mhp3rd'
$regs = Join-Path $Arbol '_registros'

function ConvertTo-Posix([string] $ruta) {
    $r = $ruta -replace '\\', '/'
    if ($r -match '^([A-Za-z]):(.*)$') { return '/' + $Matches[1].ToLower() + $Matches[2] }
    return $r
}

function Import-Entorno {
    $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
    $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
    $lineas = & cmd /c "`"$vcvars`" x64 >nul 2>&1 && set"
    foreach ($l in $lineas) {
        if ($l -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
    foreach ($r in $vulkan, $sdl, $py, $bash) {
        if (-not (Test-Path -LiteralPath $r)) { throw "falta un requisito: $r (técnico §13.3)" }
    }
    $env:VULKAN_SDK = $vulkan
    $env:PATH = "$py;$vulkan\Bin;$env:PATH"
}

# Corre una orden y deja su salida en pantalla y en el registro del paso; se para si falla.
function Invoke-Registrado([string] $log, [scriptblock] $bloque) {
    & $bloque 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath $log -Append
    if ($LASTEXITCODE -ne 0) { throw "el paso falló con código $LASTEXITCODE (registro: $log)" }
}

# El bloque se cierra sobre $linea: sin GetNewClosure, PowerShell la buscaría por ámbito dinámico.
function Invoke-Bash([string] $log, [string] $ordenBash) {
    $linea = "cd '$(ConvertTo-Posix $Arbol)' && $ordenBash"
    Invoke-Registrado $log { & $bash -c $linea }.GetNewClosure()
}

function Iniciar-Registro([string] $nombre) {
    New-Item -ItemType Directory -Force -Path $regs | Out-Null
    $log = Join-Path $regs "$nombre.log"
    "== $nombre · $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" | Out-File -LiteralPath $log -Append -Encoding utf8
    return $log
}

function Paso-Arbol {
    if (Test-Path -LiteralPath (Join-Path $Arbol 'CMakeLists.txt')) { "árbol ya extraído: $Arbol"; return }
    $tmp = "$Arbol.extrayendo"
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zipCodigo, $tmp)
    Move-Item -LiteralPath (Join-Path $tmp 'Yakumo-0.6.0-alpha.4') -Destination $Arbol
    "árbol extraído en $Arbol (queda vacía $tmp)"
}

function Paso-Preparar {
    $log = Iniciar-Registro 'preparar'
    $iso = @(Get-ChildItem -LiteralPath (Join-Path $sub 'dependencias-usuario') -Filter '*.iso')
    if ($iso.Count -ne 1) { throw "se esperaba una sola ISO en dependencias-usuario y hay $($iso.Count)" }
    Invoke-Bash $log "profiles/mhp3rd/scripts/prepare_game.sh '$(ConvertTo-Posix $iso[0].FullName)' '$(ConvertTo-Posix $elf)'"
}

function Paso-Configurar {
    $log = Iniciar-Registro 'configurar'
    Invoke-Registrado $log {
        cmake -S $Arbol -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSPRECOMP_PROFILE=mhp3rd `
            -DMHP3RD_RELEASE=ON "-DCMAKE_PREFIX_PATH=$($sdl -replace '\\', '/')" `
            "-DPSPRECOMP_GENERATED_JOBS=$UnidadesALaVez"
    }
    $texto = Get-Content -LiteralPath $log -Raw
    if ($texto -notmatch 'mhp3rd: Vulkan renderer enabled') { throw 'la configuración no activó el renderer Vulkan' }
}

function Paso-Generar {
    $log = Iniciar-Registro 'generar'
    Invoke-Bash $log 'profiles/mhp3rd/scripts/generate.sh'
}

function Paso-Compilar {
    $log = Iniciar-Registro 'compilar'
    Invoke-Registrado $log { cmake -S $Arbol -B $build }
    if ((Get-Content -LiteralPath $log -Raw) -notmatch 'mhp3rd: (\d+) generated AOT units') { throw 'no hay unidades generadas: falta el paso generar' }
    Invoke-Registrado $log { cmake --build $build --target Yakumo -j $Hilos }
}

function Paso-Overlays {
    $log = Iniciar-Registro 'overlays'
    Invoke-Bash $log "profiles/mhp3rd/scripts/build_overlays.sh '$(ConvertTo-Posix $build)' $Hilos"
}

function Paso-Armar {
    $log = Iniciar-Registro 'armar'
    $bin = Join-Path $build 'bin'
    Copy-Item -LiteralPath (Join-Path $sdl 'lib\x64\SDL3.dll') -Destination $bin -Force
    $n = @(Get-ChildItem -LiteralPath (Join-Path $bin 'overlays') -Filter '*.dll' -ErrorAction SilentlyContinue).Count
    $salida = @(
        "Yakumo.exe: $((Get-Item -LiteralPath (Join-Path $bin 'Yakumo.exe')).Length) B"
        "DLL junto al exe: $((Get-ChildItem -LiteralPath $bin -Filter '*.dll' | ForEach-Object Name) -join ', ')"
        "overlays: $n DLL"
    )
    $salida | Tee-Object -FilePath $log -Append
}

$orden = 'arbol', 'preparar', 'configurar', 'generar', 'compilar', 'overlays', 'armar'
$Paso = @($Paso -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ })
foreach ($p in $Paso) { if ($p -ne 'todo' -and $orden -notcontains $p) { throw "paso desconocido: $p (valen: $($orden -join ', '), todo)" } }
Import-Entorno
$pasos = if ($Paso -contains 'todo') { $orden } else { $orden | Where-Object { $Paso -contains $_ } }
foreach ($p in $pasos) {
    $t = Get-Date
    "--- paso $p"
    switch ($p) {
        'arbol' { Paso-Arbol }
        'preparar' { Paso-Preparar }
        'configurar' { Paso-Configurar }
        'generar' { Paso-Generar }
        'compilar' { Paso-Compilar }
        'overlays' { Paso-Overlays }
        'armar' { Paso-Armar }
    }
    "--- paso ${p}: listo en $([int]((Get-Date) - $t).TotalSeconds) s"
}
