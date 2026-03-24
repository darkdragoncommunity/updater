# Milestone 9: Final System Validation & Architecture Summary
*Status: Completed*

## Final Architecture Summary

### 1. Unified Multi-Client Management
- The launcher supports an arbitrary number of game clients defined in `config.json`.
- Each client is treated as an independent lifecycle (Not Installed -> Updating -> Ready).
- `GameClient` struct stores the state, installation path, version, and the `runPath` fetched from the manifest.

### 2. High-Performance Update System
- **Layered Deployment**: Supports pre-compressed `.gz` files with in-memory decompression via `miniz`.
- **Git-Sync Integration**: `deploy_tool.py` manages updates by detecting Git changes and only uploading deltas to Cloudflare R2.
- **Fast-Scanning**: Initial checks use file size to ensure near-instant startup.
- **Action Handling**: Native support for file updates and deletions (`action: delete`).
- **File Exceptions**: Critical user files can be protected from overwriting via `skip_if_exists`.

### 3. User Experience & UI
- **ImGui + GLFW**: Lightweight and high-performance UI using OpenGL 3.0.
- **Localization**: External JSON-based language files with dynamic, over-the-air switching.
- **Visuals**: Full-screen background support and semi-transparent elements.
- **Security**: Hardcoded base URLs are encrypted in `settings.bin` via XOR + Hex.

## Validated Scenarios
- [x] Fresh installation of Classic and Interlude clients.
- [x] Running the game executable using the resolved remote `run_path`.
- [x] Auto-checking for updates after 5 minutes of run-time.
- [x] Auto-checking for updates on launcher startup.
- [x] Repairing a corrupted client via the options menu.
- [x] Uninstalling a client, ensuring all files are removed and records reset.
- [x] Dynamic language switching and background downloading of JSON language files.
- [x] File exceptions (`skip_if_exists`) respected during updates.
