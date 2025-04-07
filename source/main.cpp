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

#include <vector>

#include "meen/MachineFactory.h"
#include "meen/Error.h"
#include "i8080_arcade/MemoryController.h"

#ifdef ENABLE_MH_RP2040
/*
 Spin forever on a set up error so the user has time to spin up
 a minicom and check what it is!
*/
#define CHECK_ERROR(value, printErrorMsg)\
while(value)\
{\
	printErrorMsg;\
};\

#include <pico/stdlib.h>

#include "i8080_arcade/RPIoController.h"

extern uint8_t invadersHStart;
extern uint8_t invadersHEnd;
extern uint8_t invadersGStart;
extern uint8_t invadersGEnd;
extern uint8_t invadersFStart;
extern uint8_t invadersFEnd;
extern uint8_t invadersEStart;
extern uint8_t invadersEEnd;

extern char rpConfigStart;
extern char rpConfigEnd;
#else
/*
 Print out the error message and exit
*/
#define CHECK_ERROR(value, printErrorMsg)\
if(value)\
{\
	printErrorMsg;\
	return 0;\
};\

#include <fstream>
#include <filesystem>
#include <memory>

#include "i8080_arcade/SdlIoController.h"
#endif // ENABLE_MH_RP2040

static i8080_arcade::MemoryController* MakeMemoryController()
{
#ifdef ENABLE_MH_RP2040
	return new i8080_arcade::MemoryController(3); // 3 - Three frame for triple buffered rendering
#else
	return new i8080_arcade::MemoryController();
#endif
}

static i8080_arcade::IIoController* MakeIoController(bool runAsync, const JsonVariant& audioHardware, const JsonVariant& videoHardware)
{
#ifdef ENABLE_MH_RP2040
	return new i8080_arcade::RPIoController(runAsync, audioHardware, videoHardware);
#else
	return new i8080_arcade::SDLIoController(runAsync, audioHardware, videoHardware);
#endif // ENABLE_MH_RP2040
}

int main(int argc, char** argv)
{
	// Store the required json needed to load a specific rom (first is the rom name, second is the rom config json)
	std::vector<std::pair<std::string, std::string>> jsonRoms;
	// The path where our save files will reside (not applicable for embedded targets)
	std::string saveFilePath;
	// Create our custom i8080 arcade machine
	auto machine = meen::Make8080Machine();
	CHECK_ERROR(!machine, printf("Failed to create i8080 machine\n"));

	// Configure everything in a block so that all locals are destroyed upon block exit 
	{
		JsonDocument json;
	#ifdef ENABLE_MH_RP2040
		const std::unordered_map<std::string_view, uintptr_t> romNameToAddr
		{
			{ "invaders-e.bin", std::bit_cast<uintptr_t>(&invadersEStart) }, { "invaders-f.bin", std::bit_cast<uintptr_t>(&invadersFStart) }, { "invaders-g.bin", std::bit_cast<uintptr_t>(&invadersGStart) }, { "invaders-h.bin", std::bit_cast<uintptr_t>(&invadersHStart) }
		};

		stdio_init_all();
		// Open the configuration file, see the README for an explanation of each configuration option
		//cppcheck-suppress comparePointers
		auto e = deserializeJson(json, std::string_view(&rpConfigStart, &rpConfigEnd - &rpConfigStart));
	#else
		std::ifstream fin;
		auto configFilePath = argc == 1 ? "conf/config.json" : argv[1];
		// Open the configuration file, see the README for an explanation of each configuration option
		fin.open(configFilePath);
		auto e = deserializeJson(json, fin);
	#endif // ENABLE_MH_RP2040
		CHECK_ERROR(e, printf("Parse error while deserializing json config file\n"));

		saveFilePath = json["saveFilePath"] ? json["saveFilePath"].as<std::string>() : "save-files";

		auto hardware = json["i8080Arcade"]["hardware"];
		CHECK_ERROR(!hardware, printf("Invalid json config file format: hardware section not found\n"));

		auto software = json["i8080Arcade"]["software"];
		CHECK_ERROR(!software, printf("Invalid json config file format: software section not found\n"));

		for(auto&& r : software["roms"].as<JsonArray>())
		{
			CHECK_ERROR(!r["memory"], printf("No memory found in config file rom\n"));
			CHECK_ERROR(!r["memory"]["rom"], printf("No rom found in config file memory\n"));
			CHECK_ERROR(!r["memory"]["rom"]["block"], printf("No rom block found in config file memory\n"));

	// For baremetal platforms we need to update the config file so it loads from flash rather than a file
	#ifdef ENABLE_MH_RP2040
			auto&& rom = r["memory"]["rom"];
			// Update the scheme
			rom["scheme"] = "mem://";

			for (auto&& block : rom["block"].as<JsonArray>())
			{
				// Check the names of the roms and set the correct rom address accordingly
				auto name = block["bytes"].as<std::string_view>();
				CHECK_ERROR(!romNameToAddr.contains(name), printf("The memory rom block is missing bytes: %s\n", std::string(name).c_str()));
				block["bytes"] = std::to_string(romNameToAddr.at(name));
			}
	#endif // ENABLE_MH_RP2040

			// Cache all rom strings in a vector of pairs with first being the rom name
			// and the second being the rom config json.
			std::string str;
			serializeJson(r, str);
			jsonRoms.emplace_back (r["name"].as<std::string>(), std::move(str));

			// remove this once support for all other roms has been added
			break;
		}

		auto meen = hardware["meen"];
		CHECK_ERROR(!meen, printf("Invalid json config file format: meen section not found\n"));

		// Create our custom i8080 arcade I/O controller based on a specific configuration.
		auto ioController = MakeIoController(meen["runAsync"], hardware["audio"], hardware["video"]);
		CHECK_ERROR(!ioController, printf("Failed to create the i/o controller\n"));

		// Create our custom i8080 arcade memory controller.
		auto memoryController = MakeMemoryController();
		CHECK_ERROR(!memoryController, printf("Failed to create the memory controller\n"));

		// Set up the custom controllers prior to configuring the machine.

		auto err = ioController->LoadVideoTextures(software["video"]);
		CHECK_ERROR(err, printf("Failed to load video textures: %s\n", err.message().c_str()));

		err = ioController->LoadAudioSamples(software["audio"]);
		CHECK_ERROR((err && err.value() != static_cast<int>(std::errc::not_supported)), printf("Failed to load audio samples: %s\n", err.message().c_str()));

		// Configure the machine.

		// Log any error messages generated, do this as early as possible for best meen error coverage
		err = machine->OnError([](std::error_code ec, const char* fileName, const char* functionName, uint32_t line, uint32_t column, meen::IController* ioController)
		{
			auto len = snprintf(nullptr, 0, "file: %s(%u:%u) `%s`: %s\n", fileName, line, column, functionName, ec.message().c_str());
			std::string errorMsg(len, '\0');
			len = snprintf(errorMsg.data(), len, "file: %s(%u:%u) `%s`: %s\n", fileName, line, column, functionName, ec.message().c_str());

			// It's possible for this to be nullptr if the io controller has been removed (should not happen in this demo,
			// but we perform the check for correctness and print a warning message).
			if (ioController != nullptr)
			{
				// We only pass around one type of controller, so this cast is safe.
				static_cast<i8080_arcade::IIoController*>(ioController)->HandleError(std::move(errorMsg));
			}
			else
			{
				printf ("Warning: no io controller attached, error: %s", errorMsg.c_str());
			}
		});
		// Need to manually check the error here as the method could fail before the handler is registered
		CHECK_ERROR(err, printf("Failed to set the OnError handler: %s\n", err.message().c_str()));

		// Beyond this point, all meen generated errors will be picked up by our error handler

		// Load our controllers into the machine - do this immediately after the OnError handler has been registered
		// so the io controller will be available in the OnError handler.
		machine->AttachIoController(meen::IControllerPtr(std::move(ioController)));
		machine->AttachMemoryController(meen::IControllerPtr(std::move(memoryController)));

		// Will be called from a different thread if the 'runAsync' or 'saveAsync' options are set to true.
		// This is a simple implementation which will overwrite the previous save file
	#ifndef ENABLE_MH_RP2040
		machine->OnSave([&jsonRoms, &saveFilePath](const char* json, meen::IController* ioController)
		{
			std::error_code ec;
			std::filesystem::create_directory(saveFilePath, ec);

			if (ec)
			{
				return meen::errc::invalid_argument;
			}

			auto [unused, romIndex] = static_cast<i8080_arcade::IIoController*>(ioController)->GetRomIndex(jsonRoms.size());
			std::ofstream fout(saveFilePath + "/" + jsonRoms[romIndex].first + ".json", std::ios::trunc);

			if (!fout.good())
			{
				return meen::errc::invalid_argument;
			}

			fout.write(json, strlen(json));
			return meen::errc::no_error;
		});
	#endif // ENABLE_MH_RP2040
		// Will be called from a different thread if the 'runAsync' or 'loadAsync' configuration options are set to true
		machine->OnLoad([&jsonRoms, &saveFilePath](char* json, int* jsonLen, meen::IController* ioController)
		{
			auto [loadSaveState, romIndex] = static_cast<i8080_arcade::IIoController*>(ioController)->GetRomIndex(jsonRoms.size());

			if (loadSaveState == true)
			{
	#ifdef ENABLE_MH_RP2040
				return meen::errc::not_implemented;
	#else
				// The engine will generate a parse error if a truncation occurs, however, if one wanted to
				// check for that here they could by storing the return value in a different variable and then
				// compare that value to *jsonLen. When that variable is greater than or equal to *jsonLen then
				// a truncation has occurred.
				*jsonLen = snprintf(json, *jsonLen, "file://%s/%s.json", saveFilePath.c_str(), jsonRoms[romIndex].first.c_str());
	#endif // ENABLE_MH_RP2040
			}
			else
			{
				// The engine will generate a parse error if a truncation occurs, however, if one wanted to
				// check for that here they could by comparing the return value (the number of bytes written
				// excluding the null terminator) against the capacity (*jsonLen). When the return value is
				// equal to *jsonLen then a truncation has occurred.
				if (jsonRoms[romIndex].second.length() < *jsonLen)
				{
					*jsonLen = jsonRoms[romIndex].second.length();
				}
				else
				{
					printf("TRUNCATED ROM!\n");
					// should return an error here, whilst is ok not to, it can produce ub depending on what got written
				}

				std::ranges::copy_n(jsonRoms[romIndex].second.data(), *jsonLen, json);

				// todo: return the number of bytes loaded
			}

			return meen::errc::no_error;
		});

		// Will always be called from the same thread from which IMachine::Run was called (in this case, the main thread)
		machine->OnIdle([](meen::IController* ioController)
		{
			return static_cast<i8080_arcade::IIoController*>(ioController)->HandleEvent();
		});

		// Set the hardware options
		std::string meenConfig;
		serializeJson(meen, meenConfig);
		machine->SetOptions(meenConfig.c_str());
	}

	// Run the machine until the 'q' key is pressed or the window is closed (ie; the machine OnIdle handler returns true)
	auto ex = machine->Run();

	if (ex)
	{
		printf("Machine run time: %.2f seconds\n", ex.value() / 1000000000.0);
	}

	return 0;
}
