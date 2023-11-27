/*
Copyright (c) 2021-2023 Nicolas Beddows <nicolas.beddows@gmail.com>

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

/*
	Import the required modules

	SpaceInvaders:		custom module specifically targetting the Space Invaders ROM.
	MachineFactory:		module for running our custom Space Invaders machine.
*/
import <memory>;
import SpaceInvaders;
import MachineFactory;

//Just to make things simpler
using namespace SpaceInvaders;
using namespace Emulator;

int main(void)
{
	try
	{
		//The machine to run Space Invaders on.
		auto machine = MakeMachine();
		//Create our custom Space Invaders memory controller.
		auto memoryController = std::make_shared<MemoryController>(16);
		//Create our custom Space Invaders I/O controller.
		auto ioController = std::make_shared<SdlIoController>(memoryController);

		/*
			Load the ROM into memory, the layout
			is as follows:

				invaders-h 0000-07FF
				invaders-g 0800-0FFF
				invaders-f 1000-17FF
				invaders-e 1800-1FFF
		*/
		memoryController->Load("../roms/invaders-h.bin", 0x0000);
		memoryController->Load("../roms/invaders-g.bin", 0x0800);
		memoryController->Load("../roms/invaders-f.bin", 0x1000);
		memoryController->Load("../roms/invaders-e.bin", 0x1800);

		// Load our controllers into the machine.
		machine->SetMemoryController(memoryController);
		machine->SetIoController(ioController);
		// Run the machine, the io controller will determine when to quit,
		// in the case of this example, when the 'q' key is pressed.
		machine->Run(0x00);
	}
	catch (std::exception e)
	{
		printf ("%s\n", e.what());
	}

	return 0;
}