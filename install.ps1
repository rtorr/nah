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
    $version = if ($env:VERSION) { $env:VERSION } else { Get-LatestVersion }

    $archive = "nah-$platform.zip"
    $url = "https://github.com/$Repo/releases/download/$version/$archive"
    $checksumUrl = "https://github.com/$Repo/releases/download/$version/SHA256SUMS"

    Write-Host "Installing NAH $version for $platform..."

    # Create install directory
    if (!(Test-Path $InstallDir)) {
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    }

    # Download
    $tempRoot = Join-Path $env:TEMP ([System.Guid]::NewGuid().ToString())
    New-Item -ItemType Directory -Path $tempRoot | Out-Null
    $tempFile = Join-Path $tempRoot $archive
    $checksumFile = Join-Path $tempRoot "SHA256SUMS"
    Write-Host "Downloading $url..."
    Invoke-WebRequest -Uri $url -OutFile $tempFile
    Invoke-WebRequest -Uri $checksumUrl -OutFile $checksumFile
    $line = Get-Content $checksumFile | Where-Object { $_ -match [regex]::Escape($archive) + '$' } | Select-Object -First 1
    if (!$line) { throw "Release checksum is missing for $archive" }
    $expected = ($line -split '\s+')[0].ToLowerInvariant()
    $actual = (Get-FileHash -Algorithm SHA256 $tempFile).Hash.ToLowerInvariant()
    if ($actual -ne $expected) { throw "Checksum verification failed" }

    # Extract
    Write-Host "Extracting..."
    Expand-Archive -Path $tempFile -DestinationPath $InstallDir -Force

    # Cleanup
    Remove-Item $tempRoot -Recurse -Force

    # Add to PATH if not already there
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($userPath -notlike "*$InstallDir*") {
        Write-Host "Adding $InstallDir to PATH..."
        [Environment]::SetEnvironmentVariable("Path", "$userPath;$InstallDir", "User")
        $env:Path = "$env:Path;$InstallDir"
    }

    Write-Host "NAH $version installed. Restart your terminal, then run 'nah --help'."
}

Install-Nah
