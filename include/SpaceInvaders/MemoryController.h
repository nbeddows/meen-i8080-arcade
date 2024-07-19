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
#include <filesystem>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <vector>

#include "Base/Base.h"
#include "Controller/IController.h"
#include "meen_hw/MH_ResourcePool.h"

namespace SpaceInvaders
{
	/** Custom memory controller.

		A custom memory controller targetting the Space Invaders arcade ROM.
	*/
	class MemoryController final : public MachEmu::IController
	{
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

            /** VRAM frame pool

                A pool of recyclable video frames.
            */
            meen_hw::MH_ResourcePool<std::array<uint8_t, 7168>> framePool_;

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

            /** Destructor

                Free the memory controller resources.
            */
            ~MemoryController() = default;

            /** Get a copy of the current video ram

                The VideoFrame containing the current video ram is taken from a finite frame pool.

                @return         The current video ram as a recyclable resource.
            */
            meen_hw::MH_ResourcePool<std::array<uint8_t, 7168>>::ResourcePtr GetVideoFrame() const;

            /** Load ROM file

                Loads the specified rom files located at the given path into memory
                at the correct offset.

                @param      romFilePath     The path to the rom files (on local disk).

                @param      files           The rom files to load.
            */
		    void LoadRoms(const std::filesystem::path& romFilePath, const nlohmann::json& files);

            /** Memory size

                @return     The size of the memory, in this case 64k.
            */
            size_t Size() const;

            /** Read from controller

                Reads 8 bits of data from the specifed 16 bit memory address.

                @see IController::Read for further details.
            */
            uint8_t Read(uint16_t address) final;

            /** Write to controller

                Write 8 bits of data to the specifed 16 bit memory address.

                @see IController::Write for further details.
            */
            void Write(uint16_t address, uint8_t value) final;

            /** Service memory interrupts

                Memory interrupts are never generated.

                The function will always return ISR::NoInterrupt.
            */
            MachEmu::ISR ServiceInterrupts(uint64_t currTime, uint64_t cycles) final;

            /**	Uuid

                Unique universal identifier for this controller.

                @return					The uuid as a 16 byte array.
            */
            std::array<uint8_t, 16> Uuid() const final;
        };
} // namespace SpaceInvaders

#endif // MEMORY_CONTROLLER_H