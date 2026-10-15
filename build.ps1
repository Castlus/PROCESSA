# Compila o PROCESSA no Windows usando um container Ubuntu (Docker Desktop).
#   .\build.ps1                     gera build\processa.img
#   .\build.ps1 test                compila e roda o QEMU sem janela por 20 s (log serial)
#   .\build.ps1 clean               apaga build\
#   .\build.ps1 -Arquivos "extras\prog.elf extras\outro.bin"
#                                   coloca arquivos extras no disco (precisam estar
#                                   dentro da pasta do projeto; rode com "run <nome>")
param([string]$Target = "all", [string]$Arquivos = "")

docker build -q -t processa-build $PSScriptRoot | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Error "falha ao criar a imagem Docker (o Docker Desktop esta aberto?)"; exit 1 }

$Arquivos = $Arquivos -replace '\\', '/'
docker run --rm -v "${PSScriptRoot}:/src" -w /src processa-build make $Target "ARQUIVOS=$Arquivos"
exit $LASTEXITCODE
