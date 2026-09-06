import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout


class NahTestConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            cmd = f"{self.cpp.build.bindir}/test_nah"
            self.run(cmd, env="conanrun")
            dependency = self.dependencies[self.tested_reference_str]
            if str(dependency.options.build_tools).lower() == "true":
                executable = "nah.exe" if self.settings.os == "Windows" else "nah"
                tool = os.path.join(dependency.package_folder, "bin", executable)
                self.run(f'"{tool}" --version', env="conanrun")
