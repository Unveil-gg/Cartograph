# Setup script for Cartograph third-party dependencies (Windows PowerShell)
# Run from project root: .\scripts\setup_deps.ps1

$ErrorActionPreference = "Stop"

Write-Host "Setting up Cartograph dependencies..." -ForegroundColor Cyan

# Navigate to third_party directory
$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptPath
$thirdPartyPath = Join-Path $projectRoot "third_party"

if (-not (Test-Path $thirdPartyPath)) {
    New-Item -ItemType Directory -Path $thirdPartyPath | Out-Null
}
Set-Location $thirdPartyPath

# SDL3
if (-not (Test-Path "SDL")) {
    Write-Host "Cloning SDL3..." -ForegroundColor Yellow
    git clone --branch main --depth 1 https://github.com/libsdl-org/SDL.git SDL
} else {
    Write-Host "SDL already exists, skipping..." -ForegroundColor Green
}

# ImGui (docking branch)
if (-not (Test-Path "imgui")) {
    Write-Host "Cloning Dear ImGui (docking branch)..." -ForegroundColor Yellow
    git clone --branch docking --depth 1 https://github.com/ocornut/imgui.git imgui
} else {
    Write-Host "ImGui already exists, skipping..." -ForegroundColor Green
}

# nlohmann/json
if (-not (Test-Path "nlohmann/json.hpp")) {
    Write-Host "Downloading nlohmann/json..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path "nlohmann" -Force | Out-Null
    Invoke-WebRequest -Uri "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp" `
        -OutFile "nlohmann/json.hpp"
} else {
    Write-Host "nlohmann/json already exists, skipping..." -ForegroundColor Green
}

# stb libraries
if (-not (Test-Path "stb/stb_image.h")) {
    Write-Host "Downloading stb libraries..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path "stb" -Force | Out-Null
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" `
        -OutFile "stb/stb_image.h"
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h" `
        -OutFile "stb/stb_image_write.h"
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/master/stb_image_resize2.h" `
        -OutFile "stb/stb_image_resize2.h"
} else {
    Write-Host "stb libraries already exist, skipping..." -ForegroundColor Green
}

# minizip-ng
if (-not (Test-Path "minizip-ng")) {
    Write-Host "Cloning minizip-ng..." -ForegroundColor Yellow
    git clone https://github.com/zlib-ng/minizip-ng.git minizip-ng
} else {
    Write-Host "minizip-ng already exists, skipping..." -ForegroundColor Green
}

# NanoSVG (optional)
if (-not (Test-Path "nanosvg/nanosvg.h")) {
    Write-Host "Downloading NanoSVG (optional)..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path "nanosvg" -Force | Out-Null
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvg.h" `
        -OutFile "nanosvg/nanosvg.h"
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvgrast.h" `
        -OutFile "nanosvg/nanosvgrast.h"
} else {
    Write-Host "NanoSVG already exists, skipping..." -ForegroundColor Green
}

# GLAD (OpenGL loader for cross-platform GL 3.3 Core)
if (-not (Test-Path "glad")) {
    Write-Host "Cloning GLAD..." -ForegroundColor Yellow
    git clone --depth 1 https://github.com/Dav1dde/glad.git glad
    Write-Host "Generating GLAD for OpenGL 3.3 Core..." -ForegroundColor Yellow
    Set-Location glad
    python -m glad --api gl:core=3.3 --out-path build c
    Set-Location ..
} else {
    Write-Host "GLAD already exists, skipping..." -ForegroundColor Green
}

Set-Location $projectRoot

Write-Host ""
Write-Host "Dependencies setup complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Create build directory: mkdir build; cd build"
Write-Host "2. Configure: cmake .. -G `"Visual Studio 17 2022`""
Write-Host "   Or for Ninja: cmake .. -G Ninja"
Write-Host "3. Build: cmake --build . --config Release"
Write-Host "4. Run: .\Release\Cartograph.exe"
Write-Host ""
Write-Host "Note: All dependencies are vendored, no system packages needed!" -ForegroundColor Yellow
