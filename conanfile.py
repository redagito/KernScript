from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout


class KernScriptConan(ConanFile):
    name = "kernscript"
    version = "0.1.0"

    settings = "os", "compiler", "build_type", "arch"

    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("catch2/3.16.0")
        self.requires("spdlog/1.17.0")
        # Transitive dependency of spdlog, used by the ILogger interface for
        # message formatting. Declared directly so the CMake target exists.
        self.requires("fmt/12.1.0")

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test()
