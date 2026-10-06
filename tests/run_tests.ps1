$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $PSScriptRoot "navigation_unit.exe"
g++ -std=c++20 -O2 -Wall -Wextra -pedantic `
    (Join-Path $PSScriptRoot "navigation_unit.cpp") `
    (Join-Path $root "src\core\memory.cpp") `
    (Join-Path $root "src\nav\transition.cpp") `
    (Join-Path $root "src\nav\sim.cpp") `
    -o $out
& $out
if ($LASTEXITCODE -ne 0) { throw "navigation unit test failed" }
Remove-Item $out -Force
