# Security Policy

## Scope

sudoku is an offline desktop application. It has no network-facing components, no server, and no user accounts. The attack surface is limited to:

- **Save file parsing** (YAML + zlib + libsodium decryption)
- **Local file I/O** (save/load, statistics, configuration)

Save files are treated as untrusted input: every numeric field the loader reads (move positions, move values, cell values, note values, move type, and the move index) is range-validated against the board's fixed domain at the deserializer boundary. A save with an out-of-range field is rejected as invalid data — and preserved aside, never mishandled on undo/redo or overwritten.

## Reporting a Vulnerability

All security issues can be reported publicly via [GitHub Issues](../../issues).

Since this is a local-only desktop application with no network exposure, there is no need for private disclosure. If you believe an issue requires private reporting, contact the maintainer at <darkstar79@gmx.net>.

## Supported Versions

Only the latest version on the `main` branch is supported. There are no stable release branches.
