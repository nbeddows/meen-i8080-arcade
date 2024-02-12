/*
Copyright (c) 2021-2024 Nicolas Beddows <nicolas.beddows@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef IO_CONTROLLER_H
#define IO_CONTROLLER_H

#include <array>
#include <atomic>
#include <memory>

#include "Base/Base.h"
#include "nlohmann/json_fwd.hpp"
#include "SpaceInvaders/MemoryController.h"

namespace SpaceInvaders
{
	/** Custom io controller.

		A custom io controller targetting the Space Invaders arcade ROM.
	*/
	class IoController : public MachEmu::IController
	{
	private:
		enum BlitFlags
		{
			Native		= 0 << 0,
			Rgb332		= 1 << 0,
			Upright		= 1 << 1,
			Upright8bpp = Upright | Rgb332
		};

		/** The next interrupt to execute.

			nextInterrupt_ holds the next ISR that will be sent to the cpu.

			Space Invaders listens for two interrupts, ISR::One and ISR::Two.
			ISR::One is issued when the 'CRT beam' is near the center of the screen.
			ISR::Two is issued when the 'CRT beam' is at the end of the screen (VBLANK start).
		*/
		MachEmu::ISR nextInterrupt_{ MachEmu::ISR::One };
		
		/** Last cpu time.

			lastTime_ holds the previous cpu time that ServiceInterrupts was last called.

			In order to emulate a 60hz display we need to fire ISR::One and ISR::Two
			at 60hz intervals.
		*/
		uint64_t lastTime_{};
		
		/** Dedicated Shift Hardware.

			The 8080 instruction set does not include opcodes for shifting.
			An 8-bit pixel image must be shifted into a 16-bit word for the desired bit-position on the screen.
			Space Invaders adds a hardware shift register to help with the math.

			shiftIn_, shiftAmount_ and shiftData_ help emulate this.
		*/
		//cppcheck-suppress unusedStructMember
		uint8_t shiftIn_{};
		//cppcheck-suppress unusedStructMember
		uint8_t shiftAmount_{};
		//cppcheck-suppress unusedStructMember
		uint16_t shiftData_{};

		/**	Backup port 3 and port 5 bytes.
		
			These ports are used for writing to an output audio device.
			We only want to write the audio when the required port bit
			changes from off to on, hence we need to backup these bytes
			to make that comparison.
		*/
		//cppcheck-suppress unusedStructMember
		uint8_t port3Byte_{};
		//cppcheck-suppress unusedStructMember
		uint8_t port5Byte_{};

		// compression/orientation flags - default: cocktail at 1bpp
		uint8_t blitMode_{ BlitFlags::Native };

		// vram foregound colour - default is black (this is ignored when Rgb332 is off which it is by default)
		uint8_t colour_{ 0xFF };

	protected:
		/** The maximum number of output audio sample files.

			There are only 9 audio files that are used.

			@see wavFiles_
		*/
		static constexpr uint8_t totalWavFiles_ = 16;

		/** Video RAM access.

			When the video ram is ready to be blitted it is sampled
			from the memory controller at the vram address.
		*/
		std::shared_ptr<MemoryController> memoryController_;
		
		/** Exit control loop.

			A value of true will cause the Machine control loop to exit.
			This can be set, for example, when the keyboard 'q' key is pressed.
		*/
		std::atomic_bool quit_{};

		/** The audio files to use for sound effects.

			NOTE: DO NOT change the order of these files as they corrospond to the
			correct port number bits of port 3 (low 8 bits) and 5 (high 8 bits)
		*/
		std::array<const char*, totalWavFiles_> wavFiles_ 
		{
			ROMS_DIR"/ufo_highpitch.wav",	/**< UFO */
			ROMS_DIR"/shoot.wav",			/**< Player fire */
			ROMS_DIR"/explosion.wav",		/**< Player killed */
			ROMS_DIR"/invaderkilled.wav",	/**<	Invader killed */
			nullptr,						/**< Extended Play */
			nullptr,						/**< AMP Enable */
			nullptr,						/**< Unused */
			nullptr,						/**< Unused */
			ROMS_DIR"/fastinvader1.wav",		/**< Invader fleet movement 1 */
			ROMS_DIR"/fastinvader2.wav",		/**< Invader fleet movement 1 */
			ROMS_DIR"/fastinvader3.wav",		/**< Invader fleet movement 1 */
			ROMS_DIR"/fastinvader4.wav",		/**< Invader fleet movement 1 */
			ROMS_DIR"/ufo_lowpitch.wav",		/**< UFO hit */
			nullptr,						/**< Unused */
			nullptr,						/**< Unused */
			nullptr							/**< Unused */
		};

		// Default resolution - native (cocktail (horizontal), 1bpp)
		int width_{ MemoryController::VideoFrame::width };
		int height_{ MemoryController::VideoFrame::height };

	public:
		/** Initialisation contructor.

			Creates an io controller which has access to the memory controller for
			video ram access.

			@param		memoryController	The memory controller where the video ram resides.
		*/
		IoController(const std::shared_ptr<MemoryController>& memoryController, const nlohmann::json& config);

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

			@param		port		The input device to read from.

			@return		uint8_t		Non zero if the port was read from, zero otherwise.
		*/
		uint8_t ReadFrom(uint16_t port);

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

			@param	port		The output device to write to.
			@param	data		The data to write to the output device.

			@return				Audio that requires rendering as described above.
		*/
		uint8_t WriteTo(uint16_t port, uint8_t data);

		/** Service io interrupts.

			Return ISR::One and ISR::Two at 60hz intervals.
			This informs the ROM that it is safe to draw to
			the top and bottom of the video ram.

			@return		ISR		ISR::One when the 'beam' is near the centre of the screen,
								ISR::Two when the 'beam' is at the end (vBlank). 
		*/
		MachEmu::ISR ServiceInterrupts(uint64_t currTime, uint64_t cycles) override;

		/** Write space invaders vram to texture.
		
			The vram is written with a 90 degree rotation, therefore it needs to be
			rotated a further 270 degrees so it can be rendered with the correct
			orientation.

			@param	dst			The video memory to write to (texture memory).
			@param	src			The video ram to copy.
			@param	rowBytes	The width of each scanline in bytes.

		*/
		void Blit(uint8_t* dst, uint8_t* src, uint8_t rowBytes);
	};
} // namespace SpaceInvaders

#endif // IO_CONTROLLER_H