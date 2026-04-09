param(
    [Parameter(Mandatory = $true)]
    [string]$ClangFormat
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$extensions = @('*.h', '*.hpp', '*.c', '*.cpp')
$roots = @('Libraries', 'src', 'tests')

foreach ($root in $roots) {
    $fullRoot = Join-Path $repoRoot $root
    if (-not (Test-Path -LiteralPath $fullRoot)) {
        continue
    }

    Get-ChildItem -LiteralPath $fullRoot -Recurse -File -Include $extensions | ForEach-Object {
        $formatted = & $ClangFormat $_.FullName
        [System.IO.File]::WriteAllText($_.FullName, ($formatted -join "`n") + "`n", $utf8NoBom)
    }
}
