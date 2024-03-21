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
#include <future>
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
		// Space Invaders runs at 60Hz with 2 interrupts per second, set the machine clock resolution accordingly
		auto machine = MachEmu::MakeMachine(std::format(R"({{"clockResolution":{}}})", (1000000000 / 60) / 2).c_str());
		//Create our custom Space Invaders memory controller.
		auto memoryController = std::make_shared<SpaceInvaders::MemoryController>();
		//Create our custom Space Invaders I/O controller.
		auto ioController = std::make_shared<SpaceInvaders::SdlIoController>(memoryController, config);

		/*
			Load the ROM into memory, the layout
			is as follows:

				invaders-h 0000-07FF
				invaders-g 0800-0FFF
				invaders-f 1000-17FF
				invaders-e 1800-1FFF
		*/
		memoryController->Load(ROMS_DIR"/invaders-h.bin", 0x0000);
		memoryController->Load(ROMS_DIR"/invaders-g.bin", 0x0800);
		memoryController->Load(ROMS_DIR"/invaders-f.bin", 0x1000);
		memoryController->Load(ROMS_DIR"/invaders-e.bin", 0x1800);

		// Load our controllers into the machine.
		machine->SetMemoryController(memoryController);
		machine->SetIoController(ioController);
		auto err = machine->SetOptions(R"({"runAsync":true})");
		
		if (err == MachEmu::ErrorCode::NoError)
		{
			machine->Run(0x00);
			ioController->EventLoop();
			machine->WaitForCompletion();
		}
		else
		{
			// Run the machine on a separate thread, the io controller will determine when to quit,
			// in the case of this example, when the 'q' key is pressed or the window is closed.
			auto future = std::async(std::launch::async, [&]
			{
				machine->Run(0x00);
			});

			// Run the main event loop
			ioController->EventLoop();

			// Wait for the machine to finish.
			future.wait();
		}
	}
	catch (const std::exception& e)
	{
		printf ("%s\n", e.what());
	}

	return 0;
}