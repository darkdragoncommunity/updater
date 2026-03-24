# Milestone 3: Run Action
*Status: Completed*

## Objectives
- Add a "Run" button for installed games.
- Ensure executable paths are not hardcoded.

## Requirements
- Executable path must be fetched remotely from the manifest/version config.
- Example remote config: `classic.run_path = system/classic.exe`.
- Launcher logic: `run_path = install_path + remote_run_path`.

## Implementation Detail
- `Manifest` class updated to parse `run_path` from the JSON.
- `GameClient` struct updated to store `runPath`.
- `Updater` updates the `GameClient`'s `runPath` upon manifest parsing.
- `LauncherUI` uses the resolved absolute path to call `LauncherUtils::LaunchGame`.
