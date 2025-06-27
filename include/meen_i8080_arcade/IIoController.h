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

#ifndef IIOCONTROLLER_H
#define IIOCONTROLLER_H

#include <ArduinoJson.h>
#include <system_error>
#include <tuple>

#include "meen/IController.h"

namespace meen_i8080_arcade
{
    /** An abstract base class describing a generic io controller
	
		Ths is the foundation for all i8080 arcade io controllers. Along with implementing
		the IController base class methods, this extension allows for the ability to allocate
		space for audio and video buffers.
	*/
	struct IIoController : public meen::IController
    {
		/** Game play screens

			The phases of the i8080 arcade emulator.

			@remark			Addtional screens to add could include Highscore for example.
			@remark			The i8080 arcade emulator starts on the RomSelect screen, its position should not be changed.
		*/
		enum Screen
		{
			RomSelect,		/**< The rom select screen where the user can select a rom to load. */
			Gameplay		/**< The emulated game play for the selected rom that was loaded in the rom select screen. */
		};

        /**	Event handler

			Process all incoming events.

			Events include audio/video rendering, keyboard processing and window close.

	        @return                 True to quit MEEN, false otherwise.
		*/
		virtual bool HandleEvent() = 0;

		/** Error handler

			Process any generated errors.

			These errors may come from MEEN or meen-i8080-arcade itself.

			@param	errorMsg		A `std::string` containing the error message.
		*/
		virtual void HandleError(std::string&& errorMsg) = 0;

		/** Load complete handler
		
			Handle the transition into game play when a rom has been sucessfully loaded.
		*/
		virtual void HandleLoadComplete() = 0;

		/** Load Audio Samples

            Loads the audio samples from the configuration file.

			@param	audioSamples	JSON object representing the audio sample files.

			@return					An `std::error_code` determined by the implementation.
		*/
		virtual std::error_code LoadAudioSamples(const JsonVariantConst audioSamples) = 0;

		/** Load Video Textures

			Create the video texture that will be rendered to the screen.

			@param	videoTextures	JSON object describing the video texture.
			@param	frameWidth		The width in pixels of the memory controller video frame.
			@param	frameHeight		The height in pixels of the memory controller video frame.

			@return					A `std::error_code` determined by the implemntation.
		*/
		virtual std::error_code LoadVideoTextures(const JsonVariantConst videoTextures, int frameWidth, int frameHeight) = 0;

		/** Get the rom index

			Load the selected rom or the save state of the currently selected rom.

			@return					A tuple holding two values:<br><br>
									`bool`: only valid when loading roms, true if the save file is to be loaded, false if the rom is to be loaded.<br>
									`int`: the index into the roms array for the rom to be loaded or saved.
		*/
		virtual std::tuple<bool, int> GetRomIndex() = 0;

        /** Virtual destructor

            Free any used resources
        */
        virtual ~IIoController() = default;
    };
} // namespace meen_i8080_arcade

#endif // IIOCONTROLLER_H