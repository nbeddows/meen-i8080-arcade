/*
Copyright (c) 2021-2026 Nicolas Beddows <nicolas.beddows@gmail.com>

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

#ifndef SDL2IO_H
#define SDL2IO_H

#include <array>
#include <SDL.h>
#include <system_error>

#include "meen_i8080_arcade/IOControllerTypes.h"

namespace meen_i8080_arcade
{
	/** SDL IO Controllable.

		A controllable adhering to the IOControllable concept.

		This should be used as a template parameter to the IOController
		base template class

		`IOController<SDL2IO> ioController;`
	*/
	class SDL2IO final
	{
		private:
			/** SDL_Window

				The window to draw the video ram to.
			*/
			//cppcheck-suppress unusedStructMember
			SDL_Window* window_{};

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

			/** Audio device identifer

				This is the audio device id as returned by SDL_OpenAudioDevice.
			*/
			SDL_AudioDeviceID audioDeviceId_{};

			/** The returned audio properties

				When SDL audio is opened with a desired format, the obtained format is what SDL audio actually returns.
			*/
			SDL_AudioSpec obtainedSpec_{};

			/** The SDL keyboard state

				This is the return value of the SDL_GetKeyboardState api call.
			*/
			const uint8_t* sdlKbState_{};
		public:
			/** Default constructor
			
				Just use the default implementation.
			*/
			SDL2IO() = default;

			/** Destructor

				Free the various required SDL objects.
			*/
			~SDL2IO();

            /** One time callback registration

                This method is registered with MEEN who will invoke it on a thread determined
                by the MEEN `runAsync` configuration parameter.

				@remark		This method is a no-op for this controllable.
            */
            static void Init() {};

			/** Video Device setup
			
				Configure the SDL video subsystem in order to render video frames.

				@param	width		The width of the display window.
				@param	height		The height of the display window.
				@param	fullscreen	True to run the window in fullscreen, false otherwise.

				@return				A std::errc indicating success or failure.
			*/
			std::errc ConfigureVideoDevice(int width, int height, int fullscreen);

			/** Audio device setup
			
				Configure the SDL audio subsystem in order to render audio frames.

				@param	sampleRate	The output audio device number of samples per second.
				@param	channels	The output audio device number of channels.
				@param	sampleSize	The number of output samples to process.

				@return				A std::errc indicating success or failure.
			*/
			std::errc ConfigureAudioDevice(int sampleRate, int channels, int sampleSize);

			/** Peripheral device setup
			
				Configure the SDL events subsystem in order to process user input.

				@return                A std::errc indicating success or failure.
			*/
			std::errc ConfigurePeripheralDevice();

			/**	Uuid

				Unique universal identifier for this controller.

				@return                The uuid as a 16 byte array.
			*/
			std::array<uint8_t, 16> Uuid() const;

			/** Queue the next audio sample
			
				Use SDL_QueueAudio API to deliver the next audio frame to the speaker.

				@param    audioFrame        The next audio frame to render. The format of the
				                            output audio frame will always be 8 bit stereo.
				@param    audioFrameSize    The length of the audio frame in bytes.
				@param    timstamp          Not used.

				@return                     A std::errc indicating success or failure.
			*/
			std::errc RenderAudioFrame(const int32_t* audioFrame, int audioFrameSize, uint64_t timestamp);
            
			/** Start the video frame rendering process.

				Lock the SDL_Texture buffer and return the raw buffer for blitting.

				@param    dst            A pointer to the raw texture buffer that was configured
									     in `ConfigureVideoDevice`.
				@param    rowBytes       The number of bytes in each scanline of the raw texture
									     buffer pointed to by dst.
				@param    scanlineStart  The row number in the texture buffer to set the dst pointer to.
				@param    numScanlines   The number of scanlines that the dst pointer refers to.

				@return                  A std::errc indicating success or failure.

				@remark                  Must be paired with a call to EndVideoFrame
			*/
			std::errc GetVideoFrameBuffer(uint8_t** dst, int* dstRowBytes, int scanlineStart, int numScanlines) const;

			/** Render the next video frame.
			
			    Unlock the SDL texture and present the texture for display to the screen.

				@param    scanlineStart       The row to start renderering from.
				@param    numScanlines        The number of scanlnes to render.
				@param    timestamp           Not used.

				@return                       A std::errc indicating success or failure.

				@remark              Must be paired with a call to GetVideoFrameBuffer
			*/
			std::errc RenderVideoFrame(int scanlineStart, int numScanlines, uint64_t timestamp);

			/** End the video frame rendering process.

                Indicate to SDL2 that this frame has finished drawing and can be presented to the display.

				@param    timestamp           Not used.
			*/
			std::errc DisplayVideoFrame(uint64_t timestamp);

			/** Print an error message
			
				Print the error message to the console.

				@param    error    The error message string

				@return            A std::errc indicating success or failure.
			*/
			std::errc RenderErrorString(const std::string& error);

			/** Clear the display
			
			    Clear the screen within a bounding box to black.

				@param    rect    The section of the screen to clear.

				@return            Always returns std::errc{}.
			*/
			std::errc ClearDisplay(BoundingBox&& rect);
			
			/** Read user input
			
			    Scan the keyboard for user input. Each scan code is mapped
				to an Input enum.
				
				@return    A uint32_t mask of Input values. At most 32 keys can be supported.

				@sa        IOControllerTypes.h
			*/
			uint32_t ReadPeripheralDevice();
			
			/** Perform required tasks when the screen is updated.
			
			    The main action performed is pausing/unpausing the audio device
				as well as cleaning and queued audio.

				@param    curr    The screen that we are on and are about to leave.
				@param    next    The screen that we are moving to.
			*/
			std::errc ScreenTransition(Screen curr, Screen next);

			/** Perform any actions once the audio samples are loaded.
			
			    Currently, this method is unsed.

				@param    sampleRate    Not used.
				@param    channels      Not used.
				@param    sampleSize    Not used.

				@return                 A std::errc indicating success or failure.
			*/
			std::errc LoadAudioSamples(int sampleRate, int channels, int sampleSize);
			
			/** Create the SDL Texture for rendering

			    Sets the required texure properties.

				@param    bpp            The bit depth of the texture to create. Only 8 and 16 bit is supported.
				@param    textureWidth   The width of the texture to create.
				@param    textureHeight  The height of the texture to create.
				@param    numScanlines   The number of scanlines that should be rendered in one pass.
				                         For SDL2 this should be set to the texture height (full frame rendering).
										 Setting it to any other value may cause issues with rendering performance.
			
				@return                  A std::errc indicating success or failure.
			*/
			std::errc LoadVideoTextures(int bpp, int textureWidth, int textureHeight, int* numScanlines);
	};
} // namespace meen_i8080_arcade

#endif // SDL2IO_H
