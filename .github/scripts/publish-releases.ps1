$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
. (Join-Path $PSScriptRoot 'qt-package.ps1')
$releases = @($env:RELEASE_PLAN | ConvertFrom-Json)
$repository = $env:GITHUB_REPOSITORY
$runRoot = Join-Path $env:RUNNER_TEMP "moexwatch-releases-$($env:GITHUB_RUN_ID)-$($env:GITHUB_RUN_ATTEMPT)"
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
foreach ($release in $releases) {
    $version = [string]$release.version; $commit = [string]$release.commit
    if ($version -notmatch '^\d+\.\d+\.\d+$' -or $commit -notmatch '^[a-f0-9]{40}$') { throw 'Invalid version or commit' }
    $tag = "v$version"
    $existingTag = git rev-parse --verify "${tag}^{commit}" 2>$null
    if ($LASTEXITCODE -eq 0 -and $existingTag.Trim() -ne $commit) { throw "Existing tag $tag targets another commit" }
    $sourceDir = Join-Path $runRoot "source-$version"
    $buildDir = Join-Path $runRoot "build-$version"
    $packageDir = Join-Path $runRoot "package-$version"
    git worktree add --detach $sourceDir $commit
    if ($LASTEXITCODE -ne 0) { throw 'Checkout failed' }
    if ((Get-Content (Join-Path $sourceDir 'VERSION.txt') -Raw).Trim() -ne $version) { throw 'Version mismatch' }
    cmake -S $sourceDir -B $buildDir -G 'Visual Studio 17 2022' -A x64 -DMOEX_BUILD_QT=ON
    if ($LASTEXITCODE -ne 0) { throw 'Configuration failed' }
    cmake --build $buildDir --config Release --parallel 2
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    ctest --test-dir $buildDir -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    Deploy-QtPackage -SourceDir $sourceDir -BuildDir $buildDir -PackageDir $packageDir
    $signed = $false
    if ($env:MOEX_SIGNING_CERT_THUMBPRINT) {
        & (Join-Path $PSScriptRoot 'sign-windows.ps1') -FilePath (Join-Path $packageDir 'MOEXWatch-Qt.exe') -CertificateThumbprint $env:MOEX_SIGNING_CERT_THUMBPRINT -TimestampUrl $env:MOEX_SIGNING_TIMESTAMP_URL
        $signed = $true
    } elseif ($env:MOEX_REQUIRE_SIGNING -eq 'true') {
        throw 'Signing is required, but no certificate thumbprint is configured'
    }
    Test-QtPackage -PackageDir $packageDir -LogPrefix (Join-Path $runRoot 'deployed')
    $binaryZip = Join-Path $runRoot "MOEXWatch-$version-Windows-x64.zip"
    $sourceZip = Join-Path $runRoot "MOEXWatch-$version-source.zip"
    Compress-Archive -Path (Join-Path $packageDir '*') -DestinationPath $binaryZip
    git -C $sourceDir archive --format=zip "--output=$sourceZip" HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Source archive failed' }
    $extracted = Join-Path $runRoot 'extracted'
    Expand-Archive $binaryZip $extracted
    if ($signed) {
        $signature = Get-AuthenticodeSignature -LiteralPath (Join-Path $extracted 'MOEXWatch-Qt.exe')
        if ($signature.Status -ne 'Valid' -or -not $signature.TimeStamperCertificate -or
            $signature.SignerCertificate.Thumbprint -ne $env:MOEX_SIGNING_CERT_THUMBPRINT) {
            throw 'Extracted EXE signature verification failed'
        }
    }
    Test-QtPackage -PackageDir $extracted -LogPrefix (Join-Path $runRoot 'extracted')
    $hashFile = Join-Path $runRoot "MOEXWatch-$version-SHA256SUMS.txt"
    @($binaryZip,$sourceZip) | ForEach-Object { "$((Get-FileHash $_ -Algorithm SHA256).Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($_))" } | Set-Content $hashFile -Encoding utf8
    $notes = Join-Path $runRoot 'notes.md'
    @"
## MOEXWatch $version — рабочее место на Qt Quick

CSV-импорт/экспорт операций в Qt с предпросмотром, проверкой ошибок/дубликатов и ID. Сводка портфеля со стоимостью, себестоимостью, качеством котировок, концентрацией и ручным остатком денег. Дивиденды: ожидаемые права отдельно от фактически подтверждённых поступлений, указанное удержание налога, итог дохода и привязка к ID позиции. Опциональный бэктест использует только неполную выборку подтверждённых поступлений профиля и личные даты зачисления; IMOEX остаётся ценовым индексом. Календарь прав ограничен 2026 годом. Полные правила и ограничения — PORTFOLIO_0_8.md в архиве.

Сохранены обзор/график, скринер, сигналы, план покупки, компания/сравнение, события, бэктест, портфель и дневник.

Распакуйте **MOEXWatch-$version-Windows-x64.zip** целиком и запустите **MOEXWatch-Qt.exe**. Qt и Visual Studio устанавливать не нужно. Перед запуском закройте старые копии. В архиве только Qt-приложение.

Фундаментальные показатели вводятся из отчёта или импортируются с источником, датой и периодом; автоматическое извлечение отчётности не реализовано. Календарь отчётностей/собраний заполняется пользователем, дивиденды загружаются из открытых источников. Бэктест имеет явно указанные ограничения (ценовой IMOEX, дробное количество, неполные выбранные выплаты, без налогов/проскальзывания).

Перед публикацией пройдены CTest, запуск развёрнутого и повторно распакованного приложения без Qt SDK в PATH на Windows Server2022/MSVC2022. Ручная проверка Windows10/GPU отдельно не выполнялась. Данные в APPDATA/MOEXWatch; перед обновлением создайте резервную копию. Полное описание — README в архиве.

Исходники: $commit. Контрольные суммы архивов приложены. Предыдущие выпуски сохраняются.
"@ | Set-Content $notes -Encoding utf8
    $versionNotes = Join-Path $sourceDir "docs/releases/$version.md"
    if (Test-Path $versionNotes) { Copy-Item $versionNotes $notes -Force }
    $signatureNote = if ($signed) { 'EXE: Authenticode signature and RFC3161 timestamp verified.' } else { 'EXE: not Authenticode-signed. SHA256 checksums verify archive integrity only.' }
    Add-Content $notes "`n$signatureNote" -Encoding utf8
    $existingText = gh api "repos/$repository/releases/tags/$tag" 2>$null
    if ($LASTEXITCODE -eq 0) {
        $existing = $existingText | ConvertFrom-Json
        if (-not $existing.draft) { Write-Host "$tag already published; preserving assets"; continue }
        if ($existing.target_commitish -ne $commit) { throw 'Existing draft targets another commit' }
    } else {
        gh release create $tag --repo $repository --target $commit --title "MOEXWatch $version · Qt" --notes-file $notes --draft
        if ($LASTEXITCODE -ne 0) { throw 'Draft creation failed' }
    }
    gh release upload $tag $binaryZip $sourceZip $hashFile --repo $repository --clobber
    if ($LASTEXITCODE -ne 0) { throw 'Upload failed' }
    gh release edit $tag --repo $repository --draft=false --latest
    if ($LASTEXITCODE -ne 0) { throw 'Publication failed' }
}
