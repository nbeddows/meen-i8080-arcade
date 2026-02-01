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
#include "meen_i8080_arcade/io_controllables/PicoIO.h"
#include "meen_i8080_arcade/io_controllables/PicoIOAudioOut.pio.h"

namespace meen_i8080_arcade
{
    bool PicoIO::buttonPress_[Pin::MAX] = {};
    bool PicoIO::prevEdgeFall_[Pin::MAX] = {};
    bool PicoIO::prevEdgeRise_[Pin::MAX] = {};

    PicoIO::~PicoIO()
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

    void PicoIO::WriteCmd(uint8_t cmd)
    {
        gpio_put(Pin::DC, 0);
        gpio_put(Pin::CS, 0);
        spi_write_blocking(spi1, &cmd, sizeof(uint8_t));
        gpio_put(Pin::CS, 1);
    };

    void PicoIO::WriteParam(uint8_t param)
    {
        gpio_put(Pin::DC, 1);
        gpio_put(Pin::CS, 0);
        spi_write_blocking(spi1, &param, sizeof(uint8_t));
        gpio_put(Pin::CS, 1);
    };

    void PicoIO::SetRegion(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend)
    {
        //set the X coordinates
        PicoIO::WriteCmd(0x2A);
        PicoIO::WriteParam(Xstart >>8);
        PicoIO::WriteParam(Xstart & 0xff);
        PicoIO::WriteParam((Xend - 1) >> 8);
        PicoIO::WriteParam((Xend-1) & 0xFF);

        //set the Y coordinates
        PicoIO::WriteCmd(0x2B);
        PicoIO::WriteParam(Ystart >>8);
        PicoIO::WriteParam(Ystart & 0xff);
        PicoIO::WriteParam((Yend - 1) >> 8);
        PicoIO::WriteParam((Yend - 1) & 0xff);
    };

    void PicoIO::Init()
    {
        gpio_set_irq_callback([](uint gpio, uint32_t eventMask)
        {
            // Ignore consecutive edge rise/fall on the same pin.
            if ((eventMask & GPIO_IRQ_EDGE_FALL) != 0 && PicoIO::prevEdgeFall_[gpio] == false)
            {
                PicoIO::buttonPress_[gpio] = true;
                // Keep track of the edge fall/rise to prevent spurious falls/rises.
                PicoIO::prevEdgeFall_[gpio] = true;
                PicoIO::prevEdgeRise_[gpio] = false;
            }

            if ((eventMask & GPIO_IRQ_EDGE_RISE) != 0 && PicoIO::prevEdgeRise_[gpio] == false)
            {
                PicoIO::buttonPress_[gpio] = false;
                // keep track of the edge fall/rise to prevent spurious falls/rises.
                PicoIO::prevEdgeFall_[gpio] = false;
                PicoIO::prevEdgeRise_[gpio] = true;
            }
        });

        // Enable the pin interrupts required for the rom select screen (the initial screen state)
        gpio_set_irq_enabled(Pin::K0, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
        gpio_set_irq_enabled(Pin::K1, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
        gpio_set_irq_enabled(Pin::K2, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
        gpio_set_irq_enabled(Pin::K3, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
        irq_set_enabled(IO_IRQ_BANK0, true);
    }

    std::errc PicoIO::ConfigureVideoDevice(int width, int height, int fullscreen)
    {
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
        SetRegion(0, 0, width, height);
        // write to lcd ram
        WriteCmd(0X2C);

        // Write 16 bits at a time
        spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

        gpio_put(Pin::DC, 1);
        gpio_put(Pin::CS, 0);

        // TODO: need to check that the width/height is that of the target panel, return not_supported on failure.
        width_ = width;
        height_ = height;

        return std::errc{};
    }

    std::errc PicoIO::ConfigureAudioDevice(int sampleRate, int channels, int sampleSize)
	{
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

        return std::errc{};
    }

    std::errc PicoIO::ConfigurePeripheralDevice()
    {
        return std::errc{};
    }

    std::errc PicoIO::LoadVideoTextures(int bpp, int textureWidth, int textureHeight)
    {
        // We decompress and write one scanline at a time to lcd ram
        texture_.resize((textureWidth * bpp) / 8); // 8 - bits per pixel
        // Used to center the video ram on the display
        widthOffset_ = (width_ - textureWidth) / 2;
        heightOffset_ = (height_ - textureHeight) / 2;

        // This needs to be done via ClearDisplay
        // Rather than taking a bool, it needs to take a bounding box of the screen to clear
/*
        // Blit the first frame

        gpio_put(Pin::CS, 1);
        // Write 8 bits at a time
        spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        // Center the graphics on the display
        PicoIO::SetRegion(widthOffset_, heightOffset_, width_ - widthOffset_, height_ - heightOffset_);
        // write to lcd ram
        PicoIO::WriteCmd(0X2C);
        // Write 16 bits at a time
        spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

        gpio_put(Pin::DC, 1);
        gpio_put(Pin::CS, 0);

        auto bb = backBuffer_->data();
        auto compressedWidth = textureWidth >> 3;
        auto dst = texture_.data();
        auto dstSize = texture_.size();
        auto dst16 = std::bit_cast<uint16_t*>(dst);

        // loop back buffer, blit each scan line
        for(int i = 0; i < textureHeight_; i++)
        {
            i8080ArcadeIO_->BlitVRAM(std::span(dst, dstSize), textureWidth, dstSize, std::span(bb, compressedWidth), MemoryController::frameWidth);
            spi_write16_blocking(spi1, dst16, textureWidth);
            bb += compressedWidth;
        }
*/

        textureWidth_ = textureWidth;
        textureHeight_ = textureHeight;
        return std::errc{};
    }

    std::errc PicoIO::LoadAudioSamples([[maybe_unused]] int sampleRate, [[maybe_unused]] int channels, [[maybe_unused]] int sampleSize)
    {
#if 0
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
            uint32_t divider = system_clock_frequency * 4 / sampleRate; // avoid arithmetic overflow
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
#endif
        return std::errc{};
    };

    void PicoIO::ScreenTransition(Screen curr, Screen next)
    {
        screen_ = next;

        switch (curr)
		{
			case Screen::Gameplay:
			{
				switch (next)
				{
					case Screen::RomSelect:
                    {
                        // I think needs to be moved??? Into ReadPeripheralDevice??
                        //buttonPress_[Pin::K1] = false;
                        
                        // Not used on rom select, disable it
                        gpio_set_irq_enabled(Pin::K0, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
                        // Abort any current dmac operation
                        dma_channel_abort(0);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                break;
            }
            case Screen::RomSelect:
            {
				switch (next)
				{
					case Screen::Gameplay:
					{
                        /*** In read peripheral device we need to set one player input at the same time that we add a credit in order to move straight to a 1P game */

                        // I think needs to be moved??? Into ReadPeripheralDevice??
                        //buttonPress_[Pin::K0] = false;
                        
                        // Add a credit
                        //ret |= 0x01;
                        // Move straight to a 1P game.
                        // Set the amount of ships (this could be also 4/5/6 if this demo supported setting the ship count)
                        ships_ = 3;
						break;
					}
					default:
					{
						break;
					}
				}
				break;
			}
			default:
			{
				break;
			}
        }
    }

    std::array<uint8_t, 16> PicoIO::Uuid() const
    {
        return{ 0x87, 0x4C, 0xD4, 0x1C, 0xC1, 0xB0, 0x44, 0x86, 0xA2, 0x02, 0xCC, 0xB7, 0x0B, 0xB3, 0x44, 0xBB };
    }

    std::errc PicoIO::RenderAudioFrame(const int32_t* audioFrame, [[maybe_unused]] uint64_t timestamp)
    {
        // The timing won't be precise (but close enough), so we need to possibly wait for the dmac to finish.
        // This is good enough for this application.
        if (dma_channel_is_busy(0) == true)
        {
            dma_channel_wait_for_finish_blocking(0);
        }

        // Start the audio transfer
        dma_channel_transfer_from_buffer_now(0, frame.bitstream->data(), frame.bitstream->size());

        return std::errc{}
    }

    std::errc PicoIO::GetTextureBuffer(uint8_t** dst, int* dstRowBytes) const
    {
        *dst = texture_.data();
        *dstRowBytes = textureWidth;

        return std::errc{};
    }

    std::errc PicoIO::RenderVideoFrame(const uint8_t* videoFrame, [[maybe_unused]] uint64_t timestamp)
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
                    PicoIO::SetRegion(widthOffset_, heightOffset_ + i, width_ - widthOffset_, height_ - heightOffset_);
                    // write to lcd ram
                    PicoIO::WriteCmd(0X2C);
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
//          else
//              this scanline is the same as its counterpart in the previous frame, no need to render this scanline
        }

        // We are done, move the video frame to the back buffer.
        // This will return the previous back buffer to the memory controller frame pool.
        backBuffer_ = std::move(frame.bitstream);

        return std::errc{};
    }

    // Buttons have different meaning depending on what screen we are on - TODO: cahe the current screen so we can use it here
    uint32_t PicoIO::ReadPeripheralDevice()
    {
        int buttons = 0;

        if (screen_ == Screen::Gameplay)
        {
            // if are in screen::gameplay
            if (PicoIO::buttonPress_[Pin::K1] == true)
            {
                PicoIO::buttonPress_[Pin::K1] == false
                buttons = Input::QuitRom;
            }
            else
            {
                if (ships_ > 0)
                {
                    buttons |= (PicoIO::buttonPress_[Pin::K0] * Input::P1Left);
                    buttons |= (PicoIO::buttonPress_[Pin::K3] * Input::P1Right);
                    buttons |= (PicoIO::buttonPress_[Pin::K2] * Input::P1Fire);
                }
                else
                {
                    if (PicoIO::buttonPress_[Pin::K0] == true)
                    {
                        PicoIO::buttonPress_[Pin::K0] = false;
                        // Add a credit and move straight to a one player game
                        buttons |= (Input::Credit | Input::OnePlayer);
                        // Set the amount of ships (this could be also 4/5/6 if this demo supported setting the ship count)
                        ships_ = 3;
                    }
                }
            }
        }
        else
        {
            // Set Inputs for scrolling the rom select screen

            // Check button 1 press to trigger a rom load interrupt
            if (PicoIO::buttonPress_[Pin::K1] == true)
            {
                buttons |= (PicoIO::buttonPress_[Pin::K1] * Input::SelectRom);
                PicoIO::buttonPress_[Pin::K1] = false;
            }

            if(PicoIO::buttonPress_[Pin::K2] == true)
            {
                buttons |= (PicoIO::buttonPress_[Pin::K2] * Input::NextRom);
                PicoIO::buttonPress_[Pin::K2] = false;
            }

            if(PicoIO::buttonPress_[Pin::K3] == true)
            {
                buttons |= (PicoIO::buttonPress_[Pin::K3] * Input::PreviousRom);
                PicoIO::buttonPress_[Pin::K3] = false;
            }
        }

        return buttons;
    }

    std::errc PicoIO::RenderErrorString(const std::string& error)
    {
        printf("%s\n", error.c_str());
        return std::errc{};
    }

    std::errc PicoIO::ClearDisplay(bool clearDisplay)
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
            PicoIO::SetRegion(wo, i /* row start */, width_ - wo, 1/* Cover a height of 1, ie; 1 row*/);
            // Write to lcd ram
            PicoIO::WriteCmd(0X2C);
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

        return std::errc{};
    }
} // namespace meen_i8080_arcade
