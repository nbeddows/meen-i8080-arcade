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

#include <array>
#include <memory>
#include <mutex>
#include <vector>

#include "Base/Base.h"
#include "Controller/IController.h"

namespace SpaceInvaders
{
	/** Custom memory controller.

		A custom memory controller targetting the Space Invaders arcade ROM.
	*/
	class MemoryController final : public MachEmu::IController
	{
        public:
            /** Video ram wrapper
            
                Individual frame bytes that will be housed in a frame pool.

                @remark     Wrap unique pointer so it is more compatible with C based API's.
            */
            struct VideoFrame
            {
                /** Video ram width
                
                    The width of the compressed video ram in bytes.
                */
                static constexpr int width = 32;

                /** Video ram width

                    The width of the compressed video ram in bytes.
                */
                static constexpr int height = 224;

                /** Video ram size
                
                    The size in bytes.
                */
                static constexpr int size = width * height;
                
                /** Video ram

                    The bytes for each frame that will be copied out of memory
                    when interrupt service routine ISR::Two is fired.
                */
                std::unique_ptr<std::array<uint8_t, size>> vram;
            };

        private:
            /** Memory size

                The size in bytes of the memory.
            */
            //cppcheck-suppress unusedStructMember
            size_t memorySize_{ 1 << 16 };

            /** Memory buffer

                The memory bytes that the cpu will read from and write to.
            */
            std::unique_ptr<uint8_t[]> memory_;
            
            /** Frame pool mutex
            
                Mutual exclusion between the main thread and the machine thread for frame pool access.
            
                @remark     marked as mutable so GetVideoFrame can remain const
            */

            mutable std::mutex frameMutex_;

            /** Frame pool
            
                A pool of video frames that can be used to copy the current vram into.

                @remark     marked as mutable so GetVideoFrame can remain const
            */
            mutable std::vector<VideoFrame> framePool_;

        public:
            /** Constructor

                Create a memory controller that can handle the memory requirements
                of Space Invaders. Space Invaders runs on an Intel8080 with 64k
                of memory therefore the memory controller will be of this size.
            
                @param      framePoolSize       The amount frames to allocate, each frame is 7168 bytes in length.

                @remark     default frame pool size is 1.

                @see framePool_
            */
            explicit MemoryController(int framePoolSize = 1);

            ~MemoryController() = default;

            /** Get a copy of the current video ram

                The VideoFrame containing the current video ram is taken from a finite frame pool.

                @return         The current video ram as a unique_ptr wrapped in a VideoFrame.

                @remark         The VideoFrame MUST be returned to the memory controller.

                @see            ReturnVideoFrame.
            */
            VideoFrame GetVideoFrame() const;

            /** Return the frame to the frame pool
            
                @param      frame   the frame to be returned.

                @remark     Not returning the frame (in at least real-time) will cause frame drops.
            */
            void ReturnVideoFrame(VideoFrame&& frame);

            /** Load ROM file

                Loads the specified rom file and the given memory address offset.

                Space Invaders rom files have the following ROM layout:

                    invaders-h 0000-07FF
                    invaders-g 0800-0FFF
                    invaders-f 1000-17FF
                    invaders-e 1800-1FFF

                @param      romFile     the path to the rom file (on local disk).

                @param      offset      the memory offset at which to load the rom.
            */
            void Load(const char* romFile, uint16_t offset);

            /** Memory size

                @return     the size of the memory, in this case 64k.
            */
            size_t Size() const;

            /** Read from controller

                Reads 8 bits of data from the specifed 16 bit memory address.

                @see IController::Read for further details.
            */
            uint8_t Read(uint16_t address) override final;

            /** Write to controller

                Write 8 bits of data to the specifed 16 bit memory address.

                @see IController::Write for further details.
            */
            void Write(uint16_t address, uint8_t value) override final;

            /** Service memory interrupts

                Memory interrupts are never generated.

                The function will always return ISR::NoInterrupt.
            */
            MachEmu::ISR ServiceInterrupts(uint64_t currTime, uint64_t cycles) override final;
        };
} // namespace SpaceInvaders

#endif // MEMORY_CONTROLLER_H