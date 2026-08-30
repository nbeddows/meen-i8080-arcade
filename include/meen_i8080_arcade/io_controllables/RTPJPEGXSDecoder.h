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

#ifndef RTPJPEGXSDECODER_H
#define RTPJPEGXSDECODER_H

#include <svt-jpegxs/SvtJpegxsDec.h>

namespace meen_i8080_arcade
{
    class RTPJPEGXSDecoder final
    {
    private:
        svt_jpeg_xs_decoder_api_t decoder_{};
        svt_jpeg_xs_image_buffer_t image_{};
        svt_jpeg_xs_bitstream_buffer_t bitstream_{};

    public:
        RTPJPEGXSDecoder(int width, int height);
        ~RTPJPEGXSDecoder();

        void Decode(uint8_t* dst, uint8_t* src);
    };
} // namespace meen_i8080_arcade
#endif // RTPJPEGXSDECODER_H
