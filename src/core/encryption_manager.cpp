// sudoku-cpp - Offline Sudoku Game
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

#include "encryption_manager.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include <fmt/base.h>
#include <fmt/format.h>
#include <sodium/core.h>
#include <sodium/crypto_pwhash.h>
#include <sodium/crypto_secretbox.h>
#include <sodium/randombytes.h>
#include <sodium/utils.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#    include <lmcons.h>  // For UNLEN
#    include <windows.h>
#else
#    include <pwd.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif

namespace sudoku::core {

EncryptionManager::EncryptionManager() {
    // Initialize libsodium
    if (sodium_init() < 0) {
        spdlog::error("Failed to initialize libsodium");
        throw std::runtime_error("libsodium initialization failed");
    }
    spdlog::debug("EncryptionManager initialized with libsodium");
}

std::expected<std::vector<uint8_t>, EncryptionError> EncryptionManager::encrypt(const std::vector<uint8_t>& plaintext) {
    return encryptWithFlags(plaintext, 0);
}

std::expected<std::vector<uint8_t>, EncryptionError>
EncryptionManager::encryptInteractive(const std::vector<uint8_t>& plaintext) {
    return encryptWithFlags(plaintext, FLAG_INTERACTIVE_KDF);
}

std::expected<std::vector<uint8_t>, EncryptionError>
EncryptionManager::encryptWithFlags(const std::vector<uint8_t>& plaintext, uint8_t flags) {
    if (plaintext.empty()) {
        spdlog::error("Cannot encrypt empty data");
        return std::unexpected(EncryptionError::InvalidInput);
    }

    bool interactive = (flags & FLAG_INTERACTIVE_KDF) != 0;

    // Generate random salt and nonce
    auto salt = generateRandomBytes(SALT_SIZE);
    auto nonce = generateRandomBytes(NONCE_SIZE);

    // Derive encryption key from system identifiers
    auto key_result = deriveKey(salt, interactive);
    if (!key_result) {
        return std::unexpected(key_result.error());
    }
    auto key = *key_result;

    // Allocate ciphertext buffer (plaintext + MAC)
    std::vector<uint8_t> ciphertext(plaintext.size() + MAC_SIZE);

    // Encrypt using XSalsa20-Poly1305
    int result = crypto_secretbox_easy(ciphertext.data(), plaintext.data(), plaintext.size(), nonce.data(), key.data());

    // Securely zero out key material
    sodium_memzero(key.data(), key.size());

    if (result != 0) {
        spdlog::error("Encryption failed");
        return std::unexpected(EncryptionError::EncryptionFailed);
    }

    // Build final encrypted file format:
    // [Magic(4)] [Version(1)] [Flags(1)] [Salt(32)] [Nonce(24)] [Ciphertext+MAC]
    std::vector<uint8_t> encrypted_file;
    encrypted_file.reserve(HEADER_SIZE + SALT_SIZE + NONCE_SIZE + ciphertext.size());

    // Header
    encrypted_file.insert(encrypted_file.end(), MAGIC_BYTES.begin(), MAGIC_BYTES.end());
    encrypted_file.push_back(VERSION);
    encrypted_file.push_back(flags);

    // Salt and nonce
    encrypted_file.insert(encrypted_file.end(), salt.begin(), salt.end());
    encrypted_file.insert(encrypted_file.end(), nonce.begin(), nonce.end());

    // Ciphertext
    encrypted_file.insert(encrypted_file.end(), ciphertext.begin(), ciphertext.end());

    spdlog::debug("Encrypted {} bytes to {} bytes (interactive={})", plaintext.size(), encrypted_file.size(),
                  interactive);
    return encrypted_file;
}

std::expected<std::vector<uint8_t>, EncryptionError>
EncryptionManager::decrypt(const std::vector<uint8_t>& encrypted_data) {
    // Validate minimum size: header + salt + nonce + at least MAC
    const size_t min_size = HEADER_SIZE + SALT_SIZE + NONCE_SIZE + MAC_SIZE;
    if (encrypted_data.size() < min_size) {
        spdlog::error("Encrypted data too small: {} bytes", encrypted_data.size());
        return std::unexpected(EncryptionError::InvalidFileFormat);
    }

    // Validate magic bytes
    if (!std::equal(MAGIC_BYTES.begin(), MAGIC_BYTES.end(), encrypted_data.begin())) {
        spdlog::error("Invalid magic bytes");
        return std::unexpected(EncryptionError::InvalidFileFormat);
    }

    // Validate version
    uint8_t version = encrypted_data[4];
    if (version != VERSION) {
        spdlog::error("Unsupported encryption version: {}", version);
        return std::unexpected(EncryptionError::InvalidFileFormat);
    }

    // Read flags to determine KDF cost level
    uint8_t flags = encrypted_data[5];
    bool interactive = (flags & FLAG_INTERACTIVE_KDF) != 0;

    // Extract salt and nonce
    std::vector<uint8_t> salt(encrypted_data.begin() + HEADER_SIZE, encrypted_data.begin() + HEADER_SIZE + SALT_SIZE);
    std::vector<uint8_t> nonce(encrypted_data.begin() + HEADER_SIZE + SALT_SIZE,
                               encrypted_data.begin() + HEADER_SIZE + SALT_SIZE + NONCE_SIZE);

    // Extract ciphertext
    const size_t ciphertext_start = HEADER_SIZE + SALT_SIZE + NONCE_SIZE;
    std::vector<uint8_t> ciphertext(encrypted_data.begin() + ciphertext_start, encrypted_data.end());

    // Derive decryption key (cost level from flags)
    auto key_result = deriveKey(salt, interactive);
    if (!key_result) {
        return std::unexpected(key_result.error());
    }
    auto key = *key_result;

    // Allocate plaintext buffer
    std::vector<uint8_t> plaintext(ciphertext.size() - MAC_SIZE);

    // Decrypt using XSalsa20-Poly1305 (includes MAC verification)
    int result =
        crypto_secretbox_open_easy(plaintext.data(), ciphertext.data(), ciphertext.size(), nonce.data(), key.data());

    // Securely zero out key material
    sodium_memzero(key.data(), key.size());

    if (result != 0) {
        spdlog::error("Decryption failed (wrong key or corrupted data)");
        return std::unexpected(EncryptionError::DecryptionFailed);
    }

    spdlog::debug("Decrypted {} bytes to {} bytes", encrypted_data.size(), plaintext.size());
    return plaintext;
}

bool EncryptionManager::isEncrypted(const std::vector<uint8_t>& data) {
    if (data.size() < MAGIC_BYTES.size()) {
        return false;
    }
    return std::equal(MAGIC_BYTES.begin(), MAGIC_BYTES.end(), data.begin());
}

std::expected<std::vector<uint8_t>, EncryptionError> EncryptionManager::deriveKey(const std::vector<uint8_t>& salt,
                                                                                  bool interactive) {
    if (salt.size() != SALT_SIZE) {
        spdlog::error("Invalid salt size: {}", salt.size());
        return std::unexpected(EncryptionError::KeyDerivationFailed);
    }

    // Get system identifier
    auto system_id_result = getSystemIdentifier();
    if (!system_id_result) {
        return std::unexpected(system_id_result.error());
    }
    std::string system_id = *system_id_result;

    // Derive key using Argon2id
    std::vector<uint8_t> key(KEY_SIZE);

    // Select KDF cost level
    auto opslimit = interactive ? crypto_pwhash_OPSLIMIT_INTERACTIVE : crypto_pwhash_OPSLIMIT_MODERATE;
    auto memlimit = interactive ? crypto_pwhash_MEMLIMIT_INTERACTIVE : crypto_pwhash_MEMLIMIT_MODERATE;

    int result = crypto_pwhash(key.data(), key.size(), system_id.c_str(), system_id.length(), salt.data(), opslimit,
                               memlimit, crypto_pwhash_ALG_ARGON2ID13);

    // Securely zero out password material
    sodium_memzero(const_cast<char*>(system_id.data()), system_id.size());

    if (result != 0) {
        spdlog::error("Key derivation failed (out of memory?)");
        return std::unexpected(EncryptionError::KeyDerivationFailed);
    }

    spdlog::debug("Derived encryption key from system identifiers (interactive={})", interactive);
    return key;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
// Justification: Kept as member function for potential future caching/configuration
std::expected<std::string, EncryptionError> EncryptionManager::getSystemIdentifier() {
    std::string identifier;

#ifdef _WIN32
    // Windows: computer name + username + volume serial
    std::array<char, MAX_COMPUTERNAME_LENGTH + 1> computer_name{};
    DWORD computer_name_size = static_cast<DWORD>(computer_name.size());
    if (GetComputerNameA(computer_name.data(), &computer_name_size)) {
        identifier += computer_name.data();
        identifier += "|";
    }

    std::array<char, UNLEN + 1> username{};
    DWORD username_size = static_cast<DWORD>(username.size());
    if (GetUserNameA(username.data(), &username_size)) {
        identifier += username.data();
        identifier += "|";
    }

    // Get volume serial number of C: drive
    DWORD volume_serial = 0;
    if (GetVolumeInformationA("C:\\", nullptr, 0, &volume_serial, nullptr, nullptr, nullptr, 0)) {
        identifier += std::to_string(volume_serial);
        identifier += "|";
    }

#else
    // Linux: hostname + username + machine-id
    std::array<char, 256> hostname{};
    if (gethostname(hostname.data(), hostname.size()) == 0) {
        identifier += hostname.data();
        identifier += "|";
    }

    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);
    if (pw && pw->pw_name) {
        identifier += pw->pw_name;
        identifier += "|";
    }

    // Read /etc/machine-id for unique system identifier
    std::ifstream machine_id_file("/etc/machine-id");
    if (machine_id_file.is_open()) {
        std::string machine_id;
        std::getline(machine_id_file, machine_id);
        if (!machine_id.empty()) {
            identifier += machine_id;
            identifier += "|";
        }
    }
#endif

    // Add application-specific constant
    identifier += "sudoku-encryption-v1";

    if (identifier.empty() || identifier.length() < 20) {
        spdlog::error("Failed to retrieve sufficient system identifiers");
        return std::unexpected(EncryptionError::SystemInfoUnavailable);
    }

    spdlog::debug("System identifier length: {} bytes", identifier.length());
    return identifier;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
// Justification: Kept as member function for potential future configuration (e.g., test mode with deterministic RNG)
std::vector<uint8_t> EncryptionManager::generateRandomBytes(size_t size) {
    std::vector<uint8_t> bytes(size);
    randombytes_buf(bytes.data(), size);
    return bytes;
}

}  // namespace sudoku::core
