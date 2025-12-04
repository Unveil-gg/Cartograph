# Building Cartograph

This guide covers building Cartograph from source for development and distribution.

## Prerequisites

### All Platforms
- CMake 3.16+
- C++17 compiler
- Git (for submodules)

### macOS
- Xcode Command Line Tools or Xcode
- macOS 10.15+ (Catalina or later)

### Windows
- Visual Studio 2019+ with C++ workload
- Windows 10+

### Linux
- GCC 9+ or Clang 10+
- OpenGL development libraries
- SDL3 (installed via package manager or built from source)

## Quick Build

### macOS / Linux

```bash
git clone https://github.com/unveil/cartograph.git
cd cartograph
git submodule update --init --recursive

mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run (macOS)
./Cartograph.app/Contents/MacOS/Cartograph

# Run (Linux)
./Cartograph
```

### Windows

```bash
git clone https://github.com/unveil/cartograph.git
cd cartograph
git submodule update --init --recursive

mkdir build && cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release

# Run
.\Release\Cartograph.exe
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_WEB` | OFF | Build for Web (Emscripten) |
| `USE_STATIC_CRT` | OFF | Use static CRT on Windows |
| `USE_SVG_ICONS` | OFF | Enable SVG icon support |

Example:
```bash
cmake -DUSE_STATIC_CRT=ON ..
```

---

## macOS Distribution

For distributing Cartograph outside the App Store, you need to:
1. Sign the app with a Developer ID certificate
2. Notarize it with Apple

### Prerequisites

1. **Apple Developer Account** ($99/year at developer.apple.com)
2. **Developer ID Application certificate** (create in Xcode or developer portal)
3. **App-specific password** for notarization (create at appleid.apple.com)

### Code Signing

The signing script auto-detects your Developer ID certificate:

```bash
# After building
./scripts/codesign.sh build/Cartograph.app
```

Or specify the identity explicitly:
```bash
CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
  ./scripts/codesign.sh build/Cartograph.app
```

### Notarization

After signing, notarize for Gatekeeper approval:

```bash
APPLE_ID="your@email.com" \
APPLE_TEAM_ID="XXXXXXXXXX" \
APPLE_APP_PASSWORD="xxxx-xxxx-xxxx-xxxx" \
  ./scripts/notarize.sh build/Cartograph.app
```

This process:
1. Uploads the app to Apple's notarization service
2. Waits for approval (typically 1-5 minutes)
3. Staples the notarization ticket to the app

### Distribution Build (One Command)

For CI/CD or scripted builds:

```bash
# Set credentials
export APPLE_ID="your@email.com"
export APPLE_TEAM_ID="XXXXXXXXXX"
export APPLE_APP_PASSWORD="xxxx-xxxx-xxxx-xxxx"
export CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"

# Build, sign, and notarize
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCODESIGN_IDENTITY="$CODESIGN_IDENTITY" \
      -DDEVELOPMENT_TEAM="$APPLE_TEAM_ID" ..
make -j$(nproc)
cd ..
./scripts/codesign.sh build/Cartograph.app
./scripts/notarize.sh build/Cartograph.app
```

---

## Windows Distribution

Windows builds include version information displayed in file properties.

### Building for Distribution

```bash
cmake -G "Visual Studio 16 2019" -A x64 -DUSE_STATIC_CRT=ON ..
cmake --build . --config Release
```

The `USE_STATIC_CRT` option embeds the C runtime, avoiding dependency on Visual C++ Redistributable.

### Version Information

Version info is defined in `src/platform/windows/app.rc`. Update the version numbers when releasing:

```c
#define VER_MAJOR 1
#define VER_MINOR 0
#define VER_PATCH 0
```

Keep this synchronized with `CMakeLists.txt`:
```cmake
project(Cartograph VERSION 1.0.0 ...)
```

---

## Troubleshooting

### macOS: "App is damaged" error
The app needs to be signed and notarized. For development:
```bash
xattr -cr Cartograph.app  # Remove quarantine attribute
```

### macOS: Signing fails with "no identity found"
Install your Developer ID certificate:
1. Download from developer.apple.com
2. Double-click to install in Keychain
3. Or use Xcode > Preferences > Accounts > Download Manual Profiles

### Windows: Missing DLLs
Build with static CRT to avoid runtime dependencies:
```bash
cmake -DUSE_STATIC_CRT=ON ..
```

### Linux: OpenGL errors
Ensure OpenGL development packages are installed:
```bash
# Ubuntu/Debian
sudo apt install libgl1-mesa-dev

# Fedora
sudo dnf install mesa-libGL-devel
```

