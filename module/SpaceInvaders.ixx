export module SpaceInvaders;

import <chrono>;
import IController;
import Base;

using namespace std::chrono;
using namespace Emulator;

namespace SpaceInvaders
{
	/** Custom memory controller.

		A custom memory controller targetting the Space Invaders arcade ROM.

		This class uses the supplied IMemoryController interface which adds
	*/
	export class MemoryController final : public IMemoryController
	{
	private:
		/** Memory size.

			The size in bytes of the memory.
		*/
		size_t memorySize_{};

		/** Memory buffer.

			The memory bytes that the cpu will read from and write to.
		*/
		std::unique_ptr<uint8_t[]> memory_;
	public:
		/** Contructor.

			Create a memory controller that can address memory of the
			specified address bus size. For this demo Space Invaders
			runs on an Intel8080 with 64k of memory, therefore the
			address bus size will be 16.

			@param		addressBusSize	The size of the address bus.
										This will be 16.
		*/
		MemoryController(uint8_t addressBusSize);

		/** Destructor.

			Performs operations that are required when the memory controller
			will be destroyed.

			For this demonstration, we are going to write out the state of the
			video memory to disk as a bitmap located in roms/invaders.bmp.
		*/
		~MemoryController();

		/** Load ROM file.

			Loads the specified rom file and the given memory address offset.

			@see	IMemoryController::Load for parameter explanations and expected behaviours.

			Space Invaders rom files have the following ROM layout:

				invaders-h 0000-07FF
				invaders-g 0800-0FFF
				invaders-f 1000-17FF
				invaders-e 1800-1FFF
		*/
		void Load(std::filesystem::path romFile, uint16_t offset) override final;

		/** Memory size.

			Returns the size of the memory, in our example it will be 64k (2^addressBusSize(16))

			@see	IMemoryController::Size for further details.
		*/
		size_t Size() const override final;

		/** Read from controller.

			Reads 8 bits of data from the specifed 16 bit memory address.

			@see IController::Read for further details.
		*/
		uint8_t Read(uint16_t address) override final;

		/** Write to controller.

			Write 8 bits of data to the specifed 16 bit memory address.

			@see IController::Write for further details.
		*/
		void Write(uint16_t address, uint8_t value) override final;

		/** Service memory interrupts.

			Memory interrupts are never generated.

			The function will always return ISR::NoInterrupt.
		*/
		ISR ServiceInterrupts(nanoseconds currTime) override final;
	};

	/** Custom io controller.

		A custom io controller targetting the Space Invaders arcade ROM.
	*/
	export class IoController final : public IController
	{
	private:
		/** The next interrupt to execute.

			nextInterrupt_ holds the next ISR that will be sent to the cpu.

			Space Invaders listens for two interrupts, ISR::One and ISR::Two.
			ISR::One is issued when the 'CRT beam' is near the center of the screen.
			ISR::Two is issued when the 'CRT beam' is at the end of the screen (VBLANK start).
		*/
		ISR nextInterrupt_{ ISR::NoInterrupt };

		/** Last cpu time.

			lastTime_ holds the previous cpu time that ServiceInterrupts was last called.

			In order to emulate a 60hz display we need to fire ISR::One and ISR::Two
			at 60hz intervals.
		*/
		nanoseconds lastTime_{};

		/** Dedicated Shift Hardware.

			The 8080 instruction set does not include opcodes for shifting.
			An 8-bit pixel image must be shifted into a 16-bit word for the desired bit-position on the screen.
			Space Invaders adds a hardware shift register to help with the math.

			shiftIn_, shiftAmount_ and shiftData_ help emulate this.
		*/
		uint8_t shiftIn_{};
		uint8_t shiftAmount_{};
		uint8_t shiftData_{};
	public:
		/** Read from controller.

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

			Currently, a message with the port number is printed out and zero will be returned.

			@param		port		The input device to read from.

			@return		uint8_t		Currently this always returns 0.

			@todo		Complete the port reading and data writing according to the above information.
		*/
		uint8_t Read(uint16_t port) override final;

		/** Write to controller.

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

			Currently we are just printing a message with port and
			specified data.

			@param	port	The output device to write to.
			@param	data	The data to write to the output device.

			@todo			Write data to the specified output device according to
							the above information.
		*/
		void Write(uint16_t port, uint8_t data) override final;

		/** Service io interrupts.

			For demonstration purposes we will return ISR::Quit
			after 10 seconds of run time.

			@return		ISR		During the 10 seconds of run time ISR::One and ISR::Two
								will be returned at 60hz intervals. This informs the ROM
								that it is safe to draw to the top and bottom of the
								video ram.
		*/
		ISR ServiceInterrupts(nanoseconds currTime) override final;
	};
}