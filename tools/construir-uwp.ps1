<#
  Etapa 4 del camino a la Xbox: el port UWP de Yakumo v0.6.0-alpha.4 para la consola en modo
  desarrollador. Parte del árbol de la etapa 3, $($env:MHP3RD_ARBOL), y su código generado; los cambios del port van
  bajo MHP3RD_UWP, así que la compilación de escritorio de out\mhp3rd no cambia. La música y las películas
  salen del FFmpeg para UWP que compila ffmpeg_uwp.ps1 (-FFmpeg); con -SinFFmpeg, el paquete va sin ellas,
  como las versiones 0.1.0.0 a 0.1.0.6.

  Pasos, en este orden (-Paso uno,otro o todo):
    parchar     parchar_uwp.py: los cambios del port en $($env:MHP3RD_ARBOL) (se puede repetir)
    puente      el vulkan-1.dll puente (puente_vulkan\) en $($env:MHP3RD_ARBOL)\out\puente
    configurar  CMake para WindowsStore en $($env:MHP3RD_ARBOL)\out\uwp: Release, MHP3RD_RELEASE, MHP3RD_UWP, sin
                FFmpeg y con el SDL3 para UWP del espejo
    compilar    Yakumo.exe para UWP; comprueba el bit APPCONTAINER y el CRT _app
    overlays    las 355 overlays para UWP (build_overlays.sh sobre out\uwp)
    empaquetar  arma $($env:MHP3RD_PKG)\contenido y deja el .msix firmado en xbox\build\
  Cada paso deja su registro en $($env:MHP3RD_ARBOL)\_registros\uwp-<paso>.log. Requisitos: los de la etapa 3
  (técnico §13.3) y el certificado CN=Decompilaciones en Cert:\CurrentUser\My.
#>
param(
    [string[]] $Paso = @('todo'),
    [string] $Arbol = "$($env:MHP3RD_ARBOL)",
    [string] $Trabajo = "$($env:MHP3RD_PKG)",
    [int] $UnidadesALaVez = 12,
    [int] $Hilos = 16,
    # Paquete de humo: sin las overlays, que el juego corre entonces en su intérprete (más lento).
    # Sirve para probar el arranque en la consola mientras se compilan.
    [switch] $SinOverlays,
    # El FFmpeg para UWP (bin, lib, include) que deja ffmpeg_uwp.ps1.
    [string] $FFmpeg = "$($env:MHP3RD_FFMPEG)\install\ffmpeg",
    # Sin FFmpeg: el juego queda sin música y se salta las películas.
    [switch] $SinFFmpeg,
    # La definición del paquete (AppxManifest.xml y Assets\) y dónde deja el .msix. Por defecto, los de este
    # subproyecto; la versión en español pasa los suyos y arma su paquete con los mismos ejecutables.
    [string] $Paquete = (Join-Path $PSScriptRoot 'package'),
    [string] $Salida = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'xbox\build')
)
$ErrorActionPreference = 'Stop'

$aqui = $PSScriptRoot
$sub = Split-Path -Parent (Split-Path -Parent $aqui)
$espejo = Join-Path $sub 'xbox\espejo'
$vulkan = 'C:\Program Files\VulkanSDK\1.4.357.0'
$sdl = Join-Path $espejo 'sdl3-uwp-3.4.8'
$py = "$($env:MHP3RD_PY)\Scripts"
$bash = 'C:\Program Files\Git\bin\bash.exe'
$sdkBin = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64'
$huella = $env:MHP3RD_CERT   # CN=Decompilaciones
$build = Join-Path $Arbol 'out\uwp'
$puente = Join-Path $Arbol 'out\puente'
$regs = Join-Path $Arbol '_registros'

function ConvertTo-Posix([string] $ruta) {
    $r = $ruta -replace '\\', '/'
    if ($r -match '^([A-Za-z]):(.*)$') { return '/' + $Matches[1].ToLower() + $Matches[2] }
    return $r
}

function Import-Entorno {
    $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
    $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
    $lineas = & cmd /c "`"$vcvars`" x64 store >nul 2>&1 && set"
    foreach ($l in $lineas) {
        if ($l -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
    foreach ($r in $vulkan, $sdl, $py, $bash, $sdkBin) {
        if (-not (Test-Path -LiteralPath $r)) { throw "falta un requisito: $r (técnico §13.3)" }
    }
    if (-not $SinFFmpeg -and -not (Test-Path -LiteralPath (Join-Path $FFmpeg 'include\libavcodec\avcodec.h'))) {
        throw "falta el FFmpeg para UWP en ${FFmpeg}: pwsh -File ffmpeg_uwp.ps1 (o -SinFFmpeg, sin música ni películas)"
    }
    $env:VULKAN_SDK = $vulkan
    $env:PATH = "$py;$vulkan\Bin;$env:PATH"
}

function Invoke-Registrado([string] $log, [scriptblock] $bloque) {
    & $bloque 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath $log -Append
    if ($LASTEXITCODE -ne 0) { throw "el paso falló con código $LASTEXITCODE (registro: $log)" }
}

function Iniciar-Registro([string] $nombre) {
    New-Item -ItemType Directory -Force -Path $regs | Out-Null
    $log = Join-Path $regs "uwp-$nombre.log"
    "== uwp-$nombre · $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" | Out-File -LiteralPath $log -Append -Encoding utf8
    return $log
}

# El exe o la DLL tiene que traer el bit APPCONTAINER, y el exe el CRT _app, o la consola no lo activa.
function Test-Uwp([string] $ruta, [switch] $Crt) {
    $b = [System.IO.File]::ReadAllBytes($ruta)
    $pe = [BitConverter]::ToInt32($b, 0x3C)
    $carac = [BitConverter]::ToUInt16($b, $pe + 24 + 70)
    if (-not ($carac -band 0x1000)) { throw "a $ruta le falta el bit APPCONTAINER" }
    if ($Crt -and -not [System.Text.Encoding]::ASCII.GetString($b).ToLowerInvariant().Contains('vcruntime140_app.dll')) {
        throw "$ruta no importa el CRT _app: ¿se compiló sin «store»?"
    }
}

function Paso-Parchar {
    $log = Iniciar-Registro 'parchar'
    Invoke-Registrado $log { & "$py\python.exe" -X utf8 (Join-Path $aqui 'parchar_uwp.py') $Arbol }
}

function Paso-Puente {
    $log = Iniciar-Registro 'puente'
    Invoke-Registrado $log {
        cmake -S (Join-Path $aqui 'puente_vulkan') -B $puente -G Ninja -DCMAKE_SYSTEM_NAME=WindowsStore `
            "-DCMAKE_SYSTEM_VERSION=10.0" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL `
            "-DPython3_EXECUTABLE=$py\python.exe"
    }
    Invoke-Registrado $log { cmake --build $puente }
    Test-Uwp (Join-Path $puente 'vulkan-1.dll') -Crt
    "vulkan-1.dll verificado: APPCONTAINER y CRT _app" | Tee-Object -FilePath $log -Append
}

function Paso-Configurar {
    $log = Iniciar-Registro 'configurar'
    $ff = if ($SinFFmpeg) { @('-DMHP3RD_FFMPEG=OFF') } else {
        @('-DMHP3RD_FFMPEG=bundled', "-DMHP3RD_FFMPEG_UWP_PREFIX=$($FFmpeg -replace '\\', '/')")
    }
    Invoke-Registrado $log {
        cmake -S $Arbol -B $build -G Ninja -DCMAKE_SYSTEM_NAME=WindowsStore "-DCMAKE_SYSTEM_VERSION=10.0" `
            -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DPSPRECOMP_PROFILE=mhp3rd `
            -DMHP3RD_RELEASE=ON -DMHP3RD_UWP=ON @ff "-DSDL3_DIR=$($sdl -replace '\\', '/')" `
            "-DPSPRECOMP_GENERATED_JOBS=$UnidadesALaVez"
    }.GetNewClosure()
    $texto = Get-Content -LiteralPath $log -Raw
    if ($texto -notmatch 'mhp3rd: Vulkan renderer enabled') { throw 'la configuración no activó el renderer Vulkan' }
    if ($texto -notmatch 'mhp3rd: (\d+) generated AOT units') { throw 'faltan las unidades generadas de la etapa 3' }
    if (-not $SinFFmpeg -and $texto -notmatch 'built for UWP; music and movies enabled') {
        throw 'la configuración no tomó el FFmpeg para UWP (¿falta el parche de FFmpeg.cmake? paso parchar)'
    }
}

function Paso-Compilar {
    $log = Iniciar-Registro 'compilar'
    Invoke-Registrado $log { cmake --build $build --target Yakumo -j $Hilos }
    Test-Uwp (Join-Path $build 'bin\Yakumo.exe') -Crt
    "Yakumo.exe verificado: APPCONTAINER y CRT _app" | Tee-Object -FilePath $log -Append
}

function Paso-Overlays {
    $log = Iniciar-Registro 'overlays'
    $linea = "cd '$(ConvertTo-Posix $Arbol)' && profiles/mhp3rd/scripts/build_overlays.sh '$(ConvertTo-Posix $build)' $Hilos"
    Invoke-Registrado $log { & $bash -c $linea }.GetNewClosure()
}

function Paso-Empaquetar {
    $log = Iniciar-Registro 'empaquetar'
    $bin = Join-Path $build 'bin'
    $contenido = Join-Path $Trabajo 'contenido'
    New-Item -ItemType Directory -Force -Path (Join-Path $contenido 'overlays'), (Join-Path $contenido 'fonts') | Out-Null
    Copy-Item -LiteralPath (Join-Path $bin 'Yakumo.exe') -Destination $contenido -Force
    if ($SinOverlays) {
        $hay = @(Get-ChildItem -LiteralPath (Join-Path $contenido 'overlays') -Filter '*.dll' -ErrorAction SilentlyContinue).Count
        if ($hay) { throw "el contenido ya tiene $hay overlays: el paquete de humo se arma en una carpeta de trabajo nueva" }
        'paquete de humo: sin overlays' | Tee-Object -FilePath $log -Append
    } else {
        $ovl = @(Get-ChildItem -LiteralPath (Join-Path $bin 'overlays') -Filter '*.dll')
        if ($ovl.Count -ne 355) { throw "se esperaban 355 overlays y hay $($ovl.Count): falta el paso overlays" }
        foreach ($o in $ovl) { Copy-Item -LiteralPath $o.FullName -Destination (Join-Path $contenido 'overlays') -Force }
    }
    Copy-Item -LiteralPath (Join-Path $sdl 'x64\bin\SDL3.dll') -Destination $contenido -Force
    Copy-Item -LiteralPath (Join-Path $puente 'vulkan-1.dll') -Destination $contenido -Force
    $ffDlls = 'avcodec-61.dll', 'avutil-59.dll'
    if ($SinFFmpeg) {
        foreach ($f in $ffDlls) {
            if (Test-Path -LiteralPath (Join-Path $contenido $f)) { throw "el contenido ya trae ${f}: el paquete sin FFmpeg se arma en una carpeta de trabajo nueva" }
        }
    } else {
        # FFmpeg (LGPL): sus dos DLL, su licencia y la nota de dónde sale, junto al exe.
        foreach ($f in $ffDlls + 'FFmpeg-LICENSE.txt', 'FFmpeg-SOURCE.txt') {
            Copy-Item -LiteralPath (Join-Path $bin $f) -Destination $contenido -Force
        }
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $z = [System.IO.Compression.ZipFile]::OpenRead((Join-Path $espejo 'mesa-uwp-26.2.2.zip'))
    try {
        $e = $z.Entries | Where-Object { $_.FullName -match '(^|/)bin/vulkan_dzn\.dll$' } | Select-Object -First 1
        if (-not $e) { throw 'el zip de mesa-uwp no trae bin/vulkan_dzn.dll' }
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, (Join-Path $contenido 'vulkan_dzn.dll'), $true)
    } finally { $z.Dispose() }
    Copy-Item -LiteralPath (Join-Path $espejo 'dxil.dll') -Destination (Join-Path $contenido 'DXIL.dll') -Force
    Copy-Item -LiteralPath (Join-Path $espejo 'fuentes\NotoSansCJKjp-Regular.otf') -Destination (Join-Path $contenido 'fonts') -Force
    Copy-Item -LiteralPath (Join-Path $Paquete 'AppxManifest.xml') -Destination $contenido -Force
    Copy-Item -LiteralPath (Join-Path $Paquete 'Assets') -Destination $contenido -Recurse -Force
    $comprobar = @('Yakumo.exe', 'SDL3.dll', 'vulkan-1.dll') + $(if ($SinFFmpeg) { @() } else { $ffDlls })
    foreach ($f in $comprobar) { Test-Uwp (Join-Path $contenido $f) }

    $pri = Join-Path $Trabajo 'priconfig.xml'
    & "$sdkBin\makepri.exe" createconfig /cf $pri /dq en-US /o | Out-Null
    & "$sdkBin\makepri.exe" new /pr $contenido /cf $pri /of (Join-Path $contenido 'resources.pri') /o | Out-Null
    if ($LASTEXITCODE) { throw 'makepri falló' }
    [xml]$man = Get-Content -LiteralPath (Join-Path $Paquete 'AppxManifest.xml')
    $ver = $man.Package.Identity.Version
    New-Item -ItemType Directory -Force -Path $Salida | Out-Null
    $msix = Join-Path $Salida "$($man.Package.Identity.Name)_${ver}_x64.msix"
    & "$sdkBin\makeappx.exe" pack /o /d $contenido /p $msix | Out-Null
    if ($LASTEXITCODE) { throw 'makeappx falló' }
    & "$sdkBin\signtool.exe" sign /fd SHA256 /sha1 $huella /s My $msix | Out-Null
    if ($LASTEXITCODE) { throw 'signtool falló' }
    "LISTO: $msix ($((Get-Item -LiteralPath $msix).Length) bytes)" | Tee-Object -FilePath $log -Append
}

$orden = 'parchar', 'puente', 'configurar', 'compilar', 'overlays', 'empaquetar'
$Paso = @($Paso -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ })
foreach ($p in $Paso) { if ($p -ne 'todo' -and $orden -notcontains $p) { throw "paso desconocido: $p (valen: $($orden -join ', '), todo)" } }
Import-Entorno
$pasos = if ($Paso -contains 'todo') { $orden } else { $orden | Where-Object { $Paso -contains $_ } }
foreach ($p in $pasos) {
    $t = Get-Date
    "--- paso $p"
    switch ($p) {
        'parchar' { Paso-Parchar }
        'puente' { Paso-Puente }
        'configurar' { Paso-Configurar }
        'compilar' { Paso-Compilar }
        'overlays' { Paso-Overlays }
        'empaquetar' { Paso-Empaquetar }
    }
    "--- paso ${p}: listo en $([int]((Get-Date) - $t).TotalSeconds) s"
}
