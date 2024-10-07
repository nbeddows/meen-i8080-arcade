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

#include "i8080_arcade/MIA_Types.h"
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
    auto event = i8080_arcade::MIA_Event::None;
    std::array<std::string, 5> roms = { "space-invaders", "space-invaders-ii", "space-invaders-deluxe", "balloon-bomber", "lunar-rescue" };
    int romIndex = 0;

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
    // Create our custom i8080 arcade memory controller. Three video frames for triple buffering.
    auto memoryController = std::make_shared<i8080_arcade::MemoryController>(3);

    auto loadRomAndMemLayout = [&](const std::string& romName)
    {
        const auto& arcadeGame = software[romName];

        if (arcadeGame == nullptr)
        {
            printf("The rom %s does not exist in the software section of the config file\n", romName.c_str());
            return -1;
        }

#ifdef ENABLE_MH_RP2040
        if (memoryController->LoadRoms(arcadeGame["memory"]["rom"]["file"]) < 0)
#else
        if (memoryController->LoadRoms(romFilePath, arcadeGame["memory"]["rom"]["file"]) < 0)
#endif
        {
            printf("Memory controller failed to load rom %s\n", romName.c_str());
        }

        // Load the memory layout into the machine
        serializeJson(software[gameRom]["memory"]/*arcadeGame["memory"]*/, meenConfig);

        if (meenConfig.empty() == true)
        {
            printf("Parse error while serializing %s memory\n", romName.c_str());
            return -1;
        }

        auto ec = machine->SetOptions(meenConfig.c_str());

        if(ec)
        {
            printf("Failed to set %s memory layout: %s", romName.c_str(), ec.message().c_str());
            return -1;
        }

        return 0;
    };

#ifdef ENABLE_MH_RP2040
    // Create our custom i8080 arcade I/O controller based on a specific configuration.
    auto ioController = std::make_shared<i8080_arcade::RPIoController>(memoryController, hardware["audio"], hardware["video"]);
#else
    // Create our custom i8080 arcade I/O controller based on a specific configuration.
    auto ioController = std::make_shared<i8080_arcade::SDLIoController>(memoryController, hardware["audio"], hardware["video"]);
    ioController->LoadAudioSamples(audioFilePath, software["audio"]);

    // Will be called from a different thread, this is a simple implementation which will overwrite the previous save file
    auto ec = machine->OnSave([](const char* json)
    {
        std::error_code ec;
        // Need to make a copy of the json if you want to hold the json string longer than the scope of this function
        std::filesystem::create_directory(saveFilePath, ec);
        
        if (!ec)
        {
            std::ofstream fout((saveFilePath / gameRom).string() + ".json", std::ios::trunc);

            if (fout)
            {
                fout.write(json, strlen(json));
            }
        }
    });

    // Will be accessed from a different thread
    std::string loadJson;
    // Will be called from a different thread
    ec = machine->OnLoad([&loadJson]
    {
        const char* json = nullptr;
        std::ifstream fin((saveFilePath / gameRom).string() + ".json", std::ios::ate);

        if(fin)
        {
            auto len = fin.tellg();
            fin.seekg(0, std::ios::beg);
            loadJson.resize(len);
            fin.read(loadJson.data(), len);
            json = loadJson.c_str();
        }
        
        return json;
    });
#endif // ENABLE_MH_RP2040
    auto ret = loadRomAndMemLayout(gameRom);
    
    if (ret < 0)
    {
        return ret;
    }

    ret = ioController->LoadVideoTextures(software["video"]);

    if (ret < 0)
    {
        printf("Failed to create rendering surfaces\n");
        return ret;
    }

    ec = machine->SetMemoryController(memoryController);
   
    if (ec)
    {
        printf("Failed to set the memory controller: %s\n", ec.message().c_str());
    }

    ec = machine->SetIoController(ioController);

    if (ec)
    {
        printf("Failed to set the io controller: %s\n", ec.message().c_str());
    }

    while(event != i8080_arcade::MIA_Event::Quit)
    {
        // Run the machine asynchronously, the machine now owns the controllers and they should not be accessed
        machine->Run(0x00);
        // Run the io event loop until the 'q' key is pressed or the window is closed, or in the case of st7889vw top left button is pressed
        event = ioController->EventLoop();
        // Wait for the machine to finish, once complete the controllers can be accessed safely
        machine->WaitForCompletion();

        switch (event)
        {
            case i8080_arcade::MIA_Event::Quit:
            {
                break;
            }
            case i8080_arcade::MIA_Event::Reset:
            {
                break;
            }
            case i8080_arcade::MIA_Event::NextRom:
            {
                romIndex = ++romIndex % 5; // 5 - total number of supported roms
                gameRom = roms[romIndex];
                ret = loadRomAndMemLayout(gameRom);
                break;
            }
            case i8080_arcade::MIA_Event::PreviousRom:
            {
                --romIndex;
                
                if(romIndex < 0)
                {
                    romIndex = 4; // 4 - total number of supported roms (0 based, therefore 5 roms supported)
                }

                gameRom = roms[romIndex];
                ret = loadRomAndMemLayout(gameRom);
                break;
            }
            default:
            {
                printf("Unknown event: %d\n", event);
                break;
            }
        }

        if (ret < 0)
        {
            printf("Post event loop action failed, resetting the current rom\n");
        }
    }

    return 0;
}
