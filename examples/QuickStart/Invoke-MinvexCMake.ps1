param(
    [Parameter(Mandatory)][string]$CMake,
    [Parameter(Mandatory)][string[]]$CMakeArguments
)

$ErrorActionPreference = 'Stop'
$command = @{ Program = $CMake; Arguments = $CMakeArguments } | ConvertTo-Json -Compress
$payload = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($command))

# MSVC encodes /showIncludes using the console code page. Give it a private
# UTF-8 console so an automation host can build Unicode paths without a visible window.
$childCommand = @'
$ErrorActionPreference = 'Stop'
chcp.com 65001 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the UTF-8 console.' }
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$command = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('__MINVEX_PAYLOAD__')) | ConvertFrom-Json
$arguments = [string[]]$command.Arguments
& $command.Program @arguments
exit $LASTEXITCODE
'@
$childCommand = $childCommand.Replace('__MINVEX_PAYLOAD__', $payload)
$encodedCommand = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($childCommand))
$stdout = [IO.Path]::GetTempFileName()
$stderr = [IO.Path]::GetTempFileName()
try {
    $shell = (Get-Process -Id $PID).Path
    $process = Start-Process -FilePath $shell -ArgumentList '-NoProfile', '-NonInteractive', '-EncodedCommand', $encodedCommand `
        -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $output = Get-Content -LiteralPath $stdout -Raw -Encoding utf8
    $errors = Get-Content -LiteralPath $stderr -Raw -Encoding utf8
    if ($output) { Write-Host $output.TrimEnd() }
    if ($errors) { [Console]::Error.WriteLine($errors.TrimEnd()) }
    exit $process.ExitCode
} finally {
    Remove-Item -LiteralPath $stdout, $stderr
}
