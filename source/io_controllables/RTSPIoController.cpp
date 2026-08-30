/*
Copyright (c) 2021-2025 Nicolas Beddows <nicolas.beddows@gmail.com>

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

#include <charconv>
#include <format>
#include <string>

#include "meen_hw/MH_Factory.h"
#include "meen_i8080_arcade/MemoryController.h"
#include "meen_i8080_arcade/RTSPIoController.h"

namespace meen_i8080_arcade
{
    RTSPIoController::RTSPIoController(int romCount, const JsonVariantConst audioHardware, const JsonVariantConst videoHardware)
        : romCount_{ romCount }
		, romIndex_{ romCount - 1 }
    {
        // todo: we need to fail if the runAsync option is not set, only supporting async

        printf("MEEN HW Version: %s\n", meen_hw::Version());

        i8080ArcadeIO_ = meen_hw::MakeI8080ArcadeIO();

        if (i8080ArcadeIO_ == nullptr)
        {
            printf("Failed to create i8080 arcade hardware");
        }

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

        // Create the data channel
        dataChannel_.socket = socket(AF_INET, SOCK_DGRAM, 0);

        if (dataChannel_.socket == INVALID_SOCKET)
        {
            printf("Data channel socket creation failed");
        }

        dataChannel_.sockaddr.sin_family = AF_INET;
        dataChannel_.sockaddr.sin_port = htons(11000); // 11000 - this needs to be configured in the config file
        dataChannel_.sockaddr.sin_addr.S_un.S_addr = INADDR_ANY;

        // Put the socket into non-blocking
        u_long mode = 1;

        if (ioctlsocket(dataChannel_.socket, FIONBIO, &mode) < SOCKET_ERROR)
        {
            printf("Failed to set non blocking option on the control channel\n");
        }

        if (bind(dataChannel_.socket, (sockaddr*)&dataChannel_.sockaddr, sizeof(dataChannel_.sockaddr)) < SOCKET_ERROR)
        {
            printf("Failed to bind data channel\n");
        }

#if 0
        // Get the host ip
        char hostname[256];
        
        if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR)
        {
            printf("gethostname failed\n");
        }

        struct hostent* hostInfo = gethostbyname(hostname);

        if (hostInfo == nullptr)
        {
            printf("gethostbyname failed\n");
        }
        else
        {
            for (auto i = 0; hostInfo->h_addr_list[i] != nullptr; i++)
            {
                struct in_addr addr;
                memcpy(&addr, hostInfo->h_addr_list[i], hostInfo->h_length);
                const char* ip = inet_ntoa(addr);
                printf("IP: %s\n", ip);
            }
        }
#endif
        /* print out basic instructions */
    }

    RTSPIoController::~RTSPIoController()
    {
        if (controlSocket_ != INVALID_SOCKET)
        {
            closesocket(controlSocket_);
        }

#ifdef USE_TCP_FOR_DATA
        if (dataChannel_ != INVALID_SOCKET)
        {
            closesocket(dataChannel_);
        }
#else
        if (dataChannel_.socket != INVALID_SOCKET)
        {
            closesocket(dataChannel_.socket);
        }
#endif // USE_TC_FOR_DATA
        if (videoChannel_.socket != INVALID_SOCKET)
        {
            closesocket(videoChannel_.socket);
        }

#ifdef _WINDOWS
        WSACleanup();
#endif
    }

    SOCKET RTSPIoController::OpenTCPConnection(const char* port, char* clientIp, int timeout)
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
#ifdef _WINDOWS
                    SOCKADDR recvAddr{};
#else
#error "Not implemented\n"
#endif // _WINDOWS
                    int recvAddrSize = sizeof(recvAddr);

                    if (bind(sock, ptr->ai_addr, ptr->ai_addrlen) < 0)
                    {
                        printf("Failed to bind to port %s\n", port);

                        closesocket(sock);
                        sock = INVALID_SOCKET;
                        continue;
                    }

                    if (listen(sock, 1) < 0)
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

    std::error_code RTSPIoController::LoadVideoTextures(const JsonVariantConst videoTextures, int textureWidth, int textureHeight)
    {
        char meenConfig[512]{};
        serializeJson(videoTextures, meenConfig);

        if (strlen(meenConfig) == 0)
        {
            return std::make_error_code(std::errc::io_error);
        }

        if (videoTextures["bpp"] == nullptr || videoTextures["bpp"] != 8)
        {
            return std::make_error_code(std::errc::not_supported);
        }

        auto err = i8080ArcadeIO_->SetOptions(meenConfig);

        if (err)
        {
            return err;
        }

        // swap width/height based on orientation
        if (videoTextures["orientation"].as<std::string_view>() == "upright")
        {
            textureWidth ^= textureHeight ^= textureWidth ^= textureHeight;
        }

        RTPH264Encoder_ = std::make_unique<RTPH264Encoder>(textureWidth, textureHeight, [this](uint8_t* pkt, int pktLen)
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
        });

        if (RTPH264Encoder_ == nullptr)
        {
            return std::make_error_code(std::errc::not_enough_memory);
        }

        // Allocate the planer yuv420 surface
        int len = textureWidth * textureHeight * 1.5;
        yuv420p_ = std::make_unique<uint8_t[]>(len);
        // The chroma will be constant, we will only be updating the luma part of the yuv420p buffer
        memset(yuv420p_.get(), 0x80, len);
        textureWidth_ = textureWidth;
        textureHeight_ = textureHeight;

        return std::error_code{};
    }

    std::error_code RTSPIoController::LoadAudioSamples([[maybe_unused]] const JsonVariantConst audioSamples)
    {
        return std::error_code{};
    };

    uint8_t RTSPIoController::Read(uint16_t port, [[maybe_unused]] meen::IController* memoryController)
    {
        auto ret = i8080ArcadeIO_->ReadPort(port);

        if (ret == 0)
        {
            if (port == 1 || port == 2)
            {
                if (1)//(keyboard_ & Key::Escape) == false)
                {
                    // perform action on the keys
                }
                else
                {
                    // Wait until all outstanding events have been handled before attempting to clear the memory controller.

                    // This uses a condition variable
                    //std::unique_lock<std::mutex> lg(eventQMutex_);

                    // This needs to wait on the memory controller resource pool to return to maximum capacity
                    //static_cast<MemoryController*>(memoryController)->
                    
                    // need to handle spurious wake up .... do this in a loop, we need to store the frame pool size, it needs to be passed in via the config file
                    //cv_.wait(lg, [memoryController] { return static_cast<MemoryController*>(memoryController)->FramePoolSize() == framePoolSize_; });

                    // This uses std::atomic_flag (not quite right, hence using std:condition_variable)
                    //eventDataCv_.wait(false);
                    //eventDataCv_.clear();

                    // Clear the memory controller ram and frame buffers.
                    // We don't use a back buffer with this controller, pass nullptr.
                    static_cast<MemoryController*>(memoryController)->Clear(nullptr); /** THIS METHOD NEEDS TO HAVE WAIT/CV LOGIC ON BUFFER FULLNESS, NEED TO PASS RUNASYNC TO MEMORY CONTROLLER CONSTRUCTOR **/
                    screen_ = Screen::RomSelect;

                }
            }
        }



        //char buff[128]{ '\0' };
        //printf("PRE READ\n");
        //auto bytes = recv(dataChannel_, buff, 1, 0);
        //printf("POST READ: %d, %c\n", bytes, buff[0]);
  
        return ret;
    }

    void RTSPIoController::Write(uint16_t port, uint8_t data, [[maybe_unused]] meen::IController* memoryController)
    {
        [[maybe_unused]] auto audio = i8080ArcadeIO_->WritePort(port, data);
    }

    meen::ISR RTSPIoController::GenerateInterrupt(uint64_t currTime, uint64_t cycles, meen::IController* memoryController)
    {

        // Don't send out frames until we are connected
        // We could HALT the machine here and then UNHALT the machine when the HandleEvent method returns (We can't HALT in single threaded mode)
        if (state_ != State::PLAYING)
        {
            return meen::ISR::NoInterrupt;
        }

        meen::ISR isr{ meen::ISR::NoInterrupt };

        auto interrupt = i8080ArcadeIO_->GenerateInterrupt(currTime, cycles);

        switch (interrupt)
        {
            case 0:
            {
#ifndef ENABLE_EVENT_Q
                // Gameplay keys are handled in the Read IoController override method
                if (screen_ != Screen::Gameplay)
                {
                    int kb = ReadDataChannel();

                    auto scrollIndex = [this](int dir)
                    {
                        // rom index no longer needs to be atomic
                        int romIndex = romIndex_;

                        romIndex = (romIndex + dir) % romCount_;

                        if (romIndex < 0)
                        {
                            romIndex = romCount_ - 1;
                        }

                        romIndex_ = romIndex;
                    };

                    if (kb & 0x20)
                    {
                        scrollIndex(1);
                    }
                    else if (kb & 0x40)
                    {
                        scrollIndex(-1);
                    }
                    else if (kb & 0x80)
                    {
                        isr = meen::ISR::Load;
                    }
                }
#else
                isr = loadSaveInterrupt_.exchange(meen::ISR::NoInterrupt);
#endif
                break;
            }
            case 1:
            {
                if (screen_ == Screen::Gameplay)
                {
                    isr = meen::ISR::One;
                }
                break;
            }
            case 2:
            {
                if (screen_ == Screen::Gameplay)
                {
                    isr = meen::ISR::Two;
                }

#ifdef ENABLE_EVENT_Q
                EventData eventData;
                Frame frame;
                auto mc = static_cast<MemoryController*>(memoryController);

                switch (screen_)
                {
                    case Screen::RomSelect:
                        frame.bitstream = mc->GetRomSelectFrame(romIndex_, currTime);
                        break;
                    case Screen::Gameplay:
                        frame.bitstream = mc->GetGameplayFrame(currTime);
                        break;
                    default:
                        break;
                }

                if (frame.bitstream != nullptr)
                {
                    frame.timestamp = currTime;
                    eventData = std::move(frame);
                }
                else
                {
                    eventData = "Failed to get the frame from the memory controller, frame dropped";
                }

                {
                    std::lock_guard<std::mutex> lg(eventQMutex_);
                    eventQ_.push_back(std::move(eventData));
                }

                eventQCv_.notify_one();
#else
                meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr videoFrame;
                // ffplay -f rawvideo -pixel_format yuv420p -video_size 240x320 -framerate 60 input.yuv
                auto mc = static_cast<MemoryController*>(memoryController);

                switch (screen_)
                {
                    case Screen::RomSelect:
                        videoFrame = mc->GetRomSelectFrame(romIndex_, currTime);
                        break;
                    case Screen::Gameplay:
                        videoFrame = mc->GetGameplayFrame(currTime);
                        break;
                    default:
                        break;
                }

                if (videoFrame != nullptr)
                {
                    /*
                        If this thread gets saturated we have to move the blitting (this block) to the main thread like the sdl io controller.
                    */

                    std::span<uint8_t> s(yuv420p_.get(), textureWidth_ * textureHeight_);
                    i8080ArcadeIO_->BlitVRAM(s, textureWidth_, textureWidth_, std::span(videoFrame->data(), videoFrame->size()), MemoryController::frameWidth);
                    videoFrame = nullptr;

                    if (RTPH264Encoder_->Encode(yuv420p_.get(), currTime) == false)
                    {
                        // Write to the dataChannel indicating the next frame failed to transmit, restart the rtsp client at the user discretion.
                        std::string_view errMsg{ "Failed to encode and deliver the next video frame" };
                        sendto(dataChannel_.socket, errMsg.data(), errMsg.length(), 0, (sockaddr*)&dataChannel_.sockaddr, sizeof(dataChannel_.sockaddr));
                        printf("RTPH264Encder->Encode failed\n");
                    }
                }
                else
                {
                    printf("Failed to get the frame from the memory controller, frame dropped\n");
                }
#endif // ENABLE_EVENT_Q
                break;
            }
            default:
            {
                break;
            }
        }

        return isr;
    }

    int RTSPIoController::ReadDataChannel()
    {
        int kb = 0;
        int kbLen = sizeof(int);
        int addrSize = sizeof(dataChannel_.sockaddr);
        auto bytes = recvfrom(dataChannel_.socket, std::bit_cast<char*>(&kb), kbLen, 0, (sockaddr*)&dataChannel_.sockaddr, &addrSize);
        
        // We expect to get back 4 bytes of data per call
        if (bytes < kbLen)
        {
#ifdef _WINDOWS
            auto err = WSAGetLastError();

            if (err != WSAEWOULDBLOCK /* WSAETIMEDOUT */ || bytes >= 0)
            {
                printf("Received %d bytes from recvfrom with err %d\n", bytes, err);
            }
#endif
            kb = 0;
        }
        else
        {
            auto ip = inet_ntoa(dataChannel_.sockaddr.sin_addr);

            // Only accept incoming datagrams from the client ip
            if (strncmp(ip, clientIp_, strlen(clientIp_)) != 0) // todo: clientIp needs to be a config file parameter, currently just using the ip from which the rtsp client connects from
            {
                kb = 0;
            }
        }

        return kb;
    }

    std::array<uint8_t, 16> RTSPIoController::Uuid() const
    {
        return { 0xf2, 0xb0, 0x36, 0xa3, 0x6a, 0xd3, 0x49, 0xa7, 0x85, 0xb8, 0xd3, 0x9a, 0x59, 0xe6, 0x45, 0xec };
    }

    bool RTSPIoController::HandleEvent()
    {
#ifdef ENABLE_EVENT_Q
        if (state_ == State::PLAYING)
        {
            std::unique_lock<std::mutex> lg(eventQMutex_);
            eventQCv_.wait(lg, [this] { return !eventQ_.empty(); /* size() == maxEventData_;*/ });
            auto eventData = std::move(eventQ_.front());
            eventQ_.pop_front();
            lg.unlock();

            std::visit(overloaded
            {
                [](const std::string& error)
                {
                    printf("%s\n", error.c_str());
                },
                [this](Frame& frame)
                {
                    std::span<uint8_t> s(yuv420p_.get(), textureWidth_ * textureHeight_);
                    i8080ArcadeIO_->BlitVRAM(s, textureWidth_, textureWidth_, std::span(frame.bitstream->data(), frame.bitstream->size()), MemoryController::frameWidth);
                    // Return the bitstream immediately to the memory controller frame pool
                    frame.bitstream = nullptr;

                    // We could be in a waiting state if we are attempting to clear the screen during the processing of the escape key
                    // Call notify to wake up machine thread if it is in such a state, must be done after the frame is returned to the memroy controller (performed in the previous statement)

                    // cv_.notify_one();

                    if (RTPH264Encoder_->Encode(yuv420p_.get(), frame.timestamp) == false)
                    {
                        // Write to the dataChannel indicating the next frame failed to transmit, restart the rtsp client at the user's discretion.
                        std::string_view errMsg{ "Failed to encode and deliver the next video frame" };
                        sendto(dataChannel_.socket, errMsg.data(), errMsg.length(), 0, (sockaddr*)&dataChannel_.sockaddr, sizeof(dataChannel_.sockaddr));
                        printf("RTPH264Encder->Encode failed\n");
                    }

                    // Lock the reading of the data channel to the video frame rate

                    int kb = ReadDataChannel();

                    if (kb > 0)
                    {
                        if (screen_ == Screen::RomSelect)
                        {
                            auto scrollIndex = [this](int dir)
                            {
                                int romIndex = romIndex_;

                                romIndex = (romIndex + dir) % romCount_;

                                if (romIndex < 0)
                                {
                                    romIndex = romCount_ - 1;
                                }

                                romIndex_ = romIndex;
                            };

                            if (kb & 0x020)
                            {
                                scrollIndex(1);
                            }
                            else if (kb & 0x040)
                            {
                                scrollIndex(-1);
                            }
                            else if (kb & 0x100)
                            {
                                loadSaveInterrupt_ = meen::ISR::Load;
                            }
                        }
                        else
                        {
                            // store the current keyboard so we can read them from the read method
                            // keyboard_ = kb;
                        }
                    }
                }
            }, eventData);
        }
#endif // ENABLE_EVENT_Q

        if (controlSocket_ == INVALID_SOCKET)
        {
            assert(state_ == State::INIT);
            printf("Waiting for RTSP client connection on port 554\n");
            // Open a connection to the rtsp client, blocking
            controlSocket_ = OpenTCPConnection("554", clientIp_, 0); // rtsp default port

            if (controlSocket_ == INVALID_SOCKET)
            {
                printf("Failed to create RTSP control channel socket\n");
                return true;
            }
        }

        //HandleRTSPCommand();

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
                if (state_ != State::PLAYING)
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
                        return true;
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
                        return true;
                    }

                    videoChannel_.sockaddr.sin_family = AF_INET,
                    videoChannel_.sockaddr.sin_port = htons(rtpPort);

                    auto err = inet_pton(AF_INET, clientIp_, &videoChannel_.sockaddr.sin_addr);

                    if (err == 0)
                    {
                        printf("Failed to create video channel address: %s\n", clientIp_);
                        return true;
                    }
                }
                else
                {
                    printf("Invalid RTSP state transition\n");
                    return true;
                }

                state_ = State::READY;
            }
            else if (bufferSv.starts_with("PLAY") == true)
            {
                if (state_ == State::INIT)
                {
                    printf("Invalid RTSP state transition\n");
                    return true;
                }

                str = std::format("RTSP/1.0 200 OK\r\nCSeq: {}\r\n\r\n", cseq);

#ifdef ENABLE_EVENT_Q
#ifdef _WINDOWS
                // Put the control socket into non-blocking since we are now playing
                u_long mode = 1;

                if (ioctlsocket(controlSocket_, FIONBIO, &mode) < SOCKET_ERROR)
                {
                    printf("Failed to set non blocking option on the control channel\n");
                }
#else
#error "Non blocking socket Linux implementation required\n"
#endif // _WINDOWS
#endif // ENABLE_EVENT_Q
                // The rtsp player is now connected, allow audio/video frames to be delivered to it.
                state_ = State::PLAYING;
            }
            else if (bufferSv.starts_with("TEARDOWN") == true)
            {
                str = std::format("RTSP/1.0 200 OK\r\nCSeq: {}\r\n\r\n", cseq);

                // No matter what our running state is we always destroy the session and revert back to an INIT state
                state_ = State::INIT;

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
            // getsockopt with SO_ERROR
            // Either we have gracefully closed or received some other critical error, shutdown and try to re-establish a connection
            if (len == 0 || WSAGetLastError() != WSAEWOULDBLOCK)
            {
                printf("Closing the RTSP control channel\n");
                std::string_view errMsg{ "The RTSP client has been closed, re-open to continue" };
                // Always write it regardless of whether the dataChannel is active or not ... we ignore the error
                [[maybe_unused]] auto bytes = sendto(dataChannel_.socket, errMsg.data(), errMsg.length(), 0, (sockaddr*)&dataChannel_.sockaddr, sizeof(dataChannel_.sockaddr));
                shutdown(controlSocket_, SD_BOTH);
                closesocket(controlSocket_);
                controlSocket_ = INVALID_SOCKET;
                state_ = State::INIT;
            }
        }

        return false;
    }

    void RTSPIoController::HandleError(std::string&& errMsg)
    {
        std::string_view em{ "Server error: check the server logs for further details\n" };
        [[maybe_unused]] auto bytes = sendto(dataChannel_.socket, em.data(), em.length(), 0, (sockaddr*)&dataChannel_.sockaddr, sizeof(dataChannel_.sockaddr));

        // 2. Write the error to the server log, currently printing it to the console.
        printf("%s\n", errMsg.c_str());
    }

    void RTSPIoController::HandleLoadComplete()
    {
        // We successfully loaded the rom, transition into gameplay.
        screen_ = Screen::Gameplay;
        // Reset the internal state of the hardware
        i8080ArcadeIO_->Reset();
    }

    std::tuple<bool, int> RTSPIoController::GetRomIndex()
    {
        //return std::tuple(loadSaveState_.exchange(false), romIndex_.load());

        return std::tuple(false, romIndex_.load());
    }
} // namespace meen_i8080_arcade
