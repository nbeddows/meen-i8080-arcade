/*
	Import the required modules

	SpaceInvaders:		custom module specifically targetting the Space Invaders ROM.
	ControllerFactory:	module required for making a default memory controller with 64k of RAM.
	MachineFactory:		module for running our custom Space Invaders machine.
*/
import SpaceInvaders;
import ControllerFactory;
import MachineFactory;

//Just to make things simpler
using namespace SpaceInvaders;
using namespace Emulator;

int main(void)
{
	//The machine to run Space Invaders on.
	auto machine = MakeMachine();
	//Create our custom Space Invaders I/O controller.
	auto ioController = std::make_unique<IoController>(machine->ControlBus());
	//The default memory controller is sufficient for demonstration purposes.
	auto memoryController = MakeDefaultMemoryController(16);

	/*
		Load the ROM into memory, The layout
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
	machine->SetMemoryController(std::move(memoryController));
	machine->SetIoController(std::move(ioController));
	
	//Only returns when signalled by I/O, for our demonstration it will be after 10 seconds.
	machine->Run(0x00);

	return 0;
}