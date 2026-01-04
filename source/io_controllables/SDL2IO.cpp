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

#include "meen_i8080_arcade/io_controllables/SDL2IO.h"

namespace meen_i8080_arcade
{
	std::errc SDL2IO::ConfigureVideoDevice(int width, int height, int fullscreen)
	{
		SDL_SetMainReady();

		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		{
			printf("Failed to initialise SDL video");
			return std::errc::no_such_device;
		}

		window_ = SDL_CreateWindow("meen i8080 arcade",
								SDL_WINDOWPOS_UNDEFINED,
								SDL_WINDOWPOS_UNDEFINED,
								width,
								height,
								fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

		if (window_ == nullptr)
		{
			printf("Failed to allocate window\n");
			return std::errc::no_such_device;
		}

		renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);

		if (renderer_ == nullptr)
		{
			printf("Failed to allocate an accelerated renderer, falling back to software\n");
			renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);

			if (renderer_ == nullptr)
			{
				printf("Failed to allocate an SDL renderer");
				return std::errc::not_supported;
			}
		}

		return std::errc{};
	}

	std::errc SDL2IO::ConfigureAudioDevice(int sampleRate, int channels, int sampleSize)
	{
		SDL_SetMainReady();

		if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
		{
			printf("Failed to initialise SDL audio");
			return std::errc::no_such_device;
		}

		//SDL_CloseAudioDevice(audioDeviceId_);

		// Fill out the desired output format
		SDL_AudioSpec desiredSpec{};
		desiredSpec.freq = sampleRate;    // sample rate
		desiredSpec.format = AUDIO_S16;   // 16-bit signed audio
		// this needs to be pulled out of the audioHardware
		desiredSpec.channels = channels;         // stereo
		// Allocate a sample buffer large enough to span one video frame duration (video runs at 60Hz).
		// Note: depending on the sample rate this may not be a whole number and will be truncated,
		// hence it could be one sample less than a video frame duration (this should be fine).
		desiredSpec.samples = sampleSize; // Internal sample buffer spanning a video frame duration (approx).

		audioDeviceId_ = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, &obtainedSpec_, 0);

		if (audioDeviceId_ == 0)
		{
			printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
			return std::errc::no_such_device;
		}

		// Check to make sure we got what we asked for
		if (desiredSpec.freq != obtainedSpec_.freq || desiredSpec.format != obtainedSpec_.format || desiredSpec.channels != obtainedSpec_.channels || desiredSpec.samples != obtainedSpec_.samples)
		{
			printf("Failed to open the audio device with the desired specifications\n");
			return std::errc::not_supported;
		}

		return std::errc{};
	}

	std::errc SDL2IO::ConfigurePeripheralDevice()
	{
		SDL_SetMainReady();

		if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0)
		{
			printf("Failed to initialise SDL peripherals");
			return std::errc::no_such_device;
		}

		SDL_SetEventFilter([]([[maybe_unused]] void* eventType, SDL_Event* e)
		{
			// Ignore all events except quit.
			return static_cast<int>(e->type == SDL_QUIT);
		},
		nullptr);

		// Monitor key presses/releases
		sdlKbState_ = SDL_GetKeyboardState(nullptr);

		return std::errc{};
	}

	SDL2IO::~SDL2IO()
	{
		if (renderer_ != nullptr)
		{
			SDL_DestroyRenderer(renderer_);
		}

		if (window_ != nullptr)
		{
			SDL_DestroyWindow(window_);
		}

		SDL_Quit();
	}

	std::errc SDL2IO::LoadAudioSamples(int sampleRate, int channels, int sampleSize)
	{
		// Left here for reference - if we had music playing at all times we would enable it here
		// Currently, we only have music playing in the game play screen, so we pause and unpause
		// audio during screen transitions
		// SDL_PauseAudioDevice(audioDeviceId_, 0);

		return std::errc{};
	}

	std::errc SDL2IO::LoadVideoTextures(int bpp, int textureWidth, int textureHeight)
	{
		int windowWidth = 0;
		int windowHeight = 0;
		auto pf = SDL_PIXELFORMAT_UNKNOWN;

		switch(bpp)
		{
			case 8:
				pf = SDL_PIXELFORMAT_RGB332;
				break;
			case 16:
				pf = SDL_PIXELFORMAT_RGB565;
				break;
			default:
				return std::errc::not_supported;
		}

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		texture_ = SDL_CreateTexture(renderer_, pf, SDL_TEXTUREACCESS_STREAMING, textureWidth, textureHeight);

		if (texture_ == nullptr)
		{
			return std::errc::not_enough_memory;
		}

		if (SDL_QueryTexture(texture_, nullptr, nullptr, &dstRect_.w, &dstRect_.h) < 0)
		{
			return std::errc::not_enough_memory;
		}

		// Position the destination blitting rectangle in the middle of the screen.
		SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
		dstRect_.x = (windowWidth - dstRect_.w) / 2;
		dstRect_.y = (windowHeight - dstRect_.h) / 2;
		return std::errc{};
	}

	void SDL2IO::ScreenTransition(Screen curr, Screen next)
	{
		switch (curr)
		{
			case Screen::Gameplay:
			{
				switch (next)
				{
					case Screen::RomSelect:
					{
						// Drop any remaining audio
						SDL_ClearQueuedAudio(audioDeviceId_);
						// Pause the audio device
						SDL_PauseAudioDevice(audioDeviceId_, 1);
						break;
					}
					default:
					{
						break;
					}
				}
				break;
			}
			case Screen::RomSelect:
			{
				switch (next)
				{
					case Screen::Gameplay:
					{
						// Unpause the audio device
						SDL_PauseAudioDevice(audioDeviceId_, 0);
						break;
					}
					default:
					{
						break;
					}
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}

	std::array<uint8_t, 16> SDL2IO::Uuid() const
	{
		return{ 0x22, 0x61, 0xC9, 0x53, 0x9A, 0x36, 0x4B, 0xD3, 0xB9, 0x68, 0x47, 0x67, 0x6F, 0x52, 0x6D, 0x48 };
	}

	std::errc SDL2IO::RenderAudioFrame(const int32_t* audioFrame, [[maybe_unused]] uint64_t timestamp)
	{
        SDL_QueueAudio(audioDeviceId_, static_cast<const void*>(audioFrame), obtainedSpec_.size);
        return std::errc{};
	}

	std::errc SDL2IO::GetTextureBuffer(uint8_t** dst, int* dstRowBytes) const
	{
		auto err = SDL_LockTexture(texture_, nullptr, std::bit_cast<void**>(dst), dstRowBytes);
		assert(err == 0);
		return std::errc{};
	}

	std::errc SDL2IO::RenderVideoFrame(const uint8_t* videoFrame, [[maybe_unused]] uint64_t timestamp)
	{
		// todo: need to move pumpEvents/HasEvent to ReadPeripheralDevice, it needs to return std::expected
		SDL_PumpEvents();

		if (SDL_HasEvent(SDL_EventType::SDL_QUIT))
		{
			return std::errc::connection_aborted;
		}

		SDL_UnlockTexture(texture_);
        auto err = SDL_RenderCopy(renderer_, texture_, nullptr, &dstRect_);
        assert(err == 0);
        SDL_RenderPresent(renderer_);

        return std::errc{};
    }

	uint32_t SDL2IO::ReadPeripheralDevice()
	{
		int keys = 0;

		keys |= ((sdlKbState_[SDL_SCANCODE_C] != 0) * Input::Credit);
		keys |= ((sdlKbState_[SDL_SCANCODE_1] != 0) * Input::OnePlayer);
		keys |= ((sdlKbState_[SDL_SCANCODE_2] != 0) * Input::TwoPlayer);
		keys |= ((sdlKbState_[SDL_SCANCODE_A] != 0) * Input::P1Left);
		keys |= ((sdlKbState_[SDL_SCANCODE_S] != 0) * Input::P1Fire);
		keys |= ((sdlKbState_[SDL_SCANCODE_D] != 0) * Input::P1Right);
		keys |= ((sdlKbState_[SDL_SCANCODE_3] != 0) * Input::ThreeShips);
		keys |= ((sdlKbState_[SDL_SCANCODE_4] != 0) * Input::FourShips);
		keys |= ((sdlKbState_[SDL_SCANCODE_5] != 0) * Input::FiveShips);
		keys |= ((sdlKbState_[SDL_SCANCODE_6] != 0) * Input::SixShips);
		keys |= ((sdlKbState_[SDL_SCANCODE_T] != 0) * Input::Tilt);
		keys |= ((sdlKbState_[SDL_SCANCODE_E] != 0) * Input::ExtraShip);
		keys |= ((sdlKbState_[SDL_SCANCODE_J] != 0) * Input::P2Left);
		keys |= ((sdlKbState_[SDL_SCANCODE_K] != 0) * Input::P2Fire);
		keys |= ((sdlKbState_[SDL_SCANCODE_L] != 0) * Input::P2Right);
		keys |= ((sdlKbState_[SDL_SCANCODE_I] != 0) * Input::CoinInfo);
		keys |= ((sdlKbState_[SDL_SCANCODE_R] != 0) * Input::LoadRom);
		keys |= ((sdlKbState_[SDL_SCANCODE_Y] != 0) * Input::SaveRom);
		keys |= ((sdlKbState_[SDL_SCANCODE_ESCAPE] != 0) * Input::QuitRom);
		keys |= ((sdlKbState_[SDL_SCANCODE_Q] != 0) * Input::Exit);
		keys |= ((sdlKbState_[SDL_SCANCODE_UP] != 0) * Input::PreviousRom);
		keys |= ((sdlKbState_[SDL_SCANCODE_DOWN] != 0) * Input::NextRom);
		keys |= ((sdlKbState_[SDL_SCANCODE_RETURN] != 0) * Input::SelectRom);

		return keys;
	}

    std::errc SDL2IO::RenderErrorString(const std::string& error)
	{
        printf("%s\n", error.c_str());
        return std::errc{};
	}

    std::errc SDL2IO::ClearDisplay(bool clearDisplay)
	{
		return std::errc{};
	}
} // namespace meen_i8080_arcade