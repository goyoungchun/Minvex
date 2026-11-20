param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$SdkRoot,
    [Parameter(Mandatory)][string]$SdkData,
    [Parameter(Mandatory)][string]$SdkHeader,
    [Parameter(Mandatory)][string]$CMake,
    [Parameter(Mandatory)][string]$Configuration
)

$ErrorActionPreference = 'Stop'
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
if (!(Test-Path -LiteralPath $koreanMessages) -and $env:GITHUB_ACTIONS -eq 'true' -and $instance) {
    $installer = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/setup.exe'
    $installerArgs = @('modify', '--installPath', ('"' + $instance + '"'), '--addProductLang', 'ko-KR', '--quiet', '--norestart')
    $setup = Start-Process -FilePath $installer -ArgumentList $installerArgs -WindowStyle Hidden -Wait -PassThru
    if ($setup.ExitCode -notin 0, 3010) { throw "Korean Visual Studio language pack installation failed ($($setup.ExitCode))." }
}
if (!(Test-Path -LiteralPath $koreanMessages)) { throw 'The active MSVC toolset requires Korean compiler resources (1042/clui.dll).' }
$env:VSLANG = '1042'

$verificationRoot = Split-Path -Parent $SdkRoot
$localizedSdk = Join-Path $verificationRoot '설치 SDK'
& $CMake -E copy_directory $SdkRoot $localizedSdk
if ($LASTEXITCODE -ne 0) { throw 'Could not stage the SDK in a Korean path.' }
$SdkData = Join-Path $localizedSdk ([System.IO.Path]::GetRelativePath($SdkRoot, $SdkData))
$SdkHeader = Join-Path $localizedSdk ([System.IO.Path]::GetRelativePath($SdkRoot, $SdkHeader))
$build = Join-Path $verificationRoot '빠른 시작 빌드'
$options = @('-S', (Join-Path $SdkData 'examples/QuickStart'), '-B', $build, '-G', 'Ninja',
    "-DCMAKE_PREFIX_PATH=$localizedSdk", "-DCMAKE_BUILD_TYPE=$Configuration", '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF')
$manifest = Get-Content -LiteralPath (Join-Path $SdkData 'sdk-manifest.json') -Raw | ConvertFrom-Json
$options += "-DCMAKE_MSVC_RUNTIME_LIBRARY=$($manifest.runtime)"
& $CMake @options
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
