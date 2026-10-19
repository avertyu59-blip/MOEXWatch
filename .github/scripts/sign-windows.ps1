param(
    [Parameter(Mandatory)][string]$FilePath,
    [Parameter(Mandatory)][string]$CertificateThumbprint,
    [Parameter(Mandatory)][string]$TimestampUrl
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
if ($env:OS -ne 'Windows_NT') { throw 'Authenticode signing requires Windows' }
if ($CertificateThumbprint -notmatch '^[a-fA-F0-9]{40}$') { throw 'Expected a 40-digit certificate thumbprint' }
$timestamp = $null
if (-not [Uri]::TryCreate($TimestampUrl, [UriKind]::Absolute, [ref]$timestamp) -or
    $timestamp.Scheme -ne 'https' -or -not $timestamp.Host -or $timestamp.UserInfo) {
    throw 'A valid HTTPS RFC3161 timestamp service is required'
}
$file = Get-Item -LiteralPath $FilePath
if ($file.PSIsContainer -or $file.Extension -ne '.exe' -or
    ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Expected a regular EXE file' }
$cert = Get-Item -LiteralPath "Cert:\CurrentUser\My\$CertificateThumbprint"
$now = Get-Date
if (-not $cert.HasPrivateKey -or $cert.NotBefore -gt $now -or $cert.NotAfter -le $now -or
    -not ($cert.EnhancedKeyUsageList | Where-Object { $_.ObjectId.Value -eq '1.3.6.1.5.5.7.3.3' })) {
    throw 'A current code-signing certificate with a private key is required'
}
$tool = Get-Command signtool.exe -ErrorAction SilentlyContinue
if ($tool) { $signtool = $tool.Source } else {
    $signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" |
        Sort-Object { [version]$_.Directory.Parent.Name } -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $signtool) { throw 'Windows SDK signtool.exe not found' }
& $signtool sign /s My /sha1 $CertificateThumbprint /fd SHA256 /tr $TimestampUrl /td SHA256 $file.FullName
if ($LASTEXITCODE -ne 0) { throw 'Authenticode signing failed; do not publish this package' }
& $signtool verify /pa /all $file.FullName
if ($LASTEXITCODE -ne 0) { throw 'Authenticode verification failed' }
$signature = Get-AuthenticodeSignature -LiteralPath $file.FullName
if ($signature.Status -ne 'Valid' -or -not $signature.TimeStamperCertificate -or
    $signature.SignerCertificate.Thumbprint -ne $CertificateThumbprint) {
    throw 'Expected trusted signer and timestamp were not verified'
}
Write-Host 'Authenticode signer and timestamp verified.'
