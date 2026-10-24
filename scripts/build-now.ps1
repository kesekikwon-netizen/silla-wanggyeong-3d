param([string]$Config = "Release", [switch]$Test)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
. "$PSScriptRoot\dev-env.ps1"
if (-not (Test-Path "build\CMakeCache.txt")) {
  cmake --preset vs
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
cmake --build build --config $Config --parallel --target strata-3d test_strata_3d
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($Test) {
  ctest --test-dir build -C $Config --output-on-failure
  exit $LASTEXITCODE
}
