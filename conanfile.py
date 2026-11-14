from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


class KernScriptConan(ConanFile):
    name = "kernscript"
    version = "1.0.0"

    settings = "os", "compiler", "build_type", "arch"

    options = {
        "with_tests": [True, False],
        "with_samples": [True, False],
        "with_gui_samples": [True, False],
        "with_benchmarks": [True, False],
    }
    default_options = {
        "with_tests": True,
        "with_samples": False,
        "with_gui_samples": False,
        "with_benchmarks": False,
    }
    generators = "CMakeDeps"

    def requirements(self):
        if self.options.with_tests:
            self.requires("catch2/3.16.0")
        self.requires("spdlog/1.17.0")
        # Transitive dependency of spdlog, used by the ILogger interface for
        # message formatting. Declared directly so the CMake target exists.
        self.requires("fmt/12.1.0")
        # Cross-platform GUI toolkit for the FltkEvents sample, the portable
        # twin of the Win32-only NativeEvents sample.
        if self.options.with_gui_samples:
            self.requires("fltk/1.4.4")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTING"] = bool(self.options.with_tests)
        tc.variables["KERNSCRIPT_BUILD_SAMPLES"] = bool(self.options.with_samples)
        tc.variables["KERNSCRIPT_BUILD_GUI_SAMPLES"] = bool(self.options.with_gui_samples)
        tc.variables["KERNSCRIPT_BUILD_BENCHMARKS"] = bool(self.options.with_benchmarks)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test()
