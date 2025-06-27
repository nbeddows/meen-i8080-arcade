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

#include "meen_i8080_arcade/GlyphRenderer.h"
#include "meen/Base.h"
#include "meen/IController.h"
#include "meen_hw/MH_ResourcePool.h"

namespace meen_i8080_arcade
{
    /** Custom memory controller.

        A custom memory controller targetting Space Invaders arcade hardware compatible ROMs.

        The emulated hardware runs on an Intel8080 with 64k of memory therefore the memory
        controller's internal memory size will be set to this. A subsection of this memory
        houses the video ram which is a 1bpp 224 * 256 buffer.
        The controller contains a video frame pool with multiple 1bpp frame buffers of
        a custom size (to allow for rendering of custom widgets beyond the bounds of the vram)
        to facillitate double/triple buffering when required.
        The title and credits are rendered to the top centre of the display.
        Additional metadata rendered to the center bottom of the display from left to right
        includes: current frame rate, current time (24hr), current up time (loops every hour)
        and current physical ram usage (see GetPhysicalMemoryUsage method for further details).
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

        /** The width of the vram

            The 1bpp width in bytes of the vram that resides in memory.
        */
        static constexpr int vramWidth{ 32 };

        /** The height of the vram

            The vram that resides in memory in pixels.
        */
        static constexpr int vramHeight{ 224 };

        /** Constructor

            Create a memory controller that can handle the memory requirements
            of i8080 arcade. This includes a 64k memory buffer along with a video
            ram frame pool for single/double/triple buffered video frames for
            optimised rendering.

            @param      jsonRoms            A vector of pairs with first being the rom name and second being the rom json configuration.
            @param      framePoolSize       The number of frames to allocate, each frame will be MemoryController::frameWidth * MemoryController::frameHeight bytes in length.

            @remark     The default frame pool size is 1.
        */
        MemoryController(const std::vector<std::pair<std::string, std::string>>& jsonRoms, int framePoolSize = 1);

        /** Destructor

            Free the memory controller resources.
        */
        ~MemoryController() = default;

        /** Get a copy of the current video ram

            The VideoFrame containing the current video ram is taken from a finite frame pool.

            @return             The current video ram as a recyclable resource.

            @param  currTime    The cpu clock time in nanoseconds.
        */
        meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr GetGameplayFrame(uint64_t currTime);

        /** Generate the current rom select screen

            The rom select screen lists the rom names defined in the config file that are available to load.

            @param  romIndex    The rom index into the rom names to be rendered that will be highlighted as the currently
                                selected rom.
            @param  currTime    The cpu clock time in nanoseconds.
        */
        meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr GetRomSelectFrame(int romIndex, uint64_t currTime);

        /** Clear all internal memory and frame buffers

            All buffers will be set to 0x00.

            @param    backBuffer    The current off screen buffer to clear.
        */
        void Clear(std::vector<uint8_t>* backBuffer);

        /** Populate the memory controller frame pool

            This function MUST be called before registering this controller with MEEN.

            @param    framePoolSize    The number of frames to allocate in the native pixel format with the specified
                                       resolution.

            @return                    The initial frame to render, this can then be used as the initial frame in a
                                       double buffered system.
        */
        meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr MakeFramePool(int framePoolSize);

        /** IController::Read override

            Reads 8 bits of data from the specifed 16 bit memory address.

            @see IController::Read for further details.
        */
        uint8_t Read(uint16_t address, meen::IController* controller) final;

        /** IController::Write override

            Write 8 bits of data to the specifed 16 bit memory address.

            @see IController::Write for further details.
        */
        void Write(uint16_t address, uint8_t value, meen::IController* controller) final;

        /** IController::GenerateInterrupt override

            Memory interrupts are never generated.

            The function will always return ISR::NoInterrupt.

            @param  currTime        The current CPU run time in nanoseconds.
            @param  cycles          The number of CPU cycles completed.
            @param  ioController    The io controller that has been registered with MEEN.

            @return                 `ISR::NoInterrupt`: the method did not generate an iterrupt.
        */
        meen::ISR GenerateInterrupt(uint64_t currTime, uint64_t cycles, meen::IController* ioController) final;

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

        /** VRAM size

            The total size in bytes.
        */
        static constexpr int vramSize_{ vramWidth * vramHeight };

        /** VRAM memory offset

            The offset into memory at which the beginning of the vram resides.
        */
        static constexpr int vramOffset_{ 0x2400 };

        /** VRAM centre offset

            The offset from the beginning of the frame at which to blit the vram so that it is blitted in the middle of the frame.
        */
        static constexpr int centreOffset_{ (((frameHeight - vramHeight) / 2) * frameWidth) + ((frameWidth - vramWidth) / 2) };

        /** Memory buffer

             The memory bytes that the cpu will read from and write to.
        */
        std::vector<uint8_t> memory_;

        /** VRAM frame pool

            A pool of recyclable video frames.

            See meen_hw/ResourcePool.h for further details.
        */
        meen_hw::MH_ResourcePool<std::vector<uint8_t>> framePool_;

        /** Rom title glyph renderer

            The rom title list will be configured to blit in the centre of the vram frame.

            @sa GlyphRenderer.h
        */
        GlyphRenderer romList_{ frameWidth };

        /** Metadata glyph renderer

            The metadata will be configured to blit at the bottom centre of the frame

            @sa GlyphRenderer.h
        */
        GlyphRenderer metadata_{ frameWidth };

        /** Credits glyph renderer

            The credits will be configured to blit at the top centre of the frame

            @sa GlyphRenderer.h
        */
        GlyphRenderer credits_{ frameWidth };

        /** The last machine clock time

            The time in nanoseconds that is used to compare intervals.
            Used in conjuction with rendering metadata, int this case, every second.
        */
        int64_t lastTime_{};

        /** The current frame rate

            @remark measured in frames per second.
        */
        int fps_{};

        /** The seconds portion of the current up time

            @remark reset to 0 when it reaches 60.
        */
        uint8_t seconds_{};

        /** The minutes portion of the current up time.

            @remark incremented when the seconds parameter reaches 60.
            @remark reset to zero when it reaches 60.
        */
        uint8_t minutes_{};

        /** Periodical metadata update

            Update the metadata every second.

            @param  currTime    The cpu clock time in nanoseconds.
            @param  frame       The arcade surface to blit to.
        */
        void UpdateAndBlitMetadata(uint64_t currTime, std::vector<uint8_t>* frame);

        /** Obtain the current ram usage for the application

            The behaviour of this method varies depending on the platform.

            @return     The total usage in kilobytes.

            @remark Under Windows it will return the total ram usage in the current woring set
            (as opposed to the private working set).
            @remark Under RP2040 it will return the total amount of the heap that has been used.
            @remark Other supported platforms will return the /proc/self/stat resident set size when available.
        */
        static int GetPhysicalMemoryUsage();
    };
} // namespace meen_i8080_arcade

#endif // MEMORYCONTROLLER_H
