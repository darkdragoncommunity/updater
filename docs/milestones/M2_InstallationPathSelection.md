# Milestone 2: Installation Path Selection
*Status: Completed*

## Objectives
- Allow users to select where each client is installed.
- Support different paths for different clients.

## Requirements
- User can choose the destination directory during installation.
- Paths must be saved locally in `config.json`.
- Paths must persist between launcher restarts.
- Paths must be editable only if the client is not yet installed.

## Implementation Detail
- UI: Added an `ImGui::InputText` field for the installation path in the client block.
- Logic: `Config::SetClientInstallPath` updates the `GameClient` and persists it to `config.json`.
- Restriction: The path input is set to `ImGuiInputTextFlags_ReadOnly` once the client's state moves beyond `NOT_INSTALLED`.
