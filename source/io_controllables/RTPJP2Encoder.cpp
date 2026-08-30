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

#include <bit>

#include "meen_i8080_arcade/RTPJP2Encoder.h"

static int offset = 0;

namespace meen_i8080_arcade
{
    void RTPJP2Encoder::ErrorCallback(const char* msg, [[maybe_unused]] void* clientData)
    {
        fprintf(stderr, "[ERROR] %s\n", msg);
    }

    void RTPJP2Encoder::WarningCallback(const char* msg, [[maybe_unused]] void* clientData)
    {
        fprintf(stderr, "[WARNING] %s\n", msg);
    }
    
    void RTPJP2Encoder::InfoCallback(const char* msg, [[maybe_unused]] void* clientData)
    {
        fprintf(stdout, "[INFO] %s\n", msg);
    }

    RTPJP2Encoder::RTPJP2Encoder(uint32_t width, uint32_t height)
    {
        //opj_cparameters cparams;
        //opj_set_default_encoder_parameters(&cparams);

        // Create image object
        opj_image_cmptparm_t cmptparm
        {
            .dx = 1,
            .dy = 1,
            .w = width,
            .h = height,
            .prec = 8,
        };

        //cparams.tile_size_on = OPJ_TRUE;
        //cparams.cp_tdx = 320;
        //cparams.cp_tdy = 240;

        //opj_write_tile()

        //cparams.cod_format = 1; // J2K

        //opj_create

        image_ = opj_image_tile_create(1, &cmptparm, OPJ_CLRSPC_GRAY);
        //image_ = opj_image_create(1, &cmptparm, OPJ_CLRSPC_GRAY);

        if (image_ == nullptr)
        {
            printf("Failed to allocate image\n");
            return;
        }

        image_->x1 = width;
        image_->y1 = height;

/*
        // Encoder creation
        encoder_ = opj_create_compress(OPJ_CODEC_J2K); //JP2

        if (encoder_ == nullptr)
        {
            printf("Failed to create the encoder\n");
            return;
        }
*/
        opj_set_error_handler(encoder_, RTPJP2Encoder::ErrorCallback, this);
        opj_set_warning_handler(encoder_, RTPJP2Encoder::WarningCallback, this);
        opj_set_info_handler(encoder_, RTPJP2Encoder::InfoCallback, this);

//        auto err = opj_setup_encoder(encoder_, &cparams, image_);

//        if (err == 0)
//        {
//            printf("Failed to set up the encoder\n");
//            return;
//        }

        //_aligned_free(nullptr);

        stream_ = opj_stream_create(OPJ_J2K_STREAM_CHUNK_SIZE, OPJ_FALSE);

        opj_stream_set_write_function(stream_, [](void* p_buffer, OPJ_SIZE_T p_nb_bytes, void* p_user_data) // user data would be 'this' ... could it just be the udp socket to write to?
        {
            memcpy(&(std::bit_cast<uint8_t*>(p_user_data))[offset], p_buffer, p_nb_bytes);
            offset += p_nb_bytes;

            return p_nb_bytes;
        });

        opj_stream_set_read_function(stream_, [](void* p_buffer, OPJ_SIZE_T p_nb_bytes, void* p_user_data)
        {
            return p_nb_bytes;
        });

        opj_stream_set_seek_function(stream_, [](OPJ_OFF_T p_nb_bytes, void* p_user_data)
        {
            offset = p_nb_bytes;

            return 1;
        });
 
        opj_stream_set_skip_function(stream_, [](OPJ_OFF_T p_nb_bytes, void* p_user_data)
        {
            offset += p_nb_bytes;

            return p_nb_bytes;
        });
    }

    RTPJP2Encoder::~RTPJP2Encoder()
    {
//        opj_destroy_codec(encoder_);
        opj_stream_destroy(stream_);
        opj_image_destroy(image_);
    }

    int RTPJP2Encoder::GetEncodedFrameSize()
    {
        return 0;
    }

    void RTPJP2Encoder::Encode(uint8_t* dst2, uint8_t* src)
    {
        opj_cparameters cparams;
        opj_set_default_encoder_parameters(&cparams);

        //memcpy(image_->comps->data, src, 320 * 240);
        image_->comps->data = std::bit_cast<OPJ_INT32*>(src);
        opj_stream_set_user_data(stream_, dst2, nullptr);


        // Encoder creation
        encoder_ = opj_create_compress(OPJ_CODEC_J2K); //JP2

        //opj_stream_set_user_data_length

        if (encoder_ == nullptr)
        {
            printf("Failed to create the encoder\n");
            return;
        }

        auto err = opj_setup_encoder(encoder_, &cparams, image_);

        if (err == 0)
        {
            printf("Failed to set up the encoder\n");
            return;
        }

        err = opj_start_compress(encoder_, image_, stream_);

        if (err == 0)
        {
            printf("Failed to start compression.\n");
            return;
        }

        //err = opj_write_tile(encoder_, 0, src, 320 * 240, stream_);

        err = opj_encode(encoder_, stream_);

        if (err == 0)
        {
            printf("Failed to encode image.\n");
            return;
        }

        err = opj_end_compress(encoder_, stream_);

        if (err == 0)
        {
            printf("Failed to end compression.\n");
            return;
        }

        offset = 0;

        opj_destroy_codec(encoder_);
    }
} // namespace meen_i8080_arcade
