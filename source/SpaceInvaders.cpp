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
		/*
			Read the value from the input device (keyboard for example)
			and set the relevant bit in the return value according to
			the following:

			Port 0
				 bit 0 DIP4 (Seems to be self-test-request read at power up)
				 bit 1 Always 1
				 bit 2 Always 1
				 bit 3 Always 1
				 bit 4 Fire
				 bit 5 Left
				 bit 6 Right
				 bit 7 ? tied to demux port 7 ?

			Port 1
				 bit 0 = CREDIT (1 if deposit)
				 bit 1 = 2P start (1 if pressed)
				 bit 2 = 1P start (1 if pressed)
				 bit 3 = Always 1
				 bit 4 = 1P shot (1 if pressed)
				 bit 5 = 1P left (1 if pressed)
				 bit 6 = 1P right (1 if pressed)
				 bit 7 = Not connected

			Port 2
				 bit 0 = DIP3 00 = 3 ships  10 = 5 ships
				 bit 1 = DIP5 01 = 4 ships  11 = 6 ships
				 bit 2 = Tilt
				 bit 3 = DIP6 0 = extra ship at 1500, 1 = extra ship at 1000
				 bit 4 = P2 shot (1 if pressed)
				 bit 5 = P2 left (1 if pressed)
				 bit 6 = P2 right (1 if pressed)
				 bit 7 = DIP7 Coin info displayed in demo screen 0=ON

			Port 3
				bit 0-7 Shift register data

		*/
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
