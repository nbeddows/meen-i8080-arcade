/*
Copyright (c) 2021-2025 Nicolas Beddows <nicolas.beddows@gmail.com>

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
#include "meen/Error.h"
#include "i8080_arcade/MemoryController.h"

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
static std::filesystem::path saveFilePath;

static int ParseCmdLine(int argc, char** argv)
{
	OptionParser op("Allowed options");
	auto helpOpt = op.add<Switch>("h", "help", "produce this help message");
	auto configFileOpt = op.add<Value<std::string>>("c", "config-file", "i8080 arcade configuration file", "conf/config.json");
	auto saveFilePathOpt = op.add<Value<std::string>>("s", "save-file-path", "Path to the i8080 arcade save files directory", "save-files");
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
    saveFilePath = saveFilePathOpt->value();
    return 0;
}
#endif // ENABLE_MH_RP2040

#define CHECK_ERROR(value, printErrorMsg)\
if(value)\
{\
	printErrorMsg;\
	return 0;\
}\

int main(int argc, char** argv)
{
    std::string meenConfig;
    JsonDocument json;
#ifdef ENABLE_MH_RP2040
    stdio_init_all();
    // Open the configuration file, see the README for an explanation of each configuration option
    //cppcheck-suppress comparePointers
    auto e = deserializeJson(json, std::string(&rpConfigStart, &rpConfigEnd - &rpConfigStart));
#else
    if (ParseCmdLine(argc, argv) < 0)
    {
        // We return < 0 when we print the help, exit.
        return 0;
    }

    // Open the configuration file, see the README for an explanation of each configuration option
    std::ifstream fin(configFile);
    auto e = deserializeJson(json, fin);
#endif // ENABLE_MH_RP2040
    CHECK_ERROR(e, printf("Parse error while deserializing json config file\n"));

	auto hardware = json["i8080-arcade"]["hardware"];
	CHECK_ERROR(!hardware, printf("Invalid json config file format: hardware section not found\n"));

	auto software = json["i8080-arcade"]["software"];
	CHECK_ERROR(!software, printf("Invalid json config file format: software section not found\n"));

	// Create our custom i8080 arcade machine
	auto machine = meen::Make8080Machine();
	CHECK_ERROR(!machine, printf("Failed to create i8080 machine\n"));

	// Set the hardware options
	serializeJson(hardware["meen"], meenConfig);

	auto err = machine->SetOptions(meenConfig.c_str());
	CHECK_ERROR(err, printf("Failed to set machine options: %s\n", err.message().c_str()));

	// Create our custom i8080 arcade I/O controller based on a specific configuration.
#ifdef ENABLE_MH_RP2040
	auto ioController = new i8080_arcade::RPIoController(hardware["audio"], hardware["video"]);
#else
	auto ioController = new i8080_arcade::SDLIoController(hardware["audio"], hardware["video"]);
#endif
	CHECK_ERROR(!ioController, printf("Failed to create the i/o controller\n"));

	ioController->LoadVideoTextures(software["video"]);
#ifndef ENABLE_MH_RP2040
	ioController->LoadAudioSamples(software["audio"]);

	// Will be called from a different thread if the 'runAsync' or 'saveAsync' options are set to true.
	// This is a simple implementation which will overwrite the previous save file
	err = machine->OnSave([roms = software["roms"]](const char* json, meen::IController* ioController)
	{
		std::error_code ec;
		std::filesystem::create_directory(saveFilePath, ec);
#ifdef ENABLE_MH_RP2040
		auto [unused, romIndex] = static_cast<i8080_arcade::RPIoController*>(ioController)->GetRomIndex(roms.size());
#else
		auto [unused, romIndex] = static_cast<i8080_arcade::SDLIoController*>(ioController)->GetRomIndex(roms.size());
#endif // ENABLE_MH_RP2040
		auto rom = roms.as<JsonArrayConst>()[romIndex];
		
		if (ec)
		{
			return meen::errc::invalid_argument;
		}		
		
		std::ofstream fout((saveFilePath/rom["name"].as<std::string>()).string() + ".json", std::ios::trunc);

		if (!fout.good())
		{
			return meen::errc::invalid_argument;
		}

		fout.write(json, strlen(json));
		return meen::errc::no_error;
	});
	// A not implemented error is acceptable here
	// if (err.value() != meen::errc::not_implemnted)
	CHECK_ERROR(err, printf("Failed to register the OnSave handler: %s\n", err.message().c_str()));	
#endif

	// Will be called from a different thread if the 'runAsync' or 'loadAsync' configuration options are set to true
	err = machine->OnLoad([roms = software["roms"]](char* json, int* jsonLen, meen::IController* ioController)
	{
#ifdef ENABLE_MH_RP2040
		auto [loadSaveState, romIndex] = static_cast<i8080_arcade::RPIoController*>(ioController)->GetRomIndex(roms.size());
#else
		auto [loadSaveState, romIndex] = static_cast<i8080_arcade::SDLIoController*>(ioController)->GetRomIndex(roms.size());
#endif // ENABLE_MH_RP2040
		auto rom = roms.as<JsonArrayConst>()[romIndex];
		
		if (loadSaveState == true)
		{
			auto str = std::string("file://") + (saveFilePath/rom["name"].as<std::string>()).string() + ".json";
			// The engine will generate a parse error if a truncation occurs, however, if one wanted to
			// check for that here they could by comparing the length of str with *jsonLen. When the length
			// of str is greater than *jsonLen then a truncation has occurred.
			strncpy(json, str.c_str(), *jsonLen);
			*jsonLen = str.length();
		}
		else
		{
			// The engine will generate a parse error if a truncation occurs, however, if one wanted to
			// check for that here they could by comparing the return value (the number of bytes written
			// excluding the null terminator) against the capacity (*jsonLen). When the return value is
			// equal to *jsonLen then a truncation has occurred.
			*jsonLen = serializeJson(rom, json, *jsonLen);
		}

		return meen::errc::no_error;
	});
	CHECK_ERROR(err, printf("Failed to register the OnLoad handler: %s\n", err.message().c_str()));

	// Will always be called from the same thread from which IMachine::Run was called (in this case, the main thread)
	err = machine->OnIdle([](meen::IController* ioController)
	{
#ifdef ENABLE_MH_RP2040
		return static_cast<i8080_arcade::RPIoController*>(ioController)->HandleEvent();
#else
		return static_cast<i8080_arcade::SDLIoController*>(ioController)->HandleEvent();
#endif // ENABLE_MH_RP2040
	});
	CHECK_ERROR(err, printf("Failed to register the OnIdle handler: %s\n", err.message().c_str()));

	// Load our controllers into the machine.
	err = machine->AttachIoController(meen::IControllerPtr(std::move(ioController)));
	CHECK_ERROR(err, printf("Failed to attach io controller: %s\n", err.message().c_str()));

	err = machine->AttachMemoryController(meen::IControllerPtr(new i8080_arcade::MemoryController()));
	CHECK_ERROR(err, printf("Failed to attach memory controller: %s\n", err.message().c_str()));

	// Run the machine until the 'q' key is pressed or the window is closed (ie; the machine OnIdle handler returns true)
	auto ex = machine->Run();
	CHECK_ERROR(!ex, printf("Failed to run the machine: %s\n", ex.error().message().c_str()));

	printf("Machine run time: %.2f seconds\n", ex.value() / 1000000000.0);

	return 0;
}
