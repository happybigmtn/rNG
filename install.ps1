# RNG Installer for Windows
# Run in PowerShell: irm https://raw.githubusercontent.com/happybigmtn/rng/main/install.ps1 | iex

$ErrorActionPreference = "Stop"

$Version = if ($env:RNG_VERSION) { $env:RNG_VERSION } else { "" }
$SourceRef = if ($env:RNG_SOURCE_REF) { $env:RNG_SOURCE_REF } else { "main" }
$InstallDir = if ($env:RNG_INSTALL_DIR) { $env:RNG_INSTALL_DIR } else { "$env:LOCALAPPDATA\RNG" }
$DataDir = if ($env:RNG_DATA_DIR) { $env:RNG_DATA_DIR } else { "$env:APPDATA\RNG" }
$Repo = "happybigmtn/rng"
$GithubUrl = "https://github.com/$Repo"

function Write-Info { Write-Host "[INFO] $args" -ForegroundColor Blue }
function Write-Success { Write-Host "[OK] $args" -ForegroundColor Green }
function Write-Warn { Write-Host "[WARN] $args" -ForegroundColor Yellow }
function Write-Err { Write-Host "[ERROR] $args" -ForegroundColor Red; exit 1 }

Write-Host ""
Write-Host "╔══════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  RNG Installer for Windows              ║" -ForegroundColor Cyan
Write-Host "║  Use WSL for the live RNG network       ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check architecture
$Arch = if ([Environment]::Is64BitOperatingSystem) { "x86_64" } else { Write-Err "32-bit Windows not supported" }
Write-Info "Detected: Windows $Arch"

# Note: Native Windows builds require MSYS2/MinGW or WSL
# For now, recommend WSL for Windows users
Write-Warn "Native Windows binaries are not yet available for the live network."
Write-Host ""
Write-Host "Recommended options for Windows:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  Option 1: Use WSL (Windows Subsystem for Linux)" -ForegroundColor White
Write-Host "    wsl --install" -ForegroundColor Gray
Write-Host "    wsl" -ForegroundColor Gray
Write-Host "    git clone https://github.com/$Repo.git" -ForegroundColor Gray
Write-Host "    cd rng" -ForegroundColor Gray
Write-Host "    bash install.sh --add-path --bootstrap" -ForegroundColor Gray
Write-Host "    rng-start-miner" -ForegroundColor Gray
Write-Host ""
Write-Host "  Option 2: Use Docker Desktop from the repo checkout" -ForegroundColor White
Write-Host "    git clone https://github.com/$Repo.git" -ForegroundColor Gray
Write-Host "    cd rng" -ForegroundColor Gray
Write-Host "    docker compose up -d --build" -ForegroundColor Gray
Write-Host ""
Write-Host "  Option 3: Build with MSYS2 (advanced)" -ForegroundColor White
Write-Host "    # Install MSYS2 from https://www.msys2.org/" -ForegroundColor Gray
Write-Host "    # Then build the $SourceRef branch from source" -ForegroundColor Gray
Write-Host ""

# Offer to install WSL
$InstallWSL = Read-Host "Would you like to install WSL now? (y/n)"
if ($InstallWSL -eq 'y' -or $InstallWSL -eq 'Y') {
    Write-Info "Installing WSL..."
    wsl --install
    Write-Host ""
    Write-Success "WSL installed! Restart your computer, then run:"
    Write-Host "  wsl" -ForegroundColor Cyan
    Write-Host "  git clone https://github.com/$Repo.git" -ForegroundColor Cyan
    Write-Host "  cd rng" -ForegroundColor Cyan
    Write-Host "  bash install.sh --add-path --bootstrap" -ForegroundColor Cyan
    Write-Host "  rng-start-miner" -ForegroundColor Cyan
} else {
    Write-Host "Visit $GithubUrl for more options." -ForegroundColor Gray
}
