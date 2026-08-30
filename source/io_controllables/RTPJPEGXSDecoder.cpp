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

#include "meen_i8080_arcade/RTPJPEGXSDecoder.h"

#include <stdio.h>

namespace meen_i8080_arcade
{
    RTPJPEGXSDecoder::RTPJPEGXSDecoder(int width, int height)
    {
    
    }

    RTPJPEGXSDecoder::~RTPJPEGXSDecoder()
    {
        svt_jpeg_xs_decoder_close(&decoder_);
    }

    void RTPJPEGXSDecoder::Decode(uint8_t* dst, uint8_t* src)
    {
        bitstream_.buffer = src;

        if (decoder_.private_ptr == nullptr)
        {
            decoder_.verbose = VERBOSE_SYSTEM_INFO;
            decoder_.use_cpu_flags = CPU_FLAGS_ALL;
            decoder_.threads_num = 0;
/*
            decoder_.callback_get_data_available = [](svt_jpeg_xs_decoder_api_t* decoder, void* context)
            {
                svt_jpeg_xs_frame_t decOutput{};

                auto err = svt_jpeg_xs_decoder_get_frame(decoder, &decOutput, 1); // 1 - blocking

                if (err == SvtJxsErrorNone)
                {
                    err = err;
                }
            };
*/
            bitstream_.allocation_size = 28800; // span - src.size()
            bitstream_.used_size = 28800;

            svt_jpeg_xs_image_config_t imageConfig;
            
            auto err = svt_jpeg_xs_decoder_init(SVT_JPEGXS_API_VER_MAJOR, SVT_JPEGXS_API_VER_MINOR, &decoder_, bitstream_.buffer, bitstream_.allocation_size, &imageConfig);

            if (err != SvtJxsErrorNone)
            {
                printf("Failed to init decoder\n");
            }

            image_.stride[0] = imageConfig.width;
            image_.stride[1] = image_.stride[0] / 4;
            image_.stride[2] = image_.stride[0] / 4;

            image_.alloc_size[0] = image_.stride[0] * imageConfig.height * 1;
            image_.alloc_size[1] = image_.stride[1] * imageConfig.height * 1;
            image_.alloc_size[2] = image_.stride[2] * imageConfig.height * 1;
        }

        image_.data_yuv[0] = dst;
        image_.data_yuv[1] = dst + image_.alloc_size[0];
        image_.data_yuv[2] = dst + image_.alloc_size[0] + image_.alloc_size[1];

        svt_jpeg_xs_frame_t decInput{ .image = image_, .bitstream = bitstream_, .user_prv_ctx_ptr = nullptr };

        auto err = svt_jpeg_xs_decoder_send_frame(&decoder_, &decInput, 1 /*blocking*/);

        if (err != SvtJxsErrorNone)
        {
            printf("Failed to send frame\n");
        }

        svt_jpeg_xs_frame_t decOutput{};

        err = svt_jpeg_xs_decoder_get_frame(&decoder_, &decOutput, 1); // 1 - blocking

        if (err != SvtJxsErrorNone)
        {
            src = src;
        }
    }
} // namespace meen_i8080_arcade
