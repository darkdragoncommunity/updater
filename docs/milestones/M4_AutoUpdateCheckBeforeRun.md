# Milestone 4: Auto Update Check Before Run
*Status: Completed*

## Objectives
- Prevent launching outdated clients.
- Ensure clients are checked for updates before being allowed to run.

## Requirements
- Track the timestamp of the last "Run" click.
- Logic: `if current_time - last_run_click > 5 minutes: check_for_updates()`.
- If an update exists:
  - Start updating and do NOT launch the game.
- If no update exists:
  - Launch the game normally.

## Implementation Detail
- `GameClient` struct updated to include `lastRunCheck` (long long unix timestamp).
- `Updater` refactored to include `CheckForUpdatesSync()` which performs a synchronous manifest check.
- `LauncherUI` in `RenderClientBlock` now checks the 5-minute delta (300 seconds) before launching.
- If a check is triggered and finds updates, the `Updater` starts and launching is bypassed.
- If no update is found, `lastRunCheck` is updated and the game is launched.
