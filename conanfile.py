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
    topics = ("native", "application", "host", "launch", "contract", "manifest")

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    exports_sources = (
        "CMakeLists.txt",
        "VERSION",
        "cmake/*",
        "include/*",
        "src/*",
    )

    def requirements(self):
        self.requires("nlohmann_json/3.11.3")
        self.requires("tomlplusplus/3.4.0")
        self.requires("zlib/1.3.1")
        self.requires("openssl/3.2.1")
        self.requires("libcurl/8.6.0")
        # cpp-semver is fetched via FetchContent (not in ConanCenter)

    def build_requirements(self):
        pass  # No build-only requirements

    def validate(self):
        check_min_cppstd(self, "17")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["NAH_ENABLE_TESTS"] = False
        tc.variables["NAH_ENABLE_TOOLS"] = False
        tc.variables["NAH_INSTALL"] = False
        # Pass shared option to CMake
        tc.variables["NAH_BUILD_SHARED"] = bool(self.options.shared)
        # Ensure fPIC is set for shared libraries on non-Windows
        if self.settings.os != "Windows":
            tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = bool(
                self.options.get_safe("fPIC", True) or self.options.shared
            )
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        copy(
            self,
            "*.hpp",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
        )
        copy(
            self,
            "*.h",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
        )
        # Copy cpp-semver headers (fetched via FetchContent during build)
        copy(
            self,
            "*.hpp",
            src=os.path.join(self.build_folder, "_deps", "cpp-semver-src", "include"),
            dst=os.path.join(self.package_folder, "include"),
        )

        # Copy static libraries
        copy(
            self,
            "*.a",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "lib"),
            keep_path=False,
        )
        copy(
            self,
            "*.lib",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "lib"),
            keep_path=False,
        )

        # Copy shared libraries
        if self.options.shared:
            # Linux .so files (including versioned symlinks)
            copy(
                self,
                "*.so*",
                src=self.build_folder,
                dst=os.path.join(self.package_folder, "lib"),
                keep_path=False,
            )
            # macOS .dylib files
            copy(
                self,
                "*.dylib",
                src=self.build_folder,
                dst=os.path.join(self.package_folder, "lib"),
                keep_path=False,
            )
            # Windows .dll files go in bin
            copy(
                self,
                "*.dll",
                src=self.build_folder,
                dst=os.path.join(self.package_folder, "bin"),
                keep_path=False,
            )

    def package_info(self):
        if self.options.shared:
            # Single shared library with everything
            self.cpp_info.libs = ["nahhost"]
            self.cpp_info.defines = ["NAH_SHARED"]
        else:
            # Static libraries - order matters for linker
            # On Windows, static lib is named nahhost_static to avoid conflict with DLL import lib
            nahhost_lib = (
                "nahhost_static" if self.settings.os == "Windows" else "nahhost"
            )
            self.cpp_info.components["nahhost"].libs = [nahhost_lib]
            self.cpp_info.components["nahhost"].requires = [
                "nah_contract",
                "nah_config",
                "nah_platform",
                "nah_packaging",
                "nah_materializer",
            ]

            self.cpp_info.components["nah_materializer"].libs = ["nah_materializer"]
            self.cpp_info.components["nah_materializer"].requires = [
                "nah_packaging",
                "nah_platform",
                "openssl::crypto",
                "libcurl::libcurl",
            ]

            self.cpp_info.components["nah_contract"].libs = ["nah_contract"]
            self.cpp_info.components["nah_contract"].requires = [
                "nah_manifest",
                "nah_config",
                "nah_platform",
                "nlohmann_json::nlohmann_json",
                "tomlplusplus::tomlplusplus",
            ]

            self.cpp_info.components["nah_manifest"].libs = ["nah_manifest"]

            self.cpp_info.components["nah_config"].libs = ["nah_config"]
            self.cpp_info.components["nah_config"].requires = [
                "tomlplusplus::tomlplusplus"
            ]

            self.cpp_info.components["nah_platform"].libs = ["nah_platform"]

            self.cpp_info.components["nah_packaging"].libs = ["nah_packaging"]
            self.cpp_info.components["nah_packaging"].requires = [
                "nah_manifest",
                "nah_config",
                "nah_platform",
                "nah_contract",
                "zlib::zlib",
            ]
