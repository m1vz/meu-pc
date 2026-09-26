# Le a Serial do ESP32 por alguns segundos (opcionalmente envia um comando).
param(
  [string]$Port = "COM3",
  [int]$Seconds = 6,
  [string]$Send = ""
)
$sp = New-Object System.IO.Ports.SerialPort $Port, 115200
$sp.DtrEnable = $false
$sp.RtsEnable = $false
$sp.NewLine = "`n"
$sp.Open()
try {
  if ($Send) { Start-Sleep -Milliseconds 300; $sp.WriteLine($Send) }
  $end = (Get-Date).AddSeconds($Seconds)
  while ((Get-Date) -lt $end) {
    $data = $sp.ReadExisting()
    if ($data) { Write-Host -NoNewline $data }
    Start-Sleep -Milliseconds 100
  }
} finally { $sp.Close() }
