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
#include <time.h>

#ifdef WIN32
#include <windows.h>
#include <psapi.h>
#elif defined ENABLE_MH_RP2040
#include <malloc.h>
#endif // WIN32

#include "i8080_arcade/MemoryController.h"

namespace i8080_arcade
{
    int MemoryController::GetPhysicalMemoryUsage()
    {
        int memUsage = 0;
#ifdef WIN32
        HANDLE process = GetCurrentProcess();

        if (process != nullptr)
        {
            PROCESS_MEMORY_COUNTERS_EX pmc;

            if (GetProcessMemoryInfo(process, std::bit_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
            {
                memUsage = pmc.WorkingSetSize / 1024.0;
                //memUsage = pmc.PrivateUsage / 1024.0;
            }
        }
#elif defined ENABLE_MH_RP2040
        auto GetTotalHeap = []
        {
            extern char __StackLimit, __bss_end__;
            return &__StackLimit - &__bss_end__;
        };

        struct mallinfo m = mallinfo();
        printf("FREE HEAP: %d\n", GetTotalHeap() - m.uordblks);
#else
        // todo: a de-facto linux implementation
        printf("GetPhysicalMemoryUsage not defined for this platform\n");
#endif
        return memUsage;
    }

    MemoryController::MemoryController(const std::vector<std::pair<std::string, std::string>>& jsonRoms, int framePoolSize)
    {
        std::string txtToBlit;

        for (auto& jsonRom : jsonRoms)
        {
            txtToBlit += " " + jsonRom.first + " \n\n\n";
        }

        std::transform(txtToBlit.begin(), txtToBlit.end(), txtToBlit.begin(), ::toupper);
        txtToBlit.erase(txtToBlit.end() - 3, txtToBlit.end());

        romList_.SetText(std::move(txtToBlit));
        romList_.SetJustification(GlyphRenderer::Justification::Centre);
        // determine the offset at which to blit the text - we want to center the text on the surface
        auto widthOffset = (vramWidth_ - romList_.GetWidth()) / 2;
        auto heightOffset = ((vramHeight_ - romList_.GetHeight()) / 2) * frameWidth;
        romList_.SetAnchorPoint(centreOffset_ + widthOffset + heightOffset);

        char buf[52];// length of the metadata string + 1;
        auto t = time(nullptr);
        auto tm = localtime(&t);

        snprintf(buf, 52, "060 %02d:%02d:%02d %02d:%02d %06d\nFPS   TIME   UTIME MEMORY", tm->tm_hour, tm->tm_min, tm->tm_sec, minutes_, seconds_, GetPhysicalMemoryUsage());
        metadata_.SetText(std::string_view(buf, 51));
        // Position the metadata at the bottom centre of screen
        widthOffset = ((frameWidth - vramWidth_) / 4) - 1;
        heightOffset = ((frameHeight - metadata_.GetHeight()) / 2) * frameWidth;
        metadata_.SetAnchorPoint(widthOffset + heightOffset);

        using namespace std::string_view_literals;
        credits_.SetText("COPYRIGHT:TAITO/MIDWAY\nMEEN I8080 ARCADE"sv);
        credits_.SetJustification(GlyphRenderer::Justification::Centre);
        // Position the credits at the top centre
        widthOffset = vramWidth_ + ((frameWidth - vramWidth_) / 2) + 1;
        heightOffset = ((frameHeight - credits_.GetHeight()) / 2) * frameWidth;
        credits_.SetAnchorPoint(widthOffset + heightOffset);

        memory_.resize(memorySize_, 0);
        framePool_ = meen_hw::MH_ResourcePool<std::vector<uint8_t>>();
        static_assert((frameWidth * frameHeight) >= (vramSize_));

        for(int i = 0; i < framePoolSize; i++)
        {
            auto frame = new std::vector<uint8_t>(frameWidth * frameHeight, 0);

            // This does not take into account bounds checks ... negative values here will result in ub
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

            auto errc = metadata_.Blit(frame->begin(), frame->end(), 0 /* invert no lines */, 0 /* starting from line 0 */); // maybe todo: add line to blit, much like invert - blitLineStart, blitLineCount ... probably a bit complicated for now

            if (errc)
            {
                printf("Failed to blit text: %s\n", errc.message().c_str());
            }

            errc = credits_.Blit(frame->begin(), frame->end(), 0 /* invert no lines */, 0 /* starting from line 0 */); // maybe todo: add line to blit, much like invert - blitLineStart, blitLineCount ... probably a bit complicated for now

            if (errc)
            {
                printf("Failed to blit text: %s\n", errc.message().c_str());
            }

            framePool_.AddResource(frame);
        }
    }

    meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr MemoryController::GetRomSelectFrame(int romIndex, uint64_t currTime)
    {
        auto frame = framePool_.GetResource();

        if (frame != nullptr)
        {
            auto errc = romList_.Blit(frame->begin(), frame->end(), 1 /* invert one line */, romIndex /* starting at the rom index */); // maybe todo: add line to blit, much like invert - blitLineStart, blitLineCount ... probably a bit complicated for now

            if (errc)
            {
                printf("Failed to blit text: %s\n", errc.message().c_str());
            }

            UpdateAndBlitMetadata(currTime, frame.get());
        }

        return frame;
    }
    
    meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr MemoryController::GetGameplayFrame(uint64_t currTime)
    {
        auto frame = framePool_.GetResource();

        if(frame != nullptr)
        {
            if (vramWidth_ == frameWidth)
            {
                std::ranges::copy_n(memory_.begin() + vramOffset_, vramWidth_ * vramHeight_, frame->begin() + centreOffset_);
            }
            else
            {
                auto it = frame->begin() + centreOffset_;

                for (auto vram = memory_.cbegin() + vramOffset_; vram < memory_.cbegin() + vramOffset_ + vramSize_; std::advance(vram, vramWidth_), std::advance(it, frameWidth))
                {
                    std::ranges::copy_n(vram, vramWidth_, it);
                }
            }

            UpdateAndBlitMetadata(currTime, frame.get());
        }

        return frame;
    }

    void MemoryController::UpdateAndBlitMetadata(uint64_t currTime, std::vector<uint8_t>* frame)
    {
        // Update the metadata every second
        if (currTime - lastTime_ >= 1000000000)
        {
            char buf[26];// length of the metadata string + 1;
            auto t = time(nullptr);
            auto tm = localtime(&t);

            ++seconds_;

            if (seconds_ == 60)
            {
                minutes_ = ++minutes_ % 60;
                seconds_ = 0;
            }

            snprintf(buf, 26, "%03d %02d:%02d:%02d %02d:%02d %06d", fps_, tm->tm_hour, tm->tm_min, tm->tm_sec, minutes_, seconds_, GetPhysicalMemoryUsage());
            metadata_.Update(std::string_view(buf, 25), 0);
            auto errc = metadata_.Blit(frame->begin(), frame->end(), 0 /* invert no lines */, 0 /* starting from line 0 */); // maybe todo: add line to blit, much like invert - blitLineStart, blitLineCount ... probably a bit complicated for now

            if (errc)
            {
                printf("Failed to blit text: %s\n", errc.message().c_str());
            }

            lastTime_ = currTime;
            fps_ = 0;
        }

        ++fps_;
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
