# Milestone 5: Update Check on Launcher Start
*Status: Completed*

## Objectives
- Automatically check for updates when the launcher starts.

## Requirements
- When launcher opens: `check_for_updates()`.
- If updates are available:
  - The "Run" button must NOT appear.
  - Only the "Update" button is available.
- If no updates exist:
  - Show the "Run" button.

## Implementation Detail
- `LauncherUI` constructor now spawns background threads for each client to call `updater->CheckForUpdatesSync()`.
- `RenderClientBlock` logic updated:
  - If `client->state == UPDATE_AVAILABLE`, the primary button displays "Update" (fetched from language file) and triggers the update process.
  - The "Play" action is only accessible if the client is in `READY` or `INSTALLED` state and no update is available.
  - Added a "Updating..." visual state for the button when the updater is active.
