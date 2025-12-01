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
#include "meen_i8080_arcade/MemoryController.h"

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

#include "meen_i8080_arcade/RPIoController.h"

extern uint8_t invadersHStart;
extern uint8_t invadersHEnd;
extern uint8_t invadersGStart;
extern uint8_t invadersGEnd;
extern uint8_t invadersFStart;
extern uint8_t invadersFEnd;
extern uint8_t invadersEStart;
extern uint8_t invadersEEnd;

extern uint8_t invdeluxHStart;
extern uint8_t invdeluxHEnd;
extern uint8_t invdeluxGStart;
extern uint8_t invdeluxGEnd;
extern uint8_t invdeluxFStart;
extern uint8_t invdeluxFEnd;
extern uint8_t invdeluxEStart;
extern uint8_t invdeluxEEnd;
extern uint8_t invdeluxDStart;
extern uint8_t invdeluxDEnd;

extern uint8_t pv01Start;
extern uint8_t pv01End;
extern uint8_t pv02Start;
extern uint8_t pv02End;
extern uint8_t pv03Start;
extern uint8_t pv03End;
extern uint8_t pv04Start;
extern uint8_t pv04End;
extern uint8_t pv05Start;
extern uint8_t pv05End;

extern uint8_t tn01Start;
extern uint8_t tn01End;
extern uint8_t tn02Start;
extern uint8_t tn02End;
extern uint8_t tn03Start;
extern uint8_t tn03End;
extern uint8_t tn04Start;
extern uint8_t tn04End;
extern uint8_t tn05Start;
extern uint8_t tn05End;

extern uint8_t lrescue1Start;
extern uint8_t lrescue1End;
extern uint8_t lrescue2Start;
extern uint8_t lrescue2End;
extern uint8_t lrescue3Start;
extern uint8_t lrescue3End;
extern uint8_t lrescue4Start;
extern uint8_t lrescue4End;
extern uint8_t lrescue5Start;
extern uint8_t lrescue5End;
extern uint8_t lrescue6Start;
extern uint8_t lrescue6End;

extern uint8_t ufoHStart;
extern uint8_t ufoHEnd;
extern uint8_t ufoLStart;
extern uint8_t ufoLEnd;
extern uint8_t shootStart;
extern uint8_t shootEnd;
extern uint8_t explStart;
extern uint8_t explEnd;
extern uint8_t invkStart;
extern uint8_t invkEnd;
extern uint8_t extpStart;
extern uint8_t extpEnd;
extern uint8_t mvt1Start;
extern uint8_t mvt1End;
extern uint8_t mvt2Start;
extern uint8_t mvt2End;
extern uint8_t mvt3Start;
extern uint8_t mvt3End;
extern uint8_t mvt4Start;
extern uint8_t mvt4End;

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
#include <memory>

#include "meen_i8080_arcade/SdlIoController.h"
#endif // ENABLE_MH_RP2040

static meen_i8080_arcade::MemoryController* MakeMemoryController(const std::vector<std::pair<std::string, std::string>>&jsonRoms)
{
	return new meen_i8080_arcade::MemoryController(jsonRoms);
}

static meen_i8080_arcade::IIoController* MakeIoController(bool runAsync, meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr&& backBuffer, int romCount, JsonVariantConst audioHardware, JsonVariantConst videoHardware)
{
	if (!audioHardware || !videoHardware)
	{
		return nullptr;
	}
#ifdef ENABLE_MH_RP2040
	return new meen_i8080_arcade::RPIoController(runAsync, std::move(backBuffer), romCount, audioHardware, videoHardware);
#else
	return new meen_i8080_arcade::SDLIoController(runAsync, romCount, audioHardware, videoHardware);
#endif // ENABLE_MH_RP2040
}

int main(int argc, char** argv)
{
	printf("MEEN Version: %s\n", meen::Version());

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
		auto toPair = [](uint8_t* s, const uint8_t* e)
		{
			return std::pair<uintptr_t, uintptr_t>(std::bit_cast<uintptr_t>(s), e - s);
		};

		const std::unordered_map<std::string_view, std::pair<uintptr_t, uint16_t>> romNameToAddr
		{
			{ "invaders-e.bin", toPair(&invadersEStart, &invadersEEnd) }, { "invaders-f.bin", toPair(&invadersFStart, &invadersFEnd) }, { "invaders-g.bin", toPair(&invadersGStart, &invadersGEnd) }, { "invaders-h.bin", toPair(&invadersHStart, &invadersHEnd) },
			{ "invdelux-d.bin", toPair(&invdeluxDStart, &invdeluxDEnd) }, { "invdelux-e.bin", toPair(&invdeluxEStart, &invdeluxEEnd) }, { "invdelux-f.bin", toPair(&invdeluxFStart, &invdeluxFEnd) }, { "invdelux-g.bin", toPair(&invdeluxGStart, &invdeluxGEnd) }, { "invdelux-h.bin", toPair(&invdeluxHStart, &invdeluxHEnd) },
			{ "PV.01", toPair(&pv01Start, &pv01End) }, { "PV.02", toPair(&pv02Start, &pv02End) }, { "PV.03", toPair(&pv03Start, &pv03End) }, { "PV.04", toPair(&pv04Start, &pv04End) }, { "PV.05", toPair(&pv05Start, &pv05End) },
			{ "tn01.bin", toPair(&tn01Start, &tn01End) }, { "tn02.bin", toPair(&tn02Start, &tn02End) }, { "tn03.bin", toPair(&tn03Start, &tn03End) }, { "tn04.bin", toPair(&tn04Start, &tn04End) }, { "tn05-1.bin", toPair(&tn05Start, &tn05End) },
			{ "lrescue-1.bin", toPair(&lrescue1Start, &lrescue1End) }, { "lrescue-2.bin", toPair(&lrescue2Start, &lrescue2End) }, { "lrescue-3.bin", toPair(&lrescue3Start, &lrescue3End) }, { "lrescue-4.bin", toPair(&lrescue4Start, &lrescue4End) }, { "lrescue-5.bin", toPair(&lrescue5Start, &lrescue5End) }, { "lrescue-6.bin", toPair(&lrescue6Start, &lrescue6End) }
		};

		const std::unordered_map<std::string_view, std::pair<uintptr_t, uint16_t>> audioNameToAddr
		{
			{ "ufo_highpitch.wav", toPair(&ufoHStart, &ufoHEnd) }, { "ufo_lowpitch.wav", toPair(&ufoLStart, &ufoLEnd) }, { "shoot.wav", toPair(&shootStart, &shootEnd) },
			{ "explosion.wav", toPair(&explStart, &explEnd) }, { "invaderkilled.wav", toPair(&invkStart, &invkEnd) }, { "extendedplay.wav", toPair(&extpStart, &extpEnd) },
			{ "fastinvader1.wav", toPair(&mvt1Start, &mvt1End) }, { "fastinvader2.wav", toPair(&mvt2Start, &mvt2End) }, { "fastinvader3.wav", toPair(&mvt3Start, &mvt3End) },
			{ "fastinvader4.wav", toPair(&mvt4Start, &mvt4End) }
		};

		stdio_init_all();
		// Open the configuration file, see the README for an explanation of each configuration option
		//cppcheck-suppress[comparePointers,subtractPointers]
		auto e = deserializeJson(json, std::string_view(&rpConfigStart, &rpConfigEnd - &rpConfigStart));
#else
		std::ifstream fin;
		auto configFilePath = argc == 1 ? "conf/config.json" : argv[1];
		// Open the configuration file, see the README for an explanation of each configuration option
		fin.open(configFilePath);
		auto e = deserializeJson(json, fin);
#endif // ENABLE_MH_RP2040
		CHECK_ERROR(e, printf("Parse error while deserializing json config file\n"));

		saveFilePath = json["i8080Arcade"]["saveFilePath"] ? json["i8080Arcade"]["saveFilePath"].as<std::string>():"file://save-files";

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
				// The size parameter must be set before bytes as name is a string_view.
				// Reversing the order would cause ub as the view would be looking at the address rather than the name.
				block["size"] = romNameToAddr.at(name).second;
				block["bytes"] = std::to_string(romNameToAddr.at(name).first);
			}
#endif // ENABLE_MH_RP2040
			// Cache all rom strings in a vector of pairs with first being the rom name
			// and the second being the rom config json.
			std::string str;
			serializeJson(r, str);
			jsonRoms.emplace_back (r["name"].as<std::string>(), std::move(str));
		}

#ifdef ENABLE_MH_RP2040
		if (software["audio"])
		{
			// Update the scheme
			software["audio"]["scheme"] = "mem://";

			for(auto&& s : software["audio"]["sample"].as<JsonArray>())
			{
				// Check the names of the audio files and set the correct audio address accordingly
				auto name = s["bytes"].as<std::string_view>();

				// Ignore unused entries
				if (name.empty() == false)
				{
					CHECK_ERROR(!audioNameToAddr.contains(name), printf("The audio samples is missing bytes: %s\n", std::string(name).c_str()));
					// The size parameter must be set before bytes as name is a string_view.
					// Reversing the order would cause ub as the view would be looking at the address rather than the name.
					s["size"] = audioNameToAddr.at(name).second;
					s["bytes"] = std::to_string(audioNameToAddr.at(name).first);
				}
			}
		}
#endif // ENABLE_MH_RP2040
		auto meen = hardware["meen"];
		CHECK_ERROR(!meen, printf("Invalid json config file format: meen section not found\n"));

		// Create our custom i8080 arcade memory controller.
		auto memoryController = MakeMemoryController(jsonRoms);
		CHECK_ERROR(!memoryController, printf("Failed to create the memory controller\n"));

		// Create a frame pool of 4 frames, passing an empty one back for use as the initial io controller back buffer if required.
		auto backBuffer = memoryController->MakeFramePool(4);
		CHECK_ERROR(!backBuffer, printf("Failed to create the memory controller frame pool\n"));

		// Create our custom i8080 arcade I/O controller based on a specific configuration.
		auto ioController = MakeIoController(meen["runAsync"], std::move(backBuffer), jsonRoms.size(), hardware["audio"], hardware["video"]);
		CHECK_ERROR(!ioController, printf("Failed to create the i/o controller\n"));

		// Set up the custom controllers prior to configuring the machine.

		// The memory controller width and height is in the native i8080 arcade pixel format (1bpp cocktail) so we need to multiply it by 8 to get the total pixel width
		// in order to create a compatible texture
		auto err = ioController->LoadVideoTextures(software["video"], meen_i8080_arcade::MemoryController::frameWidth << 3, meen_i8080_arcade::MemoryController::frameHeight);
		CHECK_ERROR(err, printf("Failed to load video textures: %s\n", err.message().c_str()));

		err = ioController->LoadAudioSamples(software["audio"]);
		CHECK_ERROR((err && err.value() != static_cast<int>(std::errc::not_supported)), printf("Failed to load audio samples: %s\n", err.message().c_str()));

		// Configure the machine.

		// Log any error messages generated, do this as early as possible for best MEEN error coverage
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
				static_cast<meen_i8080_arcade::IIoController*>(ioController)->HandleError(std::move(errorMsg));
			}
			else
			{
				printf ("Warning: no io controller attached, error: %s", errorMsg.c_str());
			}
		});
		// Need to manually check the error here as the method could fail before the handler is registered
		CHECK_ERROR(err, printf("Failed to set the OnError handler: %s\n", err.message().c_str()));

		// Beyond this point, all MEEN generated errors will be picked up by our error handler

		// Load our controllers into the machine - do this immediately after the OnError handler has been registered
		// so the io controller will be available in the OnError handler.
		machine->AttachIoController(meen::IControllerPtr(std::move(ioController)));
		machine->AttachMemoryController(meen::IControllerPtr(std::move(memoryController)));

		//cppcheck-suppress constParameterPointer
		machine->OnInit([](meen::IController* ioController)
		{
#ifdef ENABLE_MH_RP2040
			meen_i8080_arcade::RPIoController::Init();
#endif // ENABLE_MH_RP2040
			return meen::errc::no_error;
		});

		// Will be called from a different thread if the 'runAsync' or 'saveAsync' options are set to true.
		// This is a simple implementation which will overwrite the previous save file
		machine->OnSave([&jsonRoms, &saveFilePath](char* uri, int* uriLen, meen::IController* ioController)
		{
			auto [unused, romIndex] = static_cast<meen_i8080_arcade::IIoController*>(ioController)->GetRomIndex();
			*uriLen = snprintf(uri, *uriLen, "%s/%s.json", saveFilePath.c_str(), jsonRoms[romIndex].first.c_str());
			return meen::errc::no_error;
		}, nullptr);

		// Will be called from a different thread if the 'runAsync' or 'loadAsync' configuration options are set to true
		machine->OnLoad([&jsonRoms, &saveFilePath](char* json, int* jsonLen, meen::IController* ioController)
		{
			auto [loadSaveState, romIndex] = static_cast<meen_i8080_arcade::IIoController*>(ioController)->GetRomIndex();

			if (loadSaveState == true)
			{
				*jsonLen = snprintf(json, *jsonLen, "%s/%s.json", saveFilePath.c_str(), jsonRoms[romIndex].first.c_str());
			}
			else
			{
				*jsonLen = snprintf(json, *jsonLen, "%s", jsonRoms[romIndex].second.c_str());
			}

			return meen::errc::no_error;
		// The load complete handler will be called from a different thread if the 'runAsync' configuration option is set to true
		}, [](meen::IController* ioController)
		{
			static_cast<meen_i8080_arcade::IIoController*>(ioController)->HandleLoadComplete();
			return meen::errc::no_error;
		});

		// Will always be called from the same thread from which IMachine::Run was called (in this case, the main thread)
		machine->OnIdle([](meen::IController* ioController)
		{
			return static_cast<meen_i8080_arcade::IIoController*>(ioController)->HandleEvent();
		});

		// Set the hardware options
		std::string meenConfig;
		serializeJson(meen, meenConfig);
		machine->SetOptions(meenConfig.c_str());
	}

	// Run the machine until the 'q' key is pressed or the window is closed (ie; the machine OnIdle handler returns true)
	[[maybe_unused]] auto runTime = machine->Run();

	return 0;
}