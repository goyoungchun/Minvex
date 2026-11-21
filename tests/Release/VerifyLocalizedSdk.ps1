param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$SdkRoot,
    [Parameter(Mandatory)][string]$SdkData,
    [Parameter(Mandatory)][string]$SdkHeader,
    [Parameter(Mandatory)][string]$CMake,
    [Parameter(Mandatory)][string]$Configuration
)

$ErrorActionPreference = 'Stop'

function Save-KoreanCompilerMessages($CatalogPath, $Toolset, $Destination, $WorkDirectory) {
    $catalog = Get-Content -LiteralPath $CatalogPath -Raw | ConvertFrom-Json -Depth 100
    $family = [regex]::Escape(([version]$Toolset).ToString(2))
    $resourceId = "^Microsoft\.VC\.$family(?:\.\d+\.\d+)?\.Tools\.HostX64\.TargetX64\.Res\.base$"
    $payloads = @($catalog.packages | Where-Object {
        $_.language -eq 'ko-KR' -and $_.id -match $resourceId
    } | ForEach-Object payloads)
    if ($payloads.Count -ne 1) { throw "Expected one Korean resource package for MSVC $Toolset." }
    $payload = $payloads[0]
    $uri = [uri]$payload.url
    if ($uri.Scheme -ne 'https' -or $uri.Host -ne 'download.visualstudio.microsoft.com') {
        throw 'The Visual Studio catalog did not specify a Microsoft download URL.'
    }

    # Extract only the compiler's message DLL from the matching, checksum-verified VSIX.
    # A full Visual Studio modify can fail or change the toolset used to build the SDK.
    $package = Join-Path $WorkDirectory 'msvc-ko-KR.vsix'
    Invoke-WebRequest -Uri $uri -OutFile $package -TimeoutSec 60
    if ((Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash -ne $payload.sha256) {
        throw 'The Korean Visual Studio resource package checksum did not match its catalog.'
    }
    $zip = [IO.Compression.ZipFile]::OpenRead($package)
    try {
        $entry = $zip.GetEntry("Contents/VC/Tools/MSVC/$Toolset/bin/Hostx64/x64/1042/clui.dll")
        if (!$entry) { throw "The Korean resource package does not match the installed MSVC $Toolset." }
        New-Item -ItemType Directory -Force (Split-Path -Parent $Destination) | Out-Null
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $Destination)
    } finally {
        $zip.Dispose()
    }
}

$instanceEntry = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt') |
    Where-Object { $_ -match '^CMAKE_GENERATOR_INSTANCE:[^=]+=' } |
    Select-Object -First 1
$instance = ($instanceEntry -replace '^CMAKE_GENERATOR_INSTANCE:[^=]+=', '') -replace ',version=.*$', ''
if ($instance) {
    Push-Location (Join-Path $instance 'Common7/Tools')
    try {
        $developerEnvironment = cmd.exe /d /c 'call VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && set'
        if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the SDK Visual Studio instance.' }
        foreach ($line in $developerEnvironment) {
            if ($line -match '^([^=]+)=(.*)$') {
                [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
            }
        }
    } finally {
        Pop-Location
    }
}

$compiler = Get-Command cl.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1
$koreanMessages = Join-Path (Split-Path -Parent $compiler.Source) '1042/clui.dll'
$verificationRoot = Split-Path -Parent $SdkRoot
if (!(Test-Path -LiteralPath $koreanMessages) -and $env:GITHUB_ACTIONS -eq 'true' -and $instance) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $installed = & $vswhere -all -prerelease -products '*' -format json | ConvertFrom-Json |
        Where-Object { $_.installationPath.Replace('\', '/') -eq $instance.Replace('\', '/') } |
        Select-Object -First 1
    if (!$installed) { throw 'Could not find the catalog for the SDK Visual Studio instance.' }
    $catalog = Join-Path $env:ProgramData "Microsoft/VisualStudio/Packages/_Instances/$($installed.instanceId)/catalog.json"
    Save-KoreanCompilerMessages $catalog $env:VCToolsVersion.Trim() $koreanMessages $verificationRoot
}
if (!(Test-Path -LiteralPath $koreanMessages)) { throw 'The active MSVC toolset requires Korean compiler resources (1042/clui.dll).' }
$env:VSLANG = '1042'

# Use the Visual Studio installation's native Ninja when available. A PATH
# shim can report a different build-file code page without a console.
$ninja = if ($env:VSINSTALLDIR) {
    Join-Path $env:VSINSTALLDIR 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
}
if (!$ninja -or !(Test-Path -LiteralPath $ninja)) {
    $ninja = (Get-Command ninja.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
}
Write-Host "Verifying Ninja at $ninja"

$localizedSdk = Join-Path $verificationRoot '설치 SDK'
& $CMake -E copy_directory $SdkRoot $localizedSdk
if ($LASTEXITCODE -ne 0) { throw 'Could not stage the SDK in a Korean path.' }
$SdkData = Join-Path $localizedSdk ([System.IO.Path]::GetRelativePath($SdkRoot, $SdkData))
$SdkHeader = Join-Path $localizedSdk ([System.IO.Path]::GetRelativePath($SdkRoot, $SdkHeader))
$build = Join-Path $verificationRoot '빠른 시작 빌드'
$options = @('-S', (Join-Path $SdkData 'examples/QuickStart'), '-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$ninja",
    "-DCMAKE_PREFIX_PATH=$localizedSdk", "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_TRY_COMPILE_CONFIGURATION=$Configuration", '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF')
$manifest = Get-Content -LiteralPath (Join-Path $SdkData 'sdk-manifest.json') -Raw | ConvertFrom-Json
$options += "-DCMAKE_MSVC_RUNTIME_LIBRARY=$($manifest.runtime)"
& $CMake @options -B $build
if ($LASTEXITCODE -ne 0) { throw 'Korean Ninja Quick Start configuration failed.' }
$prefix = Get-Content -LiteralPath (Join-Path $build 'CMakeFiles/rules.ninja') |
    Where-Object { $_ -like 'msvc_deps_prefix = *' }
if (($prefix -join '') -notmatch '[^\u0000-\u007f]') {
    throw 'Expected a localized /showIncludes prefix in Ninja rules.'
}

& $CMake --build $build --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Korean Ninja Quick Start build failed.' }
$object = Join-Path $build 'CMakeFiles/minvex_quickstart_skinning.dir/Skinning.cpp.obj'
$timestamp = (Get-Item -LiteralPath $object).LastWriteTimeUtc
& $CMake --build $build --target minvex_quickstart_skinning
if ($LASTEXITCODE -ne 0 -or (Get-Item -LiteralPath $object).LastWriteTimeUtc -ne $timestamp) {
    throw 'The unchanged Quick Start unexpectedly rebuilt.'
}

# The archive and the regular SDK verification copy remain pristine.
Add-Content -LiteralPath $SdkHeader -Value '// Minvex SDK dependency rebuild probe'
& $CMake --build $build --target minvex_quickstart_skinning
if ($LASTEXITCODE -ne 0 -or (Get-Item -LiteralPath $object).LastWriteTimeUtc -le $timestamp) {
    throw 'Ninja did not recompile the Quick Start after changing the installed Viewer.h.'
}
Write-Host 'Verified Korean MSVC /showIncludes and incremental rebuild from an installed SDK header.'

function Invoke-HeadlessCMake([string[]]$Arguments) {
    $info = [System.Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [Text.Encoding]::UTF8
    $info.StandardErrorEncoding = [Text.Encoding]::UTF8
    $info.Environment['MINVEX_TEST_CMAKE_DRIVER'] = Join-Path $SdkData 'examples/QuickStart/Invoke-MinvexCMake.ps1'
    $info.Environment['MINVEX_TEST_CMAKE'] = $CMake
    $info.Environment['MINVEX_TEST_CMAKE_ARGS'] = ConvertTo-Json -InputObject $Arguments -Compress
    $command = @'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
& $env:MINVEX_TEST_CMAKE_DRIVER -CMake $env:MINVEX_TEST_CMAKE -CMakeArguments (ConvertFrom-Json $env:MINVEX_TEST_CMAKE_ARGS)
exit $LASTEXITCODE
'@
    foreach ($argument in @('-NoProfile', '-NonInteractive', '-Command', $command)) {
        $info.ArgumentList.Add($argument)
    }
    $process = [System.Diagnostics.Process]::Start($info)
    try {
        $output = $process.StandardOutput.ReadToEndAsync()
        $errors = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        if ($output.Result) { Write-Host $output.Result.TrimEnd() }
        if ($errors.Result) { Write-Host $errors.Result.TrimEnd() }
        if ($process.ExitCode -ne 0) {
            throw "Headless Korean Ninja CMake failed for '$($Arguments -join ' ')' with exit code $($process.ExitCode)."
        }
    } finally {
        $process.Dispose()
    }
}

$headlessBuild = Join-Path $verificationRoot '콘솔 없는 빌드'
if ($Configuration -eq 'Release' -and $manifest.runtime -eq 'MultiThreadedDLL') {
    Invoke-HeadlessCMake @('-S', (Join-Path $SdkData 'examples/QuickStart'),
        '--preset', 'windows-release', '-B', $headlessBuild, "-DMinvex_ROOT=$localizedSdk",
        '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF')
} else {
    Invoke-HeadlessCMake ($options + @('-B', $headlessBuild, '-DMINVEX_QUICKSTART_BUILD_EXTRAS=OFF'))
}
Invoke-HeadlessCMake @('--build', $headlessBuild)
$headlessObject = Join-Path $headlessBuild 'CMakeFiles/minvex_quickstart.dir/Main.cpp.obj'
$headlessTimestamp = (Get-Item -LiteralPath $headlessObject).LastWriteTimeUtc
Invoke-HeadlessCMake @('--build', $headlessBuild)
if ((Get-Item -LiteralPath $headlessObject).LastWriteTimeUtc -ne $headlessTimestamp) {
    throw 'The unchanged headless Quick Start unexpectedly rebuilt.'
}
Add-Content -LiteralPath $SdkHeader -Value '// Minvex SDK headless dependency rebuild probe'
Invoke-HeadlessCMake @('--build', $headlessBuild)
if ((Get-Item -LiteralPath $headlessObject).LastWriteTimeUtc -le $headlessTimestamp) {
    throw 'Headless Ninja did not recompile the Quick Start after changing the installed Viewer.h.'
}
Write-Host 'Verified headless Korean MSVC Ninja build and incremental rebuild.'
