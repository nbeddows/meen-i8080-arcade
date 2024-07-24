import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout

class MachuEmuPackageTest(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("mach_emu/1.6.2")
        self.requires("meen_hw/0.1.0")
        self.requires("nlohmann_json/3.11.3")
        self.requires("popl/1.3.0")
        self.requires("sdl/2.28.5")
        self.requires("sdl_mixer/2.8.0")

    def configure(self):
        self.options["meen_hw/*"].with_i8080_arcade = True
        self.options["sdl/*"].shared = True
        self.options["sdl/*"].alsa = False
        self.options["sdl/*"].pulse = True
        self.options["sdl/*"].x11 = True
        self.options["sdl/*"].xcursor = False
        self.options["sdl/*"].xinerama = False
        self.options["sdl/*"].xinput = False
        self.options["sdl/*"].xrandr = False
        self.options["sdl/*"].xscrnsaver = False
        self.options["sdl/*"].xshape = False
        self.options["sdl/*"].xvm = False
        self.options["sdl/*"].wayland = False
        self.options["sdl/*"].libunwind = False
        self.options["sdl/*"].opengl = False
        self.options["sdl/*"].opengles = False
        self.options["sdl/*"].vulkan = False
        self.options["sdl_mixer/*"].shared = True
        self.options["sdl_mixer/*"].flac = False
        self.options["sdl_mixer/*"].mpg123 = False
        self.options["sdl_mixer/*"].mad = False
        self.options["sdl_mixer/*"].ogg = False
        self.options["sdl_mixer/*"].opus = False
        self.options["sdl_mixer/*"].mikmod = False
        self.options["sdl_mixer/*"].modplug = False
        self.options["sdl_mixer/*"].nativemidi = False
        self.options["sdl_mixer/*"].tinymidi = False

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)

        if self.settings.os == "Windows":            
            tc.cache_variables["machEmuBinDir"] = self.dependencies["mach_emu"].cpp_info.bindirs[0].replace("\\", "/")

            if self.dependencies["meen_hw"].options.shared:
                tc.cache_variables["meenHwBinDir"] = self.dependencies["meen_hw"].cpp_info.bindirs[0].replace("\\", "/")

            if self.dependencies["sdl"].options.shared:
                tc.cache_variables["sdlBinDir"] = self.dependencies["sdl"].cpp_info.bindirs[0].replace("\\", "/")

            if self.dependencies["sdl_mixer"].options.shared:
                tc.cache_variables["sdlMixerBinDir"] = self.dependencies["sdl_mixer"].cpp_info.bindirs[0].replace("\\", "/")

            if self.dependencies["mach_emu"].options.with_zlib and self.dependencies["zlib"].options.shared:
                tc.cache_variables["zlibBinDir"] = self.dependencies["zlib"].cpp_info.bindirs[0].replace("\\", "/")
        else:
            tc.cache_variables["machEmuBinDir"] = self.dependencies["mach_emu"].cpp_info.libdirs[0].replace("\\", "/")

            if self.dependencies["meen_hw"].options.shared:
                tc.cache_variables["meenHwBinDir"] = self.dependencies["meen_hw"].cpp_info.libdirs[0].replace("\\", "/")
    
            if self.dependencies["sdl"].options.shared:
                tc.cache_variables["sdlBinDir"] = self.dependencies["sdl"].cpp_info.libdirs[0].replace("\\", "/")

            if self.dependencies["sdl_mixer"].options.shared:
                tc.cache_variables["sdlMixerBinDir"] = self.dependencies["sdl_mixer"].cpp_info.libdirs[0].replace("\\", "/")

            if self.dependencies["mach_emu"].options.with_zlib and self.dependencies["zlib"].options.shared:
                tc.cache_variables["zlibBinDir"] = self.dependencies["zlib"].cpp_info.libdirs[0].replace("\\", "/")

        tc.generate()

    def layout(self):
        cmake_layout(self)