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

#ifndef QT6IO_H
#define QT6IO_H

#include <array>
#include <system_error>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "meen_i8080_arcade/IOControllerTypes.h"
#include "meen_i8080_arcade/io_controllables/QT6IODisplay.h"

namespace meen_i8080_arcade
{
    /** QT 6 IO Controllable.

        An io controllable based on the qt 6 framework.
	*/
    class QT6IO final : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowWidthChanged)
        Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowHeightChanged)

    public:
        Q_INVOKABLE void KeyPressed(Qt::Key key);
        Q_INVOKABLE void KeyReleased(Qt::Key key);

    signals:
        void windowWidthChanged();
        void windowHeightChanged();

    private:
        std::unique_ptr<QGuiApplication> app_;
        std::unique_ptr<QQmlApplicationEngine> engine_;
        uint32_t input_{};
        QT6IODisplay* QT6IODisplay_{};

        int windowWidth_{};
        int windowHeight_{};

        int windowWidth() const;
        int windowHeight() const;

    public:
        /** Default constructor

            Initialise the qt gui application
        */
        QT6IO() = default;

        /** Destructor

            Free the various QT6IO controller objects.
        */
        ~QT6IO() = default;

        /** One time callback registration

            This method is registered with MEEN who will invoke it on a thread determined
            by the MEEN `runAsync` configuration parameter.

            @remark		This method sets the QT style to Fusion.
        */
        static void Init();

        /** Video Device setup
			
            Configure QT6IO video subsystem in order to render video frames.

            @param    width      The width of the display window.
            @param    height     The height of the display window.
            @param    fullscreen True to run the window in fullscreen, false otherwise.

            @return              A std::errc indicating success or failure.
        */
        std::errc ConfigureVideoDevice(int width, int height, int fullscreen);

        /** Audio device setup
			
            Configure QT6IO audio subsystem in order to render audio frames.

            @param    sampleRate    The output audio device number of samples per second.
            @param    channels      The output audio device number of channels.
            @param    sampleSize    The number of output samples to process.

            @return	  A std::errc indicating success or failure.
        */
        std::errc ConfigureAudioDevice(int sampleRate, int channels, int sampleSize);

        /** Peripheral device setup

            Configure the QT6IO events subsystem in order to process user input.

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
        std::errc GetVideoFrameBuffer(const uint8_t** dst, int* dstRowBytes, int scanlineStart, int numScanlines) const;
        
        /** Render the next video frame.

            Present the texture for display to the screen.

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

        /** Read from QT6IO device

            Read user input from QT6IO device.

            @return    A 32 bit mask of Input values.

            @sa        IOControllerTypes.h
        */
        uint32_t ReadPeripheralDevice();

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
    };
} // namespace meen_i8080_arcade

#endif // QT6IO_H
