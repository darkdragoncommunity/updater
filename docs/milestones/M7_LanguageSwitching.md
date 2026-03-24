# Milestone 7: Language Switching
*Status: Completed*

## Objectives
- Allow the user to change the launcher's language.
- Trigger filesystem actions on switching.

## Requirements
- Switch language and download new language files if missing.
- Example: `switch_language("es")` -> `download("lang/es.json")`.

## Implementation Detail
- `Language::SwitchLanguage` method added to the `Language` class.
- Download Logic: If a requested language file is missing from the local `languages/` directory, it is fetched from `[baseUrl]/languages/[lang].json` using a temporary `Downloader` instance.
- Persistence: Selecting a new language updates both the `Config` (for future runs) and the `Language` service (for the current run).
- Scan logic updated to ensure "en", "es", and "ru" are always in the available list to allow discovery of remote files.
