module;

#include <fstream>

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

	MemoryController::~MemoryController()
	{
		//Dump the current state of the video ram to disk as a 1 bpp bitmap.

		/*
			Memory Map
				0000 - 1FFF 8K ROM
				2000 - 23FF 1K RAM
				2400 - 3FFF 7K Video RAM
				4000 - RAM mirror
		*/

		//A bitmap header for a 256 * 224 image @ 1bpp
		static constexpr std::array<uint8_t, 62> bmpHeader
		{
			0x42, 0x4D, 0x36, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x28, 0x00,
			0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00
		};

		printf ("Writing current video ram to ../roms/invaders.bmp\n");

		std::ofstream fout("../roms/invaders.bmp", std::ofstream::binary);

		if (fout.fail() == false)
		{
			fout.write(reinterpret_cast<char*>(const_cast<uint8_t*>(bmpHeader.data())), bmpHeader.size());
			//The start of video ram is at 0x2400
			fout.write(reinterpret_cast<char*>(memory_.get() + 0x2400), 7168); //7168 - (256 (width) * 224 (height)) / 8

			if (fout.fail() == false)
			{
				printf("Write complete\n");
			}
			else
			{
				printf ("Failed to write bitmap to ../roms/invaders.bmp.\n");
			}

			fout.close();
		}
		else
		{
			printf ("Failed to open ../roms/invaders.bmp for writing.\n");
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

	uint8_t IoController::Read(uint16_t port)
	{
		switch (port)
		{
			case 0:
				printf("INPUTS 0\n");
				break;
			case 1:
				printf("INPUTS 1\n");
				break;
			case 2:
				printf("INPUTS 2\n");
				break;
			case 3:
				printf("Bit shift register read\n");
				break;
			default:
				printf("Unknown device\n");
				break;
		}

		return 0;
	}

	void IoController::Write(uint16_t port, uint8_t data)
	{
		switch (port)
		{
			case 2:
			{
				printf("Shift amount: %d\n", data);
				break;
			}
			case 3:
			{	
				printf("Sound bits L: %d\n", data);
				break;
			}
			case 4:
			{
				printf("Shift data: %d\n", data);
				break;
			}
			case 5:
			{
				printf("Sound bits R: %d\n", data);
				break;
			}
			case 6:
			{
				printf("Watch-dog: %d\n", data);
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
				nextInterrupt_ = ISR::Two;
			}
			else
			{
				nextInterrupt_ = ISR::One;
			}
		}

		//To determine quit, one would have to poll an input device,
		//for example; a keyboard checking for an ESC key.
		//For demonstration purposes we will quit after 10 seconds.
		if (currTime >= nanoseconds(10000000000))
		{
			isr = ISR::Quit;
		}

		return isr;
	}
}
