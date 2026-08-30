$ErrorActionPreference = 'Stop'

$Root = $PSScriptRoot
$BuildDir = Join-Path $Root 'build'
$Cmake = 'C:\Users\Michael Strange\AppData\Local\Python\pythoncore-3.14-64\Scripts\cmake.exe'
$Ninja = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$ArmBin = 'C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin'
$Sdk = 'C:\repos\Pico-Examples\pico-sdk'

$required = @(
    $Cmake,
    $Ninja,
    (Join-Path $ArmBin 'arm-none-eabi-gcc.exe'),
    (Join-Path $ArmBin 'arm-none-eabi-g++.exe'),
    (Join-Path $Sdk 'pico_sdk_init.cmake')
)
foreach ($path in $required) {
    if (-not (Test-Path $path)) { throw "Required build tool not found: $path" }
}

& $Cmake -S $Root -B $BuildDir -G Ninja `
    -DPICO_BOARD=pico `
    -DPICO_SDK_PATH="$Sdk" `
    -DCMAKE_MAKE_PROGRAM="$Ninja" `
    -DCMAKE_C_COMPILER="$(Join-Path $ArmBin 'arm-none-eabi-gcc.exe')" `
    -DCMAKE_CXX_COMPILER="$(Join-Path $ArmBin 'arm-none-eabi-g++.exe')" `
    -DCMAKE_ASM_COMPILER="$(Join-Path $ArmBin 'arm-none-eabi-gcc.exe')"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

& $Cmake --build $BuildDir -j 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

$Uf2 = Join-Path $BuildDir 'squib_box_dmx_2.uf2'
if (-not (Test-Path $Uf2)) { throw "UF2 was not produced: $Uf2" }
Write-Host "UF2_READY: $Uf2"
