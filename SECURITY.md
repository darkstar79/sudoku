# Security Policy

## Scope

sudoku is an offline desktop application. It has no network-facing components, no server, and no user accounts. The attack surface is limited to:

- **Save file parsing** (YAML + zlib + libsodium decryption)
- **Local file I/O** (save/load, statistics, configuration)

## Reporting a Vulnerability

All security issues can be reported publicly via [GitHub Issues](../../issues).

Since this is a local-only desktop application with no network exposure, there is no need for private disclosure. If you believe an issue requires private reporting, contact the maintainer at <darkstar79@gmx.net>.

## Supported Versions

Only the latest version on the `main` branch is supported. There are no stable release branches.
