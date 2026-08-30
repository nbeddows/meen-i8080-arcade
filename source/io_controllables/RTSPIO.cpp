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
#include <charconv>
#include <format>

#include "meen_i8080_arcade/RTSPIO.h"

namespace meen_i8080_arcade
{
    RTSPIO::RTSPIO()
    {
#ifdef _WINDOWS
        WSADATA wsaData;

        if (WSAStartup(MAKEWORD(2, 2), &wsaData))
        {
            printf("Failed to initialise WinSock2\n");
        }
        else if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
        {
            printf("Invalid WinSock2 version number");
        }
#endif // _WINDOWS
    }

    RTSPIO::~RTSPIO()
    {
        if (controlSocket_ != INVALID_SOCKET)
        {
            closesocket(controlSocket_);
        }

        if (dataChannel_.socket != INVALID_SOCKET)
        {
            closesocket(dataChannel_.socket);
        }

        if (videoChannel_.socket != INVALID_SOCKET)
        {
            closesocket(videoChannel_.socket);
        }

#ifdef _WINDOWS
        WSACleanup();
#endif
    }

    SOCKET RTSPIO::OpenTCPConnection(const char* port, char* clientIp)
    {
        SOCKET sock = INVALID_SOCKET;
        struct addrinfo* result = nullptr;
        struct addrinfo hints
        {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP
        };

        if (getaddrinfo("0.0.0.0", port, &hints, &result) == 0)
        {
            for (auto ptr = result; ptr != nullptr && sock == INVALID_SOCKET; ptr = ptr->ai_next)
            {
                sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

                if (sock != INVALID_SOCKET)
                {
                    int reuse = 1;
                    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, std::bit_cast<const char*>(&reuse), sizeof(reuse));
         
                    // Put the socket into non-blocking
                    //u_long mode = 1;

                    //if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR)
                    //{
                    //    printf("Failed to set non blocking option on the control channel\n");
                    //    closesocket(sock);
                    //    continue;
                    //}
#ifdef _WINDOWS
                    SOCKADDR recvAddr{};
#else
#error "Not implemented\n"
#endif // _WINDOWS
                    int recvAddrSize = sizeof(recvAddr);

                    if (bind(sock, ptr->ai_addr, ptr->ai_addrlen) == SOCKET_ERROR)
                    {
                        printf("Failed to bind to port %s\n", port);

                        closesocket(sock);
                        sock = INVALID_SOCKET;
                        continue;
                    }

                    if (listen(sock, 1) == SOCKET_ERROR)
                    {
                        printf("Failed to listen\n");

                        closesocket(sock);
                        sock = INVALID_SOCKET;
                        continue;
                    }

                    if ((sock = accept(sock, &recvAddr, &recvAddrSize)) == INVALID_SOCKET)
                    {
                        printf("Failed to accept\n");

                        closesocket(sock);
                        sock = INVALID_SOCKET;
                        continue;
                    }

                    if (clientIp != nullptr)
                    {
                        inet_ntop(AF_INET, &((sockaddr_in*)&recvAddr)->sin_addr, clientIp, INET_ADDRSTRLEN);
                    }

                    printf("RTSP client %s connected\n", clientIp);
                }
           }
        }

        return sock;
    }

    std::errc RTSPIO::HandleRTSPCommand()
    {
        char buffer[512];
        int bufferSize = 512;
        auto len = recv(controlSocket_, buffer, bufferSize - 1, 0);

        if (len > 0)
        {
            buffer[len] = '\0';
            std::string_view bufferSv(buffer, len);

            int cseq = 0;
            auto pos = bufferSv.find("CSeq: ");
            auto [ptr, ec] = std::from_chars(bufferSv.data() + pos + strlen("CSeq: "), bufferSv.data() + len, cseq);
            std::string str;

            if (bufferSv.starts_with("OPTIONS") == true)
            {
                str = std::format("RTSP/1.0 200 OK\r\nCSeq: {}\r\nPublic: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n", cseq);
            }
            else if (bufferSv.starts_with("DESCRIBE") == true)
            {
// This needs to be an SDP file that is read from disk, the name of which comes from the resource of the url - "meen-i8080-arcade.sdp"
                auto sdp = std::format("v=0\r\n\
o=- 2864434397 1 IN IP4 192.168.20.2\r\n\
s=MEEN i8080 arcade\r\n\
i=i8080 arcade emulator\r\n\
c=IN IP4 192.168.20.2\r\n\
t=0 0\r\n\
m=video 8000 RTP/AVP 112\r\n\
a=rtpmap:112 H264/90000\r\n\
a=framerate:60\r\n\
a=fmtp:112 packetization-mode=1\r\n\r\n");
//a=fmtp:112 packetization-mode=1;sprop-parameter-sets=AAAAAWdCwBWMaDwpIB4RCNQAAAABaM48gA==\r\n\r\n");

                str = std::format("RTSP/1.0 200 OK\r\nCSeq: {}\r\nContent-Type: application/sdp\r\nContent-Length: {}\r\n\r\n{}", cseq, sdp.length(), sdp);
            }
            else if (bufferSv.starts_with("SETUP") == true)
            {
                // Allow recalibration if we are not currently playing ... we should also allow playing but that would require a critsec on the videoChannel socket handling
                // so we will disallow such a transition
                if (state_ != State::Playing)
                {
                    pos = bufferSv.find("client_port=") + strlen("client_port=");
                    auto end = bufferSv.find("\r\n", pos);
                    str = std::format("RTSP/1.0 200 OK\r\nCSeq: {}\r\nSession: 47112344\r\nTransport: RTP/AVP;unicast;server_port=8000-8001\r\n\r\n", cseq);
                    auto hyphen = bufferSv.find("-", pos);
                    // buffer[pos] will now point to a c string holing the rtp port number
                    buffer[hyphen] = '\0';
                    // buffer[hyphen + 1] will now point to a c string holding the rtcp port number
                    buffer[end] = '\0';

                    // point to the client rtp port number
                    int rtpPort = 0;
                    auto [ptr, ec] = std::from_chars(&buffer[pos], &buffer[hyphen], rtpPort);

                    if (ec != std::errc())
                    {
                        printf("Failed to parse RTP port\n");
                        return ec;
                    }

                    if (videoChannel_.socket != INVALID_SOCKET)
                    {
                        closesocket(videoChannel_.socket);
                    }

                    // Create the video channel
                    videoChannel_.socket = socket(AF_INET, SOCK_DGRAM, 0);

                    if (videoChannel_.socket == INVALID_SOCKET)
                    {
                        printf("Video channel socket creation failed");
                        return std::errc::not_a_socket;
                    }

                    videoChannel_.sockaddr.sin_family = AF_INET,
                    videoChannel_.sockaddr.sin_port = htons(rtpPort);

                    auto err = inet_pton(AF_INET, clientIp_, &videoChannel_.sockaddr.sin_addr);

                    if (err == 0)
                    {
                        printf("Failed to create video channel address: %s\n", clientIp_);
                        return std::errc::not_a_socket;
                    }
                }
                else
               {
                   printf("Invalid RTSP state transition\n");
                   return std::errc::not_supported;
               }

               state_ = State::Ready;
            }
            else if (bufferSv.starts_with("PLAY") == true)
            {
                if (state_ == State::Init)
                {
                    printf("Invalid RTSP state transition\n");
                    return std::errc::not_supported;
                }

                // Put the control socket into non-blocking since we are now playing
                u_long mode = 1;

                if (ioctlsocket(controlSocket_, FIONBIO, &mode) < SOCKET_ERROR)
                {
                    printf("Failed to set non blocking option on the control channel\n");
                }

                str = std::format("RTSP/1.0 200 OK\r\nCSeq: {}\r\n\r\n", cseq);
                // The rtsp player is now connected, allow audio/video frames to be delivered to it.
                state_ = State::Playing;
            }
            else if (bufferSv.starts_with("TEARDOWN") == true)
            {
                str = std::format("RTSP/1.0 200 OK\r\nCSeq: {}\r\n\r\n", cseq);

                // No matter what our running state is we always destroy the session and revert back to an INIT state
                state_ = State::Init;

                if (controlSocket_ != INVALID_SOCKET)
                {
                    closesocket(controlSocket_);
                    controlSocket_ = INVALID_SOCKET;
                }
            }
            else
            {
                // todo: print this to the server log
                printf("Unknown RTSP command: %s\n", buffer);
            }

            if (str.empty() == false)
            {
                auto bytesSent = send(controlSocket_, str.c_str(), str.length(), 0);
                assert(bytesSent == str.length());
            }
        }
        else
        {
            // Either we have gracefully closed or received some other critical error, shutdown and try to re-establish a connection
            if (len == 0 || WSAGetLastError() != WSAEWOULDBLOCK)
            {
                printf("Closing the RTSP control channel\n");
                std::string_view errMsg{ "The RTSP client has been closed, re-open to continue" };
                // Always write it regardless of whether the dataChannel is active or not ... we ignore the error
                [[maybe_unused]] auto bytes = sendto(dataChannel_.socket, errMsg.data(), errMsg.length(), 0, (sockaddr*)&dataChannel_.sockaddr, sizeof(dataChannel_.sockaddr));
                closesocket(controlSocket_);
                controlSocket_ = INVALID_SOCKET;
                state_ = State::Init;
            }
        }

        return std::errc{};
    }

    std::errc RTSPIO::ConfigureVideoDevice(int width, int height, int fullscreen)
    {
        return std::errc{};
    }

    std::errc RTSPIO::ConfigureAudioDevice(int sampleRate, int channels, int sampleSize)
    {
        return std::errc{};
    }

    std::errc RTSPIO::ConfigurePeripheralDevice()
    {
        // Create the data channel
        dataChannel_.socket = socket(AF_INET, SOCK_DGRAM, 0);

        if (dataChannel_.socket == INVALID_SOCKET)
        {
            printf("Data channel socket creation failed");
            return std::errc::not_a_socket;
        }

        dataChannel_.sockaddr.sin_family = AF_INET;
        dataChannel_.sockaddr.sin_port = htons(11000); // 11000 - this needs to be configured in the config file
        dataChannel_.sockaddr.sin_addr.S_un.S_addr = INADDR_ANY;

        // Put the socket into non-blocking
        u_long mode = 1;

        if (ioctlsocket(dataChannel_.socket, FIONBIO, &mode) == SOCKET_ERROR)
        {
            printf("Failed to set non blocking option on the data channel\n");
            return std::errc::io_error;
        }

        if (bind(dataChannel_.socket, (sockaddr*)&dataChannel_.sockaddr, sizeof(dataChannel_.sockaddr)) == SOCKET_ERROR)
        {
            printf("Failed to bind data channel\n");
            return std::errc::address_in_use;
        }

        return std::errc{};
    }

    std::errc RTSPIO::LoadAudioSamples(int sampleRate, int channels, int sampleSize)
    {
        /* TODO: Set up the audio encoder and audio encoder output buffer */

        return std::errc{};
    }

    // change name to: ConfigureVideoBuffer??
    std::errc RTSPIO::LoadVideoTextures(int bpp, int textureWidth, int textureHeight)
    {
        auto err = RTPH264Encoder_.Initialise(textureWidth, textureHeight);

        if (err == true)
        {
            return std::errc::not_enough_memory;
        }

        // Allocate the planer yuv420 surface
        int len = textureWidth * textureHeight * 1.5;
        yuv420p_ = std::make_unique<uint8_t[]>(len);
        // The chroma will be constant, we will only be updating the luma part of the yuv420p buffer
        memset(yuv420p_.get(), 0x80, len);
        rowBytes_ = textureWidth;
        return std::errc{};
    }

    std::errc RTSPIO::ScreenTransition(Screen curr, Screen next)
    {
        return std::errc{};
    }

    std::array<uint8_t, 16> RTSPIO::Uuid() const
    {
        return { 0xf2, 0xb0, 0x36, 0xa3, 0x6a, 0xd3, 0x49, 0xa7, 0x85, 0xb8, 0xd3, 0x9a, 0x59, 0xe6, 0x45, 0xec };
    }

    std::errc RTSPIO::RenderAudioFrame(const int32_t* audioFrame, uint64_t timestamp)
    {
        return std::errc{};
    }

    // dst should be const uint8_t**
    std::errc RTSPIO::GetTextureBuffer(uint8_t** dst, int* dstRowBytes) const
    {
        *dst = yuv420p_.get();
        *dstRowBytes = rowBytes_;
        return std::errc{};
    }

    std::errc RTSPIO::RenderVideoFrame(const uint8_t* videoFrame, uint64_t timestamp)
    {
        // Ony encode and the deliver the frame if we have an active connection
        if (state_ == State::Playing)
        {
            if (RTPH264Encoder_.Encode(const_cast<uint8_t*>(videoFrame), timestamp) == false)
            {
                // Write to the dataChannel indicating the next frame failed to transmit, restart the rtsp client at the user's discretion.
                std::string_view errMsg{ "Failed to encode and deliver the next video frame" };
                sendto(dataChannel_.socket, errMsg.data(), errMsg.length(), 0, (sockaddr*)&dataChannel_.sockaddr, sizeof(dataChannel_.sockaddr));
                printf("RTPH264Encder->Encode failed\n");
            }
        }
    
        return std::errc{};
    }

    // TODO: This needs to return std::expected
    uint32_t RTSPIO::ReadPeripheralDevice()
    {
        int input = 0;

        // Control socket needs to be set up as non-blocking
        if (controlSocket_ == INVALID_SOCKET)
        {
            assert(state_ == State::Init);
            printf("Waiting for RTSP client connection on port 554\n");
            // Open a connection to the rtsp client, non-blocking
            controlSocket_ = OpenTCPConnection("554", clientIp_);

            if (controlSocket_ == INVALID_SOCKET)
            {
                printf("Failed to create RTSP control channel socket\n");
                return 0;
            }
        }
        //else
        //{
            HandleRTSPCommand();
        //}

        if (dataChannel_.socket != INVALID_SOCKET)
        {
            int inputLen = sizeof(int);
            int addrSize = sizeof(dataChannel_.sockaddr);
            auto bytes = recvfrom(dataChannel_.socket, std::bit_cast<char*>(&input), inputLen, 0, (sockaddr*)&dataChannel_.sockaddr, &addrSize);

            // We expect to get back 4 bytes of data per call
            if (bytes < inputLen)
            {
#ifdef _WINDOWS
                auto err = WSAGetLastError();

                if (err != WSAEWOULDBLOCK /* WSAETIMEDOUT */ || bytes >= 0)
                {
                    printf("Received %d bytes from recvfrom with err %d\n", bytes, err);
                }
#endif
                input = 0;
            }
            else
            {
                
                auto ip = inet_ntoa(dataChannel_.sockaddr.sin_addr);

                // Only accept incoming datagrams from the client ip
                if (strncmp(ip, clientIp_, strlen(clientIp_)) != 0) // todo: clientIp needs to be a config file parameter, currently just using the ip from which the rtsp client connects from
                {
                    input = 0;
                }
                //else
                //{
                //    printf("KB: %x\n", input);
                //}


            }
        }

        return input;
    }

    std::errc RTSPIO::RenderErrorString(const std::string& error)
    {
        std::string_view em{ "Server error: check the server logs for further details\n" };
        [[maybe_unused]] auto bytes = sendto(dataChannel_.socket, em.data(), em.length(), 0, (sockaddr*)&dataChannel_.sockaddr, sizeof(dataChannel_.sockaddr));
        // 2. Write the error to the server log, currently printing it to the console.
        printf("%s\n", error.c_str());
        return std::errc{};
    }

    std::errc RTSPIO::ClearDisplay(bool clearDisplay)
    {
        return std::errc{};
    }
} // namespace meen_i8080_arcade