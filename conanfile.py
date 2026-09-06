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
    package_type = "header-library"
    license = "MIT"
    author = "rtorr <rtorruellas@gmail.com>"
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

    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "include/*",
        "tools/**",
        "tests/**",
        "VERSION",
        "LICENSE",
    )

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
        self.requires("nlohmann_json/3.11.3", transitive_headers=True)
        self.requires("zlib/1.3.1", transitive_headers=True, transitive_libs=True)

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
        if self.options.build_tools:
            CMake(self).install()
            return

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

    def package_info(self):
        # Header-only library
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        core = self.cpp_info.components["core"]
        core.includedirs = ["include"]
        core.libdirs = []
        core.bindirs = []
        core.set_property("cmake_target_name", "NAH::core")

        library = self.cpp_info.components["nah"]
        library.includedirs = ["include"]
        library.libdirs = []
        library.bindirs = []
        library.requires = ["core", "nlohmann_json::nlohmann_json"]
        library.set_property("cmake_target_name", "NAH::nah")

        package = self.cpp_info.components["package"]
        package.includedirs = ["include"]
        package.libdirs = []
        package.bindirs = []
        package.requires = ["core", "zlib::zlib"]
        package.set_property("cmake_target_name", "NAH::package")

        # If tools were built, add bin directory
        if self.options.build_tools:
            self.cpp_info.bindirs = ["bin"]

    def package_id(self):
        # Header-only package, remove all settings except OS for tool compatibility
        if not self.info.options.build_tools:
            self.info.clear()
