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

#include <assert.h>
#include <bit>
#include <string>
#include <stdio.h>
#ifdef _WINDOWS
    #include <WinSock2.h>
#endif // _WINDOWS


#include "meen_i8080_arcade/RTPH264Encoder.h"

namespace meen_i8080_arcade
{
    RTPH264Encoder::RTPH264Encoder(std::function<int(uint8_t* pkt, int pktLen)>&& deliverPkt)
        : deliverPkt_{ std::move(deliverPkt) }
    {
        WelsCreateSVCEncoder(&encoder_);
    }

    bool RTPH264Encoder::Initialise(int width, int height)
    {
        if (encoder_ == nullptr)
        {
            return true;
        }

        SEncParamBase param
        {
            .iUsageType = CAMERA_VIDEO_REAL_TIME,
            .iPicWidth = width,
            .iPicHeight = height,
            // The bitrate needs to be a config option
            .iTargetBitrate = 750000,
            .iRCMode = RC_BITRATE_MODE,
            .fMaxFrameRate = 60,
        };

        auto err = encoder_->Initialize(&param);

        if (err != cmResultSuccess)
        {
            printf("Failed to initialise the encoder\n");
            return true;
        }

        int idrInterval = 1;
        err = encoder_->SetOption(ENCODER_OPTION_IDR_INTERVAL, &idrInterval);
        
        if (err != cmResultSuccess)
        {
            printf("Failed to set the IDR interval\n");
            return true;
        }
        
        SProfileInfo profileInfo{ .iLayer = 0, .uiProfileIdc = PRO_BASELINE };
        err = encoder_->SetOption(ENCODER_OPTION_PROFILE, &profileInfo);

        if (err != cmResultSuccess)
        {
            printf("Failed to set the profile\n");
            return true;
        }

        SLevelInfo levelInfo{ .iLayer = 0, .uiLevelIdc = LEVEL_3_0 };
        err = encoder_->SetOption(ENCODER_OPTION_LEVEL, &levelInfo);

        if (err != cmResultSuccess)
        {
            printf("Failed to set the level\n");
            return true;
        }

        picture_.iColorFormat = videoFormatI420;
        picture_.iPicWidth = width;
        picture_.iPicHeight = height;
        picture_.iStride[0] = width;
        picture_.iStride[1] = width >> 1;
        picture_.iStride[2] = width >> 1;

        return false;
    }

    RTPH264Encoder::~RTPH264Encoder()
    {
        if (encoder_ != nullptr)
        {
            encoder_->Uninitialize();
            WelsDestroySVCEncoder(encoder_);
        }
    }

    bool RTPH264Encoder::Encode(uint8_t* src, int64_t ts90Khz)
//    bool RTPH264Encoder::Encode(uint8_t* src, int64_t timestamp)
    {
        SFrameBSInfo frameBsInfo{};
        picture_.pData[0] = src;
        picture_.pData[1] = picture_.pData[0] + (picture_.iPicWidth * picture_.iPicHeight);
        picture_.pData[2] = picture_.pData[1] + (picture_.iPicWidth * picture_.iPicHeight / 4);

        auto err = encoder_->EncodeFrame(&picture_, &frameBsInfo);

        if (err != cmResultSuccess)
        {
            printf("Failed to encode frame\n");
            return false;
        }

        if (frameBsInfo.eFrameType == videoFrameTypeSkip)
        {
            printf("Skip Encoded frame\n");
            // Skipping a frame is not an error but an early exit
            return true;
        }

        // assert(frameBsInfo.iFrameSizeInBytes < 10000);

        if (frameBsInfo.iFrameSizeInBytes > 10000)
        {
            printf("FRAME > 10k bytes\n");
        }

        constexpr int packetSize = payloadSize_ - 12; // 12 - RTP header size

        // TODO: meen should allow for the setting of the timescale, default is nanos, the setting can be done via the json config file,
        //       that way we can set the engine timescale to 90Khz and do away with this ugly division
        //int ts90Khz = timestamp;// / 11111.11; // 11111.11 - nanos to 90Khz

        bool success = true;

        for (int i = 0; i < frameBsInfo.iLayerNum; i++)
        {
            auto buf = frameBsInfo.sLayerInfo[i].pBsBuf;

            for (int j = 0; j < frameBsInfo.sLayerInfo[i].iNalCount; j++)
            {
                auto nal = buf;

                if (aggregateSize_ + frameBsInfo.sLayerInfo[i].pNalLengthInByte[j] <= packetSize)
                {
                    // do stap-a

                    // We are assuming 4 byte annexb start codes
                    assert(nal[0] == 0x00 && nal[1] == 0x00 && nal[2] == 0x00 && nal[3] == 0x01);

                    if (aggregateSize_ == 0)
                    {
                        // The the timestamp in network byte order
                        *(std::bit_cast<uint32_t*>(&rtpPayload_[4])) = htonl(ts90Khz);
                        // Set the stap-a header
                        rtpPayload_[12] = (nal[4] & 0xE0) | 0x18; // nri (naluh & 0x60) needs to be maximum of all nalus carried in this packet, since we are parameter sets only we are assuming they will all the the same ...
                        aggregateSize_++;
                    }

                    // Set the length of the nal unit in network byte order
                    *(std::bit_cast<uint16_t*>(&rtpPayload_[12 + aggregateSize_])) = htons(frameBsInfo.sLayerInfo[i].pNalLengthInByte[j] - 4 /* don't include the annexb start code */);
                    // Copy out the data to the payload
                    memcpy(&rtpPayload_[12 + aggregateSize_ + 2], &nal[4], frameBsInfo.sLayerInfo[i].pNalLengthInByte[j] - 4 /* no annexb start code */);
                    aggregateSize_ += (frameBsInfo.sLayerInfo[i].pNalLengthInByte[j] - 4) + 2;
                }
                else
                {
                    if (aggregateSize_ > 0)
                    {
                        // Close off the stap-a packet and deliver it
                        
                        // This is the last nal in this layer, determine the padding
                        int padding = payloadSize_ - (12 + aggregateSize_);

                        if (padding > 0)
                        {
                            // Turn on the padding bit if we have a remainder
                            rtpPayload_[0] |= 0x20;
                            // memset the padding to 0
                            memset(&rtpPayload_[payloadSize_ - padding], 0, padding);
                            rtpPayload_[payloadSize_ - 1] = padding;
                        }

                        *(std::bit_cast<uint16_t*>(&rtpPayload_[2])) = htons(sequenceNumber_++);

                        // write out the payload
                        if (deliverPkt_(rtpPayload_, payloadSize_) <= 0)
                        {
                            success = false;
                        }

                        // turn off the marker bit
                        // rtpPayload[1] &= ~0x80;

                        // turn off the padding bit
                        rtpPayload_[0] &= ~0x20;

                        // reset for the next packet
                        aggregateSize_ = 0;
                    }

                    // Set the timestamp in network byte order
                    *(std::bit_cast<uint32_t*>(&rtpPayload_[4])) = htonl(ts90Khz);

                    if (aggregateSize_ + frameBsInfo.sLayerInfo[i].pNalLengthInByte[j] <= packetSize)
                    {
                        // do stap-a()

                        // We must have finished delivering a packet, therefore this one is the first

                        // Set the stap-a header
                        rtpPayload_[12] = (nal[4] & 0xE0) | 0x18; // nri (naluh & 0x60) needs to be maximum of all nalus carried in this packet, since we are parameter sets only we are assuming they will all the the same ...
                        // Set the length on the nal unit in network byte order
                        *(std::bit_cast<uint16_t*>(&rtpPayload_[12 + aggregateSize_ + 1])) = htons(frameBsInfo.sLayerInfo[i].pNalLengthInByte[j] - 4 /* don't include the annexb start code */);
                        // Copy out the data to the payload
                        memcpy(&rtpPayload_[12 + aggregateSize_ + 3], &nal[4], frameBsInfo.sLayerInfo[i].pNalLengthInByte[j] - 4 /* no annexb start code */);
                        aggregateSize_ += (frameBsInfo.sLayerInfo[i].pNalLengthInByte[j] - 4) + 3;
                    }
                    else
                    {
                        // We only use this for stap-a
                        assert(aggregateSize_ == 0);

                        // do fu-a()

                        // Starting packet

                        // Set the frag unit indicator
                        rtpPayload_[12] = (nal[4] & 0xE0) | 0x1C;
                        // Set the frag unit header
                        rtpPayload_[13] = 0x80 | (nal[4] & 0x1F);
                        // Set the sequence number
                        *(std::bit_cast<uint16_t*>(&rtpPayload_[2])) = htons(sequenceNumber_++);
                        // Copy out the next fragment of the nal unit
                        memcpy(&rtpPayload_[14], &nal[5] /* 5 - don't include the naluh */, packetSize - 2); // 2: fui, fuh
                        
                        // Send out the fragmented packet
                        if (deliverPkt_(rtpPayload_, payloadSize_) <= 0)
                        {
                            success = false;
                        }

                        nal += (packetSize - 2) + 5; // annex b + naluh

                        // Do the middle packets, turn off the start bit
                        rtpPayload_[13] &= ~0x80;

                        // Middle packets
                        while (nal < buf + (frameBsInfo.sLayerInfo[i].pNalLengthInByte[j] - (packetSize - 2)))
                        {
                            // Set the sequence number
                            *(std::bit_cast<uint16_t*>(&rtpPayload_[2])) = htons(sequenceNumber_++);
                            // Copy out the next fragment of the nal unit
                            memcpy(&rtpPayload_[14], nal, packetSize - 2); // 2: fui, fuh
                            
                            // Send out the fragmented packet
                            if (deliverPkt_(rtpPayload_, payloadSize_) <= 0)
                            {
                                success = false;
                            }

                            nal += (packetSize - 2);
                        }

                        // Determine the remainder of the nal unit left to send
                        int remainderLen = (buf + frameBsInfo.sLayerInfo[i].pNalLengthInByte[j]) - nal;
                        // Determine the padding for the final packet
                        uint8_t padding = (packetSize - 2) - remainderLen;
                        assert(padding < packetSize - 2);

                        if (padding > 0)
                        {
                            // Turn on the padding bit
                            rtpPayload_[0] |= 0x20;
                            // Clear the padding to 0x00 as specified by the rfc
                            memset(&rtpPayload_[payloadSize_ - padding], 0x00, padding);
                            // Set the last byte the number of padding bytes (including this byte)
                            rtpPayload_[payloadSize_ - 1] = padding;
                        }

                        // This is the last packet, turn on the marker bit
                        rtpPayload_[1] |= 0x80;
                        // Turn on the end bit
                        rtpPayload_[13] |= 0x40;
                        // Set the sequence number
                        *(std::bit_cast<uint16_t*>(&rtpPayload_[2])) = htons(sequenceNumber_++);
                        // Copy out the last fragment of the nal unit
                        memcpy(&rtpPayload_[14], nal, remainderLen); // 14: fui, fua, naluh, rtp header
                        
                        // Send out the fragmented packet
                        if (success == true && deliverPkt_(rtpPayload_, payloadSize_) <= 0)
                        {
                            success = false;
                        }

                        // Turn off the padding bit
                        rtpPayload_[0] &= ~0x20;
                        // Turn off the marker bit
                        rtpPayload_[1] &= ~0x80;
                    }
                }

                // Move the buffer to the next nal unit
                buf += frameBsInfo.sLayerInfo[i].pNalLengthInByte[j];
            }
        }

        return success;
    }
} // namespace meen_i8080_arcade
