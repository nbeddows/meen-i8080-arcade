/*
Copyright (c) 2021-2024 Nicolas Beddows <nicolas.beddows@gmail.com>

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
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/spi.h>
#include <pico/stdio.h>

#include "i8080_arcade/RPIoController.h"

namespace i8080_arcade
{
    RPIoController::RPIoController(const std::shared_ptr<MemoryController>& memoryController, const JsonVariant& audioHardware, const JsonVariant& videoHardware)
        : memoryController_{ memoryController }
    {
        // the width and height of the lcd panel (only tested with 320x240 panel)
        width_ = videoHardware["width"].as<int>();
        height_ = videoHardware["height"].as<int>();

        i8080ArcadeIO_ = meen_hw::MakeI8080ArcadeIO();

        if(i8080ArcadeIO_ == nullptr)
        {
            //throw std::runtime_error("Failed to create i8080 arcade hardware");
        }

        queue_init(&videoFrameQueue_, sizeof(int), 2);
        queue_init(&freeQueue_, sizeof(int), 2);

        for(int i = 0; i < 2; i++)
        {
            int p = std::bit_cast<int>(&videoFrameWrapper_[i]);
            queue_add_blocking(&freeQueue_, static_cast<void*>(&p));
        }

        static_assert(sizeof(VideoFrameWrapper*) == sizeof(int));

        stdio_init_all();
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
        WriteParam(0x70);     //0x08 bit: off - RGB,  on - BGR

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
    }

    RPIoController::~RPIoController()
    {
        gpio_deinit(Pin::BL);
        gpio_deinit(Pin::CS);
        gpio_deinit(Pin::DC);
        gpio_deinit(Pin::RST);
        spi_deinit(spi1);
        queue_free(&freeQueue_);
        queue_free(&videoFrameQueue_);
    }

    int RPIoController::LoadVideoTextures(const JsonVariant& videoTextures)
    {
        int bpp = 16; // this needs to be updated to 12bpp for performance reasons

        if(videoTextures["bpp"] != nullptr)
        {
            bpp = videoTextures["bpp"].as<int>();

            if(bpp != 16)
            {
                printf("Invalid bits per pixel\n");
                return -1;
            }
        }

        if(videoTextures["orientation"] != nullptr)
        {
            auto orientation = videoTextures["orientation"].as<std::string>();

            if(orientation != "cocktail")
            {
                printf("Invalid orientation, only cocktail supported\n");
                return -1;
            }
        }

        std::string meenConfig;
        serializeJson(videoTextures, meenConfig);

        if(meenConfig.empty() == true)
        {
            printf("Parse error while serializing video settings\n");
            return -1;
        }

        i8080ArcadeIO_->SetOptions(meenConfig.c_str());

        // We decompress and write one scanline at a time to lcd ram
        texture_ = std::make_unique<uint8_t>(i8080ArcadeIO_->GetVRAMWidth() * 2); // 2 - 2 bytes per pixel

        if (texture_ == nullptr)
        {
            printf("Failed to allocate texture\n");
            return -1;
        }

        return 0;
    }

    uint8_t RPIoController::Read(uint16_t port)
    {
        uint8_t ret = 0;

        ret = i8080ArcadeIO_->ReadPort(port);

        if (ret == 0)
        {
            if (port == 1 || port == 2)
            {
                // TODO: get button input
            }
        }

        return ret;
    }

    void RPIoController::Write(uint16_t port, uint8_t data)
    {
        // audio is not supported
        [[maybe_unused]] auto audio = i8080ArcadeIO_->WritePort(port, data);
    }

    MachEmu::ISR RPIoController::ServiceInterrupts(uint64_t currTime, uint64_t cycles)
    {
        auto isr = MachEmu::ISR::NoInterrupt;

        auto interrupt = i8080ArcadeIO_->GenerateInterrupt(currTime, cycles);

        switch(interrupt)
        {
            case 0:
                break;
            case 1:
                isr = MachEmu::ISR::One;
                break;
            case 2:
            {
                VideoFrameWrapper* vfw;
                auto success = queue_try_remove(&freeQueue_, static_cast<void*>(&vfw));

                if (success == true)
                {
                    vfw->videoFrame = memoryController_->GetVideoFrame();

                    if(vfw->videoFrame != nullptr)
                    {
                        int p = std::bit_cast<int>(vfw);
                        success = queue_try_add(&videoFrameQueue_, static_cast<void*>(&p));

                        if(success == false)
                        {
                            printf("Failed to add frame to video queue\n");
                            // explicitly null the video frame so it is returned to the memory frame pool.
                            vfw->videoFrame = nullptr;
                            queue_add_blocking(&freeQueue_, static_cast<void*>(&p));
                        }
                    }
                    else
                    {
                        printf("CORE 1 dropped\n");
                    }
                }
                else
                {
//                    printf("1 Video frame dropped, renderer too slow\n");
                }

                isr = MachEmu::ISR::Two;
                break;
            }
            default:
                break;
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
        WriteCmd(0x2A);
        WriteParam(Xstart >>8);
        WriteParam(Xstart & 0xff);
        WriteParam((Xend - 1) >> 8);
        WriteParam((Xend-1) & 0xFF);

        //set the Y coordinates
        WriteCmd(0x2B);
        WriteParam(Ystart >>8);
        WriteParam(Ystart & 0xff);
        WriteParam((Yend - 1) >> 8);
        WriteParam((Yend - 1) & 0xff);
    };

    void RPIoController::EventLoop()
    {
        auto arcadeWidth = i8080ArcadeIO_->GetVRAMWidth();
        auto arcadeHeight = i8080ArcadeIO_->GetVRAMHeight();
        auto widthOffset = (width_ - arcadeWidth) / 2;
        auto heightOffset = (height_ - arcadeHeight) / 2;
        auto dst = std::bit_cast<uint16_t*>(texture_.get());
        VideoFrameWrapper* vfw = nullptr;

        // Write 8 bits at a time
        spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        // Write to the whole display tp clear
        SetRegion(0, 0, width_, height_);
        // write to lcd ram
        WriteCmd(0X2C);
        // Write 16 bits at a time
        spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

        gpio_put(Pin::DC, 1);
        gpio_put(Pin::CS, 0);

        // Clear the display
        for(int i = 0; i < height_; i++)
        {
             for(int j = 0; j < width_; j++)
             {
                 uint16_t p = 0x0000;
                 spi_write16_blocking(spi1, &p, 1);
             }
        }

        gpio_put(Pin::CS, 1);

        // Write 8 bits at a time
        spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        // Center the graphics on the display
        SetRegion(widthOffset, heightOffset, width_ - widthOffset, height_ - heightOffset);
        // write to lcd ram
        WriteCmd(0X2C);
        // Write 16 bits at a time
        spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

        while(1)
        {
            queue_remove_blocking(&videoFrameQueue_, static_cast<void*>(&vfw));
            auto videoFrame = std::move(vfw->videoFrame);
            // explicitly set to nullptr as there is no requirement on std::move to do this
            vfw->videoFrame = nullptr;
            auto p = std::bit_cast<int>(vfw);
            queue_add_blocking(&freeQueue_, static_cast<void*>(&p));

            if (videoFrame != nullptr)
            {
                auto vf = videoFrame.get()->data();
                int index = 0;

                gpio_put(Pin::DC, 1);
                gpio_put(Pin::CS, 0);

                for(int i = 0; i < arcadeHeight; i++)
                {
                    // Blit a scanline at a time for performance reasons

                    // TODO: This method needs to be optimised, see TODO in meen_hw::BlitVRAM
                    //i8080ArcadeIO_->BlitVRAM(std::span(texture_.get(), 512), 512, std::span(vf, 32));
                    //vf += 32;

                    for(int j = 0; j < arcadeWidth; j += 8)
                    {
                        uint8_t cp = vf[index++];

                        for(int k = 0; k < 8; k++)
                        {
                            dst[j + k] = ((cp >> k) & 0x01) * 0xFFFF;
                        }
                    }

                    spi_write16_blocking(spi1, dst, 256);
                }

                gpio_put(Pin::CS, 1);

                // explicitly set to nullptr so it is immediately returned to the memory controller.
                videoFrame = nullptr;
            }
            else
            {
                printf("Video frame dropped\n");
            }
        }
    }
} // namespace i8080_arcade
