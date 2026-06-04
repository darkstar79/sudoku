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

#include "core/file_utils.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

#include <spdlog/spdlog.h>

#ifdef _WIN32
#    include <fcntl.h>
#    include <io.h>
#else
#    include <fcntl.h>
#    include <unistd.h>
#endif

namespace sudoku::core::file_utils {

namespace {

// Flushes the file's contents to durable storage so a crash after the
// subsequent rename cannot expose a present-but-empty target. Without this the
// rename metadata can reach disk before the data, defeating the atomicity.
[[nodiscard]] std::error_code fsyncFile(const std::filesystem::path& path) {
#ifdef _WIN32
    // _commit() flushes via FlushFileBuffers, which requires a handle with
    // GENERIC_WRITE access. Opening read-only yields ERROR_ACCESS_DENIED,
    // surfaced as EBADF — so the descriptor must be writable here.
    const int fd = ::_open(path.string().c_str(), _O_WRONLY | _O_BINARY);
    if (fd == -1) {
        return {errno, std::generic_category()};
    }
    const int rc = ::_commit(fd);
    const int saved_errno = errno;
    ::_close(fd);
    if (rc != 0) {
        return {saved_errno, std::generic_category()};
    }
    return {};
#else
    const int fd = ::open(path.c_str(), O_RDONLY);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (fd == -1) {
        return {errno, std::generic_category()};
    }
    const int rc = ::fsync(fd);
    const int saved_errno = errno;
    ::close(fd);
    if (rc != 0) {
        return {saved_errno, std::generic_category()};
    }
    return {};
#endif
}

}  // namespace

std::expected<void, std::error_code> atomicWriteFile(const std::filesystem::path& target,
                                                     const std::vector<uint8_t>& data) {
    auto tmp_path = target;
    tmp_path += ".tmp";

    // 1. Write all bytes to the temp file.
    {
        std::ofstream file(tmp_path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            spdlog::error("atomicWriteFile: failed to open temp file: {}", tmp_path.string());
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
        if (!data.empty()) {
            file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }
        if (!file.good()) {
            file.close();
            std::error_code rm_ec;
            std::filesystem::remove(tmp_path, rm_ec);
            spdlog::error("atomicWriteFile: failed to write temp file: {}", tmp_path.string());
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
    }

    // 2. fsync the temp file so its contents are durable before the rename.
    if (auto sync_ec = fsyncFile(tmp_path)) {
        std::error_code rm_ec;
        std::filesystem::remove(tmp_path, rm_ec);
        spdlog::error("atomicWriteFile: fsync failed for {}: {}", tmp_path.string(), sync_ec.message());
        return std::unexpected(sync_ec);
    }

    // 3. Atomically replace the target.
    std::error_code ec;
    std::filesystem::rename(tmp_path, target, ec);
    if (ec) {
        std::error_code rm_ec;
        std::filesystem::remove(tmp_path, rm_ec);
        spdlog::error("atomicWriteFile: rename {} -> {} failed: {}", tmp_path.string(), target.string(), ec.message());
        return std::unexpected(ec);
    }

    return {};
}

}  // namespace sudoku::core::file_utils
