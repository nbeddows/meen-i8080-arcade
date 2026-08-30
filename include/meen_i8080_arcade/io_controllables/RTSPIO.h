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

#ifndef RTSPIO_H
#define RTSPIO_H

#include <array>
#include <memory>
#include <system_error>

#ifdef _WINDOWS
#include <Ws2tcpip.h>
#endif // _WINDOWS

#include "meen_i8080_arcade/IOControllerTypes.h"
#include "meen_i8080_arcade/RTPH264Encoder.h"

namespace meen_i8080_arcade
{
    /** Real Time Streaming Protocol

        A controllable based on RFC 2326.
	*/
    class RTSPIO final
    {
    private:
        enum State
        {
            Init,
            Ready,
            Playing
        };

        struct UDPConnection
        {
            SOCKET socket{ INVALID_SOCKET };
            struct sockaddr_in sockaddr;
        };

#ifdef _WINDOWS
        SOCKET controlSocket_{ INVALID_SOCKET };
        UDPConnection dataChannel_;
        UDPConnection videoChannel_;
#else
#error "NOT implemented\n"
#endif // _WINDOWS

        RTPH264Encoder RTPH264Encoder_{ [this](uint8_t* pkt, int pktLen)
        {
            int sent = 0;

            while (pktLen > 0)
            {
                auto bytes = sendto(videoChannel_.socket, std::bit_cast<char*>(pkt), pktLen, 0, (const sockaddr*)&videoChannel_.sockaddr, sizeof(videoChannel_.sockaddr));
                assert(bytes <= pktLen);

                if (bytes <= 0)
                {
                    sent = bytes;
                    break;
                }

                sent += bytes;
                pkt += bytes;
                pktLen -= bytes;
            }

            return sent;
        }};
        std::unique_ptr<uint8_t[]> yuv420p_;
        std::atomic_int state_{ State::Init };
        char clientIp_[INET_ADDRSTRLEN]{};
        int rowBytes_{};
        SOCKET OpenTCPConnection(const char* port, char* clientIp = nullptr);
        std::errc HandleRTSPCommand();
    public:
        /** Default constructor

            Default implementation.
        */
        RTSPIO();

        /** Destructor

            Free the various RTSPIO controllable objects.
        */
        ~RTSPIO();

        /** Video Device setup
			
            Configure the RTSPIO video subsystem in order to render video frames.

            @param    width      The width of the display window.
            @param    height     The height of the display window.
            @param    fullscreen True to run the window in fullscreen, false otherwise.

            @return	             A std::errc indicating success or failure.
        */
        std::errc ConfigureVideoDevice(int width, int height, int fullscreen);

        /** Audio device setup
			
            Configure the RTSPIO audio subsystem in order to render audio frames.

            @param    sampleRate    The output audio device number of samples per second.
            @param    channels      The output audio device number of channels.
            @param    sampleSize    The number of output samples to process.

            @return	  A std::errc indicating success or failure.
        */
        std::errc ConfigureAudioDevice(int sampleRate, int channels, int sampleSize);

        /** Peripheral device setup

            Configure the RTSPIO events subsystem in order to process user input.

            @return    A std::errc indicating success or failure.
        */
        std::errc ConfigurePeripheralDevice();

        /** Uuid

            Unique universal identifier for this controller.

            @return    The uuid as a 16 byte array.
        */
        std::array<uint8_t, 16> Uuid() const;

        /**

        */
        std::errc RenderAudioFrame(const int32_t* audioFrame, uint64_t timestamp);

        /**

        */
        std::errc RenderVideoFrame(const uint8_t* videoFrame, uint64_t timestmap);

        /**

        */
        std::errc RenderErrorString(const std::string& error);

        /**

        */
        std::errc ClearDisplay(bool clearDisplay);

        /**

        */
        uint32_t ReadPeripheralDevice();

        /**

        */
        std::errc GetTextureBuffer(uint8_t** dst, int* dstRowBytes) const;

        /**

        */
        std::errc ScreenTransition(Screen curr, Screen next);

        /**

        */
        std::errc LoadAudioSamples(int sampleRate, int channels, int sampleSize);

        /**

        */
        std::errc LoadVideoTextures(int bpp, int textureWidth, int textureHeight);
    };
} // namespace meen_i8080_arcade

#endif // RTSPIO_H
