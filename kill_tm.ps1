$o = @()
@(Get-Process PulseBar -EA SilentlyContinue) | ForEach-Object {
  $o += ("killing " + $_.Id)
  Stop-Process -Id $_.Id -Force -EA SilentlyContinue
}
Start-Sleep -Seconds 3
$o += ("remaining: " + @(Get-Process PulseBar -EA SilentlyContinue).Count)
$o | Out-File -Encoding utf8 'C:\Windows\Temp\killtm.txt'
