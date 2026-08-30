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

#include <array>
#include <system_error>
/*
    Add additional includes here.
*/

#include "meen_i8080_arcade/io_controllables/IOControllerTypes.h"

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

        /** One time callback registration

            This method is registered with MEEN who will invoke it on a thread determined
            by the MEEN `runAsync` configuration parameter.

            @remark		This method is a no-op for this controllable.
        */
        static void Init() {};

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

            @param    audioFrame        The next audio frame to render.
            @param    audioFrameSize    The length of the audio frame in bytes.
            @param    timestamp         The timestamp at which to render the audio frame
                                        in MEEN timescale units.

            @return    A std::errc indicating success or failure.
        */
        std::errc RenderAudioFrame(const int32_t* audioFrame, int audioFrameSize, uint64_t timestamp);

        /** Start the video frame rendering process.

            Sets the frame buffer to render to.

            @param    dst                A pointer to the raw texture buffer that was configured
                                         in `ConfigureVideoDevice`.
            @param    rowBytes           The number of bytes in each scanline of the raw texture
                                         buffer pointed to by dst.
            @param    scanlineStart      The row number in the texture buffer to set the dst pointer to.
            @param    numScanlines       The number of scanlines that the dst pointer refers to.

            @return                      A std::errc indicating success or failure.

            @remark                      Must be paired with a call to EndVideoFrame
        */
        std::errc GetVideoFrameBuffer(uint8_t** dst, int* dstRowBytes, int scanlineStart, int numScanlines) const;

        /** Render the next video frame.

            Indicate that this frame has finished drawing.

            @param    scanlineStart       The row to start renderering from.
            @param    numScanlines        The number of scanlnes to render.
            @param    timestamp           Not used.

            @return                       A std::errc indicating success or failure.

            @remark                       Must be paired with a call to GetVideoFrameBuffer
        */
        std::errc RenderVideoFrame(int scanlineStart, int numScanlines, uint64_t timestamp);

        /** End the video frame rendering process.

            Indicate that this fram can be presented to the display.

            @param    timestamp           Not used.
        */
        std::errc DisplayVideoFrame(uint64_t timestamp);

        /** Render error string

            Custom error string blitting.

            @param    error    The error string to render.

            @return    A std::errc indicating success or failure.
        */
        std::errc RenderErrorString(const std::string& error);

        /** Clear the display

            Clear the screen within the bounding box to black.

            @param    rect     The section of the screen to clear.

            @return            Always returns std::errc{}.
        */
        std::errc ClearDisplay(BoundingBox&& rect);

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

            @return           A std::errc indicating success or failure.
        */
        std::errc ScreenTransition(Screen curr, Screen next);

        /** Custom audio sample load

            Load custom audio samples via BlankIO.

            @param    sampleRate    The input sample rate of the audio samples.
            @param    channels      The input audio sample channel count.
            @param    sampleSize    The input sample size of the audio samples.

            @return    A std::errc indicating success or failure.
        */
        std::errc LoadAudioSamples(int sampleRate, int channels, int sampleSize);

        /** Custom video texture load

            Sets the required texure properties.

            @param    bpp            The bit depth of the texture to create.
            @param    textureWidth   The width of the texture to create.
            @param    textureHeight  The height of the texture to create.
            @param    numScanlines   The number of scanlines that should be rendered in one pass.

            @return                  A std::errc indicating success or failure.
        */
        std::errc LoadVideoTextures(int bpp, int textureWidth, int textureHeight, int* numScanlines);

        /*
        
        Place your public variables/types/methods here.

        */

    };
} // namespace meen_i8080_arcade

#endif // BLANKIO_H
