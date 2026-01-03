import os

from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import copy


class GameEngineSDK(ConanFile):
    """
    Example SDK that aggregates multiple Conan dependencies into a single NAK.

    This represents a realistic SDK that:
    - Provides a game engine runtime with networking, compression, and crypto
    - Has a loader binary that bootstraps applications
    - Bundles all transitive dependencies for the app developer

    Apps using this SDK just declare:
        nak_id = "com.example.gameengine"
        nak_version_req = "^1.0.0"

    They don't need to know about zlib, openssl, curl, etc.
    """

    name = "gameengine"
    version = "1.0.0"
    license = "MIT"
    description = "Example game engine SDK demonstrating Conan-to-NAK workflow"

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    exports_sources = "src/*", "include/*", "bin/*", "CMakeLists.txt"

    # Real dependencies that would be in a game engine
    def requirements(self):
        self.requires("zlib/1.3.1")  # Compression
        self.requires("openssl/3.2.1")  # Crypto/TLS
        self.requires("libcurl/8.6.0")  # HTTP client
        self.requires("nlohmann_json/3.11.3")  # JSON parsing (header-only)
        self.requires("spdlog/1.13.0")  # Logging
        self.requires("fmt/10.2.1")  # String formatting

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        # Headers
        copy(
            self,
            "*.h",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
        )
        copy(
            self,
            "*.hpp",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
        )

        # Libraries from build
        copy(
            self,
            "*.a",
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

        # Loader binary
        copy(
            self,
            "engine-loader",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "bin"),
            keep_path=False,
        )
        copy(
            self,
            "engine-loader.exe",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "bin"),
            keep_path=False,
        )

        # Resources
        copy(
            self,
            "*",
            src=os.path.join(self.source_folder, "resources"),
            dst=os.path.join(self.package_folder, "resources"),
        )

    def package_info(self):
        self.cpp_info.libs = ["gameengine"]
        self.cpp_info.includedirs = ["include"]

        # NAH properties for NAK generation
        self.cpp_info.set_property("nah:nak_id", "com.example.gameengine")
        self.cpp_info.set_property("nah:loader_exec", "bin/engine-loader")
        self.cpp_info.set_property(
            "nah:loader_args",
            [
                "--app-entry",
                "{NAH_APP_ENTRY}",
                "--app-root",
                "{NAH_APP_ROOT}",
                "--app-id",
                "{NAH_APP_ID}",
                "--engine-root",
                "{NAH_NAK_ROOT}",
            ],
        )
        self.cpp_info.set_property("nah:resource_root", "resources")
        self.cpp_info.set_property("nah:cwd", "{NAH_APP_ROOT}")
        self.cpp_info.set_property(
            "nah:environment",
            {"GAMEENGINE_VERSION": self.version, "GAMEENGINE_LOG_LEVEL": "info"},
        )
