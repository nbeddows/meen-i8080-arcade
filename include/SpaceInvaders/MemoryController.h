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

#ifndef MEMORY_CONTROLLER_H
#define MEMORY_CONTROLLER_H

#include <memory>

#include "Base/Base.h"
#include "Controller/IController.h"

namespace SpaceInvaders
{
	/** Custom memory controller.

		A custom memory controller targetting the Space Invaders arcade ROM.
	*/
	class MemoryController final : public MachEmu::IController
	{
        private:
            /** Memory size.

                The size in bytes of the memory.
            */
            //cppcheck-suppress unusedStructMember
            size_t memorySize_{ 1 << 16 };

            /** Memory buffer.

                The memory bytes that the cpu will read from and write to.
            */
            std::unique_ptr<uint8_t[]> memory_;

        public:
            /** Constructor.

                Create a memory controller that can handle the memory requirements
                of Space Invaders. Space Invaders runs on an Intel8080 with 64k
                of memory therefore the memory controller will be of this size.
            */
            MemoryController();

            ~MemoryController() = default;

            /** Screen width.

                Space Invaders has a width of 224 @ 1bpp.

                NOTE: this differs from the vram width which is 256.
                    (It is written to vram with a 90 degree rotation.)
            */
            constexpr uint16_t GetScreenWidth() const { return 224; }
            
            /** Screen height.

                Space Invaders has a height of 256 @ 1bpp.

                NOTE: this differs from the vram height which is 224.
                    (It is written to vram with a 90 degree rotation.)
            */
            constexpr uint16_t GetScreenHeight() const { return 256; }

            /** The size of the video ram.

                Space Invaders has a constant size of 7168
            */
            static constexpr uint16_t GetVramLength() { return 7168; }

            /** Video ram.

                This is a new allocation with a copy of the video ram.

                @return		unique_ptr		The video ram.

                NOTE: The length of the video ram returned is given by:

                    GetVRAMLength()

                NOTE: This isn't the best way to do this, one should use a resource pool
                    to avoid the unnecessary allocations.
            */
            std::unique_ptr<uint8_t[]> GetVram() const;

            /** Load ROM file.

                Loads the specified rom file and the given memory address offset.

                Space Invaders rom files have the following ROM layout:

                    invaders-h 0000-07FF
                    invaders-g 0800-0FFF
                    invaders-f 1000-17FF
                    invaders-e 1800-1FFF
            */
            void Load(const char* romFile, uint16_t offset);

            /** Memory size.

                Returns the size of the memory, in our example it will be 64k (2^addressBusSize(16))
            */
            size_t Size() const;

            /** Read from controller.

                Reads 8 bits of data from the specifed 16 bit memory address.

                @see IController::Read for further details.
            */
            uint8_t Read(uint16_t address) override final;

            /** Write to controller.

                Write 8 bits of data to the specifed 16 bit memory address.

                @see IController::Write for further details.
            */
            void Write(uint16_t address, uint8_t value) override final;

            /** Service memory interrupts.

                Memory interrupts are never generated.

                The function will always return ISR::NoInterrupt.
            */
            MachEmu::ISR ServiceInterrupts(uint64_t currTime, uint64_t cycles) override final;
        };
} // namespace SpaceInvaders

#endif // MEMORY_CONTROLLER_H