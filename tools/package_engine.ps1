param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$OutputDirectory = "out/package/CocoaEngine",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$preset = if ($Configuration -eq "Debug") { "windows-debug" } else { "windows-release" }
$output = if ([System.IO.Path]::IsPathRooted($OutputDirectory)) {
    [System.IO.Path]::GetFullPath($OutputDirectory)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
}

Push-Location $root
try {
    if (-not $SkipBuild) {
        cmake --preset $preset
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
        cmake --build --preset $preset
        if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }
    }

    cmake --install "out/build/$preset" --prefix $output
    if ($LASTEXITCODE -ne 0) { throw "CMake install failed." }

    Write-Host "Cocoa Engine package created: $output"
    Write-Host "Start editor: $output\run-editor.cmd"
} finally {
    Pop-Location
}
