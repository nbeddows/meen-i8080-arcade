module;

#include <assert.h>
#include <fstream>
#define SDL_MAIN_HANDLED
#include "SDL.h"

module SpaceInvaders;

import <chrono>;

using namespace std::chrono;
using namespace Emulator;

namespace SpaceInvaders
{
	MemoryController::MemoryController(uint8_t addrSize)
		: memorySize_{ static_cast<size_t>(std::pow(2, addrSize)) },
		memory_{ std::make_unique<uint8_t[]>(memorySize_) }
	{
		
	}

	std::unique_ptr<uint8_t[]> MemoryController::GetVram() const
	{
		auto vram = std::make_unique<uint8_t[]>(7168);
		memcpy(vram.get(), memory_.get() + 0x2400, 7168);
		return vram;
	}

	size_t MemoryController::Size() const
	{
		return memorySize_;
	}

	void MemoryController::Load(std::filesystem::path romFile, uint16_t offset)
	{
		std::ifstream fin(romFile, std::ios::binary | std::ios::ate);

		if (!fin)
		{
			throw std::runtime_error("The program file failed to open");
		}

		if (static_cast<size_t>(fin.tellg()) > memorySize_)
		{
			throw std::length_error("The length of the program is too big");
		}

		uint16_t size = static_cast<uint16_t>(fin.tellg());

		if (size > memorySize_ - offset)
		{
			throw std::length_error("The length of the program is too big to fit at the specified offset");
		}

		fin.seekg(0, std::ios::beg);

		if (!(fin.read(reinterpret_cast<char*>(&memory_[offset]), size)))
		{
			throw std::invalid_argument("The program specified failed to load");
		}
	}

	uint8_t MemoryController::Read(uint16_t addr)
	{
		return memory_[addr];
	}

	void MemoryController::Write(uint16_t addr, uint8_t data)
	{
		memory_[addr] = data;
	}

	ISR MemoryController::ServiceInterrupts([[maybe_unused]] nanoseconds currTime, [[maybe_unused]] uint64_t cycles)
	{
		return ISR::NoInterrupt;
	}

	IoController::IoController(const std::shared_ptr<MemoryController>& memoryController)
		: memoryController_{ memoryController }
	{

	}

	uint8_t IoController::ReadFrom(uint16_t port)
	{		
		if (port == 3)
		{
			return (shiftData_ >> (8 - shiftAmount_)) & 0xFF;
		}
		else if (port == 0)
		{
			//printf("INPUTS 0\n");
			return 0;
		}
		
		return 0;
	}

	uint8_t IoController::WriteTo(uint16_t port, uint8_t data)
	{
		if (port == 2)
		{
			//Writing to port 2 (bits 0, 1, 2) sets the offset for the 8 bit result
			shiftAmount_ = data & 0x07; //we are only interested in the first 3 bits
			return 1;
		}
		else if (port == 4)
		{
			shiftData_ = (shiftData_ >> 8) | (static_cast<uint16_t>(data) << 8);
			return 1;
		}
		else if (port == 6)
		{
			//printf("Watch-dog: %d\n", data);
			return 1;
		}

		return 0;
	}

	ISR IoController::ServiceInterrupts(nanoseconds currTime, uint64_t cycles)
	{
		auto isr = ISR::NoInterrupt;

		if (quit_ == false)
		{
			//if (cycles - lastCycleCount_ >= 16666)
			if (currTime - lastTime_ >= nanoseconds(16666666))
			{
				isr = nextInterrupt_;

				//Check last interrupt, if it is One then we are at the start of the vBlank
				if (isr == ISR::One)
				{
					//Signal vBlank interrupt.
					nextInterrupt_ = ISR::Two;
				}
				else
				{
					//Signal that the 'crt beam' is about half was down the screen.
					nextInterrupt_ = ISR::One;
				}

				//lastCycleCount_ = cycles;
				lastTime_ = currTime;
			}
		}
		else
		{
			isr = ISR::Quit;
		}

		return isr;
	}

	void IoController::Blit(uint8_t* texture, uint8_t rowBytes)
	{
		auto vram = memoryController_->GetVram().release();
		auto vramEnd = vram + memoryController_->GetVramLength();
		int8_t shift = 0;
		//Since we are decompressing the video ram, we will also perform the
		//required 270 degree rotation.
		auto start = texture + rowBytes * (memoryController_->GetScreenHeight() - 1);
		auto ptr = texture;

		while (vram < vramEnd)
		{
			//Decompress the vram from 1bpp to 8bpp.
			*ptr = ((*vram >> shift) & 0x01) * 0xFF; // 0xFF - The 8 bit colour to decompress to, in this case white, but it could be anything within the 8 bit range.
			//Cycle the shift value between 0-7.
			shift = ++shift & 0x07;
			//Move to the next vram byte if we have done a full cycle.
			vram += shift == 0;
			//If we are not at the end, move to the next row, otherwise move to the next column.
			ptr - rowBytes >= texture ? ptr -= rowBytes : ptr = ++start;
		}
	}

	SdlIoController::SdlIoController(const std::shared_ptr<MemoryController>& memoryController)
		: IoController(memoryController)
	{
		SDL_SetMainReady();

		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			throw std::runtime_error("Failed to initialise SDL");
		}

		window_ = SDL_CreateWindow("Space Invaders", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 224, 256, 0/*SDL_WINDOW_FULLSCREEN*/);

		if (window_ == nullptr)
		{
			throw std::bad_alloc();
		}

		renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);

		if (renderer_ == nullptr)
		{
			throw std::bad_alloc();
		}

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

		texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, memoryController_->GetScreenWidth(), memoryController_->GetScreenHeight());
	}

	SdlIoController::~SdlIoController()
	{
		if (renderer_ != nullptr)
		{
			SDL_DestroyRenderer(renderer_);
		}

		if (window_ != nullptr)
		{
			SDL_DestroyWindow(window_);
		}

		SDL_Quit();
	}

	uint8_t SdlIoController::Read(uint16_t port)
	{
		auto ret = IoController::ReadFrom(port);
	
		if (ret == 0)
		{
			const auto state = SDL_GetKeyboardState(nullptr);
			quit_ = state[SDL_SCANCODE_Q];

			if (quit_ == false)
			{
				auto processKeyTable = [&](int scanCode, const char* str, uint8_t bit)
				{
					if (state[scanCode] != 0)
					{
						//printf("%s\n", str);
						ret |= bit;
					}
				};

				if (port == 1)
				{
					ret = 0x08;
					processKeyTable(SDL_SCANCODE_C, "Credit", 0x01);
					processKeyTable(SDL_SCANCODE_1, "1P", 0x04);
					processKeyTable(SDL_SCANCODE_2, "2P", 0x02);
					processKeyTable(SDL_SCANCODE_A, "1P Left", 0x20);
					processKeyTable(SDL_SCANCODE_S, "1P Fire", 0x10);
					processKeyTable(SDL_SCANCODE_D, "1P Right", 0x40);
				}
				else if (port == 2)
				{
					processKeyTable(SDL_SCANCODE_3, "3 Ships", 0x00);
					processKeyTable(SDL_SCANCODE_4, "4 Ships", 0x01);
					processKeyTable(SDL_SCANCODE_5, "5 Ships", 0x02);
					processKeyTable(SDL_SCANCODE_6, "6 Ships", 0x03);
					processKeyTable(SDL_SCANCODE_T, "Tilt", 0x04);
					processKeyTable(SDL_SCANCODE_E, "Extra ship at", 0x08);
					processKeyTable(SDL_SCANCODE_J, "2P Left", 0x20);
					processKeyTable(SDL_SCANCODE_K, "2P Fire", 0x10);
					processKeyTable(SDL_SCANCODE_L, "2P Right", 0x40);
					processKeyTable(SDL_SCANCODE_I, "Show coin info", 0x80);
				}
				else
				{
					// force a failure
					assert(port == 0 || port == 3);
					//printf("Unknown device\n");
				}
			}
		}

		return ret;
	}

	void SdlIoController::Write(uint16_t port, uint8_t data)
	{
		auto ret = IoController::WriteTo(port, data);

		if (ret == 0)
		{
			if (port == 3)
			{
				//printf("Sound bits L: %d\n", data);
			}
			else if (port == 5)
			{
				//printf("Sound bits R: %d\n", data);
			}
			else
			{
				// Force a failure
				assert (port == 3 || port == 5);
				//printf("Unknown device: %d\n", data);
			}
		}
	}

	ISR SdlIoController::ServiceInterrupts(nanoseconds currTime, uint64_t cycles)
	{
		auto isr = IoController::ServiceInterrupts(currTime, cycles);
	
		if (isr == ISR::Two)
		{
			uint8_t* pix = nullptr;
			int rowBytes = 0;

			if (SDL_LockTexture(texture_, nullptr, reinterpret_cast<void**>(&pix), &rowBytes) == 0)
			{
				IoController::Blit(pix, rowBytes);
				SDL_UnlockTexture(texture_);
			}

			SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
			SDL_RenderPresent(renderer_);
		}
		else
		{
			// Check for events when we are not drawing
			SDL_PollEvent(nullptr);
		}

		return isr;
	}
}
