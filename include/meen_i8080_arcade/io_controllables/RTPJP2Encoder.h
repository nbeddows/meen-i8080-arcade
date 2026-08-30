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

#ifndef RTPJP2ENCODER_H
#define RTPJP2ENCODER_H

#include <cstdint>
#include <openjpeg.h>

namespace meen_i8080_arcade
{
    class RTPJP2Encoder final
    {
    private:
        static void ErrorCallback(const char* msg, void* client_data);
        static void WarningCallback(const char* msg, void* client_data);
        static void InfoCallback(const char* msg, void* client_data);

        opj_codec_t* encoder_{};
        opj_image_t* image_{};
        opj_stream_t* stream_{};

    public:
        RTPJP2Encoder(uint32_t width, uint32_t height);
        ~RTPJP2Encoder();

        int GetEncodedFrameSize();
        void Encode(uint8_t* dst, uint8_t* src);
    };
} // namespace meen_i8080_arcade
#endif // RTPJP2ENCODER_H
