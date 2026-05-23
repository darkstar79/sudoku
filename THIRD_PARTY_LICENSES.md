# Third-Party Licenses

This document lists all third-party dependencies used in the sudoku project and their respective licenses.

## License Compliance

This project is licensed under the **GNU General Public License v3.0 (GPLv3)** (see [LICENSE](LICENSE) file). All dependencies have been verified to be compatible with GPLv3.

## Summary

| License Type | Count | Libraries |
|-------------|-------|-----------|
| MIT | 3 | spdlog, fmt (via spdlog), yaml-cpp |
| LGPL 3.0 | 1 | Qt6 |
| zlib | 1 | zlib |
| ISC | 1 | libsodium |
| BSD 3-Clause | 1 | CMake |
| Apache 2.0 | 1 | Ninja |
| Boost Software License 1.0 | 1 | Catch2 |

**All licenses are permissive and compatible with GPLv3 licensing for this project.**

---

## Runtime Dependencies

### Qt6

- **Version:** 6.x (system package)
- **License:** LGPL 3.0
- **Purpose:** Cross-platform GUI framework (Widgets module)
- **Homepage:** https://www.qt.io/
- **License URL:** https://doc.qt.io/qt-6/lgpl.html

**License Summary:** LGPL allows use in proprietary software when dynamically linked. Must ship Qt libraries alongside binary or allow relinking.

---

### spdlog

- **Version:** 1.15.3
- **License:** MIT License
- **Purpose:** Fast C++ logging library
- **Homepage:** https://github.com/gabime/spdlog
- **License URL:** https://github.com/gabime/spdlog/blob/v1.x/LICENSE

**License Summary:** Permissive MIT license. Requires attribution.

**Note:** spdlog bundles the fmt library (MIT license) as a transitive dependency. fmt is used directly in some source files via `#include <fmt/format.h>`.

---

### fmt (via spdlog)

- **License:** MIT License
- **Purpose:** Fast string formatting library (bundled with spdlog)
- **Homepage:** https://github.com/fmtlib/fmt
- **License URL:** https://github.com/fmtlib/fmt/blob/master/LICENSE

**License Summary:** Permissive MIT license. Requires attribution.

**Usage:** Transitive dependency bundled with spdlog. Used for string formatting in solving strategy explanations.

---

### yaml-cpp

- **Version:** 0.8.0
- **License:** MIT License
- **Purpose:** YAML parser and emitter for C++
- **Homepage:** https://github.com/jbeder/yaml-cpp
- **License URL:** https://github.com/jbeder/yaml-cpp/blob/master/LICENSE

**License Summary:** Permissive MIT license. Requires attribution.

**Usage:** Save game file serialization/deserialization.

---

### zlib

- **Version:** 1.3.1
- **License:** zlib License
- **Purpose:** Compression library
- **Homepage:** https://zlib.net/
- **License URL:** https://zlib.net/zlib_license.html

**License Summary:** Permissive license allowing commercial use, modification, and distribution.

**Usage:** Save game file compression.

---

### libsodium

- **Version:** 1.0.18
- **License:** ISC License
- **Purpose:** Modern cryptographic library
- **Homepage:** https://libsodium.gitbook.io/
- **License URL:** https://github.com/jedisct1/libsodium/blob/master/LICENSE

**License Summary:** ISC is a permissive free software license similar to MIT/BSD. Allows commercial use, modification, and distribution with attribution.

**Usage:** Save game file encryption using ChaCha20-Poly1305 authenticated encryption.

---

## Build System Dependencies

### CMake

- **Version:** 3.28.1+
- **License:** BSD 3-Clause License
- **Purpose:** Cross-platform build system generator
- **Homepage:** https://cmake.org/
- **License URL:** https://gitlab.kitware.com/cmake/cmake/-/blob/master/Copyright.txt

**License Summary:** Permissive BSD license. Allows commercial use, modification, and distribution with attribution.

**Usage:** Build-time only (not distributed with application).

---

### Ninja

- **Version:** Latest (via CMakeToolchain)
- **License:** Apache License 2.0
- **Purpose:** Fast build system
- **Homepage:** https://ninja-build.org/
- **License URL:** https://github.com/ninja-build/ninja/blob/master/COPYING

**License Summary:** Permissive license allowing commercial use, modification, and distribution.

**Usage:** Optional build-time dependency (alternative to GNU Make).

---

## Test Dependencies

### Catch2

- **Version:** 3.4.0
- **License:** Boost Software License 1.0
- **Purpose:** C++ testing framework
- **Homepage:** https://github.com/catchorg/Catch2
- **License URL:** https://github.com/catchorg/Catch2/blob/devel/LICENSE.txt

**License Summary:** Permissive license similar to MIT/BSD. Allows commercial use, modification, and distribution.

**Usage:** Test-time only (not distributed with application). Only included when building with tests enabled.

---

## Package Manager

### Conan

- **Version:** 2.0+
- **License:** MIT License
- **Purpose:** C/C++ package manager
- **Homepage:** https://conan.io/
- **License URL:** https://github.com/conan-io/conan/blob/develop/LICENSE.md

**License Summary:** Permissive MIT license.

**Usage:** Development-time dependency management only (not distributed).

---

## Development Tools

### clang-format

- **Version:** 15+
- **License:** Apache License 2.0 with LLVM Exceptions
- **Purpose:** Code formatting tool
- **Homepage:** https://clang.llvm.org/docs/ClangFormat.html
- **License URL:** https://llvm.org/LICENSE.txt

**Usage:** Development-time only (code quality enforcement).

---

### clang-tidy

- **Version:** Latest
- **License:** Apache License 2.0 with LLVM Exceptions
- **Purpose:** Static analysis tool
- **Homepage:** https://clang.llvm.org/extra/clang-tidy/
- **License URL:** https://llvm.org/LICENSE.txt

**Usage:** Development-time only (code quality enforcement).

---

## License Compatibility Matrix

| Dependency | License | Commercial Use | Requires Attribution | Copyleft | Compatible with GPLv3 |
|-----------|---------|----------------|---------------------|----------|-------------------|
| Qt6 | LGPL 3.0 | ✅ | ✅ | ⚠️ (weak) | ✅ |
| spdlog | MIT | ✅ | ✅ | ❌ | ✅ |
| fmt | MIT | ✅ | ✅ | ❌ | ✅ |
| yaml-cpp | MIT | ✅ | ✅ | ❌ | ✅ |
| zlib | zlib | ✅ | ✅ | ❌ | ✅ |
| libsodium | ISC | ✅ | ✅ | ❌ | ✅ |
| CMake | BSD 3-Clause | ✅ | ✅ | ❌ | ✅ |
| Ninja | Apache 2.0 | ✅ | ✅ | ❌ | ✅ |
| Catch2 | Boost 1.0 | ✅ | ✅ | ❌ | ✅ |

---

## Attribution Requirements

The following libraries require attribution in distributed binaries:

1. **Qt6** - Copyright (C) 2024 The Qt Company Ltd
2. **spdlog** - Copyright (C) 2016 Gabi Melman
3. **yaml-cpp** - Copyright (C) 2008-2015 Jesse Beder
4. **libsodium** - Copyright (C) 2013-2024 Frank Denis

**Recommended Attribution:**
Include this THIRD_PARTY_LICENSES.md file in binary distributions or display attribution in the application's "About" dialog.

---

## Updates

This file should be updated whenever:
- New dependencies are added
- Dependency versions are upgraded
- Licenses change (check during upgrades)

**Last Updated:** 2026-03-05

---

For the main project license, see [LICENSE](LICENSE).
