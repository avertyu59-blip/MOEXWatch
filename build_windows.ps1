param([Parameter(Mandatory=$true)][string]$QtRoot)
$ErrorActionPreference = 'Stop'
# The supported desktop application is Qt Quick. Keep the familiar entry point.
& (Join-Path $PSScriptRoot 'build_qt_windows.ps1') -QtRoot $QtRoot
