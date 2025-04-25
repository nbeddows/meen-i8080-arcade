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

#include "i8080_arcade/MemoryController.h"

namespace i8080_arcade
{
    MemoryController::MemoryController(int framePoolSize)
    {
        memory_.resize(memorySize_, 0);
        framePool_ = meen_hw::MH_ResourcePool<std::vector<uint8_t>>();

        static_assert((frameWidth * frameHeight) >= (vramSize_));

        for(int i = 0; i < framePoolSize; i++)
        {
            framePool_.AddResource(new std::vector<uint8_t>(frameWidth * frameHeight, 0));
        }

        auto setAnchorPoint = [this](bool setAnchorPoint)
        {
            if (setAnchorPoint == true)
            {
                // determine the offset at which to blit the text - we want to center the text on the surface
                int widthOffset = (vramWidth_ - glyphRenderer_.GetWidth()) / 2;
                int heightOffset = ((vramHeight_ - glyphRenderer_.GetHeight()) / 2) * frameWidth;
                glyphRenderer_.SetAnchorPoint(centreOffset_ + widthOffset + heightOffset);
            }
        };

        glyphRenderer_ = GlyphRenderer(frameWidth);
        //glyphRenderer_.SetText("   BALLOON BOMBER   \n\n\n   LUNAR RESCUE   \n\n\n   TAITO   \n\n   SPACE INVADERS II   \n\n\n   MIDWAY   \n\n   SPACE INVADERS II   \n\n\n   SPACE INVADERS   ");
        // todo: need to pass in the rom names from the config file in order to generated the render text
        glyphRenderer_.SetText(" BALLOON BOMBER \n\n\n LUNAR RESCUE \n\n\n TAITO \n\n SPACE INVADERS II \n\n\n MIDWAY \n\n SPACE INVADERS II \n\n\n SPACE INVADERS ");
        glyphRenderer_.SetJustification(GlyphRenderer::Justification::Centre);
        glyphRenderer_.SetFont(GlyphRenderer::Font::I8080ArcadeRegular8x8);
        setAnchorPoint(true);
        //setAnchorPoint(!!glyphRenderer_.Update(">>"));
        //setAnchorPoint(!!glyphRenderer_.Update("<<", 18));
    }

    meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr MemoryController::GetVideoFrame(int screen) const
    {
        auto frame = framePool_.GetResource();

        if(frame != nullptr)
        {
            // Thos dpes not take into account bounds checks ... negative values here will result in ub
            auto blitBorder = [frameBegin = frame->begin()](int x0Start, int x1Start, int width, int y0Start, int y1Start, int height, int lpm, int rpm)
            {
                auto p1 = frameBegin + x1Start;

                for (auto p0 = frameBegin + x0Start; p0 < frameBegin + x0Start + width; std::advance(p0, 1), std::advance(p1, 1))
                {
                    *p0 = *p1 = 0xFF;
                }

                p1 = frameBegin + y1Start;

                for (auto p0 = frameBegin + y0Start; p0 < frameBegin + ((height - 1) * frameWidth); std::advance(p0, frameWidth), std::advance(p1, frameWidth))
                {
                    *p0 = lpm & 0xFF;
                    *p1 = rpm & 0xFF;

                    // This will keep the remaining pixels in the frame buffer
                    // *p0 |= lpm;
                    // *p1 |= rpm;
                }
            };

            // blit a border around the surface
            blitBorder(0, frame->size() - frameWidth, frameWidth, frameWidth, frameWidth * 2 - 1, frameHeight, 0x01, 0x80);
            // blit a border around the vram
            blitBorder(centreOffset_ - frameWidth, centreOffset_ + (vramHeight_ * frameWidth), vramWidth_, centreOffset_ - frameWidth - 1, centreOffset_ + vramWidth_ - frameWidth, vramHeight_ + 10, 0x80, 0x01);

            if (screen == 1 /* Screen::Gameplay */)
            {
                if (vramWidth_ == frameWidth)
                {
                    std::ranges::copy_n(memory_.begin() + vramOffset_, vramSize_, frame->begin());
                }
                else
                {
                    auto it = frame->begin() + centreOffset_;
                    //auto vramStart = memory_.begin() + vramOffset_;

                    for (auto vram = memory_.cbegin() + vramOffset_; vram < memory_.cbegin() + vramOffset_ + vramSize_; std::advance(vram, vramWidth_), std::advance(it, frameWidth))
                    {
                        std::ranges::copy_n(vram, vramWidth_, it);
                    }
                }
            }
            else
            {
                // rom selection
                auto errc = glyphRenderer_.Blit(frame->begin(), frame->end(), 0 /* invert one line */, 16 /* starting at line 16 */); // maybe todo: add line to blit, much like invert - blitLineStart, blitLineCount ... probably a bit complicated for now

                if (errc)
                {
                    printf("Failed to blit text: %s\n", errc.message().c_str());
                }
            }

            // blit additional metadata here
        }

        return frame;
    }

    void MemoryController::Clear()
    {
        std::vector<meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr> frames;
        meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr frame;
        bool empty = false;

        // Clear the memory to 0
        memory_.assign(memory_.size(), 0);

        do
        {
            frame = framePool_.GetResource();

            if (frame == nullptr)
            {
                empty = true;
            }
            else
            {
                // Clear the vram section of the video frames
                for (auto vram = frame->begin() + centreOffset_; vram < frame->cbegin() + centreOffset_ + (vramHeight_ * frameWidth); std::advance(vram, frameWidth))
                {
                    std::ranges::fill(vram, vram + vramWidth_, 0);
                }

                frames.emplace_back(std::move(frame));
            }
        }
        while(empty == false);

        // frames will get returned to the pool once this method returns.
    }

    uint8_t MemoryController::Read(uint16_t addr, [[maybe_unused]] meen::IController* controller)
    {
        return memory_[addr];
    }

    void MemoryController::Write(uint16_t addr, uint8_t data, [[maybe_unused]] meen::IController* controller)
    {
        memory_[addr] = data;
    }

    meen::ISR MemoryController::ServiceInterrupts([[maybe_unused]] uint64_t currTime, [[maybe_unused]] uint64_t cycles, [[maybe_unused]] meen::IController* controller)
    {
        return meen::ISR::NoInterrupt;
    }

    std::array<uint8_t, 16> MemoryController::Uuid() const
    {
        return{ 0x5C, 0x64, 0x7C, 0xCB, 0x71, 0x2E, 0x4A, 0x0B, 0x8A, 0x26, 0x1D, 0xE2, 0x95, 0x44, 0xA1, 0xE9 };
    }
} // namespace i8080_arcade
