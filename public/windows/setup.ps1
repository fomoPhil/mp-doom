# SPDX-License-Identifier: GPL-2.0-or-later
param([string] $Installer)

$ErrorActionPreference = 'Stop'
$interactive = -not $PSBoundParameters.ContainsKey('Installer')
$work = $null

function Assert-Hash($Path, $Expected, $Message) {
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Expected) {
        throw $Message
    }
}

try {
    $release = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'payload.json') -Raw | ConvertFrom-Json
    $root = Split-Path -Parent $PSScriptRoot
    $output = Join-Path $root 'MPC-DOOM-SD'
    if (Test-Path -LiteralPath $output) {
        throw 'MPC-DOOM-SD already exists. Move that folder somewhere safe before running setup again.'
    }
    if ($interactive) {
        Add-Type -AssemblyName System.Windows.Forms
        $picker = New-Object System.Windows.Forms.OpenFileDialog
        try {
            $picker.Title = 'Select your GOG Ultimate DOOM offline installer'
            $picker.Filter = 'Windows installer (*.exe)|*.exe'
            $picker.InitialDirectory = Join-Path $env:USERPROFILE 'Downloads'
            $picker.FileName = $release.installer_name
            if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
                Write-Host 'Setup canceled. No files were created.'
                exit 0
            }
            $Installer = $picker.FileName
        } finally {
            $picker.Dispose()
        }
    }
    if (-not $Installer -or -not (Test-Path -LiteralPath $Installer -PathType Leaf)) {
        throw 'Select the GOG offline installer you downloaded.'
    }
    $installerFile = Get-Item -LiteralPath $Installer
    $wrongInstaller = 'This is not the supported GOG offline installer. Choose ' + $release.installer_name + '.'
    if ($installerFile.Length -ne $release.installer_bytes) { throw $wrongInstaller }

    Write-Host 'Checking your GOG installer and preparing the SD card folder...'
    $extractor = Join-Path $PSScriptRoot 'innoextract\innoextract.exe'
    Assert-Hash $extractor $release.extractor_sha256 'The included extractor did not pass its file check. Extract a fresh copy of the full setup ZIP.'
    $workPath = Join-Path $root ('.mp-doom-setup-' + [guid]::NewGuid().ToString('N'))
    $work = (New-Item -Path $workPath -ItemType Directory).FullName
    $inputCopy = Join-Path $work 'installer.exe'
    [IO.File]::Copy($installerFile.FullName, $inputCopy)
    Assert-Hash $inputCopy $release.installer_sha256 $wrongInstaller
    Push-Location -LiteralPath $work
    try {
        & $extractor --extract --silent --include DOOM.WAD --output-dir extracted -- installer.exe
        if ($LASTEXITCODE -ne 0) { throw 'The extractor could not read the GOG installer.' }
    } finally {
        Pop-Location
    }
    $staged = Join-Path $work 'sd'
    $autorun = Join-Path $staged 'autorun'
    [void][IO.Directory]::CreateDirectory($autorun)
    foreach ($file in $release.autorun.PSObject.Properties) {
        $source = Join-Path (Join-Path $PSScriptRoot 'autorun') $file.Name
        $target = Join-Path $autorun $file.Name
        [IO.File]::Copy($source, $target)
        Assert-Hash $target $file.Value 'A setup file did not pass its file check. Extract a fresh copy of the full setup ZIP.'
    }
    $wad = Join-Path $autorun 'doom.wad'
    [IO.File]::Copy((Join-Path $work 'extracted\DOOM.WAD'), $wad)
    Assert-Hash $wad $release.wad_sha256 'The extracted DOOM.WAD does not match the supported version.'
    # Directory.Move fails if the destination already exists, including a concurrent run.
    [IO.Directory]::Move($staged, $output)
    Write-Host "Ready: $output"
    Write-Host 'Copy the autorun folder inside MPC-DOOM-SD to the root of your MPC Sample SD card.'
    Write-Host 'Follow the included README for SD card and backup instructions.'
} catch {
    Write-Host ('Setup stopped: ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
} finally {
    if ($work) { Remove-Item -LiteralPath $work -Recurse -Force }
}
