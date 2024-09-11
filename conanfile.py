import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout

class I8080ArcadeRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    options = {"with_sdl": [True, False], "with_st7789vw": [True, False]}
    default_options = {"with_sdl": False, "with_st7789vw": False}

    def requirements(self):
        self.requires("mach_emu/2.0.0")
        self.requires("meen_hw/0.2.1")
        self.requires("arduinojson/7.0.1")

        if not self.settings.os == "baremetal":
            self.requires("popl/1.3.0")

        if self.options.get_safe("with_sdl", False):
            self.requires("sdl/2.28.5")
            self.requires("sdl_mixer/2.8.0")

    def config_options(self):
        if self.settings.os == "baremetal":
            self.options.rm_safe("with_sdl");
        else:
            self.options.rm_safe("with_st7789vw")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)

        tc.cache_variables["enable_sdl"] = self.options.get_safe("with_sdl", False)
        tc.cache_variables["enable_rp2040"] = self.dependencies["mach_emu"].options.get_safe("with_rp2040", False)
        tc.cache_variables["enable_st7789vw"] = self.options.get_safe("with_st7789vw", False)
        tc.variables["build_os"] = self.settings.os
        tc.variables["build_arch"] = self.settings.arch
        tc.variables["archive_dir"] = self.cpp_info.libdirs[0]
        tc.variables["runtime_dir"] = self.cpp_info.bindirs[0]

        if self.settings.os == "Windows":
            tc.cache_variables["machEmuBinDir"] = self.dependencies["mach_emu"].cpp_info.bindirs[0].replace("\\", "/")

            if self.dependencies["meen_hw"].options.shared:
                tc.cache_variables["meenHwBinDir"] = self.dependencies["meen_hw"].cpp_info.bindirs[0].replace("\\", "/")

            if self.dependencies["mach_emu"].options.get_safe("with_zlib", False) and self.dependencies["zlib"].options.shared:
                tc.cache_variables["zlibBinDir"] = self.dependencies["zlib"].cpp_info.bindirs[0].replace("\\", "/")

            if self.options.get_safe("with_sdl", False):
                if self.dependencies["sdl"].options.shared:
                    tc.cache_variables["sdlBinDir"] = self.dependencies["sdl"].cpp_info.bindirs[0].replace("\\", "/")

                if self.dependencies["sdl_mixer"].options.shared:
                    tc.cache_variables["sdlMixerBinDir"] = self.dependencies["sdl_mixer"].cpp_info.bindirs[0].replace("\\", "/")

        else:
            tc.cache_variables["machEmuBinDir"] = self.dependencies["mach_emu"].cpp_info.libdirs[0].replace("\\", "/")

            if self.dependencies["meen_hw"].options.shared:
                tc.cache_variables["meenHwBinDir"] = self.dependencies["meen_hw"].cpp_info.libdirs[0].replace("\\", "/")

            if self.dependencies["mach_emu"].options.get_safe("with_zlib", False) and self.dependencies["zlib"].options.shared:
                tc.cache_variables["zlibBinDir"] = self.dependencies["zlib"].cpp_info.libdirs[0].replace("\\", "/")

            if self.options.get_safe("with_sdl", False):
                if self.dependencies["sdl"].options.shared:
                    tc.cache_variables["sdlBinDir"] = self.dependencies["sdl"].cpp_info.libdirs[0].replace("\\", "/")

                if self.dependencies["sdl_mixer"].options.shared:
                    tc.cache_variables["sdlMixerBinDir"] = self.dependencies["sdl_mixer"].cpp_info.libdirs[0].replace("\\", "/")

        tc.generate()

    def layout(self):
        cmake_layout(self)