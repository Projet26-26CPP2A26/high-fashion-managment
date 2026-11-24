param([switch]$Verifier)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$build = Join-Path $root 'build\Desktop_Qt_6_7_3_MinGW_64_bit_Debug'
$exe = Join-Path $build 'debug\AureaEmployes.exe'
$arduino = Join-Path $root 'Arduino'
$sketch = Join-Path $arduino 'pointage_badge'
$hex = Join-Path $arduino 'build\pointage_badge.ino.hex'
$cli = Join-Path $env:USERPROFILE 'arduino-cli\arduino-cli.exe'
$warnings = 0

function Show-Warning($text) {
    $script:warnings++
    Write-Host ('     Attention : ' + $text) -ForegroundColor Yellow
}

function Get-OpenApp {
    Get-Process AureaEmployes -ErrorAction SilentlyContinue | Where-Object { $_.Threads.Count -gt 0 -and $_.Path -eq $exe }
}

try {
    Write-Host '1/4  Oracle'
    $stopped = @('OracleXETNSListener', 'OracleServiceXE' | Where-Object { (Get-Service $_).Status -ne 'Running' })
    if ($stopped.Count) {
        $list = $stopped -join ','
        Start-Process powershell -Verb RunAs -Wait -WindowStyle Hidden -ArgumentList "-NoProfile -Command Start-Service $list"
        foreach ($s in $stopped) { (Get-Service $s).WaitForStatus('Running', '00:01:30') }
        Start-Sleep 5
    }

    Write-Host '2/4  Arduino'
    try {
        $ino = Join-Path $sketch 'pointage_badge.ino'
        if (-not (Test-Path $hex) -or (Get-Item $ino).LastWriteTime -gt (Get-Item $hex).LastWriteTime) {
            if (-not (Test-Path $cli)) { throw 'arduino-cli est introuvable.' }
            & $cli compile -b arduino:avr:uno --output-dir (Split-Path $hex) $sketch
            if ($LASTEXITCODE) { throw 'la compilation Arduino a echoue.' }
            (Get-Item $hex).LastWriteTime = Get-Date
        }
    } catch { Show-Warning $_.Exception.Message }

    Write-Host '3/4  Wokwi'
    try {
        $code = (Get-Command code.cmd -ErrorAction SilentlyContinue | Select-Object -First 1).Source
        if (-not $code) { $code = Join-Path $env:LOCALAPPDATA 'Programs\Microsoft VS Code\bin\code.cmd' }
        if (-not (Test-Path $code)) { throw 'VS Code est introuvable, le badge virtuel reste disponible.' }
        if (-not $Verifier) { & $code $arduino }
    } catch { Show-Warning $_.Exception.Message }

    Write-Host '4/4  AUREA'
    if (-not (Test-Path $exe)) { throw 'Compilez le projet une fois dans Qt Creator.' }
    $settings = Join-Path $root '.qtcreator\AureaEmployes.pro.user'
    if (Test-Path $settings) {
        $user = Get-Content $settings -Raw -Encoding UTF8
        foreach ($m in [regex]::Matches($user, '>(AUREA_[A-Z_]+)=([^<]*)<')) {
            [Environment]::SetEnvironmentVariable($m.Groups[1].Value, [Net.WebUtility]::HtmlDecode($m.Groups[2].Value), 'Process')
        }
    }
    if (-not $env:AUREA_ORACLE_PASSWORD) { Show-Warning 'le mot de passe Oracle est absent de Qt Creator.' }
    $env:AUREA_WOKWI = '1'
    $env:PATH = 'C:\Qt\6.7.3\mingw_64\bin;C:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
    if (@(Get-OpenApp).Count) {
        Write-Host '     AUREA est deja ouvert.'
    } elseif (-not $Verifier) {
        Start-Sleep 4
        $app = Start-Process $exe -WorkingDirectory $build -PassThru
        $null = $app.Handle
        Start-Sleep 3
        if ($app.HasExited -and $app.ExitCode -ne 0) { throw ('AUREA a quitte au demarrage (code ' + $app.ExitCode + ').') }
    }
    Write-Host 'Termine.' -ForegroundColor Green
    if ($warnings) { Start-Sleep 5 } else { Start-Sleep 1 }
} catch {
    Write-Host ('Erreur : ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
