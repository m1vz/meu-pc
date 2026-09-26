# Testa o broker MQTT pelo PC: escuta meu-pc/# e (opcionalmente) envia um comando ao ESP32.
# Host vem do secrets.h; use um usuario MQTT proprio do PC/app (nao o do ESP32).
# Ex.: powershell -ExecutionPolicy Bypass -File .\mqtt-test.ps1 -User app -Password xxx -Send LIGAR
param(
  [Parameter(Mandatory)][string]$User,
  [Parameter(Mandatory)][string]$Password,
  [string]$Send = "",
  [int]$Seconds = 10
)
$ErrorActionPreference = "Stop"
$secrets = Get-Content "$PSScriptRoot\esp32-pc-remote\secrets.h" -Raw
$mqttHost = [regex]::Match($secrets, '#define\s+MQTT_HOST\s+"([^"]+)"').Groups[1].Value
$conn = @("-h", $mqttHost, "-p", "8883", "-l", "mqtts", "-u", $User, "-P", $Password)

# Escuta em segundo plano (mostra o status retido "online"/"offline" e as respostas)
$sub = Start-Job -ScriptBlock { param($c) & npx --yes mqtt@5 sub -t "meu-pc/#" -v @c 2>&1 } -ArgumentList (,$conn)
Start-Sleep -Seconds 4
if ($Send) {
  # Sem retain: um LIGAR retido seria reexecutado a cada reconexão do ESP32
  & npx --yes mqtt@5 pub -t "meu-pc/comando" -m $Send -q 1 @conn
  Write-Host "Enviado: $Send"
}
Start-Sleep -Seconds $Seconds
Stop-Job $sub
Receive-Job $sub
Remove-Job $sub
