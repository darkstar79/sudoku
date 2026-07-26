// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace sudoku::core {

/// Error types for encryption operations
enum class EncryptionError : std::uint8_t {
    InvalidInput,           // Empty data or invalid parameters
    EncryptionFailed,       // Encryption operation failed
    DecryptionFailed,       // Decryption operation failed (wrong key or corrupted data)
    SystemInfoUnavailable,  // Cannot retrieve system identifiers
    KeyDerivationFailed,    // Key derivation (Argon2id) failed
    InvalidFileFormat,      // File doesn't have valid encryption header
    InitializationFailed    // libsodium could not be initialized (e.g. no RNG in sandbox)
};

/// Manages encryption/decryption of save files using libsodium
///
/// Uses XSalsa20-Poly1305 AEAD cipher with system-based key derivation.
/// Keys are derived from system identifiers (machine-id, hostname, username)
/// combined with Argon2id password hashing.
///
/// File Format:
/// - Magic bytes: "SDKE" (4 bytes)
/// - Version: 1 byte
/// - Flags: 1 byte (bit 0 selects the KDF cost tier: INTERACTIVE when set, else MODERATE)
/// - Salt: 32 bytes
/// - Nonce: 24 bytes
/// - Ciphertext + MAC: variable length
///
/// Stateless apart from one deliberate exception: the key derived for the machine salt is
/// memoised per KDF cost tier for the lifetime of the process, because deriving it per file made
/// listing saves cost seconds (see deriveKey / encryptionSalt and SECURITY.md). Keys for any other
/// salt are still derived per-operation and zeroed immediately. libsodium is
/// initialized lazily on first use; an init failure is reported as
/// EncryptionError::InitializationFailed rather than thrown, so callers can
/// degrade gracefully (e.g. disable save/load) instead of crashing (#28).
class EncryptionManager {
public:
    // Pure static utility: not instantiable and not copy/movable.
    EncryptionManager() = delete;
    ~EncryptionManager() = default;
    EncryptionManager(const EncryptionManager&) = delete;
    EncryptionManager& operator=(const EncryptionManager&) = delete;
    EncryptionManager(EncryptionManager&&) = delete;
    EncryptionManager& operator=(EncryptionManager&&) = delete;

    /// Encrypts plaintext using system-derived key (Argon2id MODERATE KDF)
    /// @param plaintext Data to encrypt (can be compressed YAML)
    /// @return Encrypted file format with header, salt, nonce, and ciphertext
    [[nodiscard]] static std::expected<std::vector<uint8_t>, EncryptionError>
    encrypt(const std::vector<uint8_t>& plaintext);

    /// Encrypts plaintext using system-derived key (Argon2id INTERACTIVE KDF — faster)
    /// Suitable for data written frequently (statistics). Uses flags byte 0x01 to indicate
    /// interactive KDF, so decrypt() automatically selects the correct cost level.
    /// @param plaintext Data to encrypt
    /// @return Encrypted file format with header, salt, nonce, and ciphertext
    [[nodiscard]] static std::expected<std::vector<uint8_t>, EncryptionError>
    encryptInteractive(const std::vector<uint8_t>& plaintext);

    /// Decrypts ciphertext using system-derived key
    /// Automatically detects KDF level from the flags byte.
    /// @param encrypted_data Complete encrypted file (header + salt + nonce + ciphertext)
    /// @return Decrypted plaintext data
    [[nodiscard]] static std::expected<std::vector<uint8_t>, EncryptionError>
    decrypt(const std::vector<uint8_t>& encrypted_data);

    /// Checks if data appears to be encrypted (has valid header)
    /// @param data File data to check
    /// @return true if data has "SDKE" magic bytes
    [[nodiscard]] static bool isEncrypted(const std::vector<uint8_t>& data);

private:
    /// File format constants
    static constexpr std::array<uint8_t, 4> MAGIC_BYTES = {'S', 'D', 'K', 'E'};
    static constexpr uint8_t VERSION = 1;
    static constexpr size_t HEADER_SIZE = 6;  // Magic (4) + Version (1) + Flags (1)

    /// Cryptographic constants (libsodium)
    static constexpr size_t SALT_SIZE = 32;   // crypto_pwhash_SALTBYTES
    static constexpr size_t NONCE_SIZE = 24;  // crypto_secretbox_NONCEBYTES
    static constexpr size_t KEY_SIZE = 32;    // crypto_secretbox_KEYBYTES
    static constexpr size_t MAC_SIZE = 16;    // crypto_secretbox_MACBYTES

    /// KDF cost level flags (stored in file header flags byte)
    static constexpr uint8_t FLAG_INTERACTIVE_KDF = 0x01;

    /// Lazily initializes libsodium (idempotent; safe to call on every operation).
    /// @return empty on success, EncryptionError::InitializationFailed if sodium_init() fails
    [[nodiscard]] static std::expected<void, EncryptionError> ensureInitialized();

    /// Internal encrypt implementation with configurable KDF cost
    [[nodiscard]] static std::expected<std::vector<uint8_t>, EncryptionError>
    encryptWithFlags(const std::vector<uint8_t>& plaintext, uint8_t flags);

    /// Derives encryption key from system identifiers using Argon2id.
    /// Memoised for the fixed salt (see encryptionSalt) so reading many saves costs one derivation
    /// per KDF tier per process instead of one per file; any other salt derives afresh.
    /// @param salt Salt read from the file being decrypted, or encryptionSalt() when encrypting
    /// @param interactive Use INTERACTIVE (fast) instead of MODERATE (secure) KDF cost
    /// @return 32-byte encryption key
    [[nodiscard]] static std::expected<std::vector<uint8_t>, EncryptionError>
    deriveKey(const std::vector<uint8_t>& salt, bool interactive = false);

    /// The fixed salt written into every new encrypted file: BLAKE2b over a constant application
    /// domain string. Deterministic on purpose — a random per-file salt forces a fresh Argon2id
    /// derivation for every file read, which is what made listing saves cost seconds.
    ///
    /// Deliberately NOT derived from the system identifier, even though that would also be stable:
    /// the identifier is the KDF password, so such a salt would let anyone holding a save file
    /// confirm a guessed machine identity with one cheap hash instead of a full Argon2id.
    ///
    /// What a fixed salt gives up is cross-machine precomputation resistance — one table could in
    /// principle serve many machines. Not reachable in practice: the identifier includes a 128-bit
    /// random machine id on Linux and macOS, and any table still costs one Argon2id per candidate.
    /// Files carrying any other salt still decrypt normally.
    /// @return 32-byte salt (computed once, then cached)
    [[nodiscard]] static std::expected<std::vector<uint8_t>, EncryptionError> encryptionSalt();

    /// Retrieves system-specific identifier string
    /// Combines machine-id, hostname, and username for key derivation
    /// @return System identifier string
    [[nodiscard]] static std::expected<std::string, EncryptionError> getSystemIdentifier();

    /// Generates cryptographically secure random bytes
    /// @param size Number of bytes to generate
    /// @return Random bytes
    [[nodiscard]] static std::vector<uint8_t> generateRandomBytes(size_t size);
};

}  // namespace sudoku::core
