# Lance tout : Oracle, programme Arduino, simulateur Wokwi, compilation Qt et AUREA.
# Run.bat            tout lancer
# Run.bat -SansWokwi sans le simulateur (vraie carte ou borne virtuelle)
param([switch]$SansWokwi)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$build = Join-Path $root 'build\Desktop_Qt_6_7_3_MinGW_64_bit_Debug'
$exe = Join-Path $build 'debug\AureaMatieres.exe'
$wokwi = Join-Path $root 'arduino\wokwi'
$ino = Join-Path $root 'arduino\borne_stock\borne_stock.ino'
$hex = Join-Path $wokwi 'firmware\borne_stock.ino.hex'
$qt = 'C:\Qt\6.7.3\mingw_64\bin'
$mingw = 'C:\Qt\Tools\mingw1120_64\bin'

function Test-Port([int]$port) {
    $c = New-Object Net.Sockets.TcpClient
    try { $c.Connect('127.0.0.1', $port); return $true } catch { return $false } finally { $c.Close() }
}
function Test-Listening([int]$port) {
    # Sans s'y connecter : le simulateur ne doit parler qu'a AUREA.
    return [bool](Get-NetTCPConnection -State Listen -LocalPort $port -ErrorAction SilentlyContinue)
}

try {
    Write-Host '1/4  Oracle' -ForegroundColor Yellow
    $services = @(Get-Service | Where-Object { $_.Name -match '^OracleService' -or $_.Name -match 'TNSListener$' })
    if (-not $services.Count) { throw 'Oracle est introuvable sur ce PC.' }
    $stopped = @($services | Where-Object Status -ne 'Running' | ForEach-Object Name)
    if ($stopped.Count) {
        Start-Process powershell -Verb RunAs -Wait -WindowStyle Hidden -ArgumentList "-NoProfile -Command Start-Service $($stopped -join ',')"
        foreach ($s in $stopped) { (Get-Service $s).WaitForStatus('Running', '00:01:30') }
    }
    for ($i = 0; -not (Test-Port 1521); $i++) {
        if ($i -ge 45) { throw 'Oracle ne repond pas sur le port 1521.' }
        Start-Sleep 1
    }
    Write-Host '     pret'

    Write-Host '2/4  Application Qt' -ForegroundColor Yellow
    $env:PATH = "$qt;$mingw;" + $env:PATH
    if (Get-Process AureaMatieres -ErrorAction SilentlyContinue) {
        Write-Host '     AUREA est deja ouverte (fermez-la pour recompiler)'
    } else {
        New-Item -ItemType Directory -Force $build | Out-Null
        Push-Location $build
        $log = Join-Path $build 'run.log'
        cmd /c "qmake.exe `"$root\AureaMatieres.pro`" -spec win32-g++ CONFIG+=debug > `"$log`" 2>&1 && mingw32-make -j4 >> `"$log`" 2>&1"
        $code = $LASTEXITCODE
        Pop-Location
        if ($code) { Get-Content $log -Tail 12; throw 'La compilation Qt a echoue.' }
        Write-Host '     compilee'
    }

    Write-Host '3/4  Arduino et Wokwi' -ForegroundColor Yellow
    if ($SansWokwi) {
        Write-Host '     ignore'
    } else {
        if (-not (Test-Path $hex) -or (Get-Item $ino).LastWriteTime -gt (Get-Item $hex).LastWriteTime) {
            $cli = Join-Path $env:LOCALAPPDATA 'Programs\arduino-ide\resources\app\lib\backend\resources\arduino-cli.exe'
            if (-not (Test-Path $cli)) { throw 'arduino-cli est introuvable (installez Arduino IDE).' }
            & $cli compile -b arduino:avr:uno --output-dir (Split-Path $hex) (Split-Path $ino) | Out-Null
            if ($LASTEXITCODE) { throw 'La compilation Arduino a echoue.' }
        }
        if (Test-Listening 47110) {
            Write-Host '     simulateur deja demarre'
        } else {
            $code = Join-Path $env:LOCALAPPDATA 'Programs\Microsoft VS Code\bin\code.cmd'
            if (-not (Test-Path $code)) { $code = (Get-Command code.cmd -ErrorAction SilentlyContinue).Source }
            if ($code) {
                & $code $wokwi
                for ($i = 0; $i -lt 25 -and -not (Test-Listening 47110); $i++) { Start-Sleep 1 }
            }
            if (Test-Listening 47110) { Write-Host '     simulateur demarre' }
            else { Write-Host '     dans VS Code : Ctrl+Shift+P puis "Wokwi: Start Simulator" (AUREA se connectera seule)' -ForegroundColor Cyan }
        }
    }

    Write-Host '4/4  AUREA' -ForegroundColor Yellow
    if (-not (Get-Process AureaMatieres -ErrorAction SilentlyContinue)) { Start-Process $exe -WorkingDirectory (Split-Path $exe) }
    Write-Host 'Termine.' -ForegroundColor Green
    Start-Sleep 2
} catch {
    Write-Host ('Erreur : ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
