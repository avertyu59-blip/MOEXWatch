$ErrorActionPreference = 'Stop'

cmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot 'build') -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
cmake --build (Join-Path $PSScriptRoot 'build') --config Release
if ($LASTEXITCODE -ne 0) { throw 'C++ build failed.' }
ctest --test-dir (Join-Path $PSScriptRoot 'build') -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }

Copy-Item (Join-Path $PSScriptRoot 'build\Release\moexwatch.exe') (Join-Path $PSScriptRoot 'MOEXWatch.exe') -Force
Write-Host "MOEX Watch 0.6.2 built: $(Join-Path $PSScriptRoot 'MOEXWatch.exe')"
