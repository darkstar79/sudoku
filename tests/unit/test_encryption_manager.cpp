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

#include "../../src/core/encryption_manager.h"

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("EncryptionManager basic encrypt/decrypt", "[encryption_manager]") {
    EncryptionManager manager;

    SECTION("Encrypt and decrypt simple data") {
        std::string plaintext_str = "Hello, Sudoku! This is a test message.";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        // Encrypt
        auto encrypted_result = manager.encrypt(plaintext);
        REQUIRE(encrypted_result.has_value());
        auto encrypted = *encrypted_result;

        // Verify encrypted data is different from plaintext
        REQUIRE(encrypted.size() > plaintext.size());
        REQUIRE_FALSE(std::equal(plaintext.begin(), plaintext.end(), encrypted.begin()));

        // Decrypt
        auto decrypted_result = manager.decrypt(encrypted);
        REQUIRE(decrypted_result.has_value());
        auto decrypted = *decrypted_result;

        // Verify decrypted matches original
        REQUIRE(decrypted == plaintext);
        std::string decrypted_str(decrypted.begin(), decrypted.end());
        REQUIRE(decrypted_str == plaintext_str);
    }

    SECTION("Encrypt large data") {
        // Create 10KB of data
        std::vector<uint8_t> large_data(10000);
        for (size_t i = 0; i < large_data.size(); ++i) {
            large_data[i] = static_cast<uint8_t>(i % 256);
        }

        auto encrypted = manager.encrypt(large_data);
        REQUIRE(encrypted.has_value());

        auto decrypted = manager.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(*decrypted == large_data);
    }

    SECTION("Empty data returns error") {
        std::vector<uint8_t> empty_data;
        auto result = manager.encrypt(empty_data);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EncryptionError::InvalidInput);
    }
}

TEST_CASE("EncryptionManager file format validation", "[encryption_manager]") {
    EncryptionManager manager;

    SECTION("Encrypted data has correct magic bytes") {
        std::string plaintext_str = "Test data";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        auto encrypted = manager.encrypt(plaintext);
        REQUIRE(encrypted.has_value());

        // Check magic bytes "SDKE"
        REQUIRE(encrypted->size() >= 4);
        REQUIRE((*encrypted)[0] == 'S');
        REQUIRE((*encrypted)[1] == 'D');
        REQUIRE((*encrypted)[2] == 'K');
        REQUIRE((*encrypted)[3] == 'E');
    }

    SECTION("isEncrypted detects encrypted data") {
        std::string plaintext_str = "Test";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        auto encrypted = manager.encrypt(plaintext);
        REQUIRE(encrypted.has_value());

        REQUIRE(EncryptionManager::isEncrypted(*encrypted));
    }

    SECTION("isEncrypted returns false for non-encrypted data") {
        std::vector<uint8_t> plain_data = {'H', 'e', 'l', 'l', 'o'};
        REQUIRE_FALSE(EncryptionManager::isEncrypted(plain_data));
    }

    SECTION("isEncrypted handles empty data") {
        std::vector<uint8_t> empty;
        REQUIRE_FALSE(EncryptionManager::isEncrypted(empty));
    }

    SECTION("isEncrypted handles data shorter than magic bytes") {
        std::vector<uint8_t> short_data = {'S', 'D'};
        REQUIRE_FALSE(EncryptionManager::isEncrypted(short_data));
    }
}

TEST_CASE("EncryptionManager error handling", "[encryption_manager]") {
    EncryptionManager manager;

    SECTION("Decrypt fails on corrupted data") {
        std::string plaintext_str = "Original message";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        auto encrypted = manager.encrypt(plaintext);
        REQUIRE(encrypted.has_value());

        // Corrupt the ciphertext (not the header)
        auto corrupted = *encrypted;
        if (corrupted.size() > 70) {
            corrupted[70] ^= 0xFF;  // Flip bits
        }

        auto result = manager.decrypt(corrupted);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EncryptionError::DecryptionFailed);
    }

    SECTION("Decrypt fails on truncated data") {
        std::string plaintext_str = "Test";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        auto encrypted = manager.encrypt(plaintext);
        REQUIRE(encrypted.has_value());

        // Truncate
        auto truncated = *encrypted;
        truncated.resize(20);

        auto result = manager.decrypt(truncated);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EncryptionError::InvalidFileFormat);
    }

    SECTION("Decrypt fails on invalid magic bytes") {
        std::vector<uint8_t> invalid_data = {
            'X', 'Y', 'Z', 'W',  // Wrong magic
            1,   0,              // Version, flags
            // 32 bytes salt + 24 bytes nonce + some ciphertext
        };
        invalid_data.resize(100, 0);

        auto result = manager.decrypt(invalid_data);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EncryptionError::InvalidFileFormat);
    }

    SECTION("Decrypt fails on unsupported version") {
        std::string plaintext_str = "Test";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        auto encrypted = manager.encrypt(plaintext);
        REQUIRE(encrypted.has_value());

        // Change version byte
        auto wrong_version = *encrypted;
        wrong_version[4] = 99;  // Invalid version

        auto result = manager.decrypt(wrong_version);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EncryptionError::InvalidFileFormat);
    }

    SECTION("Decrypt fails on too-small data") {
        std::vector<uint8_t> tiny_data = {'S', 'D', 'K', 'E', 1};
        auto result = manager.decrypt(tiny_data);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EncryptionError::InvalidFileFormat);
    }
}

TEST_CASE("EncryptionManager system-based key derivation", "[encryption_manager]") {
    EncryptionManager manager1;
    EncryptionManager manager2;

    SECTION("Same plaintext produces different ciphertext (random salt/nonce)") {
        std::string plaintext_str = "Same message";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        auto encrypted1 = manager1.encrypt(plaintext);
        auto encrypted2 = manager2.encrypt(plaintext);

        REQUIRE(encrypted1.has_value());
        REQUIRE(encrypted2.has_value());

        // Ciphertexts should differ (due to random salt and nonce)
        REQUIRE_FALSE(*encrypted1 == *encrypted2);

        // But both decrypt correctly
        auto decrypted1 = manager1.decrypt(*encrypted1);
        auto decrypted2 = manager2.decrypt(*encrypted2);

        REQUIRE(decrypted1.has_value());
        REQUIRE(decrypted2.has_value());
        REQUIRE(*decrypted1 == plaintext);
        REQUIRE(*decrypted2 == plaintext);
    }

    SECTION("Cross-decrypt works (same system, same key derivation)") {
        std::string plaintext_str = "Cross-decrypt test";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        // Encrypt with manager1
        auto encrypted = manager1.encrypt(plaintext);
        REQUIRE(encrypted.has_value());

        // Decrypt with manager2 (should work - same system)
        auto decrypted = manager2.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(*decrypted == plaintext);
    }
}

TEST_CASE("EncryptionManager performance characteristics", "[encryption_manager]") {
    EncryptionManager manager;

    SECTION("Encrypted size overhead") {
        std::string plaintext_str = "Test message";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        auto encrypted = manager.encrypt(plaintext);
        REQUIRE(encrypted.has_value());

        // Overhead: header (6) + salt (32) + nonce (24) + MAC (16) = 78 bytes
        size_t expected_overhead = 6 + 32 + 24 + 16;
        REQUIRE(encrypted->size() == plaintext.size() + expected_overhead);
    }

    SECTION("Handle binary data (non-text)") {
        std::vector<uint8_t> binary_data = {0x00, 0xFF, 0xAA, 0x55, 0x01, 0x02, 0x03, 0x04,
                                            0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};

        auto encrypted = manager.encrypt(binary_data);
        REQUIRE(encrypted.has_value());

        auto decrypted = manager.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(*decrypted == binary_data);
    }

    SECTION("Handle data with null bytes") {
        std::vector<uint8_t> data_with_nulls = {0x48, 0x00, 0x65, 0x00, 0x6C, 0x00, 0x6C, 0x00, 0x6F, 0x00};

        auto encrypted = manager.encrypt(data_with_nulls);
        REQUIRE(encrypted.has_value());

        auto decrypted = manager.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(*decrypted == data_with_nulls);
    }
}

TEST_CASE("EncryptionManager data integrity", "[encryption_manager]") {
    EncryptionManager manager;

    SECTION("Tampering with header detected") {
        std::string plaintext_str = "Integrity test";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        auto encrypted = manager.encrypt(plaintext);
        REQUIRE(encrypted.has_value());

        // Tamper with flags byte
        auto tampered = *encrypted;
        tampered[5] ^= 0x01;

        // Flipping FLAG_INTERACTIVE_KDF changes the KDF cost, producing a different key
        auto result = manager.decrypt(tampered);
        REQUIRE_FALSE(result.has_value());  // Wrong KDF → wrong key → MAC mismatch
    }

    SECTION("Tampering with salt detected") {
        std::string plaintext_str = "Salt tamper test";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        auto encrypted = manager.encrypt(plaintext);
        REQUIRE(encrypted.has_value());

        // Tamper with salt (affects key derivation)
        auto tampered = *encrypted;
        tampered[10] ^= 0xFF;

        // Decryption should fail (wrong key derived)
        auto result = manager.decrypt(tampered);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EncryptionError::DecryptionFailed);
    }

    SECTION("Tampering with nonce detected") {
        std::string plaintext_str = "Nonce tamper test";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        auto encrypted = manager.encrypt(plaintext);
        REQUIRE(encrypted.has_value());

        // Tamper with nonce (byte 6 + 32 = 38)
        auto tampered = *encrypted;
        tampered[38] ^= 0xFF;

        // Decryption should fail (wrong nonce)
        auto result = manager.decrypt(tampered);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EncryptionError::DecryptionFailed);
    }

    SECTION("Tampering with ciphertext detected") {
        std::string plaintext_str = "Ciphertext tamper test";
        std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

        auto encrypted = manager.encrypt(plaintext);
        REQUIRE(encrypted.has_value());

        // Tamper with ciphertext (after header + salt + nonce)
        auto tampered = *encrypted;
        size_t ciphertext_start = 6 + 32 + 24;
        if (tampered.size() > ciphertext_start) {
            tampered[ciphertext_start] ^= 0xFF;
        }

        // Decryption should fail (MAC verification fails)
        auto result = manager.decrypt(tampered);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EncryptionError::DecryptionFailed);
    }
}

TEST_CASE("EncryptionManager edge cases", "[encryption_manager]") {
    EncryptionManager manager;

    SECTION("Single byte data") {
        // Minimum valid data size
        std::vector<uint8_t> single_byte = {0x42};

        auto encrypted = manager.encrypt(single_byte);
        REQUIRE(encrypted.has_value());

        auto decrypted = manager.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(*decrypted == single_byte);
    }

    SECTION("Very large data (1 MB)") {
        // Stress test with 1MB of data
        std::vector<uint8_t> large_data(1'000'000);
        for (size_t i = 0; i < large_data.size(); ++i) {
            large_data[i] = static_cast<uint8_t>((i * 7) % 256);  // Pseudo-random pattern
        }

        auto encrypted = manager.encrypt(large_data);
        REQUIRE(encrypted.has_value());

        // Verify size overhead is reasonable (should be ~78 bytes + data size)
        REQUIRE(encrypted->size() > large_data.size());
        REQUIRE(encrypted->size() < large_data.size() + 100);

        auto decrypted = manager.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(decrypted->size() == 1'000'000);
        REQUIRE(*decrypted == large_data);
    }

    SECTION("All-zeros data") {
        // Edge case: Data with low entropy
        std::vector<uint8_t> zeros(1000, 0x00);

        auto encrypted = manager.encrypt(zeros);
        REQUIRE(encrypted.has_value());

        // Encrypted data should NOT be all zeros (randomized by salt/nonce)
        bool has_non_zero = false;
        for (auto byte : *encrypted) {
            if (byte != 0) {
                has_non_zero = true;
                break;
            }
        }
        REQUIRE(has_non_zero);

        auto decrypted = manager.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(*decrypted == zeros);
    }

    SECTION("All-ones data") {
        // Edge case: Data with all bits set
        std::vector<uint8_t> ones(1000, 0xFF);

        auto encrypted = manager.encrypt(ones);
        REQUIRE(encrypted.has_value());

        auto decrypted = manager.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(*decrypted == ones);
    }

    SECTION("Alternating bit pattern") {
        // Edge case: Repeating pattern 0xAA (10101010)
        std::vector<uint8_t> pattern(500, 0xAA);

        auto encrypted = manager.encrypt(pattern);
        REQUIRE(encrypted.has_value());

        auto decrypted = manager.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(*decrypted == pattern);
    }

    SECTION("Random-looking data") {
        // Edge case: Data that looks like encrypted data
        std::vector<uint8_t> pseudo_random(1000);
        for (size_t i = 0; i < pseudo_random.size(); ++i) {
            pseudo_random[i] = static_cast<uint8_t>((i * i * 13 + i * 7 + 42) % 256);
        }

        auto encrypted = manager.encrypt(pseudo_random);
        REQUIRE(encrypted.has_value());

        auto decrypted = manager.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(*decrypted == pseudo_random);
    }

    SECTION("Data with magic bytes prefix (but not encrypted)") {
        // Edge case: Plaintext that starts with "SDKE" magic bytes
        std::vector<uint8_t> fake_header = {'S', 'D', 'K', 'E', 'H', 'e', 'l', 'l', 'o'};

        auto encrypted = manager.encrypt(fake_header);
        REQUIRE(encrypted.has_value());

        // Encrypted form should also have magic bytes (nested)
        REQUIRE(EncryptionManager::isEncrypted(*encrypted));

        auto decrypted = manager.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(*decrypted == fake_header);

        // Decrypted plaintext starts with "SDKE", so isEncrypted() returns true
        // (isEncrypted() only checks magic bytes, not full format validation)
        REQUIRE(EncryptionManager::isEncrypted(*decrypted));

        // But trying to decrypt it as encrypted data should fail
        auto double_decrypt = manager.decrypt(*decrypted);
        REQUIRE_FALSE(double_decrypt.has_value());
    }

    SECTION("Exact header size data") {
        // Edge case: Data exactly the size of the header metadata
        size_t header_size = 6 + 32 + 24 + 16;  // header + salt + nonce + MAC
        std::vector<uint8_t> exact_size(header_size, 0x42);

        auto encrypted = manager.encrypt(exact_size);
        REQUIRE(encrypted.has_value());

        auto decrypted = manager.decrypt(*encrypted);
        REQUIRE(decrypted.has_value());
        REQUIRE(*decrypted == exact_size);
    }

    SECTION("Consecutive encryptions produce different ciphertexts") {
        // Edge case: Same plaintext encrypted multiple times
        std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5};

        auto encrypted1 = manager.encrypt(plaintext);
        auto encrypted2 = manager.encrypt(plaintext);
        auto encrypted3 = manager.encrypt(plaintext);

        REQUIRE(encrypted1.has_value());
        REQUIRE(encrypted2.has_value());
        REQUIRE(encrypted3.has_value());

        // All should be different (random salt/nonce)
        REQUIRE(*encrypted1 != *encrypted2);
        REQUIRE(*encrypted2 != *encrypted3);
        REQUIRE(*encrypted1 != *encrypted3);

        // But all decrypt to same plaintext
        auto decrypted1 = manager.decrypt(*encrypted1);
        auto decrypted2 = manager.decrypt(*encrypted2);
        auto decrypted3 = manager.decrypt(*encrypted3);

        REQUIRE(*decrypted1 == plaintext);
        REQUIRE(*decrypted2 == plaintext);
        REQUIRE(*decrypted3 == plaintext);
    }

    SECTION("isEncrypted() with corrupted magic bytes") {
        // Edge case: Partial magic bytes
        std::vector<uint8_t> partial1 = {'S'};
        std::vector<uint8_t> partial2 = {'S', 'D'};
        std::vector<uint8_t> partial3 = {'S', 'D', 'K'};
        std::vector<uint8_t> complete = {'S', 'D', 'K', 'E'};
        std::vector<uint8_t> almost = {'S', 'D', 'K', 'X'};

        REQUIRE_FALSE(EncryptionManager::isEncrypted(partial1));
        REQUIRE_FALSE(EncryptionManager::isEncrypted(partial2));
        REQUIRE_FALSE(EncryptionManager::isEncrypted(partial3));
        REQUIRE(EncryptionManager::isEncrypted(complete));
        REQUIRE_FALSE(EncryptionManager::isEncrypted(almost));
    }
}
