param([Parameter(Mandatory=$true)][string]$QtRoot)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path (Join-Path $QtRoot 'lib/cmake/Qt6/Qt6Config.cmake'))) {
    throw 'Укажите каталог Qt 6.8.3 msvc2022_64, например C:\Qt\6.8.3\msvc2022_64'
}
$env:PATH = (Join-Path $QtRoot 'bin') + ';' + $env:PATH
cmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot 'build-qt') -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$QtRoot" -DMOEX_BUILD_QT=ON
if ($LASTEXITCODE -ne 0) { throw 'Configuration failed' }
cmake --build (Join-Path $PSScriptRoot 'build-qt') --config Release --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
ctest --test-dir (Join-Path $PSScriptRoot 'build-qt') -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
$preview = Join-Path $PSScriptRoot 'qt-preview'
. (Join-Path $PSScriptRoot '.github/scripts/qt-package.ps1')
Deploy-QtPackage -SourceDir $PSScriptRoot -BuildDir (Join-Path $PSScriptRoot 'build-qt') -PackageDir $preview
Test-QtPackage -PackageDir $preview -LogPrefix (Join-Path $PSScriptRoot 'qt-package-smoke')
Write-Host "Built: $preview\MOEXWatch-Qt.exe"
