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

#ifndef PICOIO_H
#define PICOIO_H

#include <ArduinoJson.h>
#include <atomic>
#include <list>
#include <variant>
#include <vector>

namespace meen_i8080_arcade
{
    /** Custom Raspberry Pi Pico io controllable.

        A custom controllable targetting Space Invaders i8080 arcade hardware compatible ROMs.

        For video output it requires an st7789vw driver compatible display for video output over spi
        and for audio output it requires a PCM5101A audio decoder to output audio over I2S.
    */
    class PicoIO final
    {
    private:
        // TODO: these pins need to change depending on the device pin configuration,
        //       may be best to pass them in via the config file.
        enum Pin
        {
            DIN = 11,  //< Video data input
            CLK = 10,
            CS = 9,
            DC = 8,
            RST = 12,
            BL = 13,
            K0 = 15,   //< Button 0
            K1 = 17,   //< Button 1
            K2 = 2,    //< Button 2
            K3 = 3,    //< Button 3
            ADIN = 26, //< Audio data input
            BCK = 27,  //< Audio data bit clock input
            LRCK = 28, //< Audio data word clock input
            MAX = 29
        };

        /** The current active button

            True if the button (pin) is pressed, false otherwise

            @remark    Only the pins K0, K1, K2, K3 are tracked (the remaining entries are unused).
        */
        static bool buttonPress_[Pin::MAX];

        /** The previous edge fall state

            Track the previous edge fall state to prevent spurious edge falls (when an edge fall is
            detected but the previous edge fall for that pin is set, then this is a spurious edge fall).

            @remark    Only the pins K0, K1, K2, K3 are tracked (the remaining entries are unused).
        */
        static bool prevEdgeFall_[Pin::MAX];

        /** The previous edge rise state

            Track the previous edge rise state to prevent spurious edge rises (when an edge rise is
            detected but the previous edge rise for that pin is set, then this is a spurious edge rise).

            @remark    Only the pins K0, K1, K2, K3 are tracked (the remaining entries are unused).
        */
        static bool prevEdgeRise_[Pin::MAX];

        int width_{};

        int height_{};

        int textureWidth_{};

        int textureHeight_{};

        /** The current screen

            See the Screen enumeration for further details.

            Made atomic as it can be called from a different thread if hte runAsync parameter is set to the true
        */
        std::atomic<Screen> screen_{};

        /** Centre width offset

            The difference in pixels of the width of the attached lcd panel and the width
            of the i8080 arcade video hardware.

            This is used to centre the output frame on the lcd panel.
        */
        int widthOffset_{};

        /** Centre height offset

            The difference in pixels of the height of the attached lcd panel and the height
            of the i8080 arcade video hardware.

            This is used to centre the output frame on the lcd panel.
        */
        int heightOffset_{};

        /** Video frame buffer

            The pixels that will be rendered to the display.
        */
        std::vector<uint8_t> texture_;

        /** The number of remaining ships

            This counter is used to track when to move from gameplay mode to
            attraction screen mode and vice versa.
            When it is greater than 0, we are in gameplay mode and buttons 0
            and 3 will be used to move the ship left and right. When it is
            0 we are in attraction screen mode and these buttons will be used
            to select which rom to load.
        */
        int ships_{};

        /** LCD command

            Write a command to the LCD driver.
        */
        static void WriteCmd(uint8_t cmd);

        /** LCD params

            Write command parameters to the LCD driver.

            @param    param    The next parameter in the parameter sequence defined
                               by the previous call to WriteCmd.
        */
        static void WriteParam(uint8_t param);

        /** Ram write region

            Define a region in display ram where pixels can be written to.

            @param    startX    The starting x coordinate of the blit region.
            @param    startY    The starting y coordinate of the blit region.
            @param    endX      The ending x coordinate of the blit region.
            @param    endX      The ending x coordinate of the blit region.
        */
        static void SetRegion(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend);

    public:
        /** Initialisation constructor

            Creates an RP2040 io controllable specific i8080 arcade IO controller.
        */
        PicoIO();

        /** Destructor

            Free the various required RP2040 objects.
        */
        ~PicoIO();

        /** One time callback registration for gpio handling

            This method is to be registered with MEEN who will invoke it on a thread determined
            by the MEEN `runAsync` configuration parameter.
        */
        static void Init();

        /** Video Device setup
        
            Configure the Pico video subsystem in order to render video frames.

            @param	width		The width of the display window.
            @param	height		The height of the display window.
            @param	fullscreen	True to run the window in fullscreen, false otherwise.

            @return				A std::errc indicating success or failure.

            @remark             The fullscreen parameter is not supported.
        */
        std::errc ConfigureVideoDevice(int width, int height, int fullscreen);

        /** Audio device setup
        
            Configure the Pico audio subsystem in order to render audio frames.

            @param	sampleRate	The output audio device number of samples per second.
            @param	channels	The output audio device number of channels.
            @param	sampleSize	The number of output samples to process.

            @return				A std::errc indicating success or failure.
        */
        std::errc ConfigureAudioDevice(int sampleRate, int channels, int sampleSize);

        /** Peripheral device setup
        
            Configure the Pico events subsystem in order to process user input.

            @return            A std::errc indicating success or failure.
        */
        std::errc ConfigurePeripheralDevice();

        /**	Uuid

            Unique universal identifier for this controller.

            @return                The uuid as a 16 byte array.
        */
        std::array<uint8_t, 16> Uuid() const;

        /** Queue the next audio sample
        
            Use the Pico DMAC API to deliver the next audio frame to the speaker.

            @param    audioFrame        The next audio frame to render. The format of the
                                        output audio frame will always be 8 bit stereo.
            @param    audioFrameSize    The length of the audio frame in bytes.
            @param    timstamp          Not used.

            @return                     A std::errc indicating success or failure.
        */
        std::errc RenderAudioFrame(const int32_t* audioFrame, int audioFrameSize, uint64_t timestamp);
        
        /** Render the next video frame.
        
            Write the texture scanline by scanline to the display.

            @param    videoFrame        The next video frame to blit.
            @param    videoFrameSize    The length of the video frame in bytes.
            @param    timestamp         Not used.

            @return                     A std::errc indicating success or failure.
        */
        std::errc RenderVideoFrame(const uint8_t* videoFrame, int videoFrameSize, uint64_t timestamp);

        /** Print an error message
        
            Print the error message to the console.

            @param    error    The error message string

            @return            A std::errc indicating success or failure.

            @remark            Error messages can be viewed via a minicom.
        */
        std::errc RenderErrorString(const std::string& error);
        
        /** Clear the display
        
            Clean the display to black scanline at a time.

            @param    clearDisplay    Not used.

            @return                   Always returns std::errc{}.
        */
        std::errc ClearDisplay(bool clearDisplay);
        
        /** Read user input
        
            Scan the GPIO buttons for user input. Each button is mapped
            to an Input enum.
            
            @return    A uint32_t mask of Input values. At most 32 buttons can be supported.

            @sa        IOControllerTypes.h
        */
        uint32_t ReadPeripheralDevice();

        /** Video display buffer
        
            Return the raw buffer for blitting.

            @param    dst        A pointer to the raw texture buffer that was configured
                                 in `ConfigureVideoDevice`.
            @param    rowBytes   The number of bytes in each scanline of the raw texture
                                 buffer pointed to by dst.

            @return              A std::errc indicating success or failure.
        */
        std::errc GetTextureBuffer(uint8_t** dst, int* dstRowBytes) const;
        
        /** Perform required tasks when the screen is updated.
        
            The main action performed is pausing/unpausing the audio device
            as well as cleaning and queued audio.

            @param    curr    The screen that we are on and are about to leave.
            @param    next    The screen that we are moving to.
        */
        void ScreenTransition(Screen curr, Screen next);

        /** Perform any actions once the audio samples are loaded
        
            Currently, this method is unsed.

            @param    sampleRate    Not used.
            @param    channels      Not used.
            @param    sampleSize    Not used.

            @return                 A std::errc indicating success or failure.
        */
        std::errc LoadAudioSamples(int sampleRate, int channels, int sampleSize);
        
        /** Create the raw scanline buffer

            Sets the required texure properties.

            @param    bpp            The bit depth of the texture to create. Only 8 and 16 bit is supported.
            @param    textureWidth   The width of the texture to create.
            @param    textureHeight  The height of the texture to create.
        
            @return                  A std::errc indicating success or failure.
        */
        std::errc LoadVideoTextures(int bpp, int textureWidth, int textureHeight);
    };
} // namespace meen_i8080_arcade
#endif // RPIOCONTROLLER_H
