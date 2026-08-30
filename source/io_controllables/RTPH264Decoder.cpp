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

#include <stdio.h>
#include <string>

#include "meen_i8080_arcade/RTPH264Decoder.h"

namespace meen_i8080_arcade
{
    RTPH264Decoder::RTPH264Decoder(int width, int height)
    {
#ifdef ENABLE_X264

#else
        auto err = WelsCreateDecoder(&decoder_);

        if (err != cmResultSuccess)
        {
            printf("Failed to create the decoder\n");
        }

        SDecodingParam decParam = {};
        decParam.uiTargetDqLayer = (uint8_t)-1;  // decode all layers
        //decParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
        //decParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
        
        err = decoder_->Initialize(&decParam);

        if (err != cmResultSuccess)
        {
            printf("Failed to initialise the decoder\n");
        }
#endif // ENABLE_X264
    }

    RTPH264Decoder::~RTPH264Decoder()
    {
#ifdef ENABLE_X264

#else
        if (decoder_ != nullptr)
        {
            decoder_->Uninitialize();
            WelsDestroyDecoder(decoder_);
        }
#endif // ENABLE_X264
    }

    void RTPH264Decoder::Decode(uint8_t* dst, uint8_t* src, int srcLen)
    {
#ifdef ENABLE_X264

#else
        SBufferInfo yuvInfo{};
        uint8_t* yuv[3];
        
        auto err = decoder_->DecodeFrameNoDelay(src, srcLen, yuv, &yuvInfo);

        if (yuvInfo.iBufferStatus == 0)
        {
            printf("NOT READY!!\n");
        }

        if (err != dsErrorFree)
        {
            printf("Error decoding video frame\n");
        }

        int width = yuvInfo.UsrData.sSystemBuffer.iWidth;
        int height = yuvInfo.UsrData.sSystemBuffer.iHeight;
        int strideY = yuvInfo.UsrData.sSystemBuffer.iStride[0];
        int strideUV = yuvInfo.UsrData.sSystemBuffer.iStride[1];

        for (int i = 0; i < height; i++)
        {
            memcpy(dst, yuv[0], width);
            dst += width;
            yuv[0] += strideY;
        }

/*
        for (int i = 0; i < height / 2; i++)
        {
            memcpy(dst, yuv[1], width / 2);
            dst += width / 2;
            yuv[1] += strideUV;
        }

        for (int i = 0; i < height / 2; i++)
        {
            memcpy(dst, yuv[2], width / 2);
            dst += width / 2;
            yuv[2] += strideUV;
        }
*/
#endif // ENABLE_X264
    }
} // namespace meen_i8080_arcade
