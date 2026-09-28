param(
    [Parameter(Mandatory)][string]$SdkRoot
)

$ErrorActionPreference = "Stop"
$sdk = (Resolve-Path -LiteralPath $SdkRoot).Path
$project = Split-Path -Parent $MyInvocation.MyCommand.Path
$cli = Join-Path $sdk "build\bin\Release\prospero.exe"
$linker = Join-Path $sdk "build\bin\Release\prospero-link.exe"
$output = Join-Path $project "build\app"
$object = Join-Path $output "main.o"
$elf = Join-Path $output "application.elf"
$eboot = Join-Path $output "eboot.bin"
$temporaryElf = Join-Path $output "application.elf.tmp"
$temporaryEboot = Join-Path $output "eboot.bin.tmp"

if (-not (Test-Path -LiteralPath $cli -PathType Leaf) -or
    -not (Test-Path -LiteralPath $linker -PathType Leaf)) {
    throw "Build the OpenProspero SDK CLI and linker at $sdk first."
}

. (Join-Path $sdk "tools\env.ps1")

& $cli build $project --profile application --release
if ($LASTEXITCODE -ne 0) { throw "OpenProspero application build failed ($LASTEXITCODE)." }
if (-not (Test-Path -LiteralPath $object -PathType Leaf)) {
    throw "OpenProspero application object missing: $object"
}

try {
    & $linker --profile application --sdk-root (Join-Path $sdk "sdk") --no-companion -- -o $temporaryElf $object
    if ($LASTEXITCODE -ne 0) { throw "Companion-free application link failed ($LASTEXITCODE)." }
    Copy-Item -LiteralPath $temporaryElf -Destination $temporaryEboot
    Move-Item -LiteralPath $temporaryElf -Destination $elf -Force
    Move-Item -LiteralPath $temporaryEboot -Destination $eboot -Force
} finally {
    if (Test-Path -LiteralPath $temporaryElf) { Remove-Item -LiteralPath $temporaryElf }
    if (Test-Path -LiteralPath $temporaryEboot) { Remove-Item -LiteralPath $temporaryEboot }
}

Write-Host "Companion-free application ELF: $elf"
Write-Host "Package eboot: $eboot"
