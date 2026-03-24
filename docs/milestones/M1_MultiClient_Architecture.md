# Milestone 1: Multi-Client Architecture
*Status: In Progress*

## Objectives
- Transition the launcher from a single-game updater to a multi-client platform.
- Support "Classic Client" and "Interlude Client" as independent entities.
- Ensure each client maintains its own state, installation path, and versioning.

## Requirements
- Launcher UI must show two separate blocks: Classic and Interlude.
- Each client must be an independent installable game.
- The updater must support installing one or both clients.

## Technical Model
- Introduce `GameClient` struct:
  - `id` (e.g., "classic", "interlude")
  - `name` (e.g., "Classic Client")
  - `install_path`
  - `version`
  - `state` (NOT_INSTALLED, INSTALLED, UPDATE_AVAILABLE)

## Configuration Changes
- `config.json` will store a map/dictionary of clients indexed by `id`.
