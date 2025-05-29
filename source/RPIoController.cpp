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

#include <assert.h>
#include <bitset>
#include <charconv>
#include <cstring>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/spi.h>
#include <pico/stdio.h>

#include "i8080_arcade/MemoryController.h"
#include "i8080_arcade/RPIoController.h"

namespace i8080_arcade
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

        // the width and height of the lcd panel (only tested with 320x240 panel)
        width_ = videoHardware["width"].as<int>();
        height_ = videoHardware["height"].as<int>();

        queue_init(&eventDataQueue_, sizeof(uintptr_t), maxEventData_);
        queue_init(&eventDataFreeQueue_, sizeof(uintptr_t), maxEventData_);

        for(int i = 0; i < maxEventData_; i++)
        {
            auto p = std::bit_cast<uintptr_t>(&eventData_[i]);
            queue_add_blocking(&eventDataFreeQueue_, static_cast<void*>(&p));
        }

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
        queue_free(&eventDataFreeQueue_);
        queue_free(&eventDataQueue_);
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

        auto bb = backBuffer_.get()->data();
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

        auto addSample = [this](const uint8_t* wav, int len)
        {
            if (len < 8)
            {
                return std::errc::invalid_argument;
            }

            // Check the 'RIFF' fourcc
            if (*(std::bit_cast<uint32_t*>(wav)) != 0x46464952)
            {
                return std::errc::protocol_not_supported;
            }

            // The length of our resource is different to what is reported
            if (len != *(std::bit_cast<uint32_t*>(wav + 4)))
            {
                return std::errc::value_too_large;
            }

            // Check for 'WAVE' fourcc
            if (*(std::bit_cast<uint32_t*>(wav + 8)) != 0x45564157)
            {
                return std::errc::protocol_not_supported;
            }

            // Check for 'fmt ' fourcc
            if (*(std::bit_cast<uint32_t*>(wav + 12)) != 0x20746D66)
            {
                return std::errc::protocol_not_supported;
            }

            // Not supporting extended data
            if (*(std::bit_cast<uint32_t*>(wav + 16)) != 16)
            {
                return std::errc::no_protocol_option;
            }

            // Only supporting PCM format
            if (*(std::bit_cast<uint16_t*>(wav + 20)) != 1)
            {
                return std::errc::no_protocol_option;
            }

            auto channels = *(std::bit_cast<uint16_t*>(wav + 22));
            auto sampleRate = *(std::bit_cast<uint32_t*>(wav + 24));
            auto bytesPerSecond = *(std::bit_cast<uint32_t*>(wav + 28));
            auto nBlockAlign = *(std::bit_cast<uint16_t*>(wav + 32));
            auto bitsPerSample = *(std::bit_cast<uint16_t*>(wav + 34));

            // Check for 'data' fourcc
            if (*(std::bit_cast<uint32_t*>(wav + 36)) != 0x61746164)
            {
                return std::errc::protocol_not_supported;
            }

            // The WAV header is 44 bytes, make sure the remaining length is whats reported
            if (*(std::bit_cast<uint32_t*>(wav + 40)) != len - 44)
            {
                return std::errc::value_too_large;
            }

            if (audioSamples_.empty() == false)
            {
                // All samples must be of the same format
                if (channels_ != channels || sampleRate_ != sampleRate || bytesPerSecond_ != bytesPerSecond || nBlockAlign_ != nBlockAlign || bitsPerSample != bitsPerSample)
                {
                    return std::errc::not_supported;
                }
            }
            else
            {
                // The first sample read in sets the expected properties of the remaining samples to be read
                channels_ = channels;
                sampleRate_ = sampleRate;
                bytesPerSecond_ = bytesPerSecond;
                nBlockAlign_ = nBlockAlign;
                bitsPerSample_ = bitsPerSample;
            }

            // Copy the sample data from flash to ram
            audioSamples_.emplace_back(wav + 44, wav + len);
            return std::errc();
        };

        for(const auto& sample : audioSamples["sample"].as<JsonArrayConst>())
        {
            auto resource = sample["bytes"].as<std::string_view>();

            if (resource.starts_with("mem://"))
            {
                resource.remove_prefix(strlen("mem://"));
            }
            else
            {
                if (scheme != "mem://")
                {
                    return std::make_error_code (std::errc::not_supported);
                }
            }

            if (resource.empty() == false)
            {
                uintptr_t value = 0;
                auto [ptr, ec] = std::from_chars (resource.data(), resource.data() + resource.size(), value, 10);

                if (ec != std::errc() || ptr != resource.data() + resource.size())
                {
                    return std::make_error_code(ec);
                }

                auto err = addSample(reinterpret_cast<const uint8_t*>(value), sample["size"].as<int>());

                if (err != std::errc())
                {
                    return std::make_error_code(err);
                }
            }
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
                    EventData* eventData = nullptr;
                    screen_ = Screen::RomSelect;
                    buttonPress_[Pin::K1] = false;

                    // Not used on rom select, displae it
                    gpio_set_irq_enabled(Pin::K0, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);

                    if (runAsync_ == true)
                    {
                        // Spin until the event data free queue size is full; ie all event data variants are returned, todo: ideally we would wait here
                        while(queue_get_level(&eventDataFreeQueue_) < maxEventData_)
                        {
                            // do nothing
                            printf("Waiting for final frame to blit before clearing\n");
                        }
                    }
                    else
                    {
                        // We need to cancel any outstanding events
                        while(queue_try_remove(&eventDataQueue_, static_cast<void*>(&eventData)) == true)
                        {
                            auto videoFrame = std::get_if<meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr>(eventData);

                            if (videoFrame != nullptr)
                            {
                                // Cancel the frame, ie; return it to the memory controller frame pool.
                                *videoFrame = nullptr;
                            }

                            queue_add_blocking(&eventDataFreeQueue_, static_cast<void*>(&eventData));
                        }
                    }

                    // We should now be safe to reset this as video frames can only be added from the thread we are currently on
                    // and the spinlock/cancellation above ensures all remaining frames have been blitted/cancelled
                    // (the HandleEvent thread (if runAsync is true) would now be in a waiting state waiting on the next frame to be added).
                    static_cast<MemoryController*>(memoryController)->Clear(backBuffer_.get());
                    queue_remove_blocking(&eventDataFreeQueue_, static_cast<void*>(&eventData));
                    // Clear the vram portion of the display
                    *eventData = true;
                    queue_add_blocking(&eventDataQueue_, static_cast<void*>(&eventData));
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
        // audio output is not supported, we use the audio to help track the state of the gameplay
        auto audio = i8080ArcadeIO_->WritePort(port, data);

        if(port == 3)
        {
            // port 3 bit 4 is extended play, need to increment the ships_ count by one
            if((audio >> 4) & 0x01)
            {
                ++ships_;
            }

            // port 3 bit 2 is player killed, need to reduce the ships_ count by one
            if((audio >> 2) & 0x01)
            {
                --ships_;
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

    meen::ISR RPIoController::ServiceInterrupts(uint64_t currTime, uint64_t cycles, meen::IController* memoryController)
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
                EventData* eventData = nullptr;
                auto success = queue_try_remove(&eventDataFreeQueue_, static_cast<void*>(&eventData));

                if (screen_ == Screen::Gameplay)
                {
                    isr = meen::ISR::Two;
                }

                if (success == true)
                {
                    switch (screen_)
                    {
                        case Screen::RomSelect:
                            *eventData = static_cast<MemoryController*>(memoryController)->GetRomSelectFrame(romIndex_, currTime);
                            break;
                        case Screen::Gameplay:
                            *eventData = static_cast<MemoryController*>(memoryController)->GetGameplayFrame(currTime);
                            break;
                        default:
                            break;
                    }

                    auto videoFrame = std::get_if<meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr>(eventData);

                    // The variant is in an invalid state or no video frame was generated.
                    if (videoFrame == nullptr || *videoFrame == nullptr)
                    {
                        *eventData = "Failed to get the frame from the memory controller, frame dropped";
                    }

                    success = queue_try_add(&eventDataQueue_, static_cast<void*>(&eventData));
                    assert(success == true);
                }
                else
                {
                    printf("Failed to dispatch video frame, increase the event data pool size\n");
                    // assert(0);
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
        EventData* eventData = nullptr;

        if (runAsync_ == true)
        {
            queue_remove_blocking(&eventDataQueue_, static_cast<void*>(&eventData));
        }
        else
        {
            if (queue_try_remove(&eventDataQueue_, static_cast<void*>(&eventData)) == false)
            {
                return false;
            }
        }

        auto quit = std::visit(overloaded
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
                        for(int j = 0; j < MemoryController::vramWidth << 3; j++)
                        {
                            uint16_t p = 0x0000;
                            spi_write16_blocking(spi1, &p, 1);
                        }
                    }
                }

                return false;
            },
            [this](meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr& videoFrame)
            {
                auto compressedWidth = textureWidth_ >> 3;
                auto dst = texture_.data();
                auto dstSize = texture_.size();
                auto dst16 = std::bit_cast<uint16_t*>(dst);
                auto vf = videoFrame.get()->data();
                uint8_t* bb = backBuffer_.get()->data();

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
                backBuffer_ = std::move(videoFrame);
                return false;
            }
        }, *eventData);

        queue_add_blocking(&eventDataFreeQueue_, static_cast<void*>(&eventData));
        return quit;
    }

    void RPIoController::HandleError(std::string&& errorMsg)
    {
        EventData* eventData = nullptr;

        queue_remove_blocking(&eventDataFreeQueue_, static_cast<void*>(&eventData));

        if (eventData != nullptr)
        {
            *eventData = std::move(errorMsg);
            queue_add_blocking(&eventDataQueue_, static_cast<void*>(&eventData));
        }
        else
        {
            printf("Failed to dispatch error message, increase the event data pool size\n");
            //assert(0);
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
} // namespace i8080_arcade
