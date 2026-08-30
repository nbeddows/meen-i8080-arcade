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

#ifndef RTSPIOCONTROLLER_H
#define RTSPIOCONTROLLER_H

#define ARDUINOJSON_ENABLE_STRING_VIEW 1
#include <ArduinoJson.h>
#include <atomic>
//#include <variant>
#include <vector>
#ifdef _WINDOWS
#include <Ws2tcpip.h>
#endif // _WINDOWS

#define ENABLE_EVENT_Q

#ifdef ENABLE_EVENT_Q
#include <list>
#include <variant>
#endif // ENABLE_EVENT_Q

#include "meen_hw/MH_II8080ArcadeIO.h"
#include "meen_i8080_arcade/IIoController.h"
#include "meen_i8080_arcade/RTPH264Encoder.h"


namespace meen_i8080_arcade
{
    /**
    
    */
    class RTSPIoController final : public IIoController
    {
    private:
        enum State
        {
            INIT,
            READY,
            PLAYING
        };

        struct UDPConnection
        {
            SOCKET socket{ INVALID_SOCKET };
            struct sockaddr_in sockaddr;
        };

#ifdef _WINDOWS
        SOCKET controlSocket_{ INVALID_SOCKET };
#ifdef USE_TCP_FOR_DATA
        SOCKET dataChannel_{ INVALID_SOCKET };
#else
        UDPConnection dataChannel_;
#endif
        UDPConnection videoChannel_;
#else
#error "NOT implemented\n"
#endif // _WINDOWS
        /**	i8080 arcade io

            The hardware emulator.
        */
        std::unique_ptr<meen_hw::MH_II8080ArcadeIO> i8080ArcadeIO_;
        std::unique_ptr<RTPH264Encoder> RTPH264Encoder_;
        std::unique_ptr<uint8_t[]> yuv420p_;

        int romCount_{};
        std::atomic_int romIndex_{};
        int textureWidth_{};
        int textureHeight_{};

        /** The current screen

            See the Screen enumeration for further details.
		*/
		Screen screen_{};

        std::atomic_int state_{ State::INIT };
        char clientIp_[INET_ADDRSTRLEN]{};

        // timeout -1: blocking recv
        SOCKET OpenTCPConnection(const char* port, char* clientIp = nullptr, int timeout = -1);
        int ReadDataChannel();

#ifdef ENABLE_EVENT_Q
        struct Frame
        {
            meen_hw::MH_ResourcePool<std::vector<uint8_t>>::ResourcePtr bitstream;
            int64_t timestamp;
        };

        /** Helper type for functional style visitor for std::visit

            This template helper type is taken straight from cppreference std::visit examples (https://en.cppreference.com/w/cpp/utility/variant/visit2)
        */
        template<class... Ts>
        struct overloaded : Ts... { using Ts::operator()...; };

        /** Generated event data

            A using directive for ease of use. This will hold the active event data to be processed.

            std::string:	The application has encountered and error.
            ResourcePtr:	The next video frame is ready to be rendered. This event drives the control loop.

            The EventData will be assigned to the SDL_Event data1 property.
        */
        using EventData = std::variant<std::string, Frame>;

        // DOCUMENT ME
        std::list<EventData> eventQ_;
        // DOCUMENT ME!
        std::mutex eventQMutex_;

        /** Event data pool condition variable

            Used in conjuction with eventDataMutex_ to wait on all outstanding events to complete. This is required for screen transition (back to rom select)
            so all video frames can be cleared preventing any stale video frames being rendered.
        */
        std::condition_variable eventQCv_;

        /**	Load or save

            A machine level interrupt which indicates whether or not the machine
            should attempt to load a new state or save its current state.

            meen::ISR::NoInterrupt: don't load or save the state.
            meen::ISR::Load: attempt to load a new rom.
            meen::ISR::Save: attempt to save the current loaded rom state.

            @remark		This value can be set from a different thread, hence it is atomic.
        */
        std::atomic<meen::ISR> loadSaveInterrupt_{ meen::ISR::NoInterrupt };
#endif // ENABLE_EVENT_Q
    public:
        RTSPIoController(int romCount, const JsonVariantConst audioHardware, const JsonVariantConst videoHardware);

        ~RTSPIoController();

        /** IController::Read override

            Sample the keyboard so the CPU can take any required action.

            @param  port                The device to read from.
            @param  memoryController    The memory controller that has been registered with MEEN.

            @return                     A bitfield indicating the action to take.
        */
        uint8_t Read(uint16_t port, meen::IController* memoryController) final;

        /** IController::Write override

            Write the relevant audio sample to the output audio device.

            @param  port                The output device to write to.
            @param  data                A bitfield indicating what data to write.
            @param  memoryController    The memory controller that has been registered with MEEN.
        */
        void Write(uint16_t port, uint8_t data, meen::IController* memoryController) final;

        /** IController::GenerateInterrupt override

            Render the video ram texture to the window via the rendering context.

            @param  currTime            The current CPU run time in nanoseconds.
            @param  cycles              The number of CPU cycles completed.
            @param  memoryController    The memory controller that has been registered with MEEN.

            @return                     One of the following meen ISRs:<br><br>
                                        `ISR::NoInterrupt`: the method did not generate an iterrupt.<br>
                                        `ISR::One`: signal MEEN that the first 96 scanlines have been rendered.<br>
                                        `ISR::Two`: signal MEEN that the remaining scanlines (up to 224) have
                                        been rendered (start of vblank).<br>
										`ISR::Load`: attempt to load a new rom.
        */
        meen::ISR GenerateInterrupt(uint64_t currTime, uint64_t cycles, meen::IController* memoryController) final;

        /** Uuid

            Unique universal identifier for this controller.

            @return    The uuid as a 16 byte array.
        */
        std::array<uint8_t, 16> Uuid() const final;

        /** Load Video Textures

            Create the video texture that will be rendered to the screen.

            @param  videoTextures   JSON object describing the video texture.
            @param  textureWidth    The width of the video texture in pixels.
            @param  textureHeight   The height of the video texture in pixels.

            @return                 On failure, a `std::error_code` with one of the following values:<br><br>
                                    `std::errc::not_supported`: the video configuration parameters are invalid.<br>
                                    `std::errc:io_error`: video configuration serialisation failure.
        */
        std::error_code LoadVideoTextures(const JsonVariantConst videoTextures, int textureWidth, int textureHeight) final;

        /** Load Audio Samples

            Loads the audio samples from the configuration file.

            @param     audioSamples     JSON object representing the audio sample files.

            @return                     On failure, a `std::error_code` with one of the following values:<br><br>
                                        `std::errc::protocol_not_supported`: The format of the audio sample is not supported.<br>
                                        `std::errc::value_to_large`: the size of the data in the audio sample is too large.<br>
                                        `std::errc::illegal_byte_sequence`: the audio sample is aligned incorrctly.<br>
                                        `std::errc::not_supported`: the audio file scheme in the cofiguration file is invalid.<br>
                                        `std::errc::invalid_argument`: the audio sample address is invalid.<br>
                                        `std::errc::result_out_of_range`: the audio sample address is invalid. 
        */
        std::error_code LoadAudioSamples(const JsonVariantConst audioSamples) final;

        /** Main control loop

            Process all incoming events.

            @return    True to quit the machine, false otherwise.

            @remark    This method will always return false, ie; the loop
                       will run until the device is switched off.
        */
        bool HandleEvent() final;

        /** Error handler

            Process any generated errors.

            These errors may come from MEEN or meen-i8080-arcade itself.

            @param    errorMsg      The error message as a `std::string`.
        */
        void HandleError(std::string&& errorMsg) final;

        /** Load complete handler

            Handle the transition into game play when a rom has been sucessfully loaded.
        */
        void HandleLoadComplete() final;

		/** Get the rom index

			Load the selected rom or the save state of the currently selected rom.

			@return					A tuple holding two values:<br><br>
									`bool`: only valid when loading roms, true if the save file is to be loaded, false if the rom is to be loaded.<br>
									`int`: the index into the roms array for the rom to be loaded or saved.
		*/
        std::tuple<bool, int> GetRomIndex() final;
    };
} // namespace meen_i8080_arcade
#endif // RPIOCONTROLLER_H
