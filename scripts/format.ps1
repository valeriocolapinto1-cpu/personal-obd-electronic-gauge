# SPDX-License-Identifier: GPL-3.0-or-later
# Format all C/C++ sources in place (Windows PowerShell).
# Usage: .\scripts\format.ps1          (fix)
#        .\scripts\format.ps1 -Check   (check only, like CI)
param([switch]$Check)
$root = Split-Path -Parent $PSScriptRoot
if ($Check) {
    python "$root\scripts\check_format.py"
} else {
    python "$root\scripts\check_format.py" --fix
}
exit $LASTEXITCODE
