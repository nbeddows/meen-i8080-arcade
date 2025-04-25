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

#ifndef MEMORYCONTROLLER_H
#define MEMORYCONTROLLER_H

#include <vector>

#include "i8080_arcade/GlyphRenderer.h"
#include "meen/Base.h"
#include "meen/IController.h"
#include "meen_hw/MH_ResourcePool.h"

namespace i8080_arcade
{
    /** Custom memory controller.

        A custom memory controller targetting Space Invaders arcade hardware compatible ROMs.

        The emulated hardware runs on an Intel8080 with 64k of memory therefore the memory
        controller's internal memory size will be set to this. A subsection of this memory
        houses the video ram which is a 1bpp 224 * 256 buffer.
        The controller contains a video frame pool with multiple 1bpp frame buffers of
        a custom size (to allow for rendering of custom widgets beyond the bounds of the vram)
        to facillitate double/triple buffering when required.
    */
    class MemoryController final : public meen::IController
    {
    public:
        /** The width of each frame pool frame in bytes

            MUST BE DIVISIBLE BY 8

            @remark     Anything less than 36 will yield rendering errors on the border. Since we are compressed on width, we'd need some bitmath to render correctly which has not yet been implemented.
            @remark     Since we are 1bpp, the width in pixels is 320.
            @remark     Note the width is larger than the vram width to allow for addtional custom graphics blitting beyond the vram.

            @todo       In the future this could become a user configurable parameter so that a custom surface size can be declared. It would then most likely involve the requirement
                        of a callback of some sorts in which to render custom graphics to the surface.
        */
        static constexpr int frameWidth{ 40 }; 

        /** The height of each frame pool frame in pixels

            MUST BE DIVISIBLE BY 8

            @remark     Note the height is larger than the vram width to allow for addtional custom graphics blitting beyond the vram.

            @todo       In the future this could become a user configurable parameter so that a custom surface size can be declared. It would then most likely involve the requirement
                        of a callback of some sorts in which to render custom graphics to the surface.
        */
        static constexpr int frameHeight{ 240 };

        /** Constructor

            Create a memory controller that can handle the memory requirements
            of i8080 arcade. This includes a 64k memory buffer along with a video
            ram frame pool for single/double/triple buffered video frames for
            optimised rendering.

            @param      framePoolSize       The amount frames to allocate, each frame will be width * height bytes in length.

            @remark     default frame pool size is 1.

            @see framePool_
        */
        MemoryController(int framePoolSize = 1);

        /** Destructor

            Free the memory controller resources.
        */
        ~MemoryController() = default;

        /** Get a copy of the current video ram

            The VideoFrame containing the current video ram is taken from a finite frame pool.

            @param  screen  The screen to render.

            @return         The current video ram as a recyclable resource.

            @todo           The screen parameter needs to be the Screen enum defined in IIOController.h.
                            Its definition needs to be moved to something like Types.h and the header
                            needs to be included in this file.
        */
        meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr GetVideoFrame(int screen) const;

        /** Clear the memory

            Wipe the all the memory and frame pool frame buffers to 0.
        */
        void Clear();

        /** Read from controller

            Reads 8 bits of data from the specifed 16 bit memory address.

            @see IController::Read for further details.
        */
        uint8_t Read(uint16_t address, meen::IController* controller) final;

        /** Write to controller

            Write 8 bits of data to the specifed 16 bit memory address.

            @see IController::Write for further details.
        */
        void Write(uint16_t address, uint8_t value, meen::IController* controller) final;

        /** Service memory interrupts

            Memory interrupts are never generated.

            The function will always return ISR::NoInterrupt.
        */
        meen::ISR ServiceInterrupts(uint64_t currTime, uint64_t cycles, meen::IController* controller) final;

        /** Uuid

            Unique universal identifier for this controller.

            @return    The uuid as a 16 byte array.
        */
        std::array<uint8_t, 16> Uuid() const final;

    private:
        /** Memory size

            The size in bytes of the memory.
        */
        //cppcheck-suppress unusedStructMember
        static constexpr size_t memorySize_{ 1 << 16 };

        /** The width of the vram

            The 1bpp width in bytes of the vram that resides in memory.
        */
        static constexpr int vramWidth_{ 32 };

        /** The height of the vram
 
            The vram that resides in memory in pixels.
        */
        static constexpr int vramHeight_{ 224 };

        /** VRAM size

            The total size in bytes.
        */
        static constexpr int vramSize_{ vramWidth_ * vramHeight_ };

        /** VRAM memory offset

            The offset into memory at which the beginning of the vram resides.
        */
        static constexpr int vramOffset_{ 0x2400 };

        /** VRAM centre offset

            The offset from the beginning of the frame at which to blit the vram so that it is blitted in the middle of the frame.
        */
        static constexpr int centreOffset_{ (((frameHeight - vramHeight_) / 2) * frameWidth) + ((frameWidth - vramWidth_) / 2) };

        /** Memory buffer

             The memory bytes that the cpu will read from and write to.
        */
        std::vector<uint8_t> memory_;

        /** VRAM frame pool

            A pool of recyclable video frames.

            See meen_hw/ResourcePool.h for further details.
        */
        meen_hw::MH_ResourcePool<std::vector<uint8_t>> framePool_;

        /**
            Glyph renderer

            See GlyphRenderer.h for further details.
        */
        GlyphRenderer glyphRenderer_{ 0 };
    };
} // namespace i8080_arcade

#endif // MEMORYCONTROLLER_H
