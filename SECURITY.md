# Security Policy

## Scope

sudoku is an offline desktop application. It has no network-facing components, no server, and no user accounts. The attack surface is limited to:

- **Save file parsing** (YAML + zlib + libsodium decryption)
- **Local file I/O** (save/load, statistics, configuration)

Save files are treated as untrusted input: every numeric field the loader reads (move positions, move values, cell values, note values, move type, and the move index) is range-validated against the board's fixed domain at the deserializer boundary. A save with an out-of-range field is rejected as invalid data — and preserved aside, never mishandled on undo/redo or overwritten.

The save id is untrusted for the same reason — it is read verbatim from file content. A save id must be exactly 16 lowercase hex characters (the form the generator emits); anything else is rejected before a filesystem path is built from it, so a crafted `save_id` cannot redirect a read, write, or delete outside the save directory.

## Save file encryption

Manual saves are encrypted with XSalsa20-Poly1305. **The key is derived from machine identity — hostname, username, and the OS machine id — not from a passphrase.** Two consequences follow, and both are deliberate:

- **Encrypted saves are bound to the machine that wrote them.** Changing your hostname or username, reinstalling the OS, or copying saves to another computer makes them permanently unreadable. There is no recovery path and no passphrase to fall back on. If you need a save to survive any of that, use **Export**, which deliberately writes a portable plaintext file.
- **This is tamper-resistance, not confidentiality.** The key derives from values that are not secret and the derivation is in this repository's source, so anyone with access to the machine and the source can reproduce it. Encryption raises the effort of hand-editing a save (for example to fake a completion time); it is not a guarantee that the contents are private. What a save contains is a puzzle, its progress, and timestamps — no credentials and no personal data.

Two implementation details worth stating plainly because they look like weaknesses out of context:

- **The salt is a fixed application constant**, not random per file. A random per-file salt forces a separate Argon2id derivation for every file read, which made simply listing saves cost seconds and grow without bound as saves accumulated. The salt is deliberately *not* derived from the machine identifier either — that would also be stable, but since the identifier is the key-derivation input, such a salt would let anyone holding a save file confirm a guessed hostname/username/machine id with one cheap hash instead of a full Argon2id. A constant salt leaks nothing about the machine. What it gives up is cross-machine precomputation resistance, which is not reachable in practice: the identifier includes a 128-bit random machine id on Linux and macOS, and any precomputed table still costs one Argon2id per candidate. The nonce remains random per file, so no nonce is ever reused under the shared key.
- **The derived key is cached in memory** for the lifetime of the running process (one key per KDF cost tier). Anything able to read the process's memory could re-derive the key from public machine facts anyway.

The play-time ledger and collected statistics are encrypted the same way and carry the same machine-binding limitation.

## Reporting a Vulnerability

All security issues can be reported publicly via [GitHub Issues](../../issues).

Since this is a local-only desktop application with no network exposure, there is no need for private disclosure. If you believe an issue requires private reporting, contact the maintainer at <darkstar79@gmx.net>.

## Supported Versions

Only the latest version on the `main` branch is supported. There are no stable release branches.
