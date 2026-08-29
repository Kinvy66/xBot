param(
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$ServerArgs
)

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
$VenvPython = Join-Path $Root ".venv\Scripts\python.exe"

if (-not (Test-Path $VenvPython)) {
  Write-Host "Creating .venv ..."
  python -m venv (Join-Path $Root ".venv")
}

$probe = & $VenvPython -c "import fastapi, uvicorn, serial" 2>&1
if ($LASTEXITCODE -ne 0) {
  Write-Host "Installing requirements ..."
  & $VenvPython -m pip install -r (Join-Path $Root "requirements.txt")
  if ($LASTEXITCODE -ne 0) { throw "pip install failed" }
}

& $VenvPython (Join-Path $Root "server.py") @ServerArgs
