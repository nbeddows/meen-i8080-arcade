#include <exception>
#include <future>
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

/*
static int display_in_use = 0; // Only using first display

int i, display_mode_count;
SDL_DisplayMode mode;
Uint32 f;

SDL_Log("SDL_GetNumVideoDisplays(): %i", SDL_GetNumVideoDisplays());

display_mode_count = SDL_GetNumDisplayModes(display_in_use);
if (display_mode_count < 1) {
	SDL_Log("SDL_GetNumDisplayModes failed: %s", SDL_GetError());
	return 1;
}
SDL_Log("SDL_GetNumDisplayModes: %i", display_mode_count);

for (i = 0; i < display_mode_count; ++i) {
	if (SDL_GetDisplayMode(display_in_use, i, &mode) != 0) {
		SDL_Log("SDL_GetDisplayMode failed: %s", SDL_GetError());
		return 1;
	}
	f = mode.format;

	SDL_Log("Mode %i\tbpp %i\t%s\t%i x %i", i,
		SDL_BITSPERPIXEL(f), SDL_GetPixelFormatName(f), mode.w, mode.h);
}
*/

int main(void)
{
	try
	{
		SDL_SetMainReady();

		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			throw; //std::runtime_error();
		}

		auto window = std::shared_ptr<SDL_Window>(SDL_CreateWindow("Space Invaders", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 224, 256, 0/*SDL_WINDOW_FULLSCREEN*/), [](SDL_Window* w) { SDL_DestroyWindow(w); });

		if (window == nullptr)
		{
			throw std::bad_alloc();
		}

		auto renderer = std::shared_ptr <SDL_Renderer>(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED), [](SDL_Renderer* r) { SDL_DestroyRenderer(r); });

		if (renderer == nullptr)
		{
			throw std::bad_alloc();
		}

		// Create a custom event for the vBlank interrupt.
		// The vBlank interrupt will fire when the 'crt beam'
		// is at end of the video display, ie; the start of
		// the vBlank. This allows us to drive the render loop.
		auto vBlankInterrupt = SDL_RegisterEvents (1);

		if (vBlankInterrupt == 0xFFFFFFFF)
		{
			throw std::bad_alloc();
		}

		//The machine to run Space Invaders on.
		auto machine = MakeMachine();
		//Create our custom Space Invaders memory controller.
		auto memoryController = std::make_shared<MemoryController>(16/* renderer*/);
		//Create our custom Space Invaders I/O controller.
		auto ioController = std::make_shared<IoController>(memoryController, vBlankInterrupt);

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		auto texture = std::shared_ptr<SDL_Texture>(SDL_CreateTexture(renderer.get(),
																	SDL_PIXELFORMAT_RGB332,
																	SDL_TEXTUREACCESS_STREAMING,
																	memoryController->GetScreenWidth(),
																	memoryController->GetScreenHeight()),
																	[]([[maybe_unused]] SDL_Texture* t) {});
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
		auto future = std::async(std::launch::async, [&]
		{
			machine->Run(0x00);
		});

		SDL_Event event;
		bool quit = false;

		auto writeVramToTexture = [&](uint8_t* vram)
		{
			uint8_t* pix = nullptr;
			int rowBytes = 0;

			if (SDL_LockTexture(texture.get(), nullptr, reinterpret_cast<void**>(&pix), &rowBytes) == 0)
			{
				auto vramEnd = vram + memoryController->GetVramLength();
				int8_t shift = 0;
				//Since we are decompressing the video ram, we will also perform the
				//required 270 degree rotation.
				auto start = pix + rowBytes * (memoryController->GetScreenHeight() - 1);
				auto ptr = pix;

				while (vram < vramEnd)
				{
					//Decompress the vram from 1bpp to 8bpp.
					*ptr = ((*vram >> shift) & 0x01) * 0xFF; //0xFF - The 8 bit colour to decompress to, in this case white, but it could be anything within the 8 bit range.
					//Cycle the shift value between 0-7.
					shift = ++shift & 0x07;
					//Move to the next vram byte if we have done a full cycle.
					vram += shift == 0;
					//If we are not at the end, move to the next row, otherwise move to the next column.
					ptr - rowBytes >= pix ? ptr -= rowBytes : ptr = ++start;
				}

				SDL_UnlockTexture(texture.get());
			}
		};

		while (quit == false)
		{
			if (SDL_WaitEvent(&event))
			{
				if (event.type == SDL_KEYDOWN)
				{
					if (event.key.keysym.sym == SDLK_q)
					{
						quit = true;
					}
				}
				else if (event.type == vBlankInterrupt)
				{
					auto vram = std::unique_ptr<uint8_t[]>(static_cast<uint8_t*>(event.user.data1));

					writeVramToTexture(vram.get());

					SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr);
					SDL_RenderPresent(renderer.get());
				}
			}
		}

		//Wait for the machine to finish.
		future.wait();

		SDL_Quit();
	}
	catch (std::exception e)
	{
		printf ("%s\n", e.what());
	}

	return 0;
}