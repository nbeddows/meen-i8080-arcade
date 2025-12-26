import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout

class I8080ArcadeRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    options = {"with_framework": ["none", "sdl", "st7789vw"]}
    default_options = {"with_framework": "none"}

    def requirements(self):
        self.requires("meen/2.1.0")
        self.requires("meen_hw/0.5.0")
        self.requires("arduinojson/7.0.1")

        if self.options.get_safe("with_framework", "none") == "sdl":
            self.requires("sdl/2.28.5")

    def configure(self):
        if self.settings.os == "baremetal":
            if self.options.get_safe("with_framework", "none") == "sdl":
                self.output.error("SDL not available on baremetal platforms")
        else:
            if self.options.get_safe("with_framework", "none") == "st7789vw":
                self.output.error("st7789vw not available on non-baremetal platforms")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)

        tc.cache_variables["enable_board"] = self.dependencies["meen"].options.get_safe("with_board", "none")
        tc.cache_variables["enable_framework"] = self.options.get_safe("with_framework", "none")
        tc.variables["build_os"] = self.settings.os
        tc.variables["build_arch"] = self.settings.arch
        tc.variables["archive_dir"] = self.cpp_info.libdirs[0]
        tc.variables["runtime_dir"] = self.cpp_info.bindirs[0]

        if self.settings.os == "Windows":
            tc.cache_variables["meenBinDir"] = self.dependencies["meen"].cpp_info.bindirs[0].replace("\\", "/")

            if self.dependencies["meen_hw"].options.shared:
                tc.cache_variables["meenHwBinDir"] = self.dependencies["meen_hw"].cpp_info.bindirs[0].replace("\\", "/")

            if self.dependencies["meen"].options.get_safe("with_zlib", False) and self.dependencies["zlib"].options.shared:
                tc.cache_variables["zlibBinDir"] = self.dependencies["zlib"].cpp_info.bindirs[0].replace("\\", "/")

            if self.options.get_safe("with_framework", "none") == "sdl":
                if self.dependencies["sdl"].options.shared:
                    tc.cache_variables["sdlBinDir"] = self.dependencies["sdl"].cpp_info.bindirs[0].replace("\\", "/")
        else:
            tc.cache_variables["meenBinDir"] = self.dependencies["meen"].cpp_info.libdirs[0].replace("\\", "/")

            if self.dependencies["meen_hw"].options.shared:
                tc.cache_variables["meenHwBinDir"] = self.dependencies["meen_hw"].cpp_info.libdirs[0].replace("\\", "/")

            if self.dependencies["meen"].options.get_safe("with_zlib", False) and self.dependencies["zlib"].options.shared:
                tc.cache_variables["zlibBinDir"] = self.dependencies["zlib"].cpp_info.libdirs[0].replace("\\", "/")

            if self.options.get_safe("with_framework", "none") == "sdl":
                if self.dependencies["sdl"].options.shared:
                    tc.cache_variables["sdlBinDir"] = self.dependencies["sdl"].cpp_info.libdirs[0].replace("\\", "/")

        tc.generate()

    def layout(self):
        cmake_layout(self)

        if self.settings.os == "Windows":
            self.folders.build = "output/build"
            self.folders.generators = "output/build/generators"
