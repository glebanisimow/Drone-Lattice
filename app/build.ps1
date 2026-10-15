# Build Drone Lattice (standalone exe + VST3) and its installer.
#   .\app\build.ps1              # Release build + dist\DroneLattice-Setup-<ver>.exe
#   .\app\build.ps1 -NoInstaller
# Needs: VS 2022 C++ Build Tools, CMake, Faust and JUCE in %LOCALAPPDATA%\drone-tools.
param([switch]$NoInstaller)

# Native tools write warnings to stderr; failures are checked via $LASTEXITCODE.
$ErrorActionPreference = "Continue"
$app = $PSScriptRoot
$root = Split-Path $app -Parent
$build = Join-Path $root "build\app"

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) { $cmake = Join-Path $env:ProgramFiles "CMake\bin\cmake.exe" }
if (-not (Test-Path $cmake)) { throw "CMake not found" }

$apps = @(@{ Target = "DroneLattice"; Name = "Drone Lattice"; Iss = "DroneLattice.iss"; Icon = "icon.png"; Tool = "make_icon.py" })

foreach ($a in $apps) {
    $icon = Join-Path $app "assets\$($a.Icon)"
    if (-not (Test-Path $icon)) {
        & (Join-Path $root ".venv\Scripts\python.exe") (Join-Path $app "tools\$($a.Tool)") $icon
    }
}

& $cmake -S $app -B $build -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
& $cmake --build $build --config Release --parallel -- /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "build failed" }

foreach ($a in $apps) {
    $art = Join-Path $build "$($a.Target)_artefacts\Release"
    if ($a.NoPlugin) { Write-Output "exe : $art\$($a.Name).exe"; continue }
    Write-Output "exe : $art\Standalone\$($a.Name).exe"
    Write-Output "vst3: $art\VST3\$($a.Name).vst3"
}

if (-not $NoInstaller) {
    $iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
              "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $iscc) { throw "Inno Setup 6 (ISCC.exe) not found" }
    foreach ($a in $apps) {
        $art = Join-Path $build "$($a.Target)_artefacts\Release"
        $ico = Join-Path $build "$($a.Target)_artefacts\JuceLibraryCode\icon.ico"
        & $iscc /Q "/DBuildDir=$art" "/DIconFile=$ico" "/DOutDir=$(Join-Path $root 'dist')" (Join-Path $app "installer\$($a.Iss)")
        if ($LASTEXITCODE -ne 0) { throw "installer build failed: $($a.Iss)" }
    }
    Get-ChildItem (Join-Path $root "dist") -Filter *.exe | ForEach-Object { Write-Output "setup: $($_.FullName)" }
}
