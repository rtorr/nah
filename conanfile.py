import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class NahConan(ConanFile):
    name = "nah"
    version = "1.0.0"
    license = "MIT"
    author = "Ryan Torr"
    url = "https://github.com/rtorr/nah"
    description = "Native Application Host - deterministic launch contracts for native applications"
    topics = ("native", "application", "host", "launch", "contract", "manifest")

    settings = "os", "compiler", "build_type", "arch"
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
        "cmake/*",
        "include/*",
        "src/*",
    )

    def requirements(self):
        self.requires("nlohmann_json/3.11.3")
        self.requires("tomlplusplus/3.4.0")
        self.requires("zlib/1.3.1")
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
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["NAH_ENABLE_TESTS"] = False
        tc.variables["NAH_ENABLE_TOOLS"] = False
        tc.variables["NAH_INSTALL"] = False
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
        copy(
            self,
            "*.so*",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "lib"),
            keep_path=False,
        )
        copy(
            self,
            "*.dylib",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "lib"),
            keep_path=False,
        )
        copy(
            self,
            "*.dll",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "bin"),
            keep_path=False,
        )

    def package_info(self):
        # Main component that most users will want
        self.cpp_info.components["nahhost"].libs = ["nahhost"]
        self.cpp_info.components["nahhost"].requires = [
            "nah_contract",
            "nah_config",
            "nah_platform",
            "nah_packaging",
        ]

        # Contract composition
        self.cpp_info.components["nah_contract"].libs = ["nah_contract"]
        self.cpp_info.components["nah_contract"].requires = [
            "nah_manifest",
            "nah_config",
            "nah_platform",
            "nlohmann_json::nlohmann_json",
            "tomlplusplus::tomlplusplus",
        ]

        # Manifest encode/decode
        self.cpp_info.components["nah_manifest"].libs = ["nah_manifest"]
        # cpp-semver is header-only, bundled via FetchContent

        # TOML config loading
        self.cpp_info.components["nah_config"].libs = ["nah_config"]
        self.cpp_info.components["nah_config"].requires = ["tomlplusplus::tomlplusplus"]

        # Platform abstraction
        self.cpp_info.components["nah_platform"].libs = ["nah_platform"]

        # Packaging (NAP/NAK)
        self.cpp_info.components["nah_packaging"].libs = ["nah_packaging"]
        self.cpp_info.components["nah_packaging"].requires = [
            "nah_manifest",
            "nah_config",
            "nah_platform",
            "nah_contract",
            "zlib::zlib",
        ]
