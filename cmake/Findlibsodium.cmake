# Findlibsodium.cmake — Fallback find module for libsodium
# Used when Conan is not available (e.g., Flatpak builds).
# Creates the libsodium::libsodium imported target.

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(_SODIUM QUIET libsodium)
endif()

find_path(libsodium_INCLUDE_DIR
    NAMES sodium.h
    HINTS ${_SODIUM_INCLUDE_DIRS}
)

find_library(libsodium_LIBRARY
    NAMES sodium
    HINTS ${_SODIUM_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libsodium
    REQUIRED_VARS libsodium_LIBRARY libsodium_INCLUDE_DIR
)

if(libsodium_FOUND AND NOT TARGET libsodium::libsodium)
    add_library(libsodium::libsodium UNKNOWN IMPORTED)
    set_target_properties(libsodium::libsodium PROPERTIES
        IMPORTED_LOCATION "${libsodium_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${libsodium_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(libsodium_INCLUDE_DIR libsodium_LIBRARY)
