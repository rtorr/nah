# NAH Installation Script for Windows
# Usage: irm https://raw.githubusercontent.com/rtorr/nah/main/install.ps1 | iex

$ErrorActionPreference = "Stop"

$Repo = "rtorr/nah"
$InstallDir = if ($env:NAH_INSTALL_DIR) { $env:NAH_INSTALL_DIR } else { "$env:LOCALAPPDATA\nah\bin" }

function Get-Platform {
    $arch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture
    switch ($arch) {
        "X64" { return "windows-x64" }
        "Arm64" { return "windows-arm64" }
        default {
            Write-Error "Unsupported architecture: $arch"
            exit 1
        }
    }
}

function Get-LatestVersion {
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/latest"
    return $release.tag_name
}

function Install-Nah {
    $platform = Get-Platform
    $version = Get-LatestVersion

    $archive = "nah-$platform.zip"
    $url = "https://github.com/$Repo/releases/download/$version/$archive"

    Write-Host "Installing NAH $version for $platform..."

    # Create install directory
    if (!(Test-Path $InstallDir)) {
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    }

    # Download
    $tempFile = Join-Path $env:TEMP $archive
    Write-Host "Downloading $url..."
    Invoke-WebRequest -Uri $url -OutFile $tempFile

    # Extract
    Write-Host "Extracting..."
    Expand-Archive -Path $tempFile -DestinationPath $InstallDir -Force

    # Cleanup
    Remove-Item $tempFile -Force

    # Add to PATH if not already there
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($userPath -notlike "*$InstallDir*") {
        Write-Host "Adding $InstallDir to PATH..."
        [Environment]::SetEnvironmentVariable("Path", "$userPath;$InstallDir", "User")
        $env:Path = "$env:Path;$InstallDir"
    }

    Write-Host ""
    Write-Host "NAH $version installed successfully!"
    Write-Host ""
    Write-Host "Restart your terminal, then run 'nah --help' to get started."
}

Install-Nah
