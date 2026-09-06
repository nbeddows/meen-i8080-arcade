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

#include <algorithm>
#include <assert.h>
#include <bitset>
#include <charconv>
#include <cstring>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <hardware/pwm.h>
#include <hardware/spi.h>
#include <pico/stdio.h>

#include "meen_i8080_arcade/MemoryController.h"
#include "meen_i8080_arcade/RPIoController.h"
#include "RPAudioOut.pio.h"

namespace meen_i8080_arcade
{
    bool RPIoController::buttonPress_[Pin::MAX] = {};
    bool RPIoController::prevEdgeFall_[Pin::MAX] = {};
    bool RPIoController::prevEdgeRise_[Pin::MAX] = {};

    RPIoController::RPIoController(bool runAsync, meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr&& backBuffer, int romCount, const JsonVariantConst audioHardware, const JsonVariantConst videoHardware)
        : runAsync_{ runAsync }
        , romCount_{ romCount }
        , romIndex_{ romCount - 1 }
        , backBuffer_{ std::move(backBuffer) }
    {
        i8080ArcadeIO_ = meen_hw::MakeI8080ArcadeIO();

        if(i8080ArcadeIO_ == nullptr)
        {
            //throw std::runtime_error("Failed to create i8080 arcade hardware");
        }

        // The width and height of the lcd panel (only tested with 320x240 panel)
        width_ = videoHardware["width"].as<int>();
        height_ = videoHardware["height"].as<int>();
        // The sample rate of the audio chunks
        sampleRate_ = audioHardware["sampleRate"].as<int>();
        audioFrameSizer_ = AudioFrameSizer{ static_cast<std::uint32_t>(sampleRate_) };

        spi_init(spi1, 62.5 * 1000000);

        gpio_set_function(Pin::CLK, GPIO_FUNC_SPI);
        gpio_set_function(Pin::DIN, GPIO_FUNC_SPI);

        gpio_init(Pin::RST);
        gpio_set_dir(Pin::RST, GPIO_OUT);
        gpio_init(Pin::DC);
        gpio_set_dir(Pin::DC, GPIO_OUT);
        gpio_init(Pin::CS);
        gpio_set_dir(Pin::CS, GPIO_OUT);
        gpio_init(Pin::BL);
        gpio_set_dir(Pin::BL, GPIO_OUT);

        gpio_init(Pin::K0);
        gpio_set_dir(Pin::K0, GPIO_IN);
        gpio_pull_up(Pin::K0);//Need to pull up

        gpio_init(Pin::K1);
        gpio_set_dir(Pin::K1, GPIO_IN);
        gpio_pull_up(Pin::K1);//Need to pull up

        gpio_init(Pin::K2);
        gpio_set_dir(Pin::K2, GPIO_IN);
        gpio_pull_up(Pin::K2);//Need to pull up

        gpio_init(Pin::K3);
        gpio_set_dir(Pin::K3, GPIO_IN);
        gpio_pull_up(Pin::K3);//Need to pull up

        gpio_put(Pin::BL, 1);
        gpio_put(Pin::CS, 1);
        gpio_put(Pin::DC, 0);
        gpio_put(Pin::RST, 1);

        // PWM Config
        gpio_set_function(Pin::BL, GPIO_FUNC_PWM);
        auto sliceNum = pwm_gpio_to_slice_num(Pin::BL);
        pwm_set_wrap(sliceNum, 100);
        pwm_set_chan_level(sliceNum, PWM_CHAN_B, 90); // backlight up to 90%
        pwm_set_clkdiv(sliceNum,50);
        pwm_set_enabled(sliceNum, true);

        // Set the read / write scan direction of the frame memory
        WriteCmd(0x36); //MX, MY, RGB mode
        WriteParam(0x70); //0x08 bit: off - RGB,  on - BGR

        // 16bpp
        WriteCmd(0x3A);
        WriteParam(0x05); // set to 0x03 for 12bpp

        // display inversion on
        WriteCmd(0x21);

        // turn on idle mode
        WriteCmd(0x39);

        // sleep out
        WriteCmd(0x11);
        sleep_ms(120);

        // display on
        WriteCmd(0x29);

        // Write 8 bits at a time
        spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        // Write to the whole display to clear
        SetRegion(0, 0, width_, height_);
        // write to lcd ram
        WriteCmd(0X2C);

        // Write 16 bits at a time
        spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

        gpio_put(Pin::DC, 1);
        gpio_put(Pin::CS, 0);
    }

    RPIoController::~RPIoController()
    {
        gpio_deinit(Pin::BL);
        gpio_deinit(Pin::CS);
        gpio_deinit(Pin::DC);
        gpio_deinit(Pin::K0);
        gpio_deinit(Pin::K1);
        gpio_deinit(Pin::K2);
        gpio_deinit(Pin::K3);
        gpio_deinit(Pin::RST);
        spi_deinit(spi1);
        dma_channel_cleanup(0);
        dma_channel_unclaim(0);
    }

    std::error_code RPIoController::LoadVideoTextures(const JsonVariantConst videoTextures, int textureWidth, int textureHeight)
    {
        int bpp = 16; // this needs to be updated to 12bpp for performance reasons

        if(videoTextures["bpp"] != nullptr)
        {
            bpp = videoTextures["bpp"].as<int>();

            if(bpp != 16)
            {
                return std::make_error_code(std::errc::not_supported);
            }
        }

        if(videoTextures["orientation"] != nullptr)
        {
            auto orientation = videoTextures["orientation"].as<std::string>();

            if(orientation != "cocktail")
            {
                return std::make_error_code(std::errc::not_supported);
            }
        }

        std::string meenConfig;
        serializeJson(videoTextures, meenConfig);

        if(meenConfig.empty() == true)
        {
            return std::make_error_code (std::errc::io_error);
        }

        i8080ArcadeIO_->SetOptions(meenConfig.c_str());
        // We decompress and write one scanline at a time to lcd ram
        texture_.resize((textureWidth * bpp) / 8); // 8 - bits per pixel
        // Used to center the video ram on the display
        widthOffset_ = (width_ - textureWidth) / 2;
        heightOffset_ = (height_ - textureHeight) / 2;
        textureWidth_ = textureWidth;
        textureHeight_ = textureHeight;

        // Blit the first frame

        gpio_put(Pin::CS, 1);
        // Write 8 bits at a time
        spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        // Center the graphics on the display
        RPIoController::SetRegion(widthOffset_, heightOffset_, width_ - widthOffset_, height_ - heightOffset_);
        // write to lcd ram
        RPIoController::WriteCmd(0X2C);
        // Write 16 bits at a time
        spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

        gpio_put(Pin::DC, 1);
        gpio_put(Pin::CS, 0);

        auto bb = backBuffer_->data();
        auto compressedWidth = textureWidth_ >> 3;
        auto dst = texture_.data();
        auto dstSize = texture_.size();
        auto dst16 = std::bit_cast<uint16_t*>(dst);

        // loop back buffer, blit each scan line
        for(int i = 0; i < textureHeight_; i++)
        {
            i8080ArcadeIO_->BlitVRAM(std::span(dst, dstSize), textureWidth_, dstSize, std::span(bb, compressedWidth), MemoryController::frameWidth);
            spi_write16_blocking(spi1, dst16, textureWidth_);
            bb += compressedWidth;
        }

        return std::error_code{};
    }

    std::error_code RPIoController::LoadAudioSamples([[maybe_unused]] const JsonVariantConst audioSamples)
    {
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

        if (sampleRate_ > 0)
        {
            for (int i = 0; i < 2 /* total number of audio frames in the pool */; i++)
            {
                // Allocate enough room for the largest frame. AudioFrameSizer
                // resizes each buffer before mixing so fractional samples are
                // distributed across the 60 Hz video frames.
                audioFramePool_.AddResource(new std::vector<int32_t>(audioFrameSizer_.Max()));
            }

            dma_channel_cleanup(0);
            dma_channel_unclaim(0);

            // Setup and configure programmable io
            gpio_set_function(Pin::ADIN, GPIO_FUNC_PIO0);
            gpio_set_function(Pin::BCK, GPIO_FUNC_PIO0);
            gpio_set_function(Pin::LRCK, GPIO_FUNC_PIO0);

            pio_sm_claim(pio0, 0);
            uint offset = pio_add_program(pio0, &audio_pio_program);
            audio_pio_program_init(pio0, 0 /* state machine index */, offset, Pin::ADIN, Pin::BCK);
            uint32_t system_clock_frequency = clock_get_hz(clk_sys);
            uint32_t divider = system_clock_frequency * 4 / sampleRate_; // avoid arithmetic overflow
            pio_sm_set_clkdiv_int_frac(pio0, 0 /* state machine index */, divider >> 8u, divider & 0xffu);
            pio_sm_set_enabled(pio0, 0 /* state machine index */, true);

            // Setup and configure direct memory access (dma) for audio output
            dma_channel_claim(0);
            dma_channel_config c = dma_channel_get_default_config(0);
            channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
            channel_config_set_read_increment(&c, true);
            channel_config_set_write_increment(&c, false);
            channel_config_set_dreq(&c, DREQ_PIO0_TX0);

            dma_channel_configure(
                0,                        // Channel to be configured
                &c,                       // The configuration we just created
                &pio0_hw->txf[0],         // The initial write address
                nullptr,                  // The initial read address (this will be set later)
                0,                        // Number of transfers (this will be set later)
                false                     // We will start the transfers later
            );
        }

        return std::error_code{};
    };

    uint8_t RPIoController::Read(uint16_t port, [[maybe_unused]] meen::IController* memoryController)
    {
        uint8_t ret = i8080ArcadeIO_->ReadPort(port);

        if (ret == 0)
        {
            if (port == 1)
            {
                // The 4th bit is always set (0x08).
                // This demo moves straight to a 1P game as soon as a credit is entered (no 2P support),
                // therefore we always set it (0x04) (it will be ignored if no credits exist)
                ret = 0x08 | 0x04;

                if (buttonPress_[Pin::K1] == true)
                {
                    screen_ = Screen::RomSelect;
                    buttonPress_[Pin::K1] = false;

                    // Not used on rom select, disable it
                    gpio_set_irq_enabled(Pin::K0, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
                    // Abort any current dmac operation
                    dma_channel_abort(0);

                    // Clear all queued chunks and reset chunk->samples to -1
                    while (audioMixChunks_.empty() == false)
                    {
                        auto audioMixChunk = audioMixChunks_.back();
                        audioMixChunks_.pop_back();
                        audioMixChunk->sampleIndex = -1;
                    }

                    if (runAsync_ == false)
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

                    // We should now be safe to reset this as video frames can only be added from the thread we are currently on
                    // and the spinlock/cancellation above ensures all remaining frames have been blitted/cancelled
                    // (the HandleEvent thread (if runAsync is true) would now be in a waiting state waiting on the next frame to be added).
                    static_cast<MemoryController*>(memoryController)->Clear(backBuffer_.get());

                    {
                        meen_hw::MH_LockGuard lg(eventQMutex_);
                        eventQ_.emplace_back(EventData{ true });
                    }

                    eventQCv_.notify_one();
                }
                else
                {
                    if (ships_ > 0)
                    {
                        ret |= (RPIoController::buttonPress_[Pin::K0] * 0x20); // 1P Left
                        ret |= (RPIoController::buttonPress_[Pin::K3] * 0x40); // 1P Right
                        ret |= (RPIoController::buttonPress_[Pin::K2] * 0x10); // 1P Fire
                    }
                    else
                    {
                        if (RPIoController::buttonPress_[Pin::K0] == true)
                        {
                            buttonPress_[Pin::K0] = false;
                            // Add a credit
                            ret |= 0x01;
                            // Move straight to a 1P game.
                            // Set the amount of ships (this could be also 4/5/6 if this demo supported setting the ship count)
                            ships_ = 3;
                        }
                    }
                }
            }
            else if (port == 2)
            {
                // Other options will run at defaults, ie; 3 ships with an extra ship every 1500 points
            }
        }

        return ret;
    }

    void RPIoController::Write(uint16_t port, uint8_t data, [[maybe_unused]] meen::IController* memoryController)
    {
        std::bitset<8> audio = i8080ArcadeIO_->WritePort(port, data);

        if (audio.count() > 0)
        {
            if(port == 3)
            {
                // port 3 bit 4 is extended play, need to increment the ships_ count by one
                if(audio.test(4) == true)
                {
                    ++ships_;
                }

                // port 3 bit 2 is player killed, need to reduce the ships_ count by one
                if(audio.test(2) == true)
                {
                    --ships_;
                }
            }

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
                                eventQ_.emplace_back(EventData{ std::string("Audio chunk ") + std::to_string(i) + " is missing from the config file" });
                            }

                            eventQCv_.notify_one();
                        }
                    }
                }
            }
        }
    }

    void RPIoController::Init()
    {
        gpio_set_irq_callback([](uint gpio, uint32_t eventMask)
        {
            // Ignore consecutive edge rise/fall on the same pin.
            if ((eventMask & GPIO_IRQ_EDGE_FALL) != 0 && RPIoController::prevEdgeFall_[gpio] == false)
            {
                RPIoController::buttonPress_[gpio] = true;
                // Keep track of the edge fall/rise to prevent spurious falls/rises.
                RPIoController::prevEdgeFall_[gpio] = true;
                RPIoController::prevEdgeRise_[gpio] = false;
            }

            if ((eventMask & GPIO_IRQ_EDGE_RISE) != 0 && RPIoController::prevEdgeRise_[gpio] == false)
            {
                RPIoController::buttonPress_[gpio] = false;
                // keep track of the edge fall/rise to prevent spurious falls/rises.
                RPIoController::prevEdgeFall_[gpio] = false;
                RPIoController::prevEdgeRise_[gpio] = true;
            }
        });

        // Enable the pin interrupts required for the rom select screen (the initial screen state)
        gpio_set_irq_enabled(Pin::K0, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
        gpio_set_irq_enabled(Pin::K1, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
        gpio_set_irq_enabled(Pin::K2, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
        gpio_set_irq_enabled(Pin::K3, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
        irq_set_enabled(IO_IRQ_BANK0, true);
    }

    meen::ISR RPIoController::GenerateInterrupt(uint64_t currTime, uint64_t cycles, meen::IController* memoryController)
    {
        auto isr = meen::ISR::NoInterrupt;
        auto interrupt = i8080ArcadeIO_->GenerateInterrupt(currTime, cycles);

        switch(interrupt)
        {
            case 0:
            {
                if (screen_ == Screen::RomSelect)
                {
                    // Check button 1 press to trigger a rom load interrupt
                    if (RPIoController::buttonPress_[Pin::K1] == true)
                    {
                        isr = meen::ISR::Load;
                        RPIoController::buttonPress_[Pin::K1] = false;
                    }

                    if(RPIoController::buttonPress_[Pin::K2] == true)
                    {
                        romIndex_ = ++romIndex_ % romCount_;
                        RPIoController::buttonPress_[Pin::K2] = false;
                    }

                    if(RPIoController::buttonPress_[Pin::K3] == true)
                    {
                        if (--romIndex_ < 0)
                        {
                            romIndex_ = romCount_ - 1;
                        }

                        RPIoController::buttonPress_[Pin::K3] = false;
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
                Frame<uint8_t> frame;
                EventData eventData;

                if (screen_ == Screen::Gameplay)
                {
                    isr = meen::ISR::Two;
                }

                switch (screen_)
                {
                    case Screen::RomSelect:
                        frame.bitstream = static_cast<MemoryController*>(memoryController)->GetRomSelectFrame(romIndex_, currTime);
                        break;
                    case Screen::Gameplay:
                        frame.bitstream = static_cast<MemoryController*>(memoryController)->GetGameplayFrame(currTime);
                        break;
                    default:
                        break;
                }

                if (frame.bitstream != nullptr)
                {
                    frame.timestamp = currTime;
                    eventData = std::move(frame);
                }
                else
                {
                    eventData = "Failed to get the frame from the memory controller, frame dropped";
                }

                if (runAsync_ == true)
                {
                    {
                        meen_hw::MH_LockGuard lg(eventQMutex_);
                        eventQ_.push_back(std::move(eventData));
                    }

                    eventQCv_.notify_one();
                }
                else
                {
                    eventQ_.push_back(std::move(eventData));
                }

                if (audioMixChunks_.empty() == false)
                {
                    Frame<int32_t> audioFrame{ .bitstream = audioFramePool_.GetResource(), .timestamp = currTime };

                    if (audioFrame.bitstream != nullptr)
                    {
                        audioFrame.bitstream->resize(audioFrameSizer_.Next());

                        // Apply some very basic mixing
                        // Only supports unsigned 8 bit mono samples for input and signed 16 bit stereo samples for output
                        for(auto& sample : *audioFrame.bitstream)
                        {
                            sample = 0;

                            for (auto audioMixChunk = audioMixChunks_.cbegin(); audioMixChunk != audioMixChunks_.cend();)
                            {
                                // convert an unsigned 8-bit sample to a signed 16-bit sample mixing it with the current sample
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
                            // Duplicate the 16-bit mono sample on both channels
                            sample = (sample << 16) | sample;
                        }

                        eventData = std::move(audioFrame);
                    }
                    else
                    {
                        eventData = "Failed to get the audio frame from the audio device, audio frame dropped";
                    }

                    // Send the mixed audio samples to the main thread
                    if (runAsync_ == true)
                    {
                        {
                            meen_hw::MH_LockGuard lg(eventQMutex_);
                            eventQ_.push_back(std::move(eventData));
                        }

                        eventQCv_.notify_one();
                    }
                    else
                    {
                        eventQ_.push_back(std::move(eventData));
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

    std::array<uint8_t, 16> RPIoController::Uuid() const
    {
        return{ 0x87, 0x4C, 0xD4, 0x1C, 0xC1, 0xB0, 0x44, 0x86, 0xA2, 0x02, 0xCC, 0xB7, 0x0B, 0xB3, 0x44, 0xBB };
    }

    void RPIoController::WriteCmd(uint8_t cmd)
    {
        gpio_put(Pin::DC, 0);
        gpio_put(Pin::CS, 0);
        spi_write_blocking(spi1, &cmd, sizeof(uint8_t));
        gpio_put(Pin::CS, 1);
    };

    void RPIoController::WriteParam(uint8_t param)
    {
        gpio_put(Pin::DC, 1);
        gpio_put(Pin::CS, 0);
        spi_write_blocking(spi1, &param, sizeof(uint8_t));
        gpio_put(Pin::CS, 1);
    };

    void RPIoController::SetRegion(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend)
    {
        //set the X coordinates
        RPIoController::WriteCmd(0x2A);
        RPIoController::WriteParam(Xstart >>8);
        RPIoController::WriteParam(Xstart & 0xff);
        RPIoController::WriteParam((Xend - 1) >> 8);
        RPIoController::WriteParam((Xend-1) & 0xFF);

        //set the Y coordinates
        RPIoController::WriteCmd(0x2B);
        RPIoController::WriteParam(Ystart >>8);
        RPIoController::WriteParam(Ystart & 0xff);
        RPIoController::WriteParam((Yend - 1) >> 8);
        RPIoController::WriteParam((Yend - 1) & 0xff);
    };

    bool RPIoController::HandleEvent()
    {
        EventData eventData;

        if (runAsync_ == true)
        {
            eventQMutex_.lock();
            eventQCv_.wait(eventQMutex_, [this] { return eventQ_.empty() == false; });
            eventData = std::move(eventQ_.front());
            eventQ_.pop_front();
            eventQMutex_.unlock();
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
            [](const std::string& error)
            {
                printf("%s\n", error.c_str());
                return false;
            },
            [this](bool clear)
            {
                if (clear == true)
                {
                    // The vram is positioned at the center of the display
                    int ho = (height_ - MemoryController::vramHeight) / 2;
                    // The vram is 1bpp hence we need to multiply the width by 8
                    int wo = (width_ - (MemoryController::vramWidth << 3)) / 2;

                    // Clear the centre of the display (where the vram is blitted), one row at a time
                    for(int i = ho; i < ho + MemoryController::vramHeight; i++)
                    {
                        gpio_put(Pin::CS, 1);
                        // Write 8 bits at a time
                        spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
                        // Clear the blittable region to the current row
                        RPIoController::SetRegion(wo, i /* row start */, width_ - wo, 1/* Cover a height of 1, ie; 1 row*/);
                        // Write to lcd ram
                        RPIoController::WriteCmd(0X2C);
                        // Write 16 bits at a time
                        spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

                        gpio_put(Pin::DC, 1);
                        gpio_put(Pin::CS, 0);

                        // Clear the current row of the display
                        for(int j = 0; j < (MemoryController::vramWidth << 3) / 2; j++)
                        {
                            uint16_t p = 0x0000;
                            spi_write16_blocking(spi1, &p, 1);
                        }
                    }
                }

                return false;
            },
            [this](Frame<int32_t>& frame)
            {
                // The timing won't be precise (but close enough), so we need to possibly wait for the dmac to finish.
                // This is good enough for this application.
                if (dma_channel_is_busy(0) == true)
                {
                    dma_channel_wait_for_finish_blocking(0);
                }

                // Start the audio transfer
                dma_channel_transfer_from_buffer_now(0, frame.bitstream->data(), frame.bitstream->size());

                // We are done with the frame, return it immediately to the audio frame pool by explicitly setting it to nullptr
                frame.bitstream = nullptr;
                return false;
            },
            [this](Frame<uint8_t>& frame)
            {
                auto compressedWidth = textureWidth_ >> 3;
                auto dst = texture_.data();
                auto dstSize = texture_.size();
                auto dst16 = std::bit_cast<uint16_t*>(dst);
                auto vf = frame.bitstream->data();
                uint8_t* bb = backBuffer_->data();

                for(int i = 0, lastScanline = 0; i < textureHeight_; i++, bb += compressedWidth, vf += compressedWidth)
                {
                    // Blit a scanline at a time for performance reasons

                    // Check to see if this scanline has changed compared to its counterpart in the back buffer (previous frame)
                    if(std::memcmp(bb, vf, compressedWidth) != 0)
                    {
                        // Update the region only if the scanline to be rendered is non contiguous from the previous scanline
                        if(i - lastScanline > 1)
                        {
                            gpio_put(Pin::CS, 1);
                            // Write 8 bits at a time
                            spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
                            // Center the graphics on the display
                            RPIoController::SetRegion(widthOffset_, heightOffset_ + i, width_ - widthOffset_, height_ - heightOffset_);
                            // write to lcd ram
                            RPIoController::WriteCmd(0X2C);
                            // Write 16 bits at a time
                            spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

                            gpio_put(Pin::DC, 1);
                            gpio_put(Pin::CS, 0);
                        }

                        // Blit and render the current scanline
                        i8080ArcadeIO_->BlitVRAM(std::span(dst, dstSize), textureWidth_, dstSize, std::span(vf, compressedWidth), MemoryController::frameWidth);
                        spi_write16_blocking(spi1, dst16, textureWidth_);

                        // Update the last scanline index
                        lastScanline = i;
                    }
//                  else
//                      this scanline is the same as its counterpart in the previous frame, no need to render this scanline
                }

                // We are done, move the video frame to the back buffer.
                // This will return the previous back buffer to the memory controller frame pool.
                backBuffer_ = std::move(frame.bitstream);
                return false;
            }
        }, eventData);
    }

    void RPIoController::HandleError(std::string&& errorMsg)
    {
        if (runAsync_ == true)
        {
            meen_hw::MH_LockGuard lg (eventQMutex_);
            eventQ_.emplace_back(EventData{ errorMsg });
        }
        else
        {
            eventQ_.emplace_back(EventData{ errorMsg });
        }
    }

    void RPIoController::HandleLoadComplete()
    {
        // We successfully loaded the rom, transition into gameplay.
        screen_ = Screen::Gameplay;
        ships_ = 0;
        // Only used for gameplay, enable it
        gpio_set_irq_enabled(Pin::K0, GPIO_IRQ_EDGE_FALL  | GPIO_IRQ_EDGE_RISE, true);
    }

    std::tuple<bool, int> RPIoController::GetRomIndex()
    {
        return std::tuple(false, romIndex_);
    }
} // namespace meen_i8080_arcade
