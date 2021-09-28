#include <exception>
#define SDL_MAIN_HANDLED
#include "SDL.h"

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
		SDL_SetMainReady();

		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			throw; //std::runtime_error();
		}

		auto window = std::shared_ptr<SDL_Window>(SDL_CreateWindow("Space Invaders", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 224, 0), [](SDL_Window* w) { SDL_DestroyWindow(w); });

		if (window == nullptr)
		{
			throw std::bad_alloc();
		}

		auto renderer = std::shared_ptr <SDL_Renderer>(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED), [](SDL_Renderer* r) { SDL_DestroyRenderer(r); });

		if (renderer == nullptr)
		{
			throw std::bad_alloc();
		}

		//The machine to run Space Invaders on.
		auto machine = MakeMachine();
		//Create our custom Space Invaders memory controller.
		auto memoryController = std::make_shared<MemoryController>(16, renderer);
		//Create our custom Space Invaders I/O controller.
		auto ioController = std::make_shared<IoController>(memoryController);

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

		//memoryController->Load("../roms/INVADERS.H", 0x0000);
		//memoryController->Load("../roms/INVADERS.G", 0x0800);
		//memoryController->Load("../roms/INVADERS.F", 0x1000);
		//memoryController->Load("../roms/INVADERS.E", 0x1800);

		//Load our controllers into the machine.
		machine->SetMemoryController(memoryController);
		machine->SetIoController(ioController);

		//Only returns when signalled by an I/O device, for our demonstration it will be when the 'q' key is pressed.
		machine->Run(0x00);

		SDL_Quit();
	}
	catch (std::exception e)
	{
		printf ("%s\n", e.what());
	}

	return 0;
}