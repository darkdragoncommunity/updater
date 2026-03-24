# Game Launcher Maintenance & Development Guide

This guide provides instructions for maintainers and developers of the Open Source Game Launcher.

## 1. Project Overview

A high-performance C++ launcher for Windows games.

- **Networking**: `libcurl` (Multi-threaded, Resumable, Compressed).
- **Security**: XOR + Hex encryption for `settings.bin`.
- **UI**: ImGui + OpenGL 3.0.
- **Compression**: `miniz` (Decompressing .gz files in memory).

## 2. Core Workflows

### 2.1 Deployment

1.  **Configure**: Edit `launcher_settings.json` with your backend URLs.
2.  **Generate Settings**:
    ```bash
    python tools/encrypt_settings.py --config launcher_settings.json
    ```
3.  **Generate Manifest**: Use `tools/generate_manifest.py` for your game files.
4.  **Build & Deploy**: Use `tools/deploy_launcher.bat` (Windows) or `tools/deploy_launcher.sh` (Linux).

### 2.2 Security

- **Encryption Key**: You MUST change the default key in `src/config.h` and the Python tools for your production environment.
- **Code Signing**: Integrate with SignPath or another signing service to ensure user trust.

## 3. Customization

### UI and Assets

- Replace the icons in the `assets/` directory (e.g., `logo.png`, `app.ico`).
- Modify `src/ui.cpp` to adjust the layout and branding.
- Update `CMakeLists.txt` with your project metadata (Product name, Company name, etc.).

### Localization

- Add or update language files in the `languages/` directory.

## 4. Maintenance Notes

- **Dependencies**: Keep dependencies updated using `vcpkg`.
- **Security Patches**: Regularly review and update the networking and decompression logic.
