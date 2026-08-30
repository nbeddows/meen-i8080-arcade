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

#include "meen_i8080_arcade/RTPJPEGXSEncoder.h"

#include <string>

namespace meen_i8080_arcade
{
    RTPJPEGXSEncoder::RTPJPEGXSEncoder(int width, int height)
    {
        auto err = svt_jpeg_xs_encoder_load_default_parameters(SVT_JPEGXS_API_VER_MAJOR, SVT_JPEGXS_API_VER_MINOR, &encoder_);

        if (err != SvtJxsErrorNone)
        {
            printf("JPEGXS encoder failed to load default params\n");
        }

        encoder_.source_width = width;
        encoder_.source_height = height;
        encoder_.input_bit_depth = 8;
        encoder_.colour_format = COLOUR_FORMAT_PLANAR_YUV420;
        encoder_.bpp_numerator = 3;

        err = svt_jpeg_xs_encoder_init(SVT_JPEGXS_API_VER_MAJOR, SVT_JPEGXS_API_VER_MINOR, &encoder_);

        if (err != SvtJxsErrorNone)
        {
            printf("JPEGXS encoder init fail\n");
        }

        image_.stride[0] = width;
        image_.stride[1] = width / 4;
        image_.stride[2] = width / 4;

        image_.alloc_size[0] = image_.stride[0] * encoder_.source_height;
        image_.alloc_size[1] = image_.stride[1] * encoder_.source_height;
        image_.alloc_size[2] = image_.stride[2] * encoder_.source_height;

        bitstream_.allocation_size = (encoder_.source_width * encoder_.source_height * encoder_.bpp_numerator / encoder_.bpp_denominator + 7) / 8;
    }

    RTPJPEGXSEncoder::~RTPJPEGXSEncoder()
    {
        svt_jpeg_xs_encoder_close(&encoder_);
    }

    int RTPJPEGXSEncoder::GetEncodedFrameSize()
    {
        return bitstream_.allocation_size;
    }

    void RTPJPEGXSEncoder::Encode(uint8_t* dst2, uint8_t* src)
    {
        image_.data_yuv[0] = src;
        image_.data_yuv[1] = src + image_.alloc_size[0];
        image_.data_yuv[2] = src + image_.alloc_size[0] + image_.alloc_size[1];

        /*
            dst should have an addiitonal 16 (rtpHeader + jpegxsHeader) bytes allocated, then start the encode at &dst[15],
            then we can copy out the rtp header to the previous 16 bytes and send the rtp packet directly from the encoded frame
        */
        uint8_t dst[28816];
        uint8_t* ptr = dst;
        bitstream_.buffer = &dst[15];

        svt_jpeg_xs_frame_t encInput{ .image = image_, .bitstream = bitstream_, .user_prv_ctx_ptr = nullptr };

        auto err = svt_jpeg_xs_encoder_send_picture(&encoder_, &encInput, 1 /*blocking*/);
        if (err != SvtJxsErrorNone)
        {
            printf("JPEGXS encoder failed to send frame\n");
        }

        svt_jpeg_xs_frame_t encOutput{};

        err = svt_jpeg_xs_encoder_get_packet(&encoder_, &encOutput, 1 /*blocking*/);
        if (err != SvtJxsErrorNone)
        {
            printf("JPEGXS encoder failed to get packet\n");
        }

        if (encOutput.bitstream.used_size != 28800)
        {
            printf("Volatile used_size!\n");
        }

        //  This gives us 60 packets per frame assuming a constant 28800 bytes per frame
        constexpr int bytesToWrite = 480;

        /*
            NOTES:
            rtpHeader[0]: does not change
            rtpHeader[1]: toggle first bit for eop, remainder does not change
            rtpHeader[2]/[3]: sequence number (BE) - increment once per packet, should start at a random position
            rtpHeader[4]/[5]/[6]/[7]: timestamp (BE) - 90KHz
            rtpHeader[8]/[9]/[10]/[11]: ssrc: random value, does not change
        */
        uint8_t rtpHeader[12]{ 0x02, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0xBB, 0xCC, 0xDD };

        /*
            NOTES:
            jpegxsHeader[0]: first 5 bits unchanged, next 3 bits: first 3 bits of the 5 bit Frame counter (increment once per frame)
            jpegXsHeader[1]: first 2 bits, last 2 bits of 5 bit Frame counter (increment once per frame), next 6 bits, first 6 bits of the SEP counter
            jpegXsHeader[2]: first 5 bits, last 5 bits of the SEP counter), last 3 bits, first 3 bits of the Packet counter.
            jpegXsHeader[3]: last 8 bits of the Packet counter
        */
        uint8_t jpegxsHeader[4]{ 0x01, 0x00, 0x00, 0x00 };

        // We are assuming 60 packets per frame, this is based on a 320 * 240 frame @ 12pp with yuv420 pixel format
        for (int i = 0; i < 60; i++)
        {
            // TODO: modify the rtpHeader as per the notes

            memcpy(ptr, rtpHeader, 12);

            // TODO: modify the jpegxsHeader as per the notes
            
            memcpy (ptr + 12, jpegxsHeader, 4);

            // TODO: deliver the packet payload of 480 + 16 (496) bytes to the udp socket

            // move to the next packet
            ptr += 480;
        }
    }
} // namespace meen_i8080_arcade
