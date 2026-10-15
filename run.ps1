# Roda o PROCESSA no QEMU para Windows.
# Instale uma vez com:  winget install SoftwareFreedomConservancy.QEMU
$img = Join-Path $PSScriptRoot "build\processa.img"
if (-not (Test-Path $img)) { Write-Error "build\processa.img nao existe: rode .\build.ps1 antes"; exit 1 }

$qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe"
if (-not (Test-Path $qemu)) { $qemu = "qemu-system-x86_64" }

& $qemu -m 128M -drive "format=raw,file=$img,if=ide" -no-reboot -serial stdio
