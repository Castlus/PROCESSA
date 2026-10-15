# Copia o PROCESSA para um pendrive em FAT32, para dar boot por UEFI.
#   .\pendrive.ps1 E:
#
# So copia arquivos (EFI\BOOT\BOOTX64.EFI e a pasta PROCESSA). Nao formata
# e nao apaga nada; por seguranca, so aceita unidade removivel em FAT32.
# Programas extras: coloque-os em E:\PROCESSA\PROGRAMAS (rode com "run").
param([Parameter(Mandatory = $true)][string]$Drive)

$letter = $Drive.TrimEnd('\').TrimEnd(':')
$vol = Get-Volume -DriveLetter $letter -ErrorAction SilentlyContinue
if (-not $vol) {
    Write-Error "unidade ${letter}: nao encontrada"; exit 1
}
if ($vol.DriveType -ne 'Removable') {
    Write-Error "${letter}: nao e um pendrive (unidade removivel). Nada foi copiado."; exit 1
}
if ($vol.FileSystem -notmatch '^FAT') {
    Write-Error "${letter}: esta em $($vol.FileSystem); o firmware UEFI le FAT32. Formate o pendrive em FAT32."; exit 1
}

& (Join-Path $PSScriptRoot "build.ps1")
if ($LASTEXITCODE -ne 0) { Write-Error "falha no build"; exit 1 }

$src = Join-Path $PSScriptRoot "build\usb"
Copy-Item -Path (Join-Path $src "*") -Destination "${letter}:\" -Recurse -Force

Write-Host ""
Write-Host "PROCESSA copiado para ${letter}: ($($vol.FileSystemLabel), $($vol.FileSystem))"
Get-ChildItem -Recurse -File "${letter}:\EFI\BOOT", "${letter}:\PROCESSA" |
    ForEach-Object { "  " + $_.FullName.Substring(3) }
Write-Host ""
Write-Host "No notebook: desligue o Secure Boot e escolha o pendrive no menu de boot."
