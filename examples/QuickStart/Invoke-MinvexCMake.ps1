param(
    [Parameter(Mandatory)][string]$CMake,
    [Parameter(Mandatory)][string[]]$CMakeArguments,
    [ValidateRange(0, 86400)][int]$TimeoutSeconds = 0
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$cmakePath = if (Test-Path -LiteralPath $CMake -PathType Leaf) { (Get-Item -LiteralPath $CMake).FullName }
    else { (Get-Command -Name $CMake -CommandType Application | Select-Object -First 1).Source }
$stage = if ($CMakeArguments -contains '--build') { 'Build' }
    elseif ($CMakeArguments -contains '--install') { 'Install' }
    elseif ($CMakeArguments -contains '--workflow') { 'Workflow' }
    elseif ($CMakeArguments -contains '-E' -or $CMakeArguments -contains '--version') { 'CMake command' }
    else { 'Configure' }
$command = @{ Program = $cmakePath; Arguments = $CMakeArguments; Stage = $stage } | ConvertTo-Json -Compress
$payload = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($command))

# MSVC encodes /showIncludes using the console code page. Give it a private
# UTF-8 console so an automation host can build Unicode paths without a visible window.
$childCommand = @'
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$PSNativeCommandUseErrorActionPreference = $false
$codePage = chcp.com 65001
if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the UTF-8 console.' }
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$command = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('__MINVEX_PAYLOAD__')) | ConvertFrom-Json
Write-Output "[Minvex] $codePage"
foreach ($tool in 'cl.exe', 'ninja.exe') {
    $path = Get-Command $tool -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($path) { Write-Output "[Minvex] $tool on PATH = $($path.Source)" }
}
Write-Output "[Minvex] $($command.Stage) started"
$arguments = [string[]]$command.Arguments
& $command.Program @arguments
exit $LASTEXITCODE
'@
$childCommand = $childCommand.Replace('__MINVEX_PAYLOAD__', $payload)
$encodedCommand = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($childCommand))
$stdout = [IO.Path]::GetTempFileName()
$stderr = [IO.Path]::GetTempFileName()
$streams = @()
$process = $null
function Write-MinvexOutput($stream, $decoder, [bool]$isError, [bool]$flush) {
    while (($count = $stream.Read($outputBuffer, 0, $outputBuffer.Length)) -gt 0) {
        $length = $decoder.GetChars($outputBuffer, 0, $count, $textBuffer, 0, $false)
        if ($length -gt 0) {
            $message = [string]::new($textBuffer, 0, $length)
            if ($isError) { [Console]::Error.Write($message) } else { Write-Host -NoNewline $message }
        }
    }
    if ($flush) {
        $length = $decoder.GetChars($outputBuffer, 0, 0, $textBuffer, 0, $true)
        if ($length -gt 0) {
            $message = [string]::new($textBuffer, 0, $length)
            if ($isError) { [Console]::Error.Write($message) } else { Write-Host -NoNewline $message }
        }
    }
}
try {
    $shell = (Get-Process -Id $PID).Path
    $directory = (Get-Location -PSProvider FileSystem).ProviderPath
    Write-Host "[Minvex] $stage using $cmakePath (starting hidden UTF-8 console)"
    $process = Start-Process -FilePath $shell -ArgumentList '-NoProfile', '-NonInteractive', '-EncodedCommand', $encodedCommand `
        -WorkingDirectory $directory -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    # Windows PowerShell's Process wrapper needs an open handle to retain ExitCode.
    $null = $process.Handle
    $streams += [IO.File]::Open($stdout, 'Open', 'Read', 'ReadWrite')
    $streams += [IO.File]::Open($stderr, 'Open', 'Read', 'ReadWrite')
    $stdoutDecoder = [Text.Encoding]::UTF8.GetDecoder()
    $stderrDecoder = [Text.Encoding]::UTF8.GetDecoder()
    $outputBuffer = [byte[]]::new(4096)
    $textBuffer = [char[]]::new([Text.Encoding]::UTF8.GetMaxCharCount($outputBuffer.Length))
    $elapsed = [Diagnostics.Stopwatch]::StartNew()
    do {
        $finished = $process.WaitForExit(100)
        Write-MinvexOutput $streams[0] $stdoutDecoder $false $finished
        Write-MinvexOutput $streams[1] $stderrDecoder $true $finished
        if (!$finished -and $TimeoutSeconds -gt 0 -and $elapsed.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
            throw "[Minvex] $stage exceeded $TimeoutSeconds seconds."
        }
    } until ($finished)
    Write-Host "[Minvex] $stage exited with code $($process.ExitCode)"
    exit $process.ExitCode
} finally {
    if ($process) {
        try {
            if (!$process.HasExited) {
                if ($PSVersionTable.PSEdition -eq 'Core') { $process.Kill($true) }
                else {
                    & taskkill.exe /PID $process.Id /T /F 2>$null | Out-Null
                    if ($LASTEXITCODE -ne 0 -and !$process.HasExited) { $process.Kill() }
                }
                [void]$process.WaitForExit(2000)
            }
        } catch {
            if (!$process.HasExited) { Write-Warning "Could not stop the hidden CMake console. $($_.Exception.Message)" }
        }
        $process.Dispose()
    }
    foreach ($stream in $streams) { $stream.Dispose() }
    Remove-Item -LiteralPath $stdout, $stderr -ErrorAction SilentlyContinue
}
