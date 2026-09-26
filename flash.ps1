# Compila o sketch e grava no ESP32.
# Antes de rodar: coloque a placa em modo download (segure BOOT, aperte/solte EN, solte BOOT).
param(
  [string]$Port = "COM3",
  [switch]$SkipCompile
)
$ErrorActionPreference = "Stop"
$root    = $PSScriptRoot
$cli     = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$cfg     = "$env:USERPROFILE\.arduinoIDE\arduino-cli.yaml"
$esptool = "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe"
$sketch  = "$root\esp32-pc-remote"
$build   = "$root\build"

if (-not $SkipCompile) {
  & $cli --config-file $cfg compile --fqbn esp32:esp32:esp32 --output-dir $build $sketch
  if ($LASTEXITCODE -ne 0) { throw "Falha na compilacao" }
}

# --before no-reset: não mexe em DTR/RTS (o reset automático tira a placa do modo download nesta placa CH340)
& $esptool --chip esp32 --port $Port --baud 115200 --before no-reset --after hard-reset `
  write-flash -z 0x0 "$build\esp32-pc-remote.ino.merged.bin"
if ($LASTEXITCODE -ne 0) { throw "Falha no upload" }
Write-Host "Upload OK. Se o sketch nao iniciar, aperte EN na placa."
