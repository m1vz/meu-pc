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

$sub = Start-Job -ScriptBlock { param($c) & npx --yes mqtt@5 sub -t "meu-pc/#" -v @c 2>&1 } -ArgumentList (,$conn)
Start-Sleep -Seconds 4
if ($Send) {
  & npx --yes mqtt@5 pub -t "meu-pc/comando" -m $Send -q 1 @conn
  Write-Host "Enviado: $Send"
}
Start-Sleep -Seconds $Seconds
Stop-Job $sub
Receive-Job $sub
Remove-Job $sub
