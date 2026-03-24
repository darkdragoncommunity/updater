# Milestone 6: Hidden Advanced Options Menu
*Status: Completed*

## Objectives
- Add additional actions but keep UI clean.
- Implement "Repair" and "Uninstall".

## Requirements
- Each client block must contain a "..." dropdown menu.
- Inside the menu:
  - Repair (re-download corrupted/missing files).
  - Uninstall (remove client files and record).

## Implementation Detail
- UI: Added a "..." button in each client block that opens an `ImGui::BeginPopup`.
- Dropdown Options:
  - "Repair": Triggers `updater->ForceRepair()`.
  - "Uninstall": Triggers `Config::UninstallClient(clientId)`.
- Uninstall Logic: `Config::UninstallClient` uses `std::filesystem::remove_all` to delete the installation directory and resets the client's state and version in `config.json`.
