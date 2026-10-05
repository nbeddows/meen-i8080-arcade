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

#include "meen_i8080_arcade/BlankIO.h"

namespace meen_i8080_arcade
{
    std::errc BlankIO::ConfigureVideoDevice(int width, int height, int fullscreen)
    {
        return std::errc{};
    }

    std::errc BlankIO::ConfigureAudioDevice(int sampleRate, int channels, int sampleSize)
    {
        return std::errc{};
    }

    std::errc BlankIO::ConfigurePeripheralDevice()
    {
        return std::errc{};
    }

    std::errc BlankIO::LoadAudioSamples(int sampleRate, int channels, int sampleSize)
    {
        return std::errc{};
    }

    std::errc BlankIO::LoadVideoTextures(int bpp, int textureWidth, int textureHeight, int* numScanlines)
    {
        return std::errc{};
    }

    std::errc BlankIO::ScreenTransition(Screen curr, Screen next)
    {
        return std::errc{};
    }

    std::array<uint8_t, 16> BlankIO::Uuid() const
    {
        // DO NOT COPY THIS UUID, ITS IS UNIQUE TO BLANKIO, CREATE A NEW ONE!
        return { 0x61, 0x21, 0x7b, 0x56, 0x7c, 0xb7, 0x4d, 0xef, 0xac, 0xdf, 0x74, 0x54, 0x56, 0x41, 0xb5, 0x8 };
    }

    std::errc BlankIO::RenderAudioFrame(const int32_t* audioFrame, int audioFrameSize, uint64_t timestamp)
    {
        return std::errc{};
    }

    std::errc BlankIO::GetVideoFrameBuffer(uint8_t** dst, int* dstRowBytes, int scanlineStart, int numScanlines) const
    {

    }

    std::errc BlankIO::GetTextureBuffer(uint8_t** dst, int* dstRowBytes) const
    {
        return std::errc{};
    }
    
    std::errc BlankIO::RenderVideoFrame(int scanlineStart, int numScanlines, uint64_t timestamp)
    {
        return std::errc{};
    }

    std::errc BlankIO::DisplayVideoFrame(uint64_t timestamp);
    {
        return std::errc{};
    }

    uint32_t BlankIO::ReadPeripheralDevice()
    {
        return 0;
    }

    std::errc BlankIO::RenderErrorString(const std::string& error)
    {
        return std::errc{};
    }

    std::errc BlankIO::ClearDisplay(BoundingBox&& rect)
    {
        return std::errc{};
    }
} // namespace meen_i8080_arcade