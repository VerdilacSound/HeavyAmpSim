[CmdletBinding()]
param(
    [string]$Generator = 'Visual Studio 17 2022',
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [switch]$CaptureInterface
)

$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') {
    throw 'Run this script on Windows with Visual Studio C++ tools, CMake and Git installed.'
}
Get-Command cmake -ErrorAction Stop | Out-Null
Get-Command ctest -ErrorAction Stop | Out-Null
Get-Command git -ErrorAction Stop | Out-Null

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuildRoot = Join-Path $RepoRoot 'build-win64'
$Bundle = Join-Path $BuildRoot "HeavyAmpSim_artefacts/$Configuration/VST3/HeavyAmpSim.vst3"
$Binary = Join-Path $Bundle 'Contents/x86_64-win/HeavyAmpSim.vst3'

$UiTools = if ($CaptureInterface) { 'ON' } else { 'OFF' }
& cmake -S $RepoRoot -B $BuildRoot -G $Generator -A x64 "-DHEAVY_BUILD_UI_TOOLS=$UiTools"
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build $BuildRoot --config $Configuration --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Windows build failed.' }
& ctest --test-dir $BuildRoot -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Windows tests failed; no package produced.' }

if (-not (Test-Path -LiteralPath $Binary -PathType Leaf)) {
    throw "Windows VST3 binary missing: $Binary"
}
$Bytes = [System.IO.File]::ReadAllBytes($Binary)
if ($Bytes.Length -lt 64 -or $Bytes[0] -ne 0x4d -or $Bytes[1] -ne 0x5a) {
    throw 'Output is not a Windows PE binary.'
}
$PeOffset = [BitConverter]::ToInt32($Bytes, 0x3c)
if ($PeOffset -lt 0 -or $PeOffset + 6 -gt $Bytes.Length -or
    [BitConverter]::ToUInt32($Bytes, $PeOffset) -ne 0x4550 -or
    [BitConverter]::ToUInt16($Bytes, $PeOffset + 4) -ne 0x8664) {
    throw 'Output is not a valid Windows x86_64 binary.'
}

$PackageRoot = Join-Path $RepoRoot 'artifacts'
New-Item -ItemType Directory -Path $PackageRoot -Force | Out-Null
$Stage = Join-Path $BuildRoot 'package-stage'
if (Test-Path -LiteralPath $Stage) { Remove-Item -LiteralPath $Stage -Recurse -Force }
New-Item -ItemType Directory -Path $Stage | Out-Null
Copy-Item -LiteralPath $Bundle -Destination $Stage -Recurse
foreach ($File in @('README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $RepoRoot $File) -Destination $Stage
}
Copy-Item -LiteralPath (Join-Path $RepoRoot 'docs/RELEASE_NOTES.md') -Destination $Stage
if ($CaptureInterface) {
    $Capture = Join-Path $BuildRoot 'ui-validation.png'
    if (-not (Test-Path -LiteralPath $Capture)) { throw 'Actual GUI capture was not produced by the UI test.' }
    Copy-Item -LiteralPath $Capture -Destination (Join-Path $Stage 'Interface.png')
    Copy-Item -LiteralPath $Capture -Destination (Join-Path $PackageRoot 'HeavyAmpSim-Interface.png')
}

$Cache = Get-Content -LiteralPath (Join-Path $BuildRoot 'CMakeCache.txt')
$JuceEntry = $Cache | Select-String -Pattern '^JUCE_SOURCE_DIR:[^=]+=(.+)$' | Select-Object -First 1
if ($null -eq $JuceEntry) { throw 'Could not locate JUCE sources for source and license packaging.' }
$JuceRoot = $JuceEntry.Matches[0].Groups[1].Value
$ExpectedJuce = '51a8a6d7aeae7326956d747737ccf1575e61e209'
$ActualJuce = & git -C $JuceRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $ActualJuce.Trim() -ne $ExpectedJuce) {
    throw 'JUCE checkout does not match the pinned release source.'
}
$Notices = Join-Path $Stage 'third-party-licenses'
New-Item -ItemType Directory -Path $Notices | Out-Null
Copy-Item -LiteralPath (Join-Path $JuceRoot 'LICENSE.md') -Destination (Join-Path $Notices 'JUCE-LICENSE.md')
Get-ChildItem -LiteralPath (Join-Path $JuceRoot 'modules') -Recurse -File |
    Where-Object { $_.Name -match '(?i)(licen[cs]e|copying)' -and $_.Extension -ne '.pdf' } |
    ForEach-Object {
        $Relative = $_.FullName.Substring($JuceRoot.Length).TrimStart([char[]]'\/')
        $Destination = Join-Path $Notices $Relative
        New-Item -ItemType Directory -Path (Split-Path $Destination) -Force | Out-Null
        Copy-Item -LiteralPath $_.FullName -Destination $Destination
    }
@'
HeavyAmpSim 0.1.0 alpha / Windows x64 VST3 prototype

Copy the entire HeavyAmpSim.vst3 folder into:
  C:\Program Files\Common Files\VST3
Then rescan plugins in your DAW and insert on a mono guitar track with stereo output.
Only lane T has audible level by default; raise other lane levels to blend them.

Amp/pedal voicings and cabinets are provisional. This is not a circuit-validated
or finished mix-ready product. See RELEASE_NOTES.md for limitations.

AGPLv3 license and third-party notices are included. Corresponding application
and JUCE source archives are attached to the same GitHub prerelease:
  https://github.com/VerdilacSound/HeavyAmpSim/releases
'@ | Set-Content -LiteralPath (Join-Path $Stage 'INSTALL.txt') -Encoding utf8

$Zip = Join-Path $PackageRoot "HeavyAmpSim-Windows-x64-$Configuration-VST3.zip"
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path -LiteralPath $Zip) { Remove-Item -LiteralPath $Zip }
[System.IO.Compression.ZipFile]::CreateFromDirectory($Stage, $Zip)
$AppSource = Join-Path $PackageRoot 'HeavyAmpSim-Source.zip'
$JuceSource = Join-Path $PackageRoot 'JUCE-8.0.6-Source.zip'
& git -C $RepoRoot archive --format=zip --output=$AppSource HEAD
if ($LASTEXITCODE -ne 0) { throw 'Application source archive failed.' }
& git -C $JuceRoot archive --format=zip --output=$JuceSource HEAD
if ($LASTEXITCODE -ne 0) { throw 'JUCE source archive failed.' }
$Sums = foreach ($File in @($Zip, $AppSource, $JuceSource)) {
    $Hash = Get-FileHash -LiteralPath $File -Algorithm SHA256
    '{0}  {1}' -f $Hash.Hash.ToLowerInvariant(), [System.IO.Path]::GetFileName($File)
}
$Sums | Set-Content -LiteralPath (Join-Path $PackageRoot 'SHA256SUMS.txt') -Encoding ascii
Write-Host "Windows VST3 package: $Zip"
Get-FileHash -LiteralPath $Zip -Algorithm SHA256 | Format-List
Write-Host 'Extract the entire HeavyAmpSim.vst3 folder into C:\Program Files\Common Files\VST3, then rescan your DAW.'
