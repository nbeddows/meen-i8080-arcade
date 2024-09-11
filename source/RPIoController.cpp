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
#include <malloc.h>

#include "i8080_arcade/RPIoController.h"

extern char __StackLimit, __bss_end__;

namespace i8080_arcade
{
    RPIoController::RPIoController(const std::shared_ptr<MemoryController>& memoryController, const JsonVariant& audioHardware, const JsonVariant& videoHardware)
        : memoryController_{ memoryController }
    {
        // todo: cap the width and height to the max resolution of the output device (st7789vw).
        // for the time being we will ignore these options and just use the native width and height as pulled from the i8080ArcadeIO_ (we wont be doing any scaling)
        //width_ = videoHardware["width"].as<int>();
        //height = videoHardware["height"].as<int>();

        i8080ArcadeIO_ = meen_hw::MakeI8080ArcadeIO();

        if(i8080ArcadeIO_ == nullptr)
        {
            //throw std::runtime_error("Failed to create i8080 arcade hardware");
        }

        queue_init(&videoFrameQueue_, sizeof(VideoFrameWrapper), 1);
    }

    RPIoController::~RPIoController()
    {
        queue_free(&videoFrameQueue_);
    }

    //void SDLIoController::LoadVideoTextures(const JsonVariant& videoTextures)
    //{
    //    i8080ArcadeIO_->SetOptions(videoTextures.dump().c_str());
    //    texture_ = (i8080ArcadeIO_->GetVRAMWidth() * i8080ArcadeIO_->GetVRAMHeight() * 16) / 8; // 16 - bpp, 8 - bits per byte

    //    if (texture_ == nullptr)
    //    {
    //        throw std::bad_alloc();
    //    }
    //}

    uint8_t RPIoController::Read(uint16_t port)
    {
        uint8_t ret = 0;

        ret = i8080ArcadeIO_->ReadPort(port);

        if (ret == 0)
        {
            if (port == 1 || port == 2)
            {
                // get button input
            }
        }

        return ret;
    }

    void RPIoController::Write(uint16_t port, uint8_t data)
    {
        [[maybe_unused]] auto audio = i8080ArcadeIO_->WritePort(port, data);

        // audio is not supprted
        //if (audio > 0)
        //{

        //}
    }

    static int count = 0;

    MachEmu::ISR RPIoController::ServiceInterrupts(uint64_t currTime, uint64_t cycles)
    {
        auto isr = MachEmu::ISR::NoInterrupt;

        auto interrupt = i8080ArcadeIO_->GenerateInterrupt(currTime, cycles);

        switch(interrupt)
        {
            case 0:
                //sem_try_acquire_(&sem_);
                break;
            case 1:
                isr = MachEmu::ISR::One;
                break;
            case 2:
            {
                count++;

                if(videoFrameMutex_.try_lock() == true)
                {
                    if(videoFrameWrapper_.videoFrame != nullptr)
                    {
                        printf("Video frame dropped, renderer too slow\n");
                        // explicitly null the video frame so it is returned to the memory frame pool.
                        videoFrameWrapper_.videoFrame = nullptr;
                    }

                    videoFrameWrapper_.videoFrame = memoryController_->GetVideoFrame();
                    auto added = queue_try_add(&videoFrameQueue_, static_cast<void*>(&videoFrameWrapper_));

                    if(added == false)
                    {
                        printf("Video frame queue full, frame dropped\n");
                        // explicitly null the video frame so it is returned to the memory frame pool.
                        videoFrameWrapper_.videoFrame = nullptr;
                    }

                    videoFrameMutex_.unlock();
                }
                else
                {
                    printf("Video frame dropped, renderer too slow\n");
                }

                isr = MachEmu::ISR::Two;
                break;
            }
            default:
                break;
        }

        if(currTime - currTime_ > 1000000000)
        {
            currTime_ = currTime;
            struct mallinfo m = mallinfo();
            printf("Frames Delivered: %d, Free Heap: %d\n", count, (&__StackLimit - &__bss_end__) - m.uordblks);
            count = 0;
        }

        return isr;
    }

    std::array<uint8_t, 16> RPIoController::Uuid() const
    {
        return{ 0x87, 0x4C, 0xD4, 0x1C, 0xC1, 0xB0, 0x44, 0x86, 0xA2, 0x02, 0xCC, 0xB7, 0x0B, 0xB3, 0x44, 0xBB };
    }

    static int count2 = 0;

    void RPIoController::EventLoop()
    {
        while(1)
        {
            queue_remove_blocking(&videoFrameQueue_, nullptr);
            videoFrameMutex_.lock();

            if (videoFrameWrapper_.videoFrame != nullptr)
            {
                count2++;

                if(count2 == 60)
                {
                    printf("Frames Rendered: 60\n");
                    count2 = 0;
                }

                // Move the frame out of the wrapper. This must be done as it allows the
                // video frame to be returned back to the memory controller, alternatively,
                // one could set videoFrameWrapper->videoFrame to nullptr once it is no longer
                // needed.
                videoFrameWrapper_.videoFrame = nullptr;
            }
            else
            {
                printf("Video frame dropped\n");
            }

            videoFrameMutex_.unlock();
        }
    }
} // namespace i8080_arcade
