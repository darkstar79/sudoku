from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class SudokuConan(ConanFile):
    name = "sudoku"
    version = "1.0.0"
    
    # Package metadata
    license = "GPL-3.0-or-later"
    description = "Offline Sudoku Game"
    topics = ("sudoku", "game", "desktop", "cpp23")
    
    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tests": [True, False]
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tests": True
    }
    
    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "tests/*"
    
    def configure(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")
    
    def requirements(self):
        # Core dependencies
        # Note: Qt6 is provided by system packages (dnf install qt6-qtbase-devel)
        self.requires("spdlog/1.15.3")
        self.requires("yaml-cpp/0.8.0")
        self.requires("zlib/1.3.1")  # For save file compression
        self.requires("libsodium/1.0.18")  # For save file encryption

        # Testing framework (conditional)
        if self.options.with_tests:
            self.requires("catch2/3.4.0")
    
    def build_requirements(self):
        self.tool_requires("cmake/3.31.11")
    
    def layout(self):
        # Simplified layout: build/Debug or build/Release (no nesting)
        # This creates a cleaner structure than cmake_layout's multi-config approach
        self.folders.build = f"build/{str(self.settings.build_type)}"
        self.folders.generators = f"build/{str(self.settings.build_type)}/generators"
    
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        
        tc = CMakeToolchain(self)
        # Use Ninja generator for faster builds
        tc.generator = "Ninja"
        # Don't generate CMakeUserPresets.json (repo-owned CMakePresets.json handles presets)
        tc.user_presets_path = False
        
        # C++23 configuration
        tc.variables["CMAKE_CXX_STANDARD"] = "23"
        tc.variables["CMAKE_CXX_STANDARD_REQUIRED"] = "ON"
        tc.variables["CMAKE_CXX_EXTENSIONS"] = "OFF"
        
        # Static linking for single executable distribution
        if self.settings.os == "Windows":
            tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        
        tc.generate()
        
        # Qt6 is provided by system packages, no Conan integration needed
        self.output.info("Qt6 is expected from system packages (qt6-qtbase-devel)")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

        # Note: Tests are built but not run automatically.
        # To run tests manually:
        #   ./build/Release/bin/tests/unit_tests
        #   ./build/Release/bin/tests/integration_tests

    # def configure_options(self):
    #     # Configure dependency options for single executable
    #     self.options["spdlog"].shared = False
    #     self.options["yaml-cpp"].shared = False