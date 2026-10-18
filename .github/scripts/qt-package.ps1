$ErrorActionPreference = 'Stop'
function Deploy-QtPackage {
    param([string]$SourceDir,[string]$BuildDir,[string]$PackageDir)
    New-Item -ItemType Directory -Path $PackageDir -Force | Out-Null
    # Reused preview directories may contain a classic EXE from an older version.
    foreach ($legacy in @('MOEXWatch-Classic.exe','MOEXWatch.exe','moexwatch.exe')) {
        $legacyPath = Join-Path $PackageDir $legacy
        if (Test-Path $legacyPath) { Remove-Item $legacyPath -Force }
    }
    Copy-Item (Join-Path $BuildDir 'qt/Release/MOEXWatch-Qt.exe') $PackageDir
    $deployer = (Get-Command windeployqt).Source
    & $deployer --release --qmldir (Join-Path $SourceDir 'qt/qml') --compiler-runtime (Join-Path $PackageDir 'MOEXWatch-Qt.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed' }
    $qtRoot = Split-Path (Split-Path $deployer)
    Copy-Item "$qtRoot/plugins/platforms/qoffscreen.dll" (Join-Path $PackageDir 'platforms/')
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $crt = Get-Item "$vs/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT" | Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $crt) { throw 'MSVC runtime not found' }
    Copy-Item "$($crt.FullName)/*.dll" $PackageDir
    foreach ($required in @('MOEXWatch-Qt.exe','Qt6Core.dll','Qt6Quick.dll','Qt6Widgets.dll','msvcp140.dll','vcruntime140.dll','platforms/qwindows.dll','platforms/qoffscreen.dll')) {
        if (-not (Test-Path (Join-Path $PackageDir $required))) { throw "Missing runtime file: $required" }
    }
    Copy-Item (Join-Path $SourceDir 'README.md'),(Join-Path $SourceDir 'VERSION.txt') $PackageDir
    Copy-Item (Join-Path $SourceDir 'docs/QT_PREVIEW.md'),(Join-Path $SourceDir 'docs/QT_THIRD_PARTY.md'),(Join-Path $SourceDir 'docs/FUNDAMENTALS_FORMAT.md') $PackageDir
    New-Item -ItemType Directory -Path (Join-Path $PackageDir 'licenses') -Force | Out-Null
    foreach ($license in @('LGPL-3.0-only.txt','GPL-3.0-only.txt','Qt-GPL-exception-1.0.txt')) {
        Invoke-WebRequest "https://raw.githubusercontent.com/qt/qtbase/v6.8.3/LICENSES/$license" -OutFile (Join-Path $PackageDir "licenses/$license")
    }
}
function Test-QtPackage {
    param([string]$PackageDir,[string]$LogPrefix)
    foreach ($legacy in @('MOEXWatch-Classic.exe','MOEXWatch.exe','moexwatch.exe')) {
        if (Test-Path (Join-Path $PackageDir $legacy)) { throw "Unexpected classic executable: $legacy" }
    }
    $previousPath = $env:PATH
    $previousPlatform = $env:QT_QPA_PLATFORM
    $previousBackend = $env:QT_QUICK_BACKEND
    $savedImports = @{}
    foreach ($name in @('QML2_IMPORT_PATH','QML_IMPORT_PATH','QT_PLUGIN_PATH')) {
        $savedImports[$name] = [Environment]::GetEnvironmentVariable($name)
        [Environment]::SetEnvironmentVariable($name,$null)
    }
    try {
        $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
        $env:QT_QPA_PLATFORM = 'offscreen'
        $env:QT_QUICK_BACKEND = 'software'
        $process = Start-Process (Join-Path $PackageDir 'MOEXWatch-Qt.exe') -WorkingDirectory $PackageDir -ArgumentList '--smoke-test' -PassThru -RedirectStandardOutput "$LogPrefix-out.txt" -RedirectStandardError "$LogPrefix-err.txt"
        if (-not $process.WaitForExit(30000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            throw 'Packaged application exceeded 30 seconds'
        }
        $process.Refresh()
        Get-Content "$LogPrefix-err.txt" -ErrorAction SilentlyContinue
        if ($process.ExitCode -ne 0) { throw "Packaged QML smoke failed: $($process.ExitCode)" }
    } finally {
        $env:PATH = $previousPath
        $env:QT_QPA_PLATFORM = $previousPlatform
        $env:QT_QUICK_BACKEND = $previousBackend
        foreach ($name in $savedImports.Keys) { [Environment]::SetEnvironmentVariable($name,$savedImports[$name]) }
    }
}
