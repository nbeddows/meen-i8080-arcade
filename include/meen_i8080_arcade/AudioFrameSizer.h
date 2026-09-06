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

#ifndef AUDIOFRAMESIZER_H
#define AUDIOFRAMESIZER_H

#include <cstdint>

namespace meen_i8080_arcade
{
    /** Distribute audio samples over the fixed 60 Hz video frames. */
    class AudioFrameSizer final
    {
    public:
        constexpr AudioFrameSizer() = default;

        explicit constexpr AudioFrameSizer(const std::uint32_t sampleRate)
            : sampleRate_{ sampleRate }
        {
        }

        /** Return the number of samples for the next video frame.

            Integer division alone drops the fractional part of the audio
            duration. Carrying the remainder makes the total number of samples
            over any complete second equal to the configured sample rate.
        */
        constexpr std::uint32_t Next() noexcept
        {
            auto frameSize = sampleRate_ / videoFrameRate;
            remainder_ += sampleRate_ % videoFrameRate;

            if (remainder_ >= videoFrameRate)
            {
                ++frameSize;
                remainder_ -= videoFrameRate;
            }

            return frameSize;
        }

        /** Return the largest frame size needed for the resource pool. */
        constexpr std::uint32_t Max() const noexcept
        {
            return sampleRate_ / videoFrameRate + (sampleRate_ % videoFrameRate != 0U ? 1U : 0U);
        }

    private:
        static constexpr std::uint32_t videoFrameRate = 60U;

        std::uint32_t sampleRate_{};
        std::uint32_t remainder_{};
    };
} // namespace meen_i8080_arcade

#endif // AUDIOFRAMESIZER_H
