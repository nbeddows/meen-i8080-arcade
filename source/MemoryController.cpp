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
#include <cstring>

#include "i8080_arcade/MemoryController.h"

#ifdef ENABLE_MH_RP2040
extern uint8_t invadersHStart;
extern uint8_t invadersHEnd;
extern uint8_t invadersGStart;
extern uint8_t invadersGEnd;
extern uint8_t invadersFStart;
extern uint8_t invadersFEnd;
extern uint8_t invadersEStart;
extern uint8_t invadersEEnd;
#else
#include <fstream>
#endif // ENABLE_MH_RP2040

namespace i8080_arcade
{
    MemoryController::MemoryController(int framePoolSize)
        : memory_{ std::make_unique<uint8_t[]>(memorySize_) }
    {
        framePool_ = meen_hw::MH_ResourcePool<std::array<uint8_t, 7168>>();

        for(int i = 0; i < framePoolSize; i++)
        {
            framePool_.AddResource(new std::array<uint8_t, 7168>);
        }

        memset(memory_.get(), 0x00, memorySize_);
    }

    meen_hw::MH_ResourcePool<std::array<uint8_t, 7168>>::ResourcePtr MemoryController::GetVideoFrame() const
    {
        auto frame = framePool_.GetResource();

        if(frame != nullptr)
        {
            std::copy_n(memory_.get() + 0x2400, frame->size(), frame->begin());
        }

        return frame;
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
