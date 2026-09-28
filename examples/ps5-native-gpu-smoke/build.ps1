param(
    [Parameter(Mandatory)][string]$SdkRoot,
    [Parameter(Mandatory)][string]$LogHost
)

$ErrorActionPreference = "Stop"
$sdk = (Resolve-Path -LiteralPath $SdkRoot).Path
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path
$output = Join-Path $PSScriptRoot "build\app"
$linker = Join-Path $sdk "build\bin\Release\prospero-link.exe"
$appLoader = Join-Path $sdk "sdk\target\lib\app_agc_dlfcn.o"
$udpLog = Join-Path $sdk "sdk\target\lib\app_udp_log.o"
$elf = Join-Path $output "application.elf"
$eboot = Join-Path $output "eboot.bin"
$temporaryElf = "$elf.tmp"
$temporaryEboot = "$eboot.tmp"
$address = [System.Net.IPAddress]::None
if (-not [System.Net.IPAddress]::TryParse($LogHost, [ref]$address) -or
    $address.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork) {
    throw "LogHost must be an IPv4 address reachable from the console."
}
$hostHex = -join ($address.GetAddressBytes() | ForEach-Object { "{0:X2}" -f $_ })

if (-not (Test-Path -LiteralPath $linker -PathType Leaf)) {
    throw "Build the OpenProspero SDK linker at $sdk first."
}
. (Join-Path $sdk "tools\env.ps1")
& (Join-Path $sdk "tools\build-crt.ps1")
if (-not $?) { throw "OpenProspero CRT build failed." }
if (-not (Test-Path -LiteralPath $appLoader -PathType Leaf)) {
    throw "Opt-in application AGC loader missing: $appLoader"
}
if (-not (Test-Path -LiteralPath $udpLog -PathType Leaf)) {
    throw "Opt-in OpenProspero UDP logger missing: $udpLog"
}

New-Item -ItemType Directory -Force -Path $output | Out-Null
$clang = Resolve-OpenProsperoTool -Name "clang" -OverrideEnvironmentVariables @("OPENPROSPERO_CLANG")
$resource = (& $clang -print-resource-dir).Trim()
$flags = @(
    "--target=x86_64-sie-ps5", "-O2", "-DNDEBUG", "-std=c99",
    "-ffreestanding", "-fPIC", "-fPIE", "-fno-builtin",
    "-fstack-protector-strong", "-femulated-tls",
    "-ffunction-sections", "-fdata-sections", "-fvisibility=hidden",
    "-nostdinc", "-isystem", (Join-Path $resource "include"),
    "-isystem", (Join-Path $sdk "sdk\target\include"),
    "-I", (Join-Path $repo "include"),
    "-I", (Join-Path $repo "tools\payload"),
    "-Wall", "-Wextra", "-Werror"
)

function Compile-Source([string]$Source, [string]$Name, [string[]]$Defines) {
    $object = Join-Path $output "$Name.o"
    & $clang @flags @Defines -c $Source -o $object
    if ($LASTEXITCODE -ne 0) { throw "PS5 compilation failed: $Source" }
    return $object
}

$probe = Compile-Source (Join-Path $repo "tools\payload\draw_raster_agc_eop.c") "probe" @(
    "-DOPENAGC_AGC_SUBMIT=1", "-DOPENAGC_AGC_COMPLETION=1",
    "-DOPENAGC_GPU_BRIDGE=1", "-DOPENAGC_APP_BRIDGE=1",
    "-DOPENAGC_LOG_HOST_IP=0x${hostHex}u", "-Wno-unused-function"
)
$gpu = Compile-Source (Join-Path $repo "src\openagc_ps5_gpu.c") "gpu" @()
$policy = Compile-Source (Join-Path $repo "src\openagc_ps5_policy.c") "policy" @()

try {
    & $linker --profile application --no-companion --sdk-root (Join-Path $sdk "sdk") -- `
        -o $temporaryElf $probe $gpu $policy $appLoader $udpLog `
        --wrap=dlopen --wrap=dlsym --wrap=dlclose --wrap=dlerror
    if ($LASTEXITCODE -ne 0) { throw "GPU application link failed ($LASTEXITCODE)." }
    Copy-Item -LiteralPath $temporaryElf -Destination $temporaryEboot
    Move-Item -LiteralPath $temporaryElf -Destination $elf -Force
    Move-Item -LiteralPath $temporaryEboot -Destination $eboot -Force
} finally {
    if (Test-Path -LiteralPath $temporaryElf) { Remove-Item -LiteralPath $temporaryElf }
    if (Test-Path -LiteralPath $temporaryEboot) { Remove-Item -LiteralPath $temporaryEboot }
}

Write-Host "Application GPU ELF: $elf"
Write-Host "Package eboot: $eboot"
