/*
Copyright (c) 2021-2024 Nicolas Beddows <nicolas.beddows@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "meen/MachineFactory.h"

static std::string gameRom = "space-invaders";

#ifdef ENABLE_MH_RP2040
#include <pico/stdlib.h>
#include "i8080_arcade/RPIoController.h"
extern char rpConfigStart;
extern char rpConfigEnd;
#else
#include <fstream>
#include <filesystem>
#include <memory>
#include <popl.hpp>
#include "i8080_arcade/SdlIoController.h"

using namespace popl;

static std::filesystem::path configFile;
static std::filesystem::path romFilePath;
static std::filesystem::path audioFilePath;
static std::filesystem::path saveFilePath;

static int ParseCmdLine(int argc, char** argv)
{
    OptionParser op("Allowed options");
    auto helpOpt = op.add<Switch>("h", "help", "produce this help message");
    auto configFileOpt = op.add<Value<std::string>>("c", "config-file", "i8080 arcade configuration file", "conf/config.json");
    auto romFilePathOpt = op.add<Value<std::string>>("r", "rom-file-path", "Path to the i8080 arcade rom files directory", "rom-files");
    auto audioFilePathOpt = op.add<Value<std::string>>("a", "audio-file-path", "Path to the i8080 arcade audio files directory", "audio-files");
    auto saveFilePathOpt = op.add<Value<std::string>>("s", "save-file-path", "Path to the i8080 arcade save files directory", "save-files");
    auto gameRomOpt = op.add<Value<std::string>>("g", "game", "The name of the i8080 arcade game to load as defined in the config file", "space-invaders");
    op.parse(argc, argv);
    auto helpCount = helpOpt->count();

    if (helpCount > 0)
    {
        switch(helpCount)
        {
            case 1:
            {
                std::cout << op << std::endl;
                break;
            }
            case 2:
            {
                std::cout << op.help(Attribute::advanced) << std::endl;
                break;
            }
            default:
            {
                std::cout << op.help(Attribute::expert) << std::endl;
                break;
            }
        }

        // print help then exit
        return -1;
    }

    configFile = configFileOpt->value();
    romFilePath = romFilePathOpt->value();
    audioFilePath = audioFilePathOpt->value();
    saveFilePath = saveFilePathOpt->value();
    gameRom = gameRomOpt->value();
    return 0;
}
#endif // ENABLE_MH_RP2040

int main(int argc, char** argv)
{
    std::string meenConfig;
    JsonDocument json;
#ifdef ENABLE_MH_RP2040
    stdio_init_all();
    // Open the configuration file, see the README for an explanation of each configuration option
    //cppcheck-suppress comparePointers
    auto err = deserializeJson(json, std::string(&rpConfigStart, &rpConfigEnd - &rpConfigStart));
#else
    if (ParseCmdLine(argc, argv) < 0)
    {
        // We return < 0 when we print the help, exit.
        return 0;
    }

    // Open the configuration file, see the README for an explanation of each configuration option
    std::ifstream fin(configFile);
    auto err = deserializeJson(json, fin);
#endif // ENABLE_MH_RP2040
    if(err)
    {
        printf("Parse error while deserializing json config file\n");
        return -1;
    }

    auto arcade = json["i8080-arcade"];

    if(arcade == nullptr)
    {
        printf("Invalid json config file format: i8080-arcade section not found\n");
        return -1;
    }

    auto software = arcade["software"];

    if(software == nullptr)
    {
        printf("Invalid json config file format: software section not found\n");
        return -1;
    }

    auto arcadeGame = software[gameRom];

    if(arcadeGame == nullptr)
    {
        printf("The game space-invaders does not exist in the software section of the config file");
        return -1;
    }

    auto hardware = arcade["hardware"];

    if(hardware == nullptr)
    {
        printf("Invalid json config file format: hardware section not found\n");
        return -1;
    }

    serializeJson(hardware["mach-emu"], meenConfig);

    if(meenConfig.empty() == true)
    {
        printf("Parse error while serializing hardware:mach_emu\n");
        return -1;
    }

    // Create our custom i8080 arcade machine
    auto machine = MachEmu::MakeMachine(meenConfig.c_str());
    // Create our custom i8080 arcade memory controller. Two video frames for double buffering.
    auto memoryController = std::make_shared<i8080_arcade::MemoryController>(2);
#ifdef ENABLE_MH_RP2040
    // load roms/textures/(audio samples)
    memoryController->LoadRoms(arcadeGame["memory"]["rom"]["file"]);
    // Create our custom i8080 arcade I/O controller based on a specific configuration.
    auto ioController = std::make_shared<i8080_arcade::RPIoController>(memoryController, hardware["audio"], hardware["video"]);
#else
    // load roms/textures/(audio samples)
    memoryController->LoadRoms(romFilePath, arcadeGame["memory"]["rom"]["file"]);
    // Create our custom i8080 arcade I/O controller based on a specific configuration.
    auto ioController = std::make_shared<i8080_arcade::SDLIoController>(memoryController, hardware["audio"], hardware["video"]);
    ioController->LoadAudioSamples(audioFilePath, software["audio"]);

    // Will be called from a different thread, this is a simple implementation which will overwrite the previous save file
    machine->OnSave([](const char* json)
    {
        // Need to make a copy of the json if you want to hold the json string longer than the scope of this function
        std::filesystem::create_directory(saveFilePath);
        std::ofstream fout((saveFilePath / gameRom).string() + ".json", std::ios::trunc);
        fout.exceptions(fout.failbit);
        fout.write(json, strlen(json));
    });

    // Will be accessed from a different thread
    std::string loadJson;
    // Will be called from a different thread
    machine->OnLoad([&loadJson]
    {
        std::ifstream fin((saveFilePath / gameRom).string() + ".json", std::ios::ate);
        fin.exceptions(fin.failbit);
        auto len = fin.tellg();
        fin.seekg(0, std::ios::beg);
        loadJson.resize(len);
        fin.read(loadJson.data(), len);
        return loadJson.c_str();
    });
#endif // ENABLE_MH_RP2040
    ioController->LoadVideoTextures(software["video"]);

    // Load the memory layout into the machine
    serializeJson(arcadeGame["memory"], meenConfig);

    if(meenConfig.empty() == true)
    {
        printf("Parse error while serializing arcadeGame:memory\n");
        return -1;
    }

    machine->SetOptions(meenConfig.c_str());
    machine->SetMemoryController(memoryController);
    machine->SetIoController(ioController);
    // Run the machine asynchronously, the machine now owns the controllers and they should not be accessed
    machine->Run(0x00);
    // Run the io event loop until the 'q' key is pressed or the window is closed, or in the case of st7889vw top left button is pressed
    ioController->EventLoop();
    // Wait for the machine to finish, once complete the controllers can be accessed safely
    machine->WaitForCompletion();
    return 0;
}
