# Game Launcher (v1.0.x)

A lightweight, high-performance C++ Game Launcher designed for Windows.

## Features

- **Efficient Updates**: Multi-threaded, resumable downloads using `libcurl`.
- **Space Optimization**: Compressed updates (.gz) decompressed in memory using `miniz`.
- **Security**: Encrypted settings using XOR + Hex to protect backend URLs.
- **Modern UI**: Built with ImGui and OpenGL 3.0 for a responsive, customizable interface.
- **Multi-Client Support**: Manage and update multiple game clients from a single launcher.
- **Localization**: Support for multiple languages (EN, ES, PT, RU, CN).

## Getting Started

### Prerequisites

- **C++17 Compiler**: (e.g., MSVC on Windows).
- **CMake**: version 3.15 or higher.
- **Vcpkg**: (Recommended) for dependency management.
- **Dependencies**: `libcurl`, `glfw3`, `opengl32`, `nlohmann-json`.

### Build Instructions

1. **Clone the repository**:
   ```bash
   git clone https://github.com/your-repo/launcher.git
   cd launcher
   ```

2. **Setup Vcpkg**:
   ```bash
   vcpkg install curl glfw3 nlohmann-json
   ```

3. **Configure and Build**:
   ```bash
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```

## Configuration

### 1. Backend Settings

The launcher reads its initial configuration from an encrypted `settings.bin` file.

1.  Edit `launcher_settings.json` with your URLs.
2.  Generate the encrypted binary:
    ```bash
    python tools/encrypt_settings.py --config launcher_settings.json
    ```

**Important**: Change the default encryption key in `src/config.h` and `tools/encrypt_settings.py` before deployment.

### 2. Generating Manifests

Use the provided tool to generate the update manifest for your game files:
```bash
python tools/generate_manifest.py [game_dir] --compress --patch-dir [output_dir]
```

## Deployment

Refer to `tools/deploy_launcher.sh` (Linux) or `tools/deploy_launcher.bat` (Windows) for automated deployment workflows including:
- Version incrementing.
- Release building.
- Cloudflare R2 / S3 synchronization.

## Documentation

- [Contributing Guide](CONTRIBUTING.md)
- [Security Policy](SECURITY.md)
- [SignPath Integration](SIGNPATH.md)

## Code Signing Policy

Free code signing provided by [SignPath.io](https://signpath.io), certificate by SignPath Foundation.

### Team Roles
- **Committers and Reviewers**: [Maintainers Team](https://github.com/orgs/your-org/teams/maintainers)
- **Approvers**: [Owners](https://github.com/orgs/your-org/people?query=role%3Aowner)

## Privacy Policy

This program will not transfer any information to other networked systems unless specifically requested by the user or the person installing or operating it.

### Data Collection (Remote Debug)
If the "Remote Debug" feature is explicitly enabled in your configuration, the launcher may upload its local `launcher.log` file to a specified HTTP endpoint for diagnostic purposes. This log contains information about download progress, file hashing, and application state. No personal user data (names, emails, passwords) is collected or transmitted.

## Uninstallation

To uninstall the Game Launcher:
1. Delete the application folder.
2. Delete the configuration and log files located at `%LOCALAPPDATA%/GameLauncher/` (on Windows).

## License

This project is licensed under the [MIT License](LICENSE).
