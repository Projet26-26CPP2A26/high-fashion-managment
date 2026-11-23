$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$build = Join-Path $root 'build\Desktop_Qt_6_7_3_MinGW_64_bit_Debug'
$exe = Join-Path $build 'debug\AureaEmployes.exe'
$sketch = Join-Path $root 'arduino\pointage_badge'
$hex = Join-Path $root 'arduino\build\pointage_badge.ino.hex'
$cli = Join-Path $env:USERPROFILE 'arduino-cli\arduino-cli.exe'

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
    $ino = Join-Path $sketch 'pointage_badge.ino'
    if (-not (Test-Path $hex) -or (Get-Item $ino).LastWriteTime -gt (Get-Item $hex).LastWriteTime) {
        & $cli compile -b arduino:avr:uno --output-dir (Split-Path $hex) $sketch
        if ($LASTEXITCODE) { throw 'La compilation Arduino a echoue.' }
    }

    Write-Host '3/4  Wokwi'
    $code = (Get-Command code.cmd -ErrorAction SilentlyContinue).Source
    if (-not $code) { $code = Join-Path $env:LOCALAPPDATA 'Programs\Microsoft VS Code\bin\code.cmd' }
    & $code (Join-Path $root 'arduino')

    Write-Host '4/4  AUREA'
    if (-not (Test-Path $exe)) { throw 'Compilez le projet une fois dans Qt Creator.' }
    $user = Get-Content (Join-Path $root '.qtcreator\AureaEmployes.pro.user') -Raw -Encoding UTF8
    foreach ($m in [regex]::Matches($user, '>(AUREA_[A-Z_]+)=([^<]*)<')) {
        [Environment]::SetEnvironmentVariable($m.Groups[1].Value, [Net.WebUtility]::HtmlDecode($m.Groups[2].Value), 'Process')
    }
    $env:AUREA_WOKWI = '1'
    $env:PATH = 'C:\Qt\6.7.3\mingw_64\bin;C:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
    if (-not (Get-Process AureaEmployes -ErrorAction SilentlyContinue)) {
        Start-Sleep 4
        Start-Process $exe -WorkingDirectory $build
    }
    Write-Host 'Termine.'
    Start-Sleep 2
} catch {
    Write-Host ('Erreur : ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
