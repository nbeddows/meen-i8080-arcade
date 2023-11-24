module;

#include <fstream>
#include "SDL.h"

module SpaceInvaders;

import <chrono>;

using namespace std::chrono;
using namespace Emulator;

namespace SpaceInvaders
{
	MemoryController::MemoryController(uint8_t addrSize)
	{
		memorySize_ = static_cast<size_t>(std::pow(2, addrSize));
		memory_ = std::make_unique<uint8_t[]>(memorySize_);
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

	IoController::IoController(const std::shared_ptr<MemoryController>& memoryController, int vBlankInterrupt)
	{
		memoryController_ = memoryController;
		vBlankInterrupt_ = vBlankInterrupt;
	}

	uint8_t IoController::Read(uint16_t port)
	{
		uint8_t ret = 0;
		const Uint8* state = SDL_GetKeyboardState(NULL);
		quit_ = state[SDL_SCANCODE_Q];

		if (quit_ == false)
		{
			auto processKeyTable = [&](int scanCode, const char* str, uint8_t bit)
			{
				if (state[scanCode] != 0)
				{
					printf("%s\n", str);
					ret |= bit;
				}
			};

			switch (port)
			{
				case 0:
					printf("INPUTS 0\n");
					break;
				case 1:
					ret = 0x08;
					processKeyTable(SDL_SCANCODE_C, "Credit", 0x01);
					processKeyTable(SDL_SCANCODE_1, "1P", 0x04);
					processKeyTable(SDL_SCANCODE_2, "2P", 0x02);
					processKeyTable(SDL_SCANCODE_A, "1P Left", 0x20);
					processKeyTable(SDL_SCANCODE_S, "1P Fire", 0x10);
					processKeyTable(SDL_SCANCODE_D, "1P Right", 0x40);
					break;
				case 2:
					processKeyTable(SDL_SCANCODE_3, "3 Ships", 0x00);
					processKeyTable(SDL_SCANCODE_4, "4 Ships", 0x01);
					processKeyTable(SDL_SCANCODE_5, "5 Ships", 0x02);
					processKeyTable(SDL_SCANCODE_6, "6 Ships", 0x03);
					processKeyTable(SDL_SCANCODE_T, "Tilt", 0x04);
					processKeyTable(SDL_SCANCODE_E, "Extra ship at", 0x04);
					processKeyTable(SDL_SCANCODE_J, "2P Left", 0x20);
					processKeyTable(SDL_SCANCODE_K, "2P Fire", 0x10);
					processKeyTable(SDL_SCANCODE_L, "2P Right", 0x40);
					processKeyTable(SDL_SCANCODE_I, "Show coin info", 0x80);
					break;
				case 3:
					ret = (shiftData_ >> (8 - shiftAmount_)) & 0xFF;
					break;
				default:
					printf("Unknown device\n");
					break;
			}
		}

		return ret;
	}

	void IoController::Write(uint16_t port, uint8_t data)
	{
		switch (port)
		{
			case 2:
			{
				//Writing to port 2 (bits 0, 1, 2) sets the offset for the 8 bit result
				shiftAmount_ = data & 0x07; //we are only interested in the first 3 bits
				break;
			}
			case 3:
			{	
				//printf("Sound bits L: %d\n", data);
				break;
			}
			case 4:
			{
				shiftData_ = (shiftData_ >> 8) | (static_cast<uint16_t>(data) << 8);
				break;
			}
			case 5:
			{
				//printf("Sound bits R: %d\n", data);
				break;
			}
			case 6:
			{
				//printf("Watch-dog: %d\n", data);
				break;
			}
			default:
			{
				printf("Unknown device: %d\n", data);
				break;
			}
		}
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
					SDL_Event event;
					event.type = vBlankInterrupt_;
					event.user.data1 = static_cast<void*>(memoryController_->GetVram().release());
					SDL_PushEvent(&event);

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
}
