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

#ifndef IOCONTROLLER_H
#define IOCONTROLLER_H

#define ARDUINOJSON_ENABLE_STRING_VIEW 1

#include <algorithm>
#include <ArduinoJson.h>
#include <atomic>
#include <bit>
#include <bitset>
#include <charconv>
#include <concepts>
#include <string>
#include <system_error>
#include <variant>

#include "meen/IController.h"
#include "meen_hw/MH_Factory.h"
#include "meen_hw/MH_ResourcePool.h"
#include "meen_i8080_arcade/IOControllerTypes.h"

namespace meen_i8080_arcade
{
    /** IO Controllable concept
    
        The rules that an io controllable must adhere to in order to be valid.

        An IO Controllable MUST implement the methods defined in the
        IOControllable concept.
    */
    template<class T>
    concept IOControllable = requires(T ioc, const int32_t* audioFrame, int audioFrameSize, uint64_t audioFrameTimestamp, const uint8_t* videoBackBuffer,
                                      const uint8_t* videoFrame, int videoFrameSize, uint64_t videoFrameTimestamp, const std::string& error, bool clearDisplay,
                                      int width, int height, int fullscreen, int bpp, int textureWidth, int textureHeight, int sampleRate, int channels,
                                      int sampleSize, uint8_t** dst, int* dstRowBytes, Screen curr, Screen next)
    {

        /** One time callback registration

            This method is registered with MEEN who will invoke it on a thread determined
            by the MEEN `runAsync` configuration parameter.
        */
        { T::Init() } -> std::same_as<void>;

        /** Render audio frame

            Called when an audio frame is ready to be rendered.
        */
        { ioc.RenderAudioFrame(audioFrame, audioFrameSize, audioFrameTimestamp) } -> std::same_as<std::errc>;

        /** Render video frame

            Called when a video frame is ready to be rendered.

            TODO: videoFrameSize needs to be a bounding box struct pointer, nullptr to render the entire frame
        */
        { ioc.RenderVideoFrame(videoBackBuffer, videoFrame, videoFrameSize, videoFrameTimestamp) } -> std::same_as<std::errc>;

        /** Render error string

            Called when an error has been encountered.
        */
        { ioc.RenderErrorString(error) } -> std::same_as<std::errc>;

        /** Clear display

            Called when the display needs to be cleared.
        */
        { ioc.ClearDisplay(clearDisplay) } -> std::same_as<std::errc>;

        /** UUID

            Called when the uuid is required.
        */
        { ioc.Uuid() } -> std::same_as<std::array<uint8_t, 16>>;

        /** Video device configuration

            Called when the video device needs to be configured.
        */
        { ioc.ConfigureVideoDevice(width, height, fullscreen) } -> std::same_as<std::errc>;

        /** Audio device configuration

            Called when the audio device needs to be configured.
        */
        { ioc.ConfigureAudioDevice(sampleRate, channels, sampleSize) } -> std::same_as<std::errc>;

        /** Peripheral device configuration

            Called when peipheral devices need to be configured.
        */
        { ioc.ConfigurePeripheralDevice() } -> std::same_as<std::errc>;

        /** Audio sample loading

            Called when audio samples need to be loaded.
        */
        { ioc.LoadAudioSamples(sampleRate, channels, sampleSize) } -> std::same_as<std::errc>;

        /** Video sample loading

            Called when video textures need to be loaded.
        */
        { ioc.LoadVideoTextures(bpp, textureWidth, textureHeight) } -> std::same_as<std::errc>;

        /** Get texture buffer

            Called when the current texture buffer is required.
        */
        { ioc.GetTextureBuffer(dst, dstRowBytes) } -> std::same_as<std::errc>;

        /** Screen transition

            Called when the screen changes from one type to another.
        */
        { ioc.ScreenTransition(curr, next) } -> std::same_as<void>;

        /** Peripheral device reading

            Called when the peripheral device needs to be read.
        */
        { ioc.ReadPeripheralDevice() } -> std::same_as<uint32_t>;
    };

    template<IOControllable T>
    class IOController final : public meen::IController
    {
private:
        /** Custom IO Controllable

            A controller that adheres to the IOControllable concept that allows the base IOController to
            target different frameworks.
        */
        T ioController_;

        /** Timed audio/video frame

            @remark    A frame templated with uint8_t is a video frame taken from video ram.
            @remark    A frame templated with int32_t is a video frame duration worth of mixed audio samples (signed 16 bit stereo).
        */
        template<class F>
        struct Frame
        {
            /** Audio/Video frame

                Video frames belong to the memory controller frame pool and audio frames belong to the audio frame pool.
            */
            meen_hw::MH_ResourcePool<std::vector<F>>::ResourcePtr bitstream;

            /** Time stamp

                The time at which the vram was sampled in MEEN timescale units.
            */
            uint64_t timestamp{};
        };

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

                @remark    MUST be equal to product of channels and wBitsPerSample divided by 8 (bits per byte).
            */
            uint16_t nBlockAlign{};

            /** PCM sample size

                Must be 8.
            */
            uint16_t bitsPerSample{};

            /** The index of the next sample to be rendered

                @remark    A -1 index indicates that this sample is currently not being rendered.
            */
            int32_t sampleIndex{ -1 };

            /** Audio samples

                A collection of audio samples described by the above properties.
            */
            std::vector<uint8_t> samples;
        };

        /** Audio sample group

            All of the supported audio samples that can be used for audio playback.
        */
        std::vector<AudioChunk> audioChunks_;

        /** The current samples to be mixed.

            The audio chunks that will be fed into the mix and sent to the speaker.
        */
        std::list<AudioChunk*> audioMixChunks_;

        /** Audio frame pool

            A pool of recyclable audio frames. Each frame holds the final mix of the next audio samples to be rendered.

            @remark    Our final output samples are 16 bit stereo, hence each sample is int32_t in size.
            @remark    The size of the final output buffer is the audio hardware sample rate / video hardware frame rate.
                       Essentially, this buffer will transfer to the speaker the number of audio samples required
                       for one video frame duration.

            See meen_hw/ResourcePool.h for further details.
        */
        meen_hw::MH_ResourcePool<std::vector<int32_t>> audioFramePool_;

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

            std::string:     The application has encountered and error.
            bool:            True to clear the vram area of the display, false otherwise.
            Frame<int32_t>:  The next audio frame is ready to be rendered. This is a video frame duration worth of samples.
            Frame<uint8_t>:  The next video frame is ready to be rendered. This event drives the control loop.
        */

        /** TODO: the clear variant needs to be a bounding box: struct{ x, y, w, h }, rather than a bool */
        using EventData = std::variant<std::string, bool, Frame<int32_t>, Frame<uint8_t>>;

        /** Event data queue

            Holds a list of events to be processed.
        */
        std::list<EventData> eventQ_;

        /** Event queue mutex

            Event data mutual exclusion between the main thread and the machine thread.
        */
        meen_hw::MH_Mutex eventQMutex_;

        /** Event queue condition variable

            Used in conjuction with eventQMutex_ to signal when new EventData is ready for processing.
        */
        meen_hw::MH_ConditionVariable eventQCv_;

        /** Load a game rom or the save state of the currently loaded game rom

            @remark    This value can be set from a different thread, hence it is atomic.
        */
        std::atomic_bool loadSaveState_{};

        /** Load or save

            A machine level interrupt which indicates whether or not the machine
            should attempt to load a new state or save its current state.

            meen::ISR::NoInterrupt: don't load or save the state.
            meen::ISR::Load: attempt to load a new rom.
            meen::ISR::Save: attempt to save the current loaded rom state.

            @remark    This value can be set from a different thread, hence it is atomic.
        */
        std::atomic<meen::ISR> loadSaveInterrupt_{ meen::ISR::NoInterrupt };

        /** The currently selected rom

            When the user presses the up and down arrows, this will keep track
            of the current index.

            Made atomic since it can be accesssed from a different thread if the runAsync/loadAsync/SaveAsync
            config options are set to true.
        */
        std::atomic_int romIndex_{};

        /** User input

            The user input represents to possible actions that can be taken by the user. Certain inputs are only
            valid in certain screens of the emulation. The `Input` enumeration in `IOControllerTypes.h` describes
            all the supported inputs.

            @remark    Depending on the controller inplementation, it may choose not to support certain user inputs.

            @see Input
        */
        std::atomic_int input_{};

        /** Previous user input

            This the previous value of the last sampled values of `input_`.
        */
        int lastInput_{};

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

        /** The audio hardware output sample rate

            This is set via the audio hardware section of the configuration file.
        */
        int sampleRate_{};

        /** The audio hardware output channel count

            This is set via the audio hardware section of the configuration file.

            @remark    Only a value of 2 (stereo) is supported.
        */
        int channels_{};

        /** The width of the output display

            This is set via the video hardware section of the configuration file.
        */
        int width_{};

        /** The height of the output display

            This is set via the audio hardware section of the configuration file.
        */
        int height_{};

        /** Fullscreen output

            This is set via the audio hardware section of the configuration file.

            True to run the emulation at fullscreen, false otherwise.

            @remark    This option won't be valid on certain platforms.
        */
        bool fullscreen_{};

        /** The width of the uncompressed video ram

            The value is used for texture memory allocation for rendering onto the output display.
        */
        int textureWidth_{};

        /** The height of the uncompressed video ram

            The value is used for texture memory allocation for rendering onto the output display.
        */
        int textureHeight_{};

        /** Add an event to the event queue

            This method is thread safe and will notify the condition
            variable when the event is added so any witing threads can
            process the new event.

            @param    eventData    The event data to be added.
        */
        void AddEventToQueue (EventData&& eventData)
        {
            if (runAsync_ == true)
            {
                {
                    meen_hw::MH_LockGuard lg(eventQMutex_);

                    if (eventQ_.size() < 5)
                    {
                        eventQ_.emplace_back(std::move(eventData));
                    }
                    else
                    {
                        //printf("Event queue limit reached\n");
                    }
                }

                eventQCv_.notify_one();
            }
            else
            {
                eventQ_.emplace_back(std::move(eventData));
            }
        }
public:
         /** Default constructor

             Not supported.
         */
         IOController() = delete;

        /** Initialisation constructor

            Creates a base i8080 arcade IO controller targetting the platform defined
            by the IOControllable template implementation.

            @param    runAsync         Run this io controller asynchronously.
            @param    backBuffer       The first frame to use for double buffering.
            @param    romCount         The number of supported roms.
            @param    audioHardware    Audio hardware configuration options.
            @param    videoHardware    Video hardware configuration options.
        */
        IOController(bool runAsync, meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr&& backBuffer, int romCount, const JsonVariantConst audioHardware, const JsonVariantConst videoHardware)
            : runAsync_{ runAsync }
            , romCount_{ romCount }
            , backBuffer_{ std::move(backBuffer) }
        {
            printf("MEEN HW Version: %s\n", meen_hw::Version());

            i8080ArcadeIO_ = meen_hw::MakeI8080ArcadeIO();

            if (i8080ArcadeIO_ == nullptr)
            {
                printf("Failed to create i8080 arcade hardware");
            }

            sampleRate_ = audioHardware["sampleRate"].as<int>();
            channels_ = audioHardware["channels"].as<int>();
            width_ = videoHardware["width"].as<int>();
            height_ = videoHardware["height"].as<int>();
            fullscreen_ = videoHardware["fullScreen"].as<bool>();
            romIndex_ = romCount_ - 1;

            auto err = ioController_.ConfigureVideoDevice(width_, height_, fullscreen_);

            if (err != std::errc{})
            {
                printf("Error configuring video device: %s\n", std::make_error_code(err).message().c_str());
            }

            err = ioController_.ConfigureAudioDevice(sampleRate_, channels_, sampleRate_ / 60);

            if (err != std::errc{})
            {
                printf("Error configuring audio device: %s\n", std::make_error_code(err).message().c_str());
            }

            err = ioController_.ConfigurePeripheralDevice();

            if (err != std::errc{})
            {
                printf("Error configuring peripheral devices: %s\n", std::make_error_code(err).message().c_str());
            }
        }

        /** IController::Read override

            Process any user input so the CPU can take any required action.

            @param	port                The device to read from.
            @param  memoryController    The memory controller that has been registered with MEEN.

            @return	                    A bitfield indicating the action to take.
        */
        uint8_t Read(uint16_t port, meen::IController* memoryController) final
        {
            auto ret = i8080ArcadeIO_->ReadPort(port);

            if (ret == 0)
            {
                if (port == 1 || port == 2)
                {
                    int input = input_;
                    //auto input = ioController_.ReadPeripheralDevice();

                    if (input & Input::QuitRom)
                    {
                        ioController_.ScreenTransition(screen_, Screen::RomSelect);

                        // Clear all queued chunks and reset chunk->samples to -1
                        while (audioMixChunks_.empty() == false)
                        {
                            auto audioMixChunk = audioMixChunks_.back();
                            audioMixChunks_.pop_back();
                            audioMixChunk->sampleIndex = -1;
                        }

                        if (runAsync_ == false) // todo: we could just do this regardless (make sure to lock the mutex) and get rid of the runAsync_ private member
                        {
                            // In single threaded mode we need to remove all outstanding i8080 arcade events and return the video frames
                            // back to the memory controller so they can be cleared.
                            while(eventQ_.empty() == false)
                            {
                                // Single threaded mode: no need to lock the eventQMutex_
                                auto eventData = std::move(eventQ_.front());
                                eventQ_.pop_front();

                                if (std::holds_alternative<Frame<uint8_t>>(eventData))
                                {
                                    auto& frame = std::get<Frame<uint8_t>>(eventData);
                                    frame.bitstream = nullptr;
                                }
                                else if (std::holds_alternative<Frame<int32_t>>(eventData))
                                {
                                    auto& frame = std::get<Frame<int32_t>>(eventData);
                                    frame.bitstream = nullptr;
                                }
                            }
                        }

                        screen_ = Screen::RomSelect;

                        // This method will ensure that all video frames are returned to the memory controller
                        // frame pool before clearing them.
                        static_cast<MemoryController*>(memoryController)->Clear(backBuffer_.get());

                        AddEventToQueue(true);
                    }
                    else
                    {
                        if (port == 1)
                        {
                            ret = 0x08;
                            ret |= ((input & Input::Credit) != 0) * 0x01; // Credit
                            ret |= ((input & Input::OnePlayer) != 0) * 0x04; // 1P
                            ret |= ((input & Input::TwoPlayer) != 0) * 0x02; // 2P
                            ret |= ((input & Input::P1Left) != 0) * 0x20; // 1P Left
                            ret |= ((input & Input::P1Fire) != 0) * 0x10; // 1P Fire
                            ret |= ((input & Input::P1Right) != 0) * 0x40; // 1P Right
                        }
                        else if (port == 2)
                        {
                            ret |= ((input & Input::ThreeShips) != 0) * 0x00; // 3 Ships
                            ret |= ((input & Input::FourShips) != 0) * 0x01; // 4 Ships
                            ret |= ((input & Input::FiveShips) != 0) * 0x02; // 5 Ships
                            ret |= ((input & Input::SixShips) != 0) * 0x03; // 6 Ships
                            ret |= ((input & Input::Tilt) != 0) * 0x04; // Tilt
                            ret |= ((input & Input::ExtraShip) != 0) * 0x08; // Extra Ship at
                            ret |= ((input & Input::P2Left) != 0) * 0x20; // 2P Left
                            ret |= ((input & Input::P2Fire) != 0) * 0x10; // 2P Fire
                            ret |= ((input & Input::P2Right) != 0) * 0x40; // 2P Right
                            ret |= ((input & Input::CoinInfo) != 0) * 0x80; // Show coin info
                        }
                        else
                        {
                            assert(0);
                            printf("Invalid Read Port: %d\n", port);
                        }

                    }
                }
            }

            return ret;
        }

        /** IController::Write override

            Select the chunks that are required for playback.

            @param	port                The output device to write to.
            @param	data                A bitfield indicating what data to write.
            @param  memoryController    The memory controller that has been registered with MEEN.

            @remark                     Chunks will be mixed and sent out in video frame duration
                                        sample sizes when the meen::ISR::Two interrupt is triggered
                                        in the `GenerateInterrupts` method. 
        */
        void Write(uint16_t port, uint8_t data, [[maybe_unused]] meen::IController* memoryController) final
        {
            std::bitset<8> audio = i8080ArcadeIO_->WritePort(port, data);

            if (audio.count() > 0)
            {
                // port will either be 3 or 5
                // when port is 3 index will be 0 and when it is 5 it will be 8
                // which will give the correct offset into the mixChunk_ array
                auto offset = (port - 3) << 2;

                for (int i = 0; i < 8; i++)
                {
                    if (i + offset < audioChunks_.size())
                    {
                        auto audioChunk = &audioChunks_[i + offset];

                        // Drop the sample if it is still playing
                        if (audio.test(i) == true && audioChunk->sampleIndex == -1)
                        {
                            if (audioChunk->samples.empty() == false)
                            {
                                audioChunk->sampleIndex = 0;
                                // Push it to the list of chunks to be played
                                audioMixChunks_.emplace_back(audioChunk);
                            }
                            else
                            {
                                {
                                    meen_hw::MH_LockGuard lg(eventQMutex_);
                                    eventQ_.emplace_back(std::string("Audio chunk ") + std::to_string(i) + " is missing from the config file");
                                }

                                eventQCv_.notify_one();
                            }
                        }
                    }
                }
            }
        }

        /** IController::GenerateInterrupt override

            Generate the required interrupts to drive the i8080 arcade emulation.

            @param  currTime            The current CPU run time in nanoseconds.
            @param  cycles              The number of CPU cycles completed.
            @param  memoryController    The memory controller that has been registered with MEEN.

            @return        One of the following meen ISRs:<br><br>
                           `ISR::NoInterrupt`: the method did not generate an iterrupt.<br>
                           `ISR::One`: signal MEEN that the first 96 scanlines have been rendered.<br>
                           `ISR::Two`: signal MEEN that the remaining scanlines (up to 224) have
                           been rendered (start of vblank).<br>
                           `ISR::Load`: attempt to load a new machine state.<br>
                           `ISR::Save`: attempt to save the current machine state.
        */
        meen::ISR GenerateInterrupt(uint64_t currTime, uint64_t cycles, meen::IController* memoryController) final
        {
            meen::ISR isr{ meen::ISR::NoInterrupt };

            auto interrupt = i8080ArcadeIO_->GenerateInterrupt(currTime, cycles);

            switch(interrupt)
            {
                case 0:
                {
                    isr = loadSaveInterrupt_.exchange(meen::ISR::NoInterrupt);

                    if (isr == meen::ISR::Load)
                    {
                        if (screen_ == Screen::RomSelect)
                        {
                            // We can't save anything from the rom select screen
                            if (loadSaveState_ == true)
                            {
                                // drop the interrupt
                                isr = meen::ISR::NoInterrupt;
                            }
                        }
                        else
                        {
                            // We can only load a save state from gameplay (as opposed to a rom), drop the interrupt
                            if (loadSaveState_ == false)
                            {
                                isr = meen::ISR::NoInterrupt;
                            }
                        }
                    }
                    else if (isr == meen::ISR::Save)
                    {
                        // We can't save anything from rom select, drop the interrupt
                        if (screen_ == Screen::RomSelect)
                        {
                            isr = meen::ISR::NoInterrupt;
                        }
                    }
                    break;
                }
                case 1:
                {
                    if (screen_ == Screen::Gameplay)
                    {
                        isr = meen::ISR::One;
                    }
                    break;
                }
                case 2:
                {
                    EventData eventData;
                    Frame<uint8_t> videoFrame;

                    if (screen_ == Screen::Gameplay)
                    {
                        isr = meen::ISR::Two;
                    }

                    auto mc = static_cast<MemoryController*>(memoryController);

                    switch (screen_)
                    {
                        case Screen::RomSelect:
                        {
                            videoFrame.bitstream = mc->GetRomSelectFrame(romIndex_, currTime);
                            break;
                        }
                        case Screen::Gameplay:
                        {
                            videoFrame.bitstream = mc->GetGameplayFrame(currTime);
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }

                    if (videoFrame.bitstream != nullptr)
                    {
                        videoFrame.timestamp = currTime;
                        //eventData = std::move(videoFrame);
                        AddEventToQueue (std::move(videoFrame));
                    }
                    else
                    {
                        //eventData = "Failed to get the video frame from the memory controller, video frame dropped";
                        AddEventToQueue ("Failed to get the video frame from the memory controller, video frame dropped");
                    }

                    // Mix a video frame duration worth of audio if we have any chunks pending
                    if (audioMixChunks_.empty() == false)
                    {
                        Frame<int32_t> audioFrame{ .bitstream = audioFramePool_.GetResource(), .timestamp = currTime };

                        if (audioFrame.bitstream != nullptr)
                        {
                            // Apply some very basic mixing
                            // Only supports 8 bit mono samples for input and 16 bit stereo samples for output
                            for(auto& sample : *audioFrame.bitstream)
                            {
                                sample = 0;

                                for (auto audioMixChunk = audioMixChunks_.cbegin(); audioMixChunk != audioMixChunks_.cend();)
                                {
                                    // convert unsigned 8bit sample to a signed 16bit sample mixing it with the current sample
                                    sample += ((*audioMixChunk)->samples[(*audioMixChunk)->sampleIndex++] - 128) * 256;// / audioMixChunks_.size();

                                    if ((*(audioMixChunk))->sampleIndex >= (*audioMixChunk)->samples.size())
                                    {
                                        // Reset the sample count for when this chunk is next played
                                        (*audioMixChunk)->sampleIndex = -1;
                                        // We are done with this chunk, remove it from the list
                                        audioMixChunk = audioMixChunks_.erase(audioMixChunk);
                                    }
                                    else
                                    {
                                        audioMixChunk++;
                                    }
                                }

                                // Clamp to 16bit to prevent overflow
                                sample = std::clamp<int32_t>(sample, -32768, 32767);
                                // Duplicate the 16bit mono sample on both channels
                                sample = (sample << 16) | sample;
                           }

                            //eventData = std::move(audioFrame);
                            AddEventToQueue (EventData{ std::move(audioFrame) });
                        }
                        else
                        {
                            //eventData = "Failed to get the audio frame from the audio device, audio frame dropped";
                            AddEventToQueue ("Failed to get the audio frame from the audio device, audio frame dropped");
                        }
                   }
                   break;
                }
                default:
                {
                    assert(interrupt >= 0 && interrupt <= 2);
                    break;
                }
            }

            return isr;
        }

        /**	Uuid

            Unique universal identifier for this controller.

            @return    The uuid as a 16 byte array.
        */
        std::array<uint8_t, 16> Uuid() const final
        {
            return ioController_.Uuid();
        }

        /**	Event handler

            Process all incoming events.

            Events include audio/video rendering, user input processing.

            @return    True to quit the machine, false otherwise.
        */
        bool HandleEvent()
        {
            EventData eventData;

            if (runAsync_ == true)
            {
                meen_hw::MH_LockGuard lg(eventQMutex_);
                eventQCv_.wait(eventQMutex_, [this]
                {
                    return eventQ_.empty() == false;
                });

                eventData = std::move(eventQ_.front());
                eventQ_.pop_front();
            }
            else
            {
                if (eventQ_.empty() == false)
                {
                    assert(eventQ_.size() == 1);
                    eventData = std::move(eventQ_.front());
                    eventQ_.pop_front();
                }
                else
                {
                    // No event to process, nothing more to do
                    return false;
                }
            }

            return std::visit(overloaded
		    {
                [this](bool clearDisplay)
                {
                    return ioController_.ClearDisplay(clearDisplay) != std::errc{};
                },
                [this](const std::string& error)
			    {
                    return ioController_.RenderErrorString(error) != std::errc{};
                },
			    [this](Frame<int32_t>& audioFrame)
			    {
                    return ioController_.RenderAudioFrame(audioFrame.bitstream->data(), audioFrame.bitstream->size(), audioFrame.timestamp) != std::errc{};
                    // frame.bitstream = nullptr ... or not
                },
			    [this](Frame<uint8_t>& videoFrame)
			    {
                    // Process user input

                    // We will check for user input at the same rate that we render video frames (60Hz).
                    auto input = input_ = ioController_.ReadPeripheralDevice();

                    if (input & Input::Exit)
                    {
                        return true;
                    }

                    // Ignore consecutive inputs
                    auto noRepeatInput = [input, lastInput = lastInput_](Input i)
                    {
                        return (input & i) ^ (lastInput & i) && (input & i);
                    };

                    if (noRepeatInput(Input::SelectRom) == true)
                    {
                        loadSaveInterrupt_ = meen::ISR::Load;
                        loadSaveState_ = false;
                    }

                    if (noRepeatInput(Input::LoadRom) == true)
                    {
                        loadSaveInterrupt_ = meen::ISR::Load;
                        loadSaveState_ = true;
                    }

                    if (noRepeatInput(Input::SaveRom) == true)
                    {
                        loadSaveInterrupt_ = meen::ISR::Save;
                        loadSaveState_ = false;
                    }

                    auto scrollIndex = [this, input, &noRepeatInput](Input i, int dir)
                    {
                        int romIndex = romIndex_;

                        if(noRepeatInput(i) == true)
                        {
                            romIndex = (romIndex + dir) % romCount_;

                            if (romIndex < 0)
                            {
                                romIndex = romCount_ - 1;
                            }
                        }

                        return romIndex;
                    };

                    romIndex_ = scrollIndex(Input::PreviousRom, 1);
                    romIndex_ = scrollIndex(Input::NextRom, -1);
                    lastInput_ = input;

                    // Do the video rendering

                    uint8_t* dst = nullptr;
                    int dstRowBytes = 0;
                    ioController_.GetTextureBuffer(&dst, &dstRowBytes);
                    i8080ArcadeIO_->BlitVRAM(std::span(dst, textureHeight_ * dstRowBytes), textureWidth_, dstRowBytes, std::span(*(videoFrame.bitstream.get())), MemoryController::frameWidth);
                    // Release the video frame bitstream immediately back to the memory controller frame pool
                    int size = videoFrame.bitstream->size();
                    videoFrame.bitstream = nullptr;

                    return ioController_.RenderVideoFrame(backBuffer_.get()->data(), dst, size, videoFrame.timestamp) != std::errc{};
                }
		    }, eventData);
        }

        /** Process the error string.

            @param    errorMsg    The generated error message.
        */
        void HandleError(std::string&& errorMsg)
        {
            AddEventToQueue(errorMsg);
        }

        /** Perfom post load actions

            Once a rom has been successfully loaded, this method will be called.
        */
        meen::errc HandleLoadComplete()
        {
            ioController_.ScreenTransition(screen_, Screen::Gameplay);

            // We successfully loaded the rom, transition into gameplay.
            screen_ = Screen::Gameplay;
            // Reset the internal state of the hardware
            i8080ArcadeIO_->Reset();

            // Based on what rom we have loaded, we may need to reconfigure the hardware,
            // for example, audio effects which repeat (ufo for space invaders) may not be the same for other roms,
            // need to confirm this.
            // i8080ArcadeIO_->SetOptions(R"({"repeat_samples":[1, 2, 4]})")

            return meen::errc::no_error;
        }

        /** Load Video Textures

            Create the video texture that will be rendered to the screen.

            @param	videoTextures	JSON object describing the video texture.
            @param  textureWidth    The width of the video texture in pixels.
            @param  textureHeight   The height of the video texture in pixels.

            @return    On failure, a `std::error_code` with one of the following values:<br><br>
                       `std::errc:io_error`: video configuration serialisation failure.<br>
                       `std::errc::not_supported`: the video configuration parameters are invalid.<br>
                       `std::errc::not_enough_memory`: failed to allocate the video textures.
        */
        std::error_code LoadVideoTextures(const JsonVariantConst videoTextures, int textureWidth, int textureHeight)
        {
// TODO: RP IO Controller is more restrictive, need to add PICO_BOARD defines to confine it

            std::string meenConfig;
            serializeJson(videoTextures, meenConfig);

            if(meenConfig.empty() == true)
            {
                return std::make_error_code (std::errc::io_error);
            }

            auto err = i8080ArcadeIO_->SetOptions(meenConfig.c_str());

            if(err)
            {
                return err;
            }

            if (videoTextures["bpp"] == nullptr)
            {
                return std::make_error_code(std::errc::io_error);
            }

            if (videoTextures["orientation"] != nullptr && videoTextures["orientation"].as<std::string>() == "upright")
            {
                textureWidth ^= textureHeight ^= textureWidth ^= textureHeight;
            }

            textureWidth_ = textureWidth;
            textureHeight_ = textureHeight;

            return std::make_error_code(ioController_.LoadVideoTextures(videoTextures["bpp"], textureWidth, textureHeight));
        }

        /** Load Audio Samples

            Load the PCM samples defined in the software audio section of the config file.

            @param    audioSamples    JSON object representing the audio sample files.

            @return    On failure, a `std::error_code` with one of the following values:<br><br>
                       `std::errc::no_such_file_or_directory`: the audio resource specified by the `file://`
                       protocol failed to open.<br>
                       `std::errc::not_supported`: the audio file scheme in the configuration file is invalid.<br>
                       `std::errc::not_supported`: the number of audio channels defined in the configuration file
                       can't be allocated.<br>
                       `std::errc::invalid_argument`: the length of the audio configuration samples array is not
                       supported.
        */
        std::error_code LoadAudioSamples([[maybe_unused]] const JsonVariantConst audioSamples)
        {
            // Setting an output sample rate of zero and an output channel count of 0 indicates the users preference for no audio so we return success
            if (sampleRate_ == 0 && channels_ == 0)
            {
                return std::error_code{};
            }

            if (sampleRate_ < 0 || channels_ != 2)
            {
                return std::make_error_code(std::errc::not_supported);
            }

            auto scheme = audioSamples["scheme"].as<std::string_view>();
            auto directory = audioSamples["directory"].as<std::string_view>();

            auto addChunk = [this](const uint8_t* wav, int len)
            {
                auto getUint32 = [](const uint8_t* ptr) -> uint32_t
                {
                    return (static_cast<uint32_t>(ptr[3]) << 24) | (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[1]) << 8) | static_cast<uint32_t>(ptr[0]);
                };

                auto getUint16 = [](const uint8_t* ptr) -> uint16_t
                {
                    return (static_cast<uint16_t>(ptr[1]) << 8) | static_cast<uint16_t>(ptr[0]);
                };

                if (len < 8)
                {
                    return std::errc::protocol_not_supported;
                }

                // Check the 'RIFF' fourcc
                if (getUint32(wav) != 0x46464952)
                {
                    return std::errc::protocol_not_supported;
                }

                // Make sure we have enough buffer
                if (len - 8 < getUint32(wav + 4))
                {
                    return std::errc::value_too_large;
                }

                // Check for 'WAVE' fourcc
                if (getUint32(wav + 8) != 0x45564157)
                {
                    return std::errc::protocol_not_supported;
                }

                // Check for 'fmt ' fourcc
                if (getUint32(wav + 12) != 0x20746D66)
                {
                    return std::errc::protocol_not_supported;
                }

                // Not supporting extended data
                if (getUint32(wav + 16) != 16)
                {
                    return std::errc::no_protocol_option;
                }

                // Only supporting PCM format
                if (getUint16(wav + 20) != 1)
                {
                    return std::errc::no_protocol_option;
                }

                // Check for 'data' fourcc
                if (getUint32(wav + 36) != 0x61746164)
                {
                    return std::errc::protocol_not_supported;
                }

                // The WAV header is 44 bytes, make sure we have enough buffer
                if (len - 44 < getUint32(wav + 40))
                {
                    return std::errc::value_too_large;
                }

                auto nBlockAlign = getUint16(wav + 32);
                auto dataLen = getUint32(wav + 40);

                // We only support 8 bit mono, any other nBlockAlign indicates otherwise
                if (nBlockAlign != 1)
                {
                    return std::errc::not_supported;
                }

                // The WAV data is not aligned correctly
                if (dataLen % nBlockAlign != 0)
                {
                    return std::errc::illegal_byte_sequence;
                }

                std::vector<uint8_t> samples(dataLen);

                audioChunks_.emplace_back
                (
                    getUint16(wav + 22),  // channels
                    getUint32(wav + 24),  // sample rate
                    getUint32(wav + 28),  // bytes per second
                    nBlockAlign        ,  // nblock align
                    getUint16(wav + 34),  // bits per sample
                    -1,                   // sample index
                    std::vector<uint8_t>(wav + 44, wav + 44 + dataLen) // audio samples
                );

                return std::errc();
            };

            auto addChunkFromFile = [this, &addChunk](std::string_view directory, std::string_view resource)
            {
#ifndef PICO_BOARD
                if (resource.empty() == false)
                {
                    std::ifstream fin(std::string(directory) + std::string(resource), std::ios::binary);

                    if (fin.bad())
                    {
                        return std::errc::bad_file_descriptor;
                    }

                    fin.seekg(0, std::ios::end);
                    int len = fin.tellg();
                    fin.seekg(0, std::ios::beg);
                    std::vector<uint8_t> wav(len);
                    fin.read(std::bit_cast<char*>(wav.data()), len);
                    return addChunk(wav.data(), len);
                }
#endif
                audioChunks_.emplace_back();
                return std::errc{};
            };

            auto addChunkFromMem = [this, &addChunk](std::string_view resource, int resourceSize)
            {
                if (resource.empty() == false)
                {
                    uintptr_t value = 0;
                    auto [ptr, ec] = std::from_chars(resource.data(), resource.data() + resource.size(), value, 10);

                    if (ec != std::errc())
                    {
                        return ec;
                    }

                    if (ptr != resource.data() + resource.size())
                    {
                        return std::errc::illegal_byte_sequence;
                    }

                    return addChunk(std::bit_cast<const uint8_t*>(value), resourceSize);

                }

                audioChunks_.emplace_back();
                return std::errc{};
            };

            for (const auto& sample : audioSamples["sample"].as<JsonArrayConst>())
            {
                auto ec = std::errc{};
                auto dir = directory;
                auto resource = sample["bytes"].as<std::string_view>();

                if (resource.starts_with("mem://"))
                {
                    resource.remove_prefix(strlen("mem://"));
                    ec = addChunkFromMem(resource, sample["size"].as<int>());
                }
                else if (resource.starts_with("file://"))
                {
                    resource.remove_prefix(strlen("file://"));
                    // A resource starting with a scheme specifies the exact location of that resource
                    dir = "";
                    ec = addChunkFromFile(directory, resource);
                }
                else
                {
                    if (scheme == "mem://")
                    {
                        ec = addChunkFromMem(resource, sample["size"].as<int>());
                    }
                    else if (scheme == "file://")
                    {
                        ec = addChunkFromFile(directory, resource);
                    }
                    else
                    {
                        return std::make_error_code(std::errc::not_supported);
                    }
                }

                if (ec != std::errc{})
                {
                    return std::make_error_code(ec);
                }
            }

            for (int i = 0; i < 2 /* total number of audio frames in the pool */; i++)
            {
                // Resize the output buffers so they write a video frame duration (or close to) of audio frames to the speaker.
                // Note: depending on the sample rate this may not be a whole number and will be truncated,
                // hence it could be one sample less than a video frame duration (this should be fine).
                audioFramePool_.AddResource(new std::vector<int32_t>(sampleRate_ / 60));
            }

            return std::make_error_code(ioController_.LoadAudioSamples(sampleRate_, channels_, sampleRate_ / 60));
        };

        /** Absolute save file path

            Constructs the path to save the machine state to.

            @param    jsonRoms        The list of supported roms.
            @param    saveFilePath    The directory where the state is to be saved.
            @param    uri             A buffer supplied by the caller to write the final path to.
            @param    uriLen          The length in bytes of uri parameter. The final length of
                                      of the uri parameter will be written to uriLen.

            @return                   A meen::errc denoting the success of the path write.
        */
        meen::errc GetSaveUri(const std::vector<std::pair<std::string, std::string>>& jsonRoms, const std::string& saveFilePath, char* uri, int* uriLen) //change to std::span
        {
            *uriLen = snprintf(uri, *uriLen, "%s/%s.json", saveFilePath.c_str(), jsonRoms[romIndex_].first.c_str());
            return meen::errc::no_error;
        }

        /** The path to the rom to load

            The path is determined by whether or not a rom is being loaded or a save state. For rom loading
            the rom path will be used, otherwise the save directory will be used.

            @param    jsonRoms        The list of supported roms.
            @param    saveFilePath    The directory where the save state is to be loaded from.
            @param    uri             A buffer supplied by the caller to write the final path to.
            @param    uriLen          The length in bytes of uri parameter. The final length of
                                      of the uri parameter will be written to uriLen.

            @return                   A meen::errc denoting the success of the path write.
        */
        meen::errc GetLoadUri(const std::vector<std::pair<std::string, std::string>>& jsonRoms, const std::string& saveFilePath, char* uri, int* uriLen) //change to std::span
        {
            if (loadSaveState_.exchange(false) == true)
            {
                *uriLen = snprintf(uri, *uriLen, "%s/%s.json", saveFilePath.c_str(), jsonRoms[romIndex_].first.c_str());
            }
            else
            {
                *uriLen = snprintf(uri, *uriLen, "%s", jsonRoms[romIndex_].second.c_str());
            }

            return meen::errc::no_error;
        }
    };
} // namespace meen_i8080_arcade

#endif // IOCONTROLLER_H