$ErrorActionPreference = 'Stop'
$script = Join-Path $PSScriptRoot 'sign-windows.ps1'
function Assert-Rejected([scriptblock]$Action) {
    $rejected = $false
    try { & $Action } catch { $rejected = $true }
    if (-not $rejected) { throw 'Invalid signing configuration was accepted' }
}
Get-ChildItem $PSScriptRoot -Filter '*.ps1' | ForEach-Object {
    $tokens = $null; $errors = $null
    [void][Management.Automation.Language.Parser]::ParseFile($_.FullName, [ref]$tokens, [ref]$errors)
    if ($errors) { throw "PowerShell parse errors in $($_.Name): $errors" }
}
$file = Join-Path $env:RUNNER_TEMP 'signing-input-validation.exe'
[IO.File]::WriteAllText($file, 'untouched test fixture')
$before = (Get-FileHash $file -Algorithm SHA256).Hash
try {
    Assert-Rejected { & $script -FilePath $file -CertificateThumbprint 'bad' -TimestampUrl 'https://example.invalid' }
    Assert-Rejected { & $script -FilePath $file -CertificateThumbprint ('0' * 40) -TimestampUrl 'http://example.invalid' }
    Assert-Rejected { & $script -FilePath $file -CertificateThumbprint ('0' * 40) -TimestampUrl 'https://user:password@example.invalid' }
    Assert-Rejected { & $script -FilePath $file -CertificateThumbprint ('0' * 40) -TimestampUrl 'https://example.invalid' }
    if ((Get-FileHash $file -Algorithm SHA256).Hash -ne $before) { throw 'Rejected signing modified the input EXE' }
} finally { Remove-Item -LiteralPath $file -Force }
Write-Host 'PowerShell parsing and rejected signing configuration tests passed.'
