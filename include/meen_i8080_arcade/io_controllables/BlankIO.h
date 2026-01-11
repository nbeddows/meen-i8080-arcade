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

#ifndef BLANKIO_H
#define BLANKIO_H

#include <system_error>
/*
    Add additional includes here.
*/

#include "meen_i8080_arcade/IOControllerTypes.h"

namespace meen_i8080_arcade
{
    /** An empty IO Controllable.

        An controllable which can be used as a template for creating
        a new IOControllable.

        @remark    Replace the class name BlankIO with your IOControllable
                   class name.
	*/
    class BlankIO final
    {
    private:
        /*
        
        Place your private variables/types/methods here.

        */
    public:
        /** Default constructor

            Default implementation.
        */
        BlankIO() = default;

        /** Destructor

            Free the various BlankIO controller objects.
        */
        ~BlankIO() = default;

        /** Video Device setup
			
            Configure the BlankIO video subsystem in order to render video frames.

            @param    width      The width of the display window.
            @param    height     The height of the display window.
            @param    fullscreen True to run the window in fullscreen, false otherwise.

            @return              A std::errc indicating success or failure.
        */
        std::errc ConfigureVideoDevice(int width, int height, int fullscreen);

        /** Audio device setup
			
            Configure the BlankIO audio subsystem in order to render audio frames.

            @param    sampleRate    The output audio device number of samples per second.
            @param    channels      The output audio device number of channels.
            @param    sampleSize    The number of output samples to process.

            @return	  A std::errc indicating success or failure.
        */
        std::errc ConfigureAudioDevice(int sampleRate, int channels, int sampleSize);

        /** Peripheral device setup

            Configure the BlankIO events subsystem in order to process user input.

            @return    A std::errc indicating success or failure.
        */
        std::errc ConfigurePeripheralDevice();

        /** Uuid

            Unique universal identifier for this controller.

            @return    The uuid as a 16 byte array.
        */
        std::array<uint8_t, 16> Uuid() const;

        /** Render audio frame

            Custom audio frame rendering

            @param    audioFrame    The next audio frame to render
            @param    timestamp     The timestamp at which to render the audio frame
                                    in MEEN timescale units.

            @return    A std::errc indicating success or failure.
        */
        std::errc RenderAudioFrame(const int32_t* audioFrame, uint64_t timestamp);

        /** Render video frame

            Custom video frame rendering.

            @param    videoFrame    The next video frame to render.
            @param    timestamp     The timestamp at which to render the video frame
                                    in MEEN timescale units.

            @return    A std::errc indicating success or failure.
        */
        std::errc RenderVideoFrame(const uint8_t* videoFrame, uint64_t timestmap);

        /** Render error string

            Custom error string blitting.

            @param    error    The error string to render.

            @return    A std::errc indicating success or failure.
        */
        std::errc RenderErrorString(const std::string& error);

        /** Clear the display.

            Clear the BlankIO display

            @param    clearDisplay    true to clear, false otherwise.

            @return    A std::errc indicating success or failure.
        */
        std::errc ClearDisplay(bool clearDisplay);

        /** Read from BlankIO device

            Read user input from BlankIO device.

            @return    A 32 bit mask of Input values.

            @sa        IOControllerTypes.h
        */
        uint32_t ReadPeripheralDevice();

        /** Get custom texture buffer

            Get the rendering buffer from BlankIO.

            @param    dst        The destination address that will hold the video memory.
            @param    rowBytes   The number of bytes in a scanline of the video memory pointed
                                 to by dst.

            @return    A std::errc indicating success or failure.
        */
        std::errc GetTextureBuffer(uint8_t** dst, int* dstRowBytes) const;

        /** Screen Transition

            This method performs any required actions between screen transitions.

            @param    curr    The current screen.
            @param    next    The screen that is being transitioned to.
        */
        void ScreenTransition(Screen curr, Screen next);

        /** Custom audio sample load

            Load custom audio samples via BlankIO.

            @param    sampleRate    The input sample rate of the audio samples.
            @param    channels      The input audio sample channel count.
            @param    sampleSize    The input sample size of the audio samples.

            @return    A std::errc indicating success or failure.
        */
        std::errc LoadAudioSamples(int sampleRate, int channels, int sampleSize);

        /** Custom video texture load

            Load custom video textures via BlankIO.

            @param    bpp             The bits per pixel of the texture to create.
            @param    textureWidth    The width of the texture to create.
            @param    textureHeight   The height of the texture to create.

            @return    A std::errc indicating success or failure.
        */
        std::errc LoadVideoTextures(int bpp, int textureWidth, int textureHeight);

        /*
        
        Place your public variables/types/methods here.

        */

    };
} // namespace meen_i8080_arcade

#endif // BLANKIO_H
