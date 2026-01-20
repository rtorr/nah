import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load


def get_version():
    try:
        version_file = os.path.join(os.path.dirname(__file__), "VERSION")
        return load(None, version_file).strip()
    except Exception:
        return "1.0.0"


class NahConan(ConanFile):
    name = "nah"
    version = get_version()
    license = "MIT"
    author = "Ryan Torr"
    url = "https://github.com/rtorr/nah"
    description = "Native Application Host - deterministic launch contracts for native applications"
    topics = (
        "native",
        "application",
        "host",
        "launch",
        "contract",
        "manifest",
        "header-only",
    )

    settings = "os", "compiler", "build_type", "arch"

    # Export headers
    exports_sources = "include/*", "VERSION", "LICENSE"

    # Tool build options
    options = {
        "build_tools": [True, False],
        "build_tests": [True, False],
    }
    default_options = {
        "build_tools": True,
        "build_tests": False,
    }

    @property
    def _min_cppstd(self):
        return "17"

    def requirements(self):
        # Required for the header-only library
        self.requires("nlohmann_json/3.11.3")

        # Required for tools (CLI)
        if self.options.build_tools:
            # CLI11 will be fetched via FetchContent
            pass

    def validate(self):
        check_min_cppstd(self, self._min_cppstd)

    def layout(self):
        cmake_layout(self, src_folder=".")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["NAH_ENABLE_TOOLS"] = self.options.build_tools
        tc.variables["NAH_ENABLE_TESTS"] = self.options.build_tests
        tc.variables["CMAKE_POLICY_DEFAULT_CMP0091"] = "NEW"  # MSVC runtime selection
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        if self.options.build_tools:
            cmake = CMake(self)
            cmake.configure()
            cmake.build()

    def package(self):
        # Copy LICENSE
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )

        # Copy header files (the library)
        copy(
            self,
            "*.h",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
            keep_path=True,
        )

        # Copy the CLI tool if built
        if self.options.build_tools:
            # Unix binary
            copy(
                self,
                "nah",
                src=os.path.join(self.build_folder, "tools", "nah"),
                dst=os.path.join(self.package_folder, "bin"),
                keep_path=False,
            )
            # Windows binary
            copy(
                self,
                "nah.exe",
                src=os.path.join(self.build_folder, "tools", "nah"),
                dst=os.path.join(self.package_folder, "bin"),
                keep_path=False,
            )
            # Also check Release subdirectory for Windows
            copy(
                self,
                "nah.exe",
                src=os.path.join(self.build_folder, "tools", "nah", "Release"),
                dst=os.path.join(self.package_folder, "bin"),
                keep_path=False,
            )

    def package_info(self):
        # Header-only library
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        # Headers
        self.cpp_info.includedirs = ["include"]

        # Requires C++17
        self.cpp_info.cxxflags = []
        self.cpp_info.set_property("cmake_target_name", "nah::core")

        # Define NAH_CORE for consuming packages
        self.cpp_info.defines = []

        # If tools were built, add bin directory
        if self.options.build_tools:
            self.cpp_info.bindirs = ["bin"]

    def package_id(self):
        # Header-only package, remove all settings except OS for tool compatibility
        if not self.info.options.build_tools:
            self.info.clear()
