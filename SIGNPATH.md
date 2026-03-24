# SignPath Integration

This project is designed to be signed using [SignPath](https://signpath.io).

## Requirements

1. **SignPath Account**: You need a SignPath account and a project configured.
2. **Certificate**: A valid code signing certificate (EV recommended).
3. **CI/CD**: Integration with GitHub Actions, AppVeyor, or similar.

## Signing Configuration

To sign the resulting executable:

1. Build the project in **Release** mode.
2. Upload the artifact to SignPath via their API or CI integration.
3. SignPath will return the signed executable.

## Why Code Signing?

Code signing ensures that:
- The executable has not been tampered with since it was signed.
- The identity of the publisher is verified, reducing "Unknown Publisher" warnings in Windows.
- It improves trust and reduces false positives from Antivirus software.
