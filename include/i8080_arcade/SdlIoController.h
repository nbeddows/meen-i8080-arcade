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
#include <SDL.h>
#include <SDL_mixer.h>
#include <vector>

#include "i8080_arcade/IIoController.h"
#include "meen_hw/MH_Factory.h"
#include "meen_hw/MH_ResourcePool.h"

namespace i8080_arcade
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

			/**	i8080_arcade

				The hardware emulator.
			*/
			std::unique_ptr<meen_hw::MH_II8080ArcadeIO> i8080ArcadeIO_;

			/** Audio samples

				The various audio samples to be played.
			*/
			//cppcheck-suppress unusedStructMember
			std::vector<Mix_Chunk*> mixChunk_;

			/** The custom i8080 arcade SDL event type

				Event codes are defined in the EventCode enumeration.

				@see EventCode
			*/
			uint64_t siEvent_{};

			/** SDL Event codes

				Individual event codes that can be set on an SDL_Event of type 'i8080 arcade Event'.

				@see siEvent_
			*/
			enum EventCode
			{
				RenderVideo,	/**< The next video frame is ready to be rendered. This event drives the control loop */
				RenderAudio,	/**< Audio is ready to be played. The siEvent data1 type is the index into the mixChunk_ to be played. */
				ReadInput		/**< Check if there is any input from the user. The siEvent data1 type is the type of input to be checked. */
			};

			/** VideoFrameWrapper

			 	A convenience wrapper used to pass unique pointers through SDL's event structure.
			*/
			struct VideoFrameWrapper
			{
				meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr videoFrame;
			};

			/** videoFrameWrapperPool_

				An array of frame wrappers used to pass video frames from the machine thread
				to the main thread.
			*/
			std::vector<std::unique_ptr<VideoFrameWrapper>> videoFrameWrapperPool_;

			/** videoFrameWrapperMutex_

				Video frame mutual exclusion between the main thread and the machine thread.
			*/
			std::mutex videoFrameWrapperMutex_;

			/** Prepare for shut down

				A value of true will skip any future waits and return immediatley to the engine
				(only used in the Read method).

				@remark		This value can be set from a different thread, hence it is atomic.
			*/
			std::atomic_bool quit_{};

			/** Load a game rom or the save state of the currently loaded game rom
			
				@remark		This value can be set from a different thread, hence it is atomic.
			*/
			std::atomic_bool loadSaveState_{};

			/**	Load or save

				A machine level interrupt which indicates whether or not the machine
				should attempt to load a new state or save its current state.

				meen::ISR::NoInterrupt: don't load or save the state.
				meen::ISR::Load: attempt to load a new machine state.
				meen::ISR::Save: attempt to save the current machine state.

				@remark		This value can be set from a different thread, hence it is atomic.
			*/
			std::atomic<meen::ISR> loadSaveInterrupt_{ meen::ISR::NoInterrupt };

			/** Keep track of previous key presses to prevent repeat events from triggering

				Key 'r' restores the currently loaded rom save state (if it exists)
				Key 'u' loads the currently selected rom
				Key 'y' save the currently selected roms state
			*/
			Uint8 lastR_{};
			Uint8 lastU_{};
			Uint8 lastY_{};

			/** The running state
			
				True if meen is to run on a different thread to the main application,
				false otherwise.
			*/
			bool runAsync_{};

			/** The current screen
			
				See the Screen enumeration for further details.
			*/
			Screen screen_{};

			/** Read input form the keyboard

				@param	port	The emulated port to read from.
				@param	state	The keyboard state.
				
				@return			A uint8_t bitwise combination informing the rom
								of the user input.
			*/
			uint8_t ReadInputDevice(uint8_t port, const uint8_t* state);

			/** Assign a load or save machine interrupt
			
				Peforms a check of the key once during a key press and release sequence.

				@param	key				The press or release state of the key.
				@param	lastKey			The previous press or release state of the key.
				@param	isr				The interrupt to assign.
				@param	loadSaveState	True if the current save state is to be loaded,
										false otherwise.

				@return					The current key state (the key parameter).
			*/
			Uint8 SetInterrupt(Uint8 key, Uint8 lastKey, meen::ISR isr, bool loadSaveState);

		public:
			/** Initialisation constructor

				Creates an SDL specific i8080 arcade IO controller.

				@param		runAsync		Run this io controller asynchronously.
				@param		audioHardware	audio hardware configuration options.
				@param		videoHardware	video hardware configuration options.
			*/
			SDLIoController(bool runAsync, const JsonVariantConst audioHardware, const JsonVariantConst videoHardware);

			/** Destructor

				Free the various required SDL objects.
			*/
			~SDLIoController();

			/** IController Read override

				Sample the keyboard so the CPU can take any required action.

				@param	port	The device to read from.

				@return			A bitfield indicating the action to take.
			*/
			uint8_t Read(uint16_t port, meen::IController* controller) final;

			/** IController write override

				Write the relevant audio sample to the output audio device.

				@param	port	The output device to write to.
				@param	data	A bitfield indicating what data to write.
			*/
			void Write(uint16_t port, uint8_t data, meen::IController* controller) final;

			/** IController::ServiceInterrupts override

				Render the video ram texture to the window via the rendering context.

				@param	currTime	The current CPU run time in nanoseconds.
				@param	cycles		The number of CPU cycles completed.
			*/
			meen::ISR ServiceInterrupts(uint64_t currTime, uint64_t cycles, meen::IController* controller) final;

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

        		These errors may come from meen or i8080-arcade itself.

				@param	errorMsg		The error message.
			*/
			void HandleError(std::string&& errorMsg) final;

			/** Load Audio Samples

				Use SDL Mixer to load the audio samples.

				@param	audioSamples	JSON object representing the audio sample files.

				@return					An error in the form of a std::error_code.
			*/
			std::error_code LoadAudioSamples(const JsonVariantConst audioSamples) final;

			/** Load Video Textures

				Create the video texture that will be rendered to the screen.

				@param	videoTextures	JSON object describing the video texture.

				@return					An error in the form of a std::error_code.
			*/
			std::error_code LoadVideoTextures(const JsonVariantConst videoTextures, int frameWidth, int frameHeight) final;

			/** Load the selected rom or the save state of the currently selected rom
			
				@param	maxSize			The total number of roms in the rom list	

				@return					A tuple holding two values:
										bool - only valid when loading roms, true if the save file is to be loaded, false if the rom is to be loaded.
										int - the index into the roms array for the rom to be loaded or saved
			*/
			std::tuple<bool, int> GetRomIndex(int maxSize) final;
	};
} // namespace i8080_arcade

#endif // SDLIOCONTROLLER_H
