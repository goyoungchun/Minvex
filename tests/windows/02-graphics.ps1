# Native D3D11, Viewer, Editor and shader combinations; desktop required.
$ErrorActionPreference = 'Stop'
$pythonCommand = if ($env:MINVEX_TEST_PYTHON) { $env:MINVEX_TEST_PYTHON } else { 'python' }
& $pythonCommand (Join-Path $PSScriptRoot '../RunTests.py') --platform windows --suite graphics --execute @args
exit $LASTEXITCODE
