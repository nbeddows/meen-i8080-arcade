module;

#include <fstream>
#include <windows.h>
//#include <unistd.h>
//#include <poll.h>
#include "SDL.h"

module SpaceInvaders;

import <chrono>;

using namespace std::chrono;
using namespace Emulator;

namespace SpaceInvaders
{
	MemoryController::MemoryController(uint8_t addrSize, const std::shared_ptr<SDL_Renderer>& renderer)
	{
		memorySize_ = static_cast<size_t>(std::pow(2, addrSize));
		memory_ = std::make_unique<uint8_t[]>(memorySize_);

		SDL_Rect rect;
		SDL_RenderGetViewport(renderer.get(), &rect);
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		videoRam_ = std::shared_ptr<SDL_Texture>(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, rect.w, rect.h), []([[maybe_unused]] SDL_Texture* t) {});

		if (videoRam_ == nullptr)
		{
			throw std::bad_alloc();
		}

		renderer_ = renderer;
	}

	MemoryController::~MemoryController()
	{
		printf("Write current memory to ../roms/memory.bin\n");

		std::ofstream fout("../roms/memory.bin", std::ofstream::binary);

		if (fout.fail() == false)
		{
			fout.write(reinterpret_cast<char*>(memory_.get()), memorySize_);

			if (fout.fail() == false)
			{
				printf("Write complete\n");
			}
			else
			{
				printf("Failed to write memory to ../roms/memory.bin.\n");
			}

			fout.close();
		}
		else
		{
			printf("Failed to open ../roms/memory.bin for writing.\n");
		}
	}

	void MemoryController::WriteVRAM()
	{
		/*
			Memory Map
				0000 - 1FFF 8K ROM
				2000 - 23FF 1K RAM
				2400 - 3FFF 7K Video RAM
				4000 - RAM mirror
		*/

#if 0
		//Dump the current state of the video ram to disk as a 1 bpp bitmap.

		//A bitmap header for a 256 * 224 image @ 1bpp
		static constexpr std::array<uint8_t, 62> bmpHeader
		{
			0x42, 0x4D, 0x36, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x28, 0x00,
			0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00
		};

		std::ofstream fout("../roms/invaders.bmp", std::ofstream::binary);

		if (fout.fail() == false)
		{
			fout.write(reinterpret_cast<char*>(const_cast<uint8_t*>(bmpHeader.data())), bmpHeader.size());
			//The start of video ram is at 0x2400
			fout.write(reinterpret_cast<char*>(memory_.get() + 0x2400), 7168); //7168 - (256 (width) * 224 (height)) / 8

			if (fout.fail())
			{
				printf("Failed to write bitmap.\n");
			}

			fout.close();
		}
		else
		{
			printf("Failed to open bitmap for writing.\n");
		}
#endif
		uint8_t* pix = nullptr;
		int rowBytes = 0;
		auto vram = memory_.get() + 0x2400;
		auto vramEnd = vram + 7168;
		int8_t shift = 7;

		if (SDL_LockTexture(videoRam_.get(), nullptr, reinterpret_cast<void**>(&pix), &rowBytes) == 0)
		{
			auto ptr = pix;

			while (vram < vramEnd)
			{
				//Decompress the vram from 1bpp to 8bpp
				*ptr = ((*vram >> shift--) & 0x01) * 0xFF;

				if (shift < 0)
				{
					shift = 7;
					vram++;
				}

				ptr++;

				if (ptr - pix == 256)
				{
					pix += rowBytes;
					ptr = pix;
				}
			}

			SDL_UnlockTexture(videoRam_.get());
			SDL_RenderCopy(renderer_.get(), videoRam_.get(), nullptr, nullptr);
			SDL_RenderPresent(renderer_.get());
		}
		else
		{
			printf ("Failed to lock texture: %s\n", SDL_GetError());
		}
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

	ISR MemoryController::ServiceInterrupts([[maybe_unused]] nanoseconds currTime)
	{
		return ISR::NoInterrupt;
	}

	IoController::IoController(const std::shared_ptr<MemoryController>& memoryController)
	{
		memoryController_ = memoryController;
	}

	uint8_t IoController::Read(uint16_t port)
	{
		DWORD numEvents = 0;

		if (GetNumberOfConsoleInputEvents (GetStdHandle(STD_INPUT_HANDLE), &numEvents) != 0 && numEvents > 0)
		{
			INPUT_RECORD rec[1];
			DWORD numEventsRead = 0;

			for (DWORD i = 0; i < numEvents; i++)
			{
				if (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), rec, 1, &numEventsRead) != 0 && numEventsRead > 0)
				{
					if (rec[0].EventType == KEY_EVENT)
					{
						keyTable_[rec[0].Event.KeyEvent.uChar.AsciiChar] = rec[0].Event.KeyEvent.bKeyDown == TRUE;
					}
				}
			}
		}

		/*
		struct pollfd mypoll = { STDIN_FILENO, POLLIN | POLLPRI };
		char string[10];

		if (poll(&mypoll, 1, 2000))
		{
			scanf("%9s", string);
			printf("Read string - %s\n", string);
		}
		else
		{
			puts("Read nothing");
		}
		*/

		uint8_t ret = 0;
		quit_ = keyTable_['q'];

		if (quit_ == false)
		{
			auto processKeyTable = [&](char key, const char* str, uint8_t bit)
			{
				if (keyTable_[key] == true)
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
					processKeyTable('c', "Credit", 0x01);
					processKeyTable('1', "1P", 0x04);
					processKeyTable('2', "2P", 0x02);
					processKeyTable('a', "1P Left", 0x20);
					processKeyTable('s', "1P Fire", 0x10);
					processKeyTable('d', "1P Right", 0x40);
					break;
				case 2:
					processKeyTable('3', "3 Ships", 0x00);
					processKeyTable('4', "4 Ships", 0x01);
					processKeyTable('5', "5 Ships", 0x02);
					processKeyTable('6', "6 Ships", 0x03);
					processKeyTable('t', "Tilt", 0x04);
					processKeyTable('e', "Extra ship at", 0x04);
					processKeyTable('j', "2P Left", 0x20);
					processKeyTable('k', "2P Fire", 0x10);
					processKeyTable('l', "2P Right", 0x40);
					processKeyTable('i', "Show coin info", 0x80);
					break;
				case 3:
					//printf("Bit shift register read\n");
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
				//printf("Shift amount: %d\n", data);

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
				//printf("Shift data: %d\n", data);

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

	ISR IoController::ServiceInterrupts(nanoseconds currTime)
	{
		auto isr = ISR::NoInterrupt;

		if (quit_ == false)
		{
			//The raster resolution is 256x224 at 60Hz.
			//Space Invaders triggers two interrupts, ISR::One
			//when it gets to the middle of the screen and
			//ISR::Two when it gets the end of the screen
			//(Start of VBLANK).
			if (currTime - lastTime_ > nanoseconds(16666666))
			{
				lastTime_ = currTime;
				isr = nextInterrupt_;

				if (isr == ISR::One)
				{
					//We are at the start of the VBLANK
					nextInterrupt_ = ISR::Two;

					memoryController_->WriteVRAM();
				}
				else
				{
					nextInterrupt_ = ISR::One;
				}
			}
		}
		else
		{
			isr = ISR::Quit;
		}

		return isr;
	}
}
