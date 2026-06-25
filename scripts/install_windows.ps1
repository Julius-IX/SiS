$ErrorActionPreference = "Stop"

$repo = "Julius-IX/SiS"
$zipName = "sis-windows-x86_x64.zip"

$installDir = Join-Path $env:LOCALAPPDATA "SiS"

$tempDir = Join-Path $env:TEMP "sis-install"
$tempZip = Join-Path $env:TEMP $zipName

Write-Host "Fetching latest release..."

# Get all releases (includes pre-releases), take the newest
$releases = Invoke-RestMethod `
    "https://api.github.com/repos/$repo/releases"

$release = $releases | Select-Object -First 1

# Find the asset
$asset = $release.assets |
    Where-Object { $_.name -eq $zipName } |
    Select-Object -First 1

if (-not $asset) {
    throw "Could not find release asset '$zipName'"
}

Write-Host "Downloading $zipName..."
Invoke-WebRequest `
    -Uri $asset.browser_download_url `
    -OutFile $tempZip

# Clean previous temp extraction
Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "Extracting..."
Expand-Archive `
    -Path $tempZip `
    -DestinationPath $tempDir

# Create install directory
New-Item `
    -ItemType Directory `
    -Force `
    -Path $installDir | Out-Null

Write-Host "Installing..."

Copy-Item "$tempDir\*" `
    -Destination $installDir `
    -Recurse `
    -Force

# Add to user PATH if needed
$currentPath = [Environment]::GetEnvironmentVariable(
    "Path",
    "User"
)

if ($currentPath -notlike "*$installDir*") {

    [Environment]::SetEnvironmentVariable(
        "Path",
        "$currentPath;$installDir",
        "User"
    )

    Write-Host "Added to PATH."
}

# Cleanup
Remove-Item $tempZip -Force -ErrorAction SilentlyContinue
Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Installation complete."
Write-Host "Restart your terminal and run:"
Write-Host "    sis"
