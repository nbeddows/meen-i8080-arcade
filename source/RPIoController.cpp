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
#include <cstring>
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
        gpio_deinit(Pin::K0);
        gpio_deinit(Pin::K1);
        gpio_deinit(Pin::K2);
        gpio_deinit(Pin::K3);
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
        texture_ = std::make_unique<uint8_t>((i8080ArcadeIO_->GetVRAMWidth() * bpp) / 8); // 8 - bits per pixel

        if (texture_ == nullptr)
        {
            printf("Failed to allocate texture\n");
            return -1;
        }

        return 0;
    }

    uint8_t RPIoController::Read(uint16_t port)
    {
        uint8_t ret = i8080ArcadeIO_->ReadPort(port);

        if (ret == 0)
        {
            if (port == 1)
            {
                auto buttonPress = [](bool button, bool& lastButton)
                {
                    bool press = false;

                    if ((button ^ lastButton) && button)
                    {
                        press = true;
                    }

                    lastButton = button;
                    return press;
                };


                // Always force single player mode (0x04) (2P mode is not supported)
                ret = 0x08 | 0x04;
                ret |= buttonPress(!gpio_get(Pin::K1), lastK1_) * 0x01; // Credit

                if (ships_ > 0)
                {
                    // We want button repeats during gameplay for player movement
                    ret |= !gpio_get(Pin::K0) * 0x20; // 1P Left
                    ret |= !gpio_get(Pin::K3) * 0x40; // 1P Right
                    ret |= buttonPress(!gpio_get(Pin::K2), lastK2_) * 0x10; // 1P Fire

                    if (ret & 0x01)
                    {
                        //ships_ = 0;
                        // turn off credit
                        ret &= ~0x01;
                        //printf("Quitting mid game\n");
                        // TODO: We are playing a game, we need to quit back to the attraction screen, we need to issue a quit event to pass back to the main
                        // function so we can reset the machine
                    }
                }
                else
                {
                    // When scrolling roms we DONT want button repeats
                    ret |= buttonPress(!gpio_get(Pin::K0), lastK0_) * 0x20; // 1P Left
                    ret |= buttonPress(!gpio_get(Pin::K3), lastK3_) * 0x40; // 1P Right

                    if (ret & 0x01)
                    {
                        //printf("Game start\n");
                        // We are starting a game, set the amount of ships (this could be also 4/5/6 if this demo supported setting the ship count)
                        ships_ = 3;
                    }

                    if (ret & 0x20)
                    {
                        //printf("Move to the previous rom\n");
                        // turn off move left
                        ret &= ~0x20;
                        // TODO: We want move to the previous rom, send previous rom event
                    }

                    if (ret & 0x40)
                    {
                        //printf("Move to the next rom\n");
                        // turn off move right
                        ret &= ~0x40;
                        // TODO: We want move to the next rom, send next rom event
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

    void RPIoController::Write(uint16_t port, uint8_t data)
    {
        // audio output is not supported, we use the audio to help track the state of the gameplay
        auto audio = i8080ArcadeIO_->WritePort(port, data);

        if(port == 3)
        {
            // port 3 bit 4 is extended play, need to increment the ships_ count by one
            if((audio >> 4) & 0x01)
            {
                //printf("Extra ship!\n");
                ++ships_;
            }

            // port 3 bit 2 is player killed, need to reduce the ships_ count by one
            if((audio >> 2) & 0x01)
            {
                //printf("Player killed\n");
                --ships_;
            }
        }
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
                    printf("1 Video frame dropped, renderer too slow\n");
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

    void RPIoController::EventLoop()
    {
        auto arcadeWidth = i8080ArcadeIO_->GetVRAMWidth();
        auto arcadeHeight = i8080ArcadeIO_->GetVRAMHeight();
        auto compressedWidth = arcadeWidth >> 3;
        auto widthOffset = (width_ - arcadeWidth) / 2;
        auto heightOffset = (height_ - arcadeHeight) / 2;
        auto dst = std::bit_cast<uint16_t*>(texture_.get());
        VideoFrameWrapper* vfw = nullptr;
	    meen_hw::MH_ResourcePool<std::array<uint8_t, 7168>>::ResourcePtr backBuffer;

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

        auto setRegion = [wo = widthOffset, ho = heightOffset, w = width_, h = height_](int hIndex)
        {
            gpio_put(Pin::CS, 1);
            // Write 8 bits at a time
            spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
            // Center the graphics on the display
            RPIoController::SetRegion(wo, ho + hIndex, w - wo, h - ho);
            // write to lcd ram
            RPIoController::WriteCmd(0X2C);
            // Write 16 bits at a time
            spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        };

        setRegion(0);

        //auto lastTime = get_absolute_time();
        //int fr = 0;

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
                uint8_t* bb = nullptr;

                if(backBuffer != nullptr)
                {
                    bb = backBuffer.get()->data();
                }

                gpio_put(Pin::DC, 1);
                gpio_put(Pin::CS, 0);

                for(int i = 0/*, lastScanline = 0*/; i < arcadeHeight; i++)
                {
                    // Blit a scanline at a time for performance reasons

                    // Utilise a back buffer so we only render scanlines that have changed
                    if(bb != nullptr)
                    {
                        // Check to see if this scanline has changed compared to its counterpart in the previous frame
                        if(std::memcmp(bb, vf, compressedWidth) != 0)
                        {
                            // Update the region only if the scanline to be rendered is non contiguous from the previous scanline
                            // TODO: this minor optimisation yields rendering errors, requires further investigation if it is needed.
                            //if(i - lastScanline > 1)
                            {
                                setRegion(i);

                                gpio_put(Pin::DC, 1);
                                gpio_put(Pin::CS, 0);
                            }

                            // Blit and render the current scanline
                            i8080ArcadeIO_->BlitVRAM(std::span(texture_.get(), 512), 512, std::span(vf, compressedWidth));
                            spi_write16_blocking(spi1, dst, arcadeWidth);

                            // Update the last scanline index
                            //lastScanline = i;
                        }
//                      else
//                          this scanline is the same as its counterpart in the previous frame, no need to render this scanline

                        // Move to the next scanline in the back buffer
                        bb += compressedWidth;
                    }
                    else
                    {
                        i8080ArcadeIO_->BlitVRAM(std::span(texture_.get(), 512), 512, std::span(vf, compressedWidth));
                        spi_write16_blocking(spi1, dst, arcadeWidth);
                    }

                    // Move to the next scanline in the front buffer
                    vf += compressedWidth;
                }

                gpio_put(Pin::CS, 1);

                // We are done, swap the front and back buffers
                std::swap(backBuffer, videoFrame);

                // Explicitly set to nullptr so it is immediately returned to the memory controller.
                videoFrame = nullptr;

                //fr++;
            }
            else
            {
                printf("Video frame dropped\n");
            }

            //auto now = get_absolute_time();

            //if(absolute_time_diff_us(lastTime, now) >= 1000000)
            //{
            //    lastTime = now;
            //    printf("FR: %d\n", fr);
            //    fr = 0;
            //}
        }
    }
} // namespace i8080_arcade
