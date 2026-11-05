# SPDX-License-Identifier: GPL-3.0-or-later
# Build the host code (core + simulator) and run unit and simulator tests.
# Usage: .\scripts\test.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build-host"
cmake -S $root -B $build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $build --config Debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir $build -C Debug -L unit --output-on-failure --no-tests=error
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir $build -C Debug -L sim --output-on-failure --no-tests=error
exit $LASTEXITCODE
