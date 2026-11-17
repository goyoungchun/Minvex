# Real SDK packages, performance comparison and 30-minute stability.
$ErrorActionPreference = 'Stop'
$pythonCommand = if ($env:MINVEX_TEST_PYTHON) { $env:MINVEX_TEST_PYTHON } else { 'python' }
& $pythonCommand (Join-Path $PSScriptRoot '../RunTests.py') --platform windows --suite release --execute @args
exit $LASTEXITCODE
