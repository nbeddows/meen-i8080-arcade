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

#ifndef SDLIOCONTROLLER_H
#define SDLIOCONTROLLER_H

#include <atomic>
#include <ArduinoJson.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <vector>

#include "meen_hw/MH_Factory.h"
#include "i8080_arcade/MemoryController.h"

namespace i8080_arcade
{
	/** Custom SDL io controller.

		A custom io controller targetting Space Invaders i8080 arcade hardware compatible ROMs.
	*/
	class SDLIoController final : public MachEmu::IController
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

			/** SDL_Window

				The window to draw the video ram to.
			*/
			//cppcheck-suppress unusedStructMember
			SDL_Window* window_{};

			/**	i8080_arcade

				The hardware emulator.
			*/
			std::unique_ptr<meen_hw::MH_II8080ArcadeIO> i8080ArcadeIO_;

			/** i8080 arcade memory

				Holds the underlying memory and vram frame pool.
			*/
			std::shared_ptr<MemoryController> memoryController_;

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
				meen_hw::MH_ResourcePool<std::array<uint8_t, 7168>>::ResourcePtr videoFrame;
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

			/** Exit control loop.

				A value of true will cause the Machine control loop to exit.
				This can be set, for example, when the keyboard 'q' key is pressed.

				@remark		This value can be set from a different thread, hence it is atomic.
			*/
			std::atomic_bool quit_{};

			/**	Load or save

				A machine level interrupt which indicates whether or not the machine
				should attempt to load a new state or save its current state.

				MachEmu::ISR::NoInterrupt: don't load or save the state.
				MachEmu::ISR::Load: attempt to load a new machine state.
				MachEmu::ISR::Save: attempt to save the current machine state.

				@remark		This value can be set from a different thread, hence it is atomic.
			*/
			std::atomic<MachEmu::ISR> loadSaveInterrupt_{ MachEmu::ISR::NoInterrupt };

		public:
			/** Initialisation constructor

				Creates an SDL specific i8080 arcade IO controller.
			*/
			SDLIoController(const std::shared_ptr<MemoryController>& memoryController, const JsonVariant& audioHardware, const JsonVariant& videoHardware);

			/** Destructor

				Free the various required SDL objects.
			*/
			~SDLIoController();

			/** IController Read override

				Sample the keyboard so the CPU can take any required action.

				@param	port	The device to read from.

				@return	int		A bitfield indicating the action to take.
			*/
			uint8_t Read(uint16_t port) final;

			/** IController write override

				Write the relevant audio sample to the output audio device.

				@param	port	The output device to write to.
				@param	data	A bitfield indicating what data to write.
			*/
			void Write(uint16_t port, uint8_t data) final;

			/** IController::ServiceInterrupts override

				Render the video ram texture to the window via the rendering context.

				@param	currTime	The current CPU run time in nanoseconds.
				@param	cycles		The number of CPU cycles completed.
			*/
			MachEmu::ISR ServiceInterrupts(uint64_t currTime, uint64_t cycles) final;

			/**	Uuid

				Unique universal identifier for this controller.

				@return					The uuid as a 16 byte array.
			*/
			std::array<uint8_t, 16> Uuid() const final;

			/**	Main control loop

				Process all incoming events.

				Events include audio/video rendering, keyboard processing and window close.
			*/
			void EventLoop();

			/** Load Audio Samples

				Use SDL Mixer to load the audio samples.

				@param	audioFilePath	The audio samples root directory.
				@param	audioSamples	JSON object representing the audio sample files.

				@return			0 on success, -1 on failure.
			*/
			int LoadAudioSamples(const std::filesystem::path& audioFilePath, const JsonVariant& audioSamples);

			/** Load Video Textures

				Create the video texture that will be rendered to the screen.

				@param	videoTextures	JSON object describing the video texture.

				@return			0 on success, -1 on failure.
			*/
			int LoadVideoTextures(const JsonVariant& videoTextures);
	};
} // namespace i8080_arcade

#endif // SDLIOCONTROLLER_H
