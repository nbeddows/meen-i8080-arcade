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

#ifndef RTPH264ENCODER_H
#define RTPH264ENCODER_H

#include <cstdint>
#include <functional>
#include <wels/codec_api.h>

namespace meen_i8080_arcade
{
    // A literal helper to prevent narrowing conversion warning
    static uint8_t operator""_ui8(unsigned long long byte)
    {
        return static_cast<uint8_t>(byte);
    }

    class RTPH264Encoder final
    {
    private:
        ISVCEncoder* encoder_{};
        SSourcePicture picture_{};
        uint16_t sequenceNumber_{};
        static constexpr int payloadSize_{ 255 };
        int aggregateSize_{};
        std::function<int(uint8_t* pkt, int pktLen)> deliverPkt_;

        /*
            The fixed rtp packet with a default header that will be updated as the stream is sent out        
        */
        uint8_t rtpPayload_[payloadSize_]{ /* RTP Header */ 0x80_ui8, 0x70_ui8, 0x00_ui8, 0x00_ui8, 0x00_ui8, 0x00_ui8, 0x00_ui8, 0x00_ui8, 0xAA_ui8, 0xBB_ui8, 0xCC_ui8, 0xDD_ui8 };

    public:
        RTPH264Encoder() = delete;
        RTPH264Encoder(std::function<int(uint8_t* pkt, int pktLen)>&& deliverPkt);
        bool Initialise(int width, int height);
        ~RTPH264Encoder();

        bool Encode(uint8_t* src, int64_t timestamp);
    };
} // namespace meen_i8080_arcade
#endif // RTPH264ENCODER_H
