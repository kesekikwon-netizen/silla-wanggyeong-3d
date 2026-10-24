param([string]$Out = "$env:TEMP\s3d_frame.png", [string]$Cam = "", [switch]$NoBuild)
# 빌드 후 --smoke-quit 로 한 장 그려 PNG 로 저장한다(눈으로 확인용). 종료 코드와 스모크 한 줄을 출력.
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
. "$PSScriptRoot\dev-env.ps1" | Out-Null
if (-not $NoBuild) {
  cmake --build build --config Release --parallel --target strata-3d 2>&1 | Select-String -Pattern "error|fatal|warning C4(?!100)" | Select-Object -First 30
}
$rep = Join-Path $env:TEMP "s3d_smoke.txt"
Remove-Item $rep, $Out -ErrorAction SilentlyContinue
$args = @("--smoke-quit", "--smoke-report", "`"$rep`"", "--smoke-image", "`"$Out`"")
if ($Cam) { $args += @("--cam", $Cam) }
$p = Start-Process build/Release/strata-3d.exe -ArgumentList $args -WorkingDirectory build/Release -Wait -PassThru
"exit=$($p.ExitCode)"
if (Test-Path $rep) { Get-Content $rep -Encoding UTF8 }
