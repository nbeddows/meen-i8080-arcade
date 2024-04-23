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

#include <format>
#include <fstream>
#include <memory>
#include "Machine/MachineFactory.h"
#include "nlohmann/json.hpp"
#include "SpaceInvaders/SdlIoController.h"

int main(void)
{
	try
	{
		std::ifstream fin(CONFIG_DIR"/config.json");
		const nlohmann::json config = nlohmann::json::parse(fin);

		// The machine to run Space Invaders on.
		// Space Invaders runs at 60Hz with 2 interrupts per second, set the machine clock resolution accordingly.
		// We require 4 interrupts, 2 for Space Invaders and 2 machine level interrupts for loading and saving.
		// Ideally we would lock the interrupt service routine frequency to the clock resolution ("isrFreq":1),
		// however, we need to spare some time for checking for load and save requests, so we bump the isrFreq down
		// by ten percent ("isrFreq":0.9). One could lower it further, this would make it more responsive (0.9 should
		// be good enough). Increasing it above 1 would make it slower and not respond to load/save requests.
		// Set the ram/rom layout.
		auto machine = MachEmu::MakeMachine(std::format(R"({{
												"clockResolution":{},
												"isrFreq":0.9,
												"loadAsync":true,
												"romOffset":0,
												"romSize":8192,
												"ramOffset":8192,
												"ramSize":57343,
												"runAsync":true,
												"saveAsync":true}})",
												(1000000000 / 60) / 2).c_str());
		
		//Create our custom Space Invaders memory controller.
		auto memoryController = std::make_shared<SpaceInvaders::MemoryController>();
		//Create our custom Space Invaders I/O controller based on a specific configuration.
		auto ioController = std::make_shared<SpaceInvaders::SdlIoController>(memoryController, config);
		// Load the ROM into memory with the following layout
		memoryController->Load(ROMS_DIR"/invaders-h.bin", 0x0000); // invaders-h 0000-07FF
		memoryController->Load(ROMS_DIR"/invaders-g.bin", 0x0800); // invaders-g 0800-0FFF
		memoryController->Load(ROMS_DIR"/invaders-f.bin", 0x1000); // invaders-f 1000-17FF
		memoryController->Load(ROMS_DIR"/invaders-e.bin", 0x1800); // invaders-e 1800-1FFF
		// Load our controllers into the machine.
		machine->SetMemoryController(memoryController);
		machine->SetIoController(ioController);
		// Will be called from a different thread
		// Need to make a copy if you want to hold the json string longer than the scope of this function
		machine->OnSave([](const char* json)
		{
			std::ofstream fout(ROMS_DIR"/spaceInvaders.json", std::ios::trunc);
			fout.exceptions(fout.failbit);
			fout.write(json, strlen(json));
		});
		// Will be accessed from a different thread
		std::string loadJson;
		// Will be called from a different thread
		machine->OnLoad([&loadJson]
		{
			std::ifstream fin(ROMS_DIR"/spaceInvaders.json", std::ios::ate);
			fin.exceptions(fin.failbit);
			auto len = fin.tellg();
			fin.seekg(0, std::ios::beg);
			loadJson.resize(len);
			fin.read(loadJson.data(), len);
			return loadJson.c_str();
		});

		machine->Run(0x00);
		ioController->EventLoop();
		machine->WaitForCompletion();
	}
	catch (const std::exception& e)
	{
		printf ("%s\n", e.what());
	}

	return 0;
}