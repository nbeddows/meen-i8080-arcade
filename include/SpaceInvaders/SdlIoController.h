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

#ifndef SDL_IO_CONTROLLER_H
#define SDL_IO_CONTROLLER_H

#include "SDL.h"
#include "SDL_mixer.h"
#include "SpaceInvaders/IoController.h"

namespace SpaceInvaders
{
	/** Custom SDL io controller.

		A custom io controller targetting the Space Invaders arcade ROM.
	*/
	class SdlIoController final : public IoController
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
			
			/** Audio samples
			
				The various audio samples to be played.

				@See IoController::WavFiles_
			*/
			//cppcheck-suppress unusedStructMember
			std::vector<Mix_Chunk*> mixChunk_;

			/** The custom Space Invaders SDL event type

				Event codes are defined in the EventCode enumeration.

				@see EventCode
			*/
			uint64_t siEvent_{};

			/** SDL Event codes
			
				Individual event codes that can be set on an SDL_Event of type 'Space Invaders Event'.

				@see siEvent_
			*/
			enum EventCode
			{
				RenderVideo,	/**< The next video frame is ready to be rendered. This event drives the control loop */
				RenderAudio,	/**< Audio is ready to be played. The siEvent data1 type is the index into the mixChunk_ to be played. */
				ReadInput		/**< Check if there is any input from the user. The siEvent data1 type is the type of input to be checked. */
			};

		public:
			/** Initialisation constructor
			
				Creates an SDL specific Space Invaders IO controller.
			*/
			SdlIoController(const std::shared_ptr<MemoryController>& memoryController, const nlohmann::json& audioHardware, const nlohmann::json& videoHardware);
			
			/** Destructor
			
				Free the various required SDL objects.
			*/
			~SdlIoController();

			/** IController Read override
			
				Sample the keyboard so the CPU can take any required action.
			
				@param	port	The device to read from.

				@return	int		A bitfield indicating the action to take.

				@see IoController::ReadFrom.
			*/
			uint8_t Read(uint16_t port) final;

			/** IController write override
			
				Write the relevant audio sample to the output audio device.
			
				@param	port	The output device to write to.
				@param	data	A bitfield indicating what data to write.

				@see IoController::WriteTo.
			*/
			void Write(uint16_t port, uint8_t data) final;

			/** IController::ServiceInterrupts override
			
				Render the video ram texture to the window via the rendering context.

				@param	currTime	The current CPU run time in nanoseconds.
				@param	cycles		The number of CPU cycles completed.

				@see IoController::ServiceInterrupts.
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

			void LoadAudioSamples(const std::filesystem::path& audioFilePath, const nlohmann::json& audioSamples);
			void LoadVideoTextures(const nlohmann::json& videoTextures);
	};
} // namespace SpaceInvaders

#endif // SDL_IO_CONTROLLER_H