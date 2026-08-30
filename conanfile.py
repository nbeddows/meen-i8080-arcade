from pathlib import Path
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout

class I8080ArcadeRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    options = {"with_framework": ["none", "qt", "sdl", "st7789vw"]}
    default_options = {"with_framework": "none"}

    def requirements(self):
        self.requires("meen/2.1.0")
        self.requires("meen_hw/0.5.0")
        self.requires("arduinojson/7.0.1")

        if self.options.get_safe("with_framework", "none") == "qt":
            self.requires("qt/6.11.1")
        elif self.options.get_safe("with_framework", "none") == "sdl":
            self.requires("sdl/2.28.5")

    def configure(self):
        if self.settings.os == "baremetal":
            if self.options.get_safe("with_framework", "none") == "qt":
                self.output.error("QT not available on baremetal platforms")
            elif self.options.get_safe("with_framework", "none") == "sdl":
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

        tc.generate()

        config = str(self.settings.build_type) #.upper()

        with open(Path(self.build_folder) / f"MeenDeployPath{config}.cmake", "w") as f:
            if self.settings.os == "Windows":
                meenBinDir = self.dependencies["meen"].cpp_info.bindirs[0].replace("\\", "/")
            else:
                meenBinDir = self.dependencies["meen"].cpp_info.libdirs[0].replace("\\", "/")

            f.write(f'set(MEEN_BIN_DIR_{config.upper()} "{meenBinDir}")\n')

            if self.dependencies["meen_hw"].options.shared:
                if self.settings.os == "Windows":
                    meenHwBinDir = self.dependencies["meen_hw"].cpp_info.bindirs[0].replace("\\", "/")
                else:
                    meenHwBinDir = self.dependencies["meen_hw"].cpp_info.libdirs[0].replace("\\", "/")

                f.write(f'set(MEENHW_BIN_DIR_{config.upper()} "{meenHwBinDir}")\n')

            if self.dependencies["meen"].options.get_safe("with_zlib", False) and self.dependencies["zlib"].options.shared:
                if self.settings.os == "Windows":
                    zlibBinDir = self.dependencies["zlib"].cpp_info.bindirs[0].replace("\\", "/")
                else:
                    zlibBinDir = self.dependencies["zlib"].cpp_info.libdirs[0].replace("\\", "/")

                f.write(f'set(ZLIB_BIN_DIR_{config.upper()} "{zlibBinDir}")\n')

            if self.options.get_safe("with_framework", "none") == "sdl":
                if self.settings.os == "Windows":
                    sdlBinDir = self.dependencies["sdl"].cpp_info.bindirs[0].replace("\\", "/")
                else:
                    sdlBinDir = self.dependencies["sdl"].cpp_info.libdirs[0].replace("\\", "/")

                f.write(f'set(SDL_BIN_DIR_{config.upper()} "{sdlBinDir}")\n')

            # TODO: need to test this on non Windows to make the directories are correct
            if self.options.get_safe("with_framework", "none") == "qt":
                package_folder = self.dependencies["qt"].package_folder.replace("\\", "/")
                f.write(f'set(QT_ROOT_DIR_{config.upper()} "{package_folder}")\n')

    def layout(self):
        cmake_layout(self)

        if self.settings.os == "Windows":
            self.folders.build = "output/build"
            self.folders.generators = "output/build/generators"
