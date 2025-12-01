/*
Copyright (c) 2021-2025 Nicolas Beddows <nicolas.beddows@gmail.com>

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

#ifndef SDLIOCONTROLLER_H
#define SDLIOCONTROLLER_H

#define ARDUINOJSON_ENABLE_STRING_VIEW 1
#include <ArduinoJson.h>
#include <atomic>
#include <condition_variable>
#include <SDL.h>
#include <SDL_mixer.h>
#include <variant>
#include <vector>

#include "meen_i8080_arcade/IIoController.h"
#include "meen_hw/MH_Factory.h"
#include "meen_hw/MH_ResourcePool.h"

namespace meen_i8080_arcade
{
	/** Custom SDL io controller.

		A custom io controller targetting Space Invaders i8080 arcade hardware compatible ROMs.
	*/
	class SDLIoController final : public IIoController
	{
		private:
			/** SDL Renderer

				The window rendering context.
			*/
			//cppcheck-suppress unusedStructMember
			SDL_Renderer* renderer_{};

			/**	SDL_texture

				The texture which will hold the video ram for rendering.
			*/
			//cppcheck-suppress unusedStructMember
			SDL_Texture* texture_{};

			/** Blitting rectangle.

				The destination bounding box within the SDL window to blit the video texture.
			*/
			//cppcheck-suppress unusedStructMember
			SDL_Rect dstRect_{};

			/** SDL_Window

				The window to draw the video ram to.
			*/
			//cppcheck-suppress unusedStructMember
			SDL_Window* window_{};

			/**	i8080 arcade io

				The hardware emulator.
			*/
			std::unique_ptr<meen_hw::MH_II8080ArcadeIO> i8080ArcadeIO_;

			/** A chunk of audio samples

    	        A collection of audio samples with identical properties.
			*/
			//cppcheck-suppress unusedStructMember
			std::vector<Mix_Chunk*> mixChunk_;

			/** Timed video frame
			
				A frame of video ram taken from the memory controller frame pool
				with the addition of a time stamp.
			*/
			struct Frame
        	{
				/** Video frame
					
					This is a memory controller frame pool resource
				*/
            	meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr bitstream;

				/** Time stamp
				
					The time at which the vram was sampled in MEEN timescale units.
				*/
            	int64_t timestamp;
        	};

			/** Helper type for functional style visitor for std::visit

				This template helper type is taken straight from cppreference std::visit examples (https://en.cppreference.com/w/cpp/utility/variant/visit2)
			*/
			template<class... Ts>
			struct overloaded : Ts... { using Ts::operator()...; };

			/** Generated event data

				A using directve for ease of use. This will hold the active event data to be processed.

				uint16_t:		Audio is ready to be played. The high 8 bits is the audio port, the low 8 bits are the audio samples to play.
				std::string:	The application has encountered and error.
				Frame:			The next video frame is ready to be rendered. This event drives the control loop.

				The EventData will be assigned to the SDL_Event data1 property.
			*/
			using EventData = std::variant<std::string, uint16_t, Frame>;
			
			/** Event data queue

				Holds a list of events to be processed.
			*/
        	std::list<EventData> eventQ_;

			/** Event queue mutex

				Event data mutual exclusion between the main thread and the machine thread.
			*/
        	meen_hw::MH_Mutex eventQMutex_;

			/** Event queue condition variable

            	Used in conjuction with eventQMutex_ to signal when new EventData is ready for processing.
    	    */
	        meen_hw::MH_ConditionVariable eventQCv_;

			/** Load a game rom or the save state of the currently loaded game rom

				@remark		This value can be set from a different thread, hence it is atomic.
			*/
			std::atomic_bool loadSaveState_{};

			/**	Load or save

				A machine level interrupt which indicates whether or not the machine
				should attempt to load a new state or save its current state.

				meen::ISR::NoInterrupt: don't load or save the state.
				meen::ISR::Load: attempt to load a new rom.
				meen::ISR::Save: attempt to save the current loaded rom state.

				@remark		This value can be set from a different thread, hence it is atomic.
			*/
			std::atomic<meen::ISR> loadSaveInterrupt_{ meen::ISR::NoInterrupt };

			/** Keep track of previous key presses to prevent repeat events from triggering

				key 'down' moves to the next rom.
				Key 'r' restores the currently loaded rom save state (if it exists).
				Key 'return' loads the currently selected rom.
				Key 'up' moves to the previous rom.
				Key 'y' save the currently selected roms state.
			*/
			Uint8 lastDown_{};
			Uint8 lastR_{};
			Uint8 lastReturn_{};
			Uint8 lastUp_{};
			Uint8 lastY_{};

			/** The currently selected rom

				When the user presses the up and down arrows, this will keep track
				of the current index.

				Made atomic since it can be accesssed from a different thread if the runAsync config option
				is set to true.
			*/
			std::atomic_int romIndex_{};

			/** The total number of supported roms for this controller.

				The value is the max limit used by the romIndex parameter to keep
				itself within range.
			*/
			int romCount_{};

			/** The running state

				True if meen is to run on a different thread to the main application,
				false otherwise.
			*/
			const bool runAsync_{};

			/** The current screen

				See the Screen enumeration for further details.
			*/
			Screen screen_{};

			/** The SDL keyboard state

				This is the return value of the SDL_GetKeyboardState api call.
			*/
			const uint8_t* sdlKbState_{};

			/** The shared keyboard state

				We copy the required keyboard state into this array for non main thead access as we assume to pointer
				returned from the SDL_GetKeyboardState method (sdlKbState_) should not be accessed from a non main thread.
			*/
			std::array<std::atomic_bool, SDL_NUM_SCANCODES> kbState_;

			/** Samples that are currently playing.

				Dedicate an individual channel to each sample (while not all samples can be played at the same time,
				it just makes things easier).

				@remark		Declare 16 channels as this is what is supported, even though some slots remain unused.
			*/
			static std::array<std::atomic_bool, MIX_CHANNELS * 2> channelPlaying_;

			/** Assign a load or save machine interrupt

				Peforms a check of the key once during a key press and release sequence.

				@param	key				The press or release state of the key.
				@param	lastKey			The previous press or release state of the key.
				@param	isr				The interrupt to assign.
				@param	loadSaveState	True if the current save state is to be loaded,
										false otherwise.

				@return					The current key state (the key parameter).
			*/
			Uint8 SetInterrupt(bool key, bool lastKey, meen::ISR isr, bool loadSaveState);

		public:
			/** Default constructor
			
				Not supported.
			*/
			SDLIoController() = delete;

			/** Initialisation constructor

				Creates an SDL specific i8080 arcade IO controller.

				@param		runAsync		Run this io controller asynchronously.
				@param		romCount		The number of supported roms.
				@param		audioHardware	Audio hardware configuration options.
				@param		videoHardware	Video hardware configuration options.
			*/
			SDLIoController(bool runAsync, int romCount, const JsonVariantConst audioHardware, const JsonVariantConst videoHardware);

			/** Destructor

				Free the various required SDL objects.
			*/
			~SDLIoController();

			/** IController::Read override

				Sample the keyboard so the CPU can take any required action.

				@param	port				The device to read from.
	            @param  memoryController    The memory controller that has been registered with MEEN.


				@return			A bitfield indicating the action to take.
			*/
			uint8_t Read(uint16_t port, meen::IController* memoryController) final;

			/** IController::Write override

				Write the relevant audio sample to the output audio device.

				@param	port				The output device to write to.
				@param	data				A bitfield indicating what data to write.
	            @param  memoryController    The memory controller that has been registered with MEEN.
			*/
			void Write(uint16_t port, uint8_t data, meen::IController* memoryController) final;

			/** IController::GenerateInterrupt override

				Render the video ram texture to the window via the rendering context.

        		@param  currTime            The current CPU run time in nanoseconds.
            	@param  cycles              The number of CPU cycles completed.
            	@param  memoryController    The memory controller that has been registered with MEEN.

            	@return                     One of the following meen ISRs:<br><br>
                	                        `ISR::NoInterrupt`: the method did not generate an iterrupt.<br>
                    	                    `ISR::One`: signal MEEN that the first 96 scanlines have been rendered.<br>
                        	                `ISR::Two`: signal MEEN that the remaining scanlines (up to 224) have
                            	            been rendered (start of vblank).<br>
											`ISR::Load`: attempt to load a new machine state.<br>
											`ISR::Save`: attempt to save the current machine state.
			*/
			meen::ISR GenerateInterrupt(uint64_t currTime, uint64_t cycles, meen::IController* memoryController) final;

			/**	Uuid

				Unique universal identifier for this controller.

				@return					The uuid as a 16 byte array.
			*/
			std::array<uint8_t, 16> Uuid() const final;

			/**	Event handler

				Process all incoming events.

				Events include audio/video rendering, keyboard processing and window close.

	            @return                 True to quit the machine, false otherwise.
			*/
			bool HandleEvent() final;

			/** Error handler

            	Process any generated errors

        		These errors may come from MEEN or meen-i8080-arcade itself.

				@param	errorMsg		The error message as a `std::string`.
			*/
			void HandleError(std::string&& errorMsg) final;

			/** Load complete handler

				Handle the transition into game play when a rom has been sucessfully loaded.
			*/
			void HandleLoadComplete() final;

			/** Load Audio Samples

				Use SDL Mixer to load the audio samples.

				@param	audioSamples	JSON object representing the audio sample files.

	            @return					On failure, a `std::error_code` with one of the following values:<br><br>
										`std::errc::no_such_file_or_directory`: the audio resource specified by the `file://`
										protocol failed to open.<br>
                                        `std::errc::not_supported`: the audio file scheme in the configuration file is invalid.<br>
                                        `std::errc::not_supported`: the number of audio channels defined in the configuration file
										can't be allocated.<br>
                                        `std::errc::invalid_argument`: the length of the audio configuration samples array is not
										supported.
			*/
			std::error_code LoadAudioSamples(const JsonVariantConst audioSamples) final;

			/** Load Video Textures

				Create the video texture that will be rendered to the screen.

				@param	videoTextures	JSON object describing the video texture.
				@param  textureWidth    The width of the videc texture in pixels.
            	@param  textureHeight   The height of the video texture in pixels.

	            @return                 On failure, a `std::error_code` with one of the following values:<br><br>
        	                            `std::errc:io_error`: video configuration serialisation failure.<br>
    	                                `std::errc::not_supported`: the video configuration parameters are invalid.<br>
										`std::errc::not_enough_memory`: failed to allocate the video textures.
			*/
			std::error_code LoadVideoTextures(const JsonVariantConst videoTextures, int textureWidth, int textureHeight) final;

			/** Get the rom index

				Load the selected rom or the save state of the currently selected rom.

				@return					A tuple holding two values:<br><br>
										`bool`: only valid when loading roms, true if the save file is to be loaded, false if the rom is to be loaded.<br>
										`int`: the index into the roms array for the rom to be loaded or saved.
			*/
			std::tuple<bool, int> GetRomIndex() final;
	};
} // namespace meen_i8080_arcade

#endif // SDLIOCONTROLLER_H
