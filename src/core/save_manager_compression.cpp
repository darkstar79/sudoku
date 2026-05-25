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

#include "core/i_save_manager.h"
#include "save_manager.h"

#include <chrono>
#include <cstdint>
#include <exception>
#include <expected>
#include <filesystem>
#include <vector>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <zconf.h>
#include <zlib.h>

namespace {
// Compression constants
inline constexpr int DEFAULT_COMPRESSION_LEVEL = 6;
inline constexpr int INITIAL_DECOMPRESSION_MULTIPLIER = 4;
inline constexpr int MAX_DECOMPRESSION_ATTEMPTS = 10;
inline constexpr int BUFFER_SIZE_MULTIPLIER = 2;
// Maximum allowed uncompressed size — decompression-bomb protection (#12 / BL-9).
// A YAML save is typically <1 MiB; 64 MiB is far above any realistic save while
// being small enough to fit in memory on any platform we support. Without this
// cap, the doubling loop below could allocate up to data.size() * 4096 (~40 GiB
// for a 10 MiB input) before giving up.
inline constexpr size_t MAX_DECOMPRESSED_SIZE = 64UL * 1024 * 1024;
}  // namespace

namespace sudoku::core {

std::expected<std::vector<uint8_t>, SaveError> SaveManager::compressData(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<uint8_t>{};
    }

    // Calculate maximum compressed size
    uLongf compressed_size = compressBound(static_cast<uLong>(data.size()));
    std::vector<uint8_t> compressed(compressed_size);

    // Compress with default level (good balance of speed/compression)
    int result = compress2(compressed.data(), &compressed_size, data.data(), static_cast<uLong>(data.size()),
                           DEFAULT_COMPRESSION_LEVEL);

    if (result != Z_OK) {
        spdlog::error("Compression failed with error code: {}", result);
        return std::unexpected(SaveError::CompressionError);
    }

    // Resize to actual compressed size
    compressed.resize(compressed_size);
    spdlog::debug("Compressed {} bytes to {} bytes ({:.1f}% reduction)", data.size(), compressed.size(),
                  100.0 * (1.0 - static_cast<double>(compressed.size()) / static_cast<double>(data.size())));

    return compressed;
}

std::expected<std::vector<uint8_t>, SaveError> SaveManager::decompressData(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<uint8_t>{};
    }

    // Start with initial multiplier of compressed size, clamped to the bomb cap.
    auto uncompressed_size =
        static_cast<uLongf>(std::min<size_t>(data.size() * INITIAL_DECOMPRESSION_MULTIPLIER, MAX_DECOMPRESSED_SIZE));
    std::vector<uint8_t> uncompressed(uncompressed_size);

    int result = Z_BUF_ERROR;
    int attempts = 0;

    // Try decompression with increasing buffer sizes, refusing to grow past the cap.
    while (result == Z_BUF_ERROR && attempts < MAX_DECOMPRESSION_ATTEMPTS) {
        result = uncompress(uncompressed.data(), &uncompressed_size, data.data(), static_cast<uLong>(data.size()));

        if (result == Z_BUF_ERROR) {
            // Refuse to grow past the decompression-bomb cap.
            if (uncompressed.size() >= MAX_DECOMPRESSED_SIZE) {
                spdlog::error("Decompression refused: would exceed max size {} bytes after {} attempts "
                              "(likely a zlib bomb)",
                              MAX_DECOMPRESSED_SIZE, attempts);
                return std::unexpected(SaveError::CompressionError);
            }
            // Buffer too small, double it (clamped to the cap).
            auto new_size = std::min<size_t>(uncompressed.size() * BUFFER_SIZE_MULTIPLIER, MAX_DECOMPRESSED_SIZE);
            uncompressed.resize(new_size);
            uncompressed_size = static_cast<uLongf>(new_size);
            ++attempts;
        }
    }

    if (result != Z_OK) {
        spdlog::error("Decompression failed with error code: {} after {} attempts", result, attempts);
        return std::unexpected(SaveError::CompressionError);
    }

    // Resize to actual uncompressed size
    uncompressed.resize(uncompressed_size);
    spdlog::debug("Decompressed {} bytes to {} bytes", data.size(), uncompressed.size());

    return uncompressed;
}

bool SaveManager::isValidSaveFile(const std::filesystem::path& file_path) {
    try {
        if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
            return false;
        }

        // Check file extension
        if (file_path.extension() != ".yaml") {
            return false;
        }

        // Try to load and parse (handles both compressed and uncompressed)
        auto result = deserializeFromYaml(file_path);
        return result.has_value();

    } catch (const std::exception&) {
        return false;
    }
}

std::chrono::system_clock::time_point SaveManager::getFileModificationTime(const std::filesystem::path& file_path) {
    try {
        auto ftime = std::filesystem::last_write_time(file_path);

        // Convert to system_clock time_point
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());

        return sctp;

    } catch (const std::exception&) {
        return std::chrono::system_clock::now();
    }
}

}  // namespace sudoku::core
