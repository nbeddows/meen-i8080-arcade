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

#include <fstream>
#include <memory>
#include "Machine/MachineFactory.h"
#include "nlohmann/json.hpp"
#include "SpaceInvaders/SdlIoController.h"

int main(void)
{
	try
	{
		// Open the configuration file, see the README for an explanation of each configuration option
		std::ifstream fin(CONFIG_DIR"/config.json");
		const nlohmann::json config = nlohmann::json::parse(fin);
		// Create our custom Space Invaders machine
		auto machine = MachEmu::MakeMachine(config["mach-emu"].dump().c_str());
		// Create our custom Space Invaders memory controller.
		auto memoryController = std::make_shared<SpaceInvaders::MemoryController>();
		// Create our custom Space Invaders I/O controller based on a specific configuration.
		auto ioController = std::make_shared<SpaceInvaders::SdlIoController>(memoryController, config["space-invaders"]["io"]);
		
		// Load the ROM into memory with the following layout
		memoryController->Load(ROMS_DIR"/invaders-h.bin", 0x0000); // invaders-h 0000-07FF
		memoryController->Load(ROMS_DIR"/invaders-g.bin", 0x0800); // invaders-g 0800-0FFF
		memoryController->Load(ROMS_DIR"/invaders-f.bin", 0x1000); // invaders-f 1000-17FF
		memoryController->Load(ROMS_DIR"/invaders-e.bin", 0x1800); // invaders-e 1800-1FFF
		// Load the memory layout into the machine
		machine->SetOptions(config["space-invaders"]["memory"].dump().c_str());
		// Load our controllers into the machine.
		machine->SetMemoryController(memoryController);
		machine->SetIoController(ioController);
		// Will be called from a different thread
		machine->OnSave([](const char* json)
		{
			// Need to make a copy of the json if you want to hold the json string longer than the scope of this function
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

		// Run the machine asynchronously, the machine now owns the controllers and they should not be accessed
		machine->Run(0x00);
		// Run the io event loop until the 'q' key is be pressed or the window is closed
		ioController->EventLoop();
		// Wait for the machine to finish, once complete the controllers can be accessed safely
		machine->WaitForCompletion();
	}
	catch (const std::exception& e)
	{
		printf ("%s\n", e.what());
	}

	return 0;
}