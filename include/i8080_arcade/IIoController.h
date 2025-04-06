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

namespace i8080_arcade
{
    struct IIoController : public meen::IController
    {
        /**	Event handler

			Process all incoming events.

			Events include audio/video rendering, keyboard processing and window close.

	        @return                 True to quit the machine, false otherwise.
		*/
		virtual bool HandleEvent() = 0;

		/** Error handler

			Process any generated errors

			These errors may come from meen or i8080-arcade itself.

			@param	errorMsg		The error message.
		*/
		virtual void HandleError(std::string&& errorMsg) = 0;

		/** Load Audio Samples

            Loads the audio samples from the configuration file.

			@param	audioSamples	JSON object representing the audio sample files.

			@return					An error in the form of a std::error_code.
		*/
		virtual std::error_code LoadAudioSamples(const JsonVariantConst audioSamples) = 0;

		/** Load Video Textures

			Create the video texture that will be rendered to the screen.

			@param	videoTextures	JSON object describing the video texture.

			@return					An error in the form of a std::error_code.
		*/
		virtual std::error_code LoadVideoTextures(const JsonVariantConst videoTextures) = 0;

		/** Load the selected rom or the save state of the currently selected rom
			
			@param	maxSize			The total number of roms in the rom list	

			@return					A tuple holding two values:
									bool - only valid when loading roms, true if the save file is to be loaded, false if the rom is to be loaded.
									int - the index into the roms array for the rom to be loaded or saved
		*/
		virtual std::tuple<bool, int> GetRomIndex(int maxSize) = 0;

        /** Free any use resources
            
        */
        virtual ~IIoController() = default;
    };
} // namespace i8080_arcade

#endif // IIOCONTROLLER_H