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
		std::array<uint8_t, 62> bmpHeader
		{
			0x42, 0x4D, 0x36, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x28, 0x00,
			0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00
		};

		//allocate the bitmap buffer
		auto bmp = std::make_unique<uint8_t[]>(7230); //(256 * 224) / 8 - Video Memory + 62 - Bitmap header
		auto bmpPtr = bmp.get();
		//The start of video ram
		auto memPtr = memory_.get() + 0x2400;
		auto bmpHeaderSize = bmpHeader.size();

		memcpy (bmpPtr, bmpHeader.data(), bmpHeaderSize);
		memcpy (bmpPtr + bmpHeaderSize, memPtr, 7168);

		printf ("Writing current video ram to ../roms/invaders.bmp\n");

		std::ofstream fout("../roms/invaders.bmp", std::ofstream::binary);

		if (fout.fail() == false)
		{
			fout.write (reinterpret_cast<char*>(bmpPtr), 7230);

			if (fout.fail() == false)
			{
				printf("Write complete\n");
			}
			else
			{
				printf ("Failed to write bitmap to ../roms/invaders.bmp.\n");
			}
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
		/*
			Write the data to the relevant output device
			according to the following:

			Port 2:
				bit 0,1,2 Shift amount

			Port 3: (discrete sounds)
				bit 0=UFO (repeats)        SX0 0.raw
				bit 1=Shot                 SX1 1.raw
				bit 2=Flash (player die)   SX2 2.raw
				bit 3=Invader die          SX3 3.raw
				bit 4=Extended play        SX4
				bit 5= AMP enable          SX5
				bit 6= NC (not wired)
				bit 7= NC (not wired)
				Port 4: (discrete sounds)
				bit 0-7 shift data (LSB on 1st write, MSB on 2nd)

			Port 5:
				bit 0=Fleet movement 1     SX6 4.raw
				bit 1=Fleet movement 2     SX7 5.raw
				bit 2=Fleet movement 3     SX8 6.raw
				bit 3=Fleet movement 4     SX9 7.raw
				bit 4=UFO Hit              SX10 8.raw
				bit 5= NC (Cocktail mode control ... to flip screen)
				bit 6= NC (not wired)
				bit 7= NC (not wired)

			Port 6:
				Watchdog ... read or write to reset
		*/
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
