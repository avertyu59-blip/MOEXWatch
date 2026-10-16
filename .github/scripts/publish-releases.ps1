$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

$releases = @($env:RELEASE_PLAN | ConvertFrom-Json)
if ($releases.Count -eq 0) { return }
$repository = $env:GITHUB_REPOSITORY
$runRoot = Join-Path $env:RUNNER_TEMP "moexwatch-releases-$($env:GITHUB_RUN_ID)-$($env:GITHUB_RUN_ATTEMPT)"
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

foreach ($release in $releases) {
    $version = [string]$release.version
    $commit = [string]$release.commit
    if ($version -notmatch '^\d+\.\d+\.\d+$' -or $commit -notmatch '^[a-f0-9]{40}$') {
        throw 'Invalid release version or commit.'
    }
    $tag = "v$version"
    $sourceDir = Join-Path $runRoot "source-$version"
    $buildDir = Join-Path $runRoot "build-$version"
    $packageDir = Join-Path $runRoot "package-$version"
    Write-Host "Building MOEXWatch $version from $commit"
    git worktree add --detach $sourceDir $commit
    if ($LASTEXITCODE -ne 0) { throw "Checkout failed for $version" }
    $actualVersion = (Get-Content (Join-Path $sourceDir 'VERSION.txt') -Raw).Trim()
    if ($actualVersion -ne $version) { throw "VERSION.txt mismatch for $version" }
    cmake -S $sourceDir -B $buildDir -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded'
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed for $version" }
    cmake --build $buildDir --config Release
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $version" }
    ctest --test-dir $buildDir -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed for $version" }

    New-Item -ItemType Directory -Path $packageDir | Out-Null
    Copy-Item (Join-Path $buildDir 'Release\moexwatch.exe') (Join-Path $packageDir 'MOEXWatch.exe')
    Copy-Item (Join-Path $sourceDir 'README.md') $packageDir
    Copy-Item (Join-Path $sourceDir 'VERSION.txt') $packageDir
    $binaryZip = Join-Path $runRoot "MOEXWatch-$version-Windows-x64.zip"
    $sourceZip = Join-Path $runRoot "MOEXWatch-$version-source.zip"
    Compress-Archive -Path (Join-Path $packageDir '*') -DestinationPath $binaryZip
    git -C $sourceDir archive --format=zip "--output=$sourceZip" HEAD
    if ($LASTEXITCODE -ne 0) { throw "Source archive failed for $version" }
    $hashFile = Join-Path $runRoot "MOEXWatch-$version-SHA256SUMS.txt"
    @($binaryZip, $sourceZip) | ForEach-Object {
        $hash = Get-FileHash -Path $_ -Algorithm SHA256
        "$($hash.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($_))"
    } | Set-Content -Path $hashFile -Encoding utf8
    $notes = Join-Path $runRoot "notes-$version.md"
    @"
## MOEXWatch $version

$($release.summary)

Тестовый выпуск для Windows 10 x64. Эта версия собрана MSVC, тесты CTest пройдены перед публикацией.

### Скачать и запустить

- **MOEXWatch-$version-Windows-x64.zip** — готовое приложение со встроенной средой выполнения MSVC. Распакуйте архив и запустите **MOEXWatch.exe**. Сборка на компьютере пользователя не нужна.
- **MOEXWatch-$version-source.zip** — исходники этой версии.
- **MOEXWatch-$version-SHA256SUMS.txt** — контрольные суммы архивов.

Перед заменой файлов закройте приложение. Пользовательские данные хранятся отдельно в `%APPDATA%\MOEXWatch`.
Подробности возможностей и ограничений — в README этой версии. Коммит исходников: $commit.
"@ | Set-Content -Path $notes -Encoding utf8

    $existingText = gh api "repos/$repository/releases/tags/$tag" 2>$null
    $existingCode = $LASTEXITCODE
    if ($existingCode -eq 0) {
        $existing = $existingText | ConvertFrom-Json
        if (-not $existing.draft) {
            Write-Host "$tag is already published; preserving existing files."
            continue
        }
        if ($existing.target_commitish -ne $commit) { throw "Existing draft $tag points to another commit." }
    } else {
        gh release create $tag --repo $repository --target $commit --title "MOEXWatch $version" --notes-file $notes --prerelease --draft
        if ($LASTEXITCODE -ne 0) { throw "Draft release creation failed for $version" }
    }
    gh release upload $tag $binaryZip $sourceZip $hashFile --repo $repository --clobber
    if ($LASTEXITCODE -ne 0) { throw "Asset upload failed for $version" }
    gh release edit $tag --repo $repository --draft=false
    if ($LASTEXITCODE -ne 0) { throw "Release publication failed for $version" }
    Write-Host "Published $tag with executable, sources and checksums."
}
