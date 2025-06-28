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

#ifndef RPIOCONTROLLER_H
#define RPIOCONTROLLER_H

#include <ArduinoJson.h>
#include <atomic>
#include <pico/util/queue.h>
#include <variant>
#include <vector>

#include "meen_i8080_arcade/IIoController.h"
#include "meen_hw/MH_Factory.h"

namespace meen_i8080_arcade
{
    /** Custom Raspberry Pi Pico io controller.

        A custom io controller targetting Space Invaders i8080 arcade hardware compatible ROMs.

        For video output it requires an st7789vw driver compatible display for video output over spi
        and for audio output it requires a PCM5101A audio decoder to output audio over I2S.
    */
    class RPIoController final : public IIoController
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

        /** Output device width

            Width in pixels.

            @remark This is determined by the hardware:video:width parameter
                    in the config file.
        */
        int width_{};

        /** Output device height

            Height in pixels.

            @remark This is determined by the hardware:video:height parameter
                    in the config file.
        */
        int height_{};

        /** Centre width offset

            The difference in pixels of the width of the attached lcd panel and the width
            of the i8080 arcahde video hardware.

            This is used to centre the output frame on the lcd panel.
        */
        int widthOffset_{};

        /** Centre height offset

            The difference in pixels of the height of the attached lcd panel and the height
            of the i8080 arcade video hardware.

            This is used to centre the output frame on the lcd panel.
        */
        int heightOffset_{};

        /** The width of the attached lcd panel in pixels

            @remark    using a different value other than the correct lcd width is untested.
        */
        int textureWidth_{};

        /** The height of the attached lcd panel in pixels

            @remark    using a different value other than the correct lcd height is untested.
        */
        int textureHeight_{};

        /** A chunk of audio samples

            A collection of audio samples with identical properties.
        */
        struct AudioChunk
        {
            /** The number of channels in the sample

                @remark    all samples must have the same number of channels.
            */
            uint16_t channels{};

            /** Sample rate in samples per second (hertz)

                @remark    all samples must have the same sample rate.
            */
            uint32_t sampleRate{};

            /** Average data transfer rate in byes per second

                @remark    all samples must have the same bytes per second.
            */
            uint32_t bytesPerSecond{};

            /** Block alignment in bytes

                @remark    MUST be equal to product of channels and wBitsPerSample divided by 8 (bits per byte)
            */
            uint16_t nBlockAlign{};

            /** PCM sample size

                Should be 8 or 16.
            */
            uint16_t bitsPerSample{};

            /** The index of the next sample to be rendered

                @remark    A -1 index indicates that this sample is currently not being rendered
            */
            int32_t sampleIndex{ -1 };

            /** Audio samples

                A collection of audio samples described by the above properties.
            */
            std::vector<uint8_t> samples;
        };

        /** Audio sample group

            A collection of audio samples that will be mixed into the dma audio buffer for audio playback.
        */
        std::vector<AudioChunk> audioChunks_;

        /** The current samples to be mixed.

            The audio chunks that will be fed into the mix and sent to the speaker via the dmac
        */
        std::list<AudioChunk*> audioMixChunks_;

        /** Audio output buffer

            This buffer is the final mix of the current playing audio samples.

            @remark    The size of the final output buffer is the audio hardware sample rate / video hardware frame rate.
                       Essentially, this buffer will transfer to the speaker via dma the number of audio samples required
                       for one video frame duration.
        */
        std::vector<uint32_t> audioDmaBuffer_;

        /** The previous video frame

            Stores the previous frame (the back buffer) generated by the i8080 arcade video hardware.

            This allows for a triple buffering system where the first frame is the frame
            being generated, the second frame is the frame being rendered, and the third
            frame, the back buffer, which is used to compare for scanline differences with the
            frame being rendered. This allows for improved rendering performance by not rendering
            scanlines that are identical to the previous frame.
        */
        meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr backBuffer_;

        /** i8080 arcade io

            The hardware emulator.
        */
        std::unique_ptr<meen_hw::MH_II8080ArcadeIO> i8080ArcadeIO_;

        /** Helper type for functional style visitor for std::visit

            This template helper type is taken straight from cppreference std::visit examples (https://en.cppreference.com/w/cpp/utility/variant/visit2)
        */
        template<class... Ts>
        struct overloaded : Ts... { using Ts::operator()...; };

        /** Generated event data

            A using directve for ease of use. This will hold the active event data to be processed.

            std::string: The application has encountered and error.
            bool:        True to clear the vram area of the lcd display, false otherwise.
            ResourcePtr: The next video frame is ready to be rendered. This event drives the control loop.
        */
        using EventData = std::variant<std::string, bool, meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr>;

        /** A finite EventData resource pool

            A vector of EventData variants to be used during the event handling process.

            @remark    Populate from a fnite array of EventData objects
        */
        queue_t eventDataQueue_;

        /** Available event data queue

            The pool of available events that can be filled.
        */
        queue_t eventDataFreeQueue_;

        /** The maximum number of events across all pools

            This can be increased/decreased depending on requirements.
        */
        static constexpr int maxEventData_{ 2 };

        /** An array of EventData variants for use with RP2040s C based queue api

            @remark    these wrappers are solely accessed via the event data queues.
        */
        EventData eventData_[maxEventData_];

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

        /** The currently selected rom

            When the user presses the up and down arrows, this will keep track
            of the current index.

            Made atomic since it can be accesssed from a different thread if the runAsync config option
            is set to true.
        */
        int romIndex_{};

        /** The total number of supported roms for this controller.

            The value is the max limit used by the romIndex parameter to keep
            itself within range.
        */
        int romCount_{};

        /** The running state

            True if meen is to run on core 1, false to run on core 0 with
            the main application.
        */
        const bool runAsync_{};

        /** The current screen

            See the Screen enumeration for further details.
        */
        Screen screen_{};

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

            Creates an RP2040 specific i8080 arcade IO controller.

            @param		runAsync		Run this io controller asynchronously.
            @param      backBuffer      The first frame to use for double buffering.
            @param		romCount		The number of supported roms.
            @param		audioHardware	audio hardware configuration options.
            @param		videoHardware	video hardware configuration options.

        */
        RPIoController(bool runAsync, meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr&& backBuffer, int romCount, const JsonVariantConst audioHardware, const JsonVariantConst videoHardware);

        /** Destructor

            Free the various required RP2040 objects.
        */
        ~RPIoController();

        /** One time callback registration for gpio handling

            This method is to be registered with MEEN who will invoke it on a thread determined
            by the MEEN `runAsync` configuration parameter.            
        */
        static void Init();

        /** IController::Read override

            Sample the keyboard so the CPU can take any required action.

            @param  port                The device to read from.
            @param  memoryController    The memory controller that has been registered with MEEN.

            @return                     A bitfield indicating the action to take.
        */
        uint8_t Read(uint16_t port, meen::IController* memoryController) final;

        /** IController::Write override

            Write the relevant audio sample to the output audio device.

            @param  port                The output device to write to.
            @param  data                A bitfield indicating what data to write.
            @param  memoryController    The memory controller that has been registered with MEEN.
        */
        void Write(uint16_t port, uint8_t data, meen::IController* memoryController) final;

        /** IController::GenerateInterrupt override

            Render the video ram texture to the window via the rendering context.

            @param  currTime            The current CPU run time in nanoseconds.
            @param  cycles              The number of CPU cycles completed.
            @param  memoryController    The memory controller that has been registered with MEEN.

            @return                     One of the following meen ISRs:<br><br>
                                        `ISR::NoInterrupt`: the method did not generate an iterrupt.<br>
                                        `ISR::One`: signal MEEN that the first 96 scanlines have been rendered.<br>
                                        `ISR::Two`: signal MEEN that the remaining scanlines (up to 224) have
                                        been rendered (start of vblank).<br>
										`ISR::Load`: attempt to load a new rom.
        */
        meen::ISR GenerateInterrupt(uint64_t currTime, uint64_t cycles, meen::IController* memoryController) final;

        /** Uuid

            Unique universal identifier for this controller.

            @return    The uuid as a 16 byte array.
        */
        std::array<uint8_t, 16> Uuid() const final;

        /** Load Video Textures

            Create the video texture that will be rendered to the screen.

            @param  videoTextures   JSON object describing the video texture.
            @param  textureWidth    The width of the video texture in pixels.
            @param  textureHeight   The height of the video texture in pixels.

            @return                 On failure, a `std::error_code` with one of the following values:<br><br>
                                    `std::errc::not_supported`: the video configuration parameters are invalid.<br>
                                    `std::errc:io_error`: video configuration serialisation failure.
        */
        std::error_code LoadVideoTextures(const JsonVariantConst videoTextures, int textureWidth, int textureHeight) final;

        /** Load Audio Samples

            Loads the audio samples from the configuration file.

            @param     audioSamples     JSON object representing the audio sample files.

            @return                     On failure, a `std::error_code` with one of the following values:<br><br>
                                        `std::errc::protocol_not_supported`: The format of the audio sample is not supported.<br>
                                        `std::errc::value_to_large`: the size of the data in the audio sample is too large.<br>
                                        `std::errc::illegal_byte_sequence`: the audio sample is aligned incorrctly.<br>
                                        `std::errc::not_supported`: the audio file scheme in the cofiguration file is invalid.<br>
                                        `std::errc::invalid_argument`: the audio sample address is invalid.<br>
                                        `std::errc::result_out_of_range`: the audio sample address is invalid. 
        */
        std::error_code LoadAudioSamples(const JsonVariantConst audioSamples) final;

        /** Main control loop

            Process all incoming events.

            @return    True to quit the machine, false otherwise.

            @remark    This method will always return false, ie; the loop
                       will run until the device is switched off.
        */
        bool HandleEvent() final;

        /** Error handler

            Process any generated errors.

            These errors may come from MEEN or meen-i8080-arcade itself.

            @param    errorMsg      The error message as a `std::string`.
        */
        void HandleError(std::string&& errorMsg) final;

        /** Load complete handler

            Handle the transition into game play when a rom has been sucessfully loaded.
        */
        void HandleLoadComplete() final;

		/** Get the rom index

			Load the selected rom or the save state of the currently selected rom.

			@return					A tuple holding two values:<br><br>
									`bool`: only valid when loading roms, true if the save file is to be loaded, false if the rom is to be loaded.<br>
									`int`: the index into the roms array for the rom to be loaded or saved.
		*/
        std::tuple<bool, int> GetRomIndex() final;
    };
} // namespace meen_i8080_arcade
#endif // RPIOCONTROLLER_H
