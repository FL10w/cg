# Native Windows launcher. Forward all arguments to the application.
$candidates = @(
    'build-msvc-debug/Debug/cg-lab-03.exe'
    'build-mingw-debug/cg-lab-03.exe'
    'build-msvc-release/Release/cg-lab-03.exe'
    'build-mingw-release/cg-lab-03.exe'
    'build-windows/Debug/cg-lab-03.exe'
    'build-windows/Release/cg-lab-03.exe'
    'build-debug/cg-lab-03.exe'
    'build-release/cg-lab-03.exe'
    'build-windows64-check/cg-lab-03.exe'
)

$executable = $null
foreach ($relativePath in $candidates) {
    $candidate = Join-Path $PSScriptRoot $relativePath
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $executable = $candidate
        break
    }
}

if (-not $executable) {
    Write-Error 'Windows executable not found. Build the Windows version as described in README.md.'
    exit 1
}

& $executable @args
exit $LASTEXITCODE
