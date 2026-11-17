# Run in an x64 Developer PowerShell. Optional: --plan or --output <directory>.
$ErrorActionPreference = 'Stop'
$pythonCommand = if ($env:MINVEX_TEST_PYTHON) { $env:MINVEX_TEST_PYTHON } else { 'python' }
& $pythonCommand (Join-Path $PSScriptRoot '../RunTests.py') --platform windows --suite contracts --execute @args
exit $LASTEXITCODE
