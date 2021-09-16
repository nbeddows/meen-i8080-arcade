/*
	Import the required modules

	SpaceInvaders:		custom module specifically targetting the Space Invaders ROM.
	MachineFactory:		module for running our custom Space Invaders machine.
*/
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
		//Create our custom Space Invaders I/O controller.
		auto ioController = std::make_shared<IoController>();
		//Create our custom Space Invaders memory controller.
		auto memoryController = std::make_shared<MemoryController>(16);

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

		//Load our controllers into the machine.
		machine->SetMemoryController(memoryController);
		machine->SetIoController(ioController);

		//Only returns when signalled by an I/O device, for our demonstration it will be after 10 seconds.
		machine->Run(0x00);
	}
	catch (std::exception e)
	{
		printf ("%s\n", e.what());
	}

	return 0;
}