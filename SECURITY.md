# Security Policy

## Supported Versions

Only the latest version of the Game Launcher is supported for security updates.

## Reporting a Vulnerability

We take the security of this project seriously. If you discover a security vulnerability, please follow these steps:

1. **Do NOT open a public issue.** This could expose users to risk.
2. **Email the maintainers**: Send a detailed report to `security@yourdomain.com` (replace with your actual security contact).
3. **Include details**: Describe the vulnerability, how to reproduce it, and any potential impact.

We will acknowledge your report within 48 hours and work on a fix as quickly as possible. Once a fix is released, we will give credit to the reporter (if desired).

## Sensitive Data

- **Encryption Key**: The encryption key in `src/config.h` is a default. **You MUST change this** for your own production deployment.
- **Settings**: Do not commit `settings.bin` or `launcher_settings.json` with production URLs to public repositories.
