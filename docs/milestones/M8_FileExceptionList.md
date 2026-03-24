# Milestone 8: File Exception List
*Status: Completed*

## Objectives
- Protect certain files from being overwritten during updates.

## Requirements
- Create a file exception list.
- During updates: `if file in exception_list AND file already exists: skip`.
- During first installation: Download everything normally.
- Example exceptions: `system/user.ini`, `system/options.ini`, `system/custom.cfg`.

## Implementation Detail
- Manifest Metadata: Added `skip_if_exists` boolean to the `ManifestFile` entry in `manifest.json`.
- Skip Logic: In `Updater::CheckForUpdatesSync`, if a file is marked `skip_if_exists` AND it exists locally AND the client state is NOT `NOT_INSTALLED`, the file is excluded from the update queue.
- This ensures that configuration files (like `user.ini`) are downloaded on the first run but protected during subsequent patches.
- Overrides: Using the **Repair** button bypasses this skip logic, ensuring a full integrity restoration.
