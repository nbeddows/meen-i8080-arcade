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

#include <assert.h>
#include <bitset>

#include "meen_i8080_arcade/MemoryController.h"
#include "meen_i8080_arcade/SdlIoController.h"
#include "meen/Base.h"

namespace meen_i8080_arcade
{
	std::array<std::atomic_bool, MIX_CHANNELS * 2> SDLIoController::channelPlaying_ = {};

    SDLIoController::SDLIoController(bool runAsync, int romCount, const JsonVariantConst audioHardware, const JsonVariantConst videoHardware)
		: runAsync_{ runAsync }
		, romCount_{ romCount }
		, romIndex_{ romCount - 1 }
	{
		printf("MEEN HW Version: %s\n", meen_hw::Version());

		SDL_SetMainReady();

		if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0)
		{
			printf("Failed to initialise SDL");
		}

		Mix_Init(0);

		window_ = SDL_CreateWindow("meen i8080 arcade",
								SDL_WINDOWPOS_UNDEFINED,
								SDL_WINDOWPOS_UNDEFINED,
								videoHardware["width"].as<int>(),
								videoHardware["height"].as<int>(),
								videoHardware["fullScreen"].as<bool>() ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

		if (window_ == nullptr)
		{
			printf("Failed to allocate window\n");
		}

		renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);

		if (renderer_ == nullptr)
		{
			printf("Failed to allocate an accelerated renderer, falling back to software\n");
			renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);

			if (renderer_ == nullptr)
			{
				printf("Failed to allocate an SDL renderer");
			}
		}

		i8080ArcadeIO_ = meen_hw::MakeI8080ArcadeIO();

		if(i8080ArcadeIO_ == nullptr)
		{
			printf("Failed to create i8080 arcade hardware");
		}

		int sampleRate = audioHardware["sampleRate"].as<int>();
		// Allocate a sample buffer large enough to span one video frame duration.
		// Note: depending on the sample rate this may not be a whole number and will be truncated,
		// hence it could be one sample less than a video frame duration (this should be fine).
		int sampleSize = sampleRate / 60; // 60 - video runs a 60hz

		if (Mix_OpenAudio(sampleRate, 8 /* format (mono) */, audioHardware["channels"].as<int>(), sampleSize) < 0)
		{
			printf("Failed to open SDL Mixer");
		}

		SDL_SetEventFilter([](void* eventType, SDL_Event* e)
		{
			// Ignore all events except quit.
			return static_cast<int>(e->type == SDL_QUIT);
		},
		nullptr);

		// Monitor key presses/releases
		sdlKbState_ = SDL_GetKeyboardState(nullptr);

		Mix_ChannelFinished([](int channel)
		{
			SDLIoController::channelPlaying_[channel] = false;
		});
	}

	SDLIoController::~SDLIoController()
	{
		if (renderer_ != nullptr)
		{
			SDL_DestroyRenderer(renderer_);
		}

		if (window_ != nullptr)
		{
			SDL_DestroyWindow(window_);
		}

		for (auto& chunk : mixChunk_)
		{
			Mix_FreeChunk(chunk);
		}

		Mix_CloseAudio();
		Mix_Quit();
		SDL_Quit();
	}

	std::tuple<bool, int> SDLIoController::GetRomIndex()
	{
		return std::tuple(loadSaveState_.exchange(false), romIndex_.load());
	}

	std::error_code SDLIoController::LoadAudioSamples(const JsonVariantConst audio)
	{
		auto scheme = audio["scheme"].as<std::string_view>();
		auto directory = audio["directory"].as<std::string_view>();

		auto addChunk = [&mixChunk = mixChunk_](std::string_view directory, std::string_view resource)
		{
			// if we want to add the additional '/' (so we don't have to put it in the config file)
			// we need to check if the directory is empty first
			auto chunk = Mix_LoadWAV((std::string(directory) + std::string(resource)).c_str());

			if (resource.empty() == false && chunk == nullptr)
			{
				return std::make_error_code (std::errc::no_such_file_or_directory);
			}

			mixChunk.emplace_back(chunk);
			return std::error_code{};
		};
		
		if (audio["sample"].size() != MIX_CHANNELS * 2)
		{
			return std::make_error_code(std::errc::invalid_argument);
		}

		if (Mix_AllocateChannels(audio["sample"].size()) != MIX_CHANNELS * 2)
		{
			return std::make_error_code(std::errc::not_supported);
		}

		for(const auto& sample : audio["sample"].as<JsonArrayConst>())
		{
			auto dir = directory;
			auto resource = sample["bytes"].as<std::string_view>();

			if (resource.starts_with("file://"))
			{
				resource.remove_prefix(strlen("file://"));
				// A resource starting with a scheme specifies the exact location of that resource
				dir = "";
			}
			else
			{
				if (scheme != "file://")
				{
					return std::make_error_code (std::errc::not_supported);
				}
			}

			auto err = addChunk(dir, resource);

			if (err)
			{
				return err;
			}
		}

		return std::error_code{};
	}

	std::error_code SDLIoController::LoadVideoTextures(const JsonVariantConst videoTextures, int textureWidth, int textureHeight)
	{
		int windowWidth = 0;
		int windowHeight = 0;
		std::string meenConfig;
		serializeJson(videoTextures, meenConfig);

		if(meenConfig.empty() == true)
		{
			return std::make_error_code (std::errc::io_error);
		}

		auto err = i8080ArcadeIO_->SetOptions(meenConfig.c_str());

		if(err)
		{
			return err;
		}

		auto pf = SDL_PIXELFORMAT_UNKNOWN;

		if(videoTextures["bpp"] != nullptr)
		{
			switch(videoTextures["bpp"].as<int>())
			{
				case 8:
					pf = SDL_PIXELFORMAT_RGB332;
					break;
				case 16:
					pf = SDL_PIXELFORMAT_RGB565;
					break;
				default:
					return std::make_error_code (std::errc::not_supported);
			}
		}
		else
		{
			return std::make_error_code (std::errc::io_error);
		}

		// swap width/height based on orientation
		if (videoTextures["orientation"].as<std::string>() == "upright")
		{
			textureWidth ^= textureHeight ^= textureWidth ^= textureHeight;
		}

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		texture_ = SDL_CreateTexture(renderer_, pf, SDL_TEXTUREACCESS_STREAMING, textureWidth, textureHeight);

		if (texture_ == nullptr)
		{
			return std::make_error_code (std::errc::not_enough_memory);
		}

		if (SDL_QueryTexture(texture_, nullptr, nullptr, &dstRect_.w, &dstRect_.h) < 0)
		{
			return std::make_error_code(std::errc::not_enough_memory);
		}

		// Position the destination blitting rectangle in the middle of the screen.
		SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
		dstRect_.x = (windowWidth - dstRect_.w) / 2;
		dstRect_.y = (windowHeight - dstRect_.h) / 2;
		return std::error_code{};
	}

	// Scan the keyboard for load and save requests
	Uint8 SDLIoController::SetInterrupt(bool key, bool lastKey, meen::ISR isr, bool loadSaveState)
	{
		if (key ^ lastKey && key)
		{
			loadSaveInterrupt_ = isr;
			loadSaveState_ = loadSaveState;
		}

		return key;
	}

	uint8_t SDLIoController::Read(uint16_t port, [[maybe_unused]] meen::IController* controller)
	{
		auto ret = i8080ArcadeIO_->ReadPort(port);

		if (ret == 0)
		{
			if (port == 1 || port == 2)
			{
				if (kbState_[SDL_SCANCODE_ESCAPE] == false)
				{
					lastR_ = SetInterrupt(kbState_[SDL_SCANCODE_R], lastR_, meen::ISR::Load, true);
					lastY_ = SetInterrupt(kbState_[SDL_SCANCODE_Y], lastY_, meen::ISR::Save, false);

					if (port == 1)
					{
						ret = 0x08;
						ret |= (kbState_[SDL_SCANCODE_C] * 0x01); // Credit
						ret |= (kbState_[SDL_SCANCODE_1] * 0x04); // 1P
						ret |= (kbState_[SDL_SCANCODE_2] * 0x02); // 2P
						ret |= (kbState_[SDL_SCANCODE_A] * 0x20); // 1P Left
						ret |= (kbState_[SDL_SCANCODE_S] * 0x10); // 1P Fire
						ret |= (kbState_[SDL_SCANCODE_D] * 0x40); // 1P Right
					}
					else if (port == 2)
					{
						ret |= (kbState_[SDL_SCANCODE_3] * 0x00); // 3 Ships
						ret |= (kbState_[SDL_SCANCODE_4] * 0x01); // 4 Ships
						ret |= (kbState_[SDL_SCANCODE_5] * 0x02); // 5 Ships
						ret |= (kbState_[SDL_SCANCODE_6] * 0x03); // 6 Ships
						ret |= (kbState_[SDL_SCANCODE_T] * 0x04); // Tilt
						ret |= (kbState_[SDL_SCANCODE_E] * 0x08); // Extra Ship at
						ret |= (kbState_[SDL_SCANCODE_J] * 0x20); // 2P Left
						ret |= (kbState_[SDL_SCANCODE_K] * 0x10); // 2P Fire
						ret |= (kbState_[SDL_SCANCODE_L] * 0x40); // 2P Right
						ret |= (kbState_[SDL_SCANCODE_I] * 0x80); // Show coin info
					}
					else
					{
						assert(0);
						printf("Invalid Read Port: %d\n", port);
					}
				}
				else
				{
					if (runAsync_ == false)
					{
						// In single threaded mode we need to remove all outstanding i8080 arcade events and return the video frames
						// back to the memory controller so they can be cleared.
						while(eventQ_.empty() == false)
						{
							auto eventData = std::move(eventQ_.front());
							eventQ_.pop_front();

							if (std::holds_alternative<Frame>(eventData))
							{
								auto& frame = std::get<Frame>(eventData);
								frame.bitstream = nullptr;
							}
						}
					}

					screen_ = Screen::RomSelect;
					// Clear the memory controller ram and frame buffers.
					// We don't use a back buffer with this controller, pass nullptr.
					static_cast<MemoryController*>(controller)->Clear(nullptr);
				}
			}
		}

		return ret;
	}

	void SDLIoController::Write(uint16_t port, uint8_t data, [[maybe_unused]] meen::IController* controller)
	{
		auto audio = i8080ArcadeIO_->WritePort(port, data);

		if (audio > 0)
		{
			// port will either be 3 or 5
			// when port is 3 index will be 0 and when it is 5 it will be 8
			// which will give the correct offset into the mixChunk_ array
			auto offset = (port - 3) << 2;

			for (int i = 0; i < 8; i++)
			{
				if ((audio >> i) & 0x01)
				{
					// Don't repeat a sample until it has finished
					if (SDLIoController::channelPlaying_[i + offset] == true)
					{
						audio &= ~(0x01 << i);
					}
					else
					{
						SDLIoController::channelPlaying_[i + offset] = true;
					}
				}
			}

			// We may have turned off all audio, check it again
			if (audio > 0)
			{
				if (runAsync_ == true)
				{
					{
						std::lock_guard<std::mutex> lg(eventQMutex_);
						eventQ_.emplace_back(EventData{ static_cast<uint16_t>((port << 8) | audio) });
					}

					eventQCv_.notify_one();
				}
				else
				{
					eventQ_.emplace_back(EventData{ static_cast<uint16_t>((port << 8) | audio) });
				}
			}
		}
	}

	meen::ISR SDLIoController::GenerateInterrupt(uint64_t currTime, uint64_t cycles, meen::IController* memoryController)
	{
		meen::ISR isr{};

		auto interrupt = i8080ArcadeIO_->GenerateInterrupt(currTime, cycles);

		switch(interrupt)
		{
			case 0:
			{
				isr = loadSaveInterrupt_.exchange(meen::ISR::NoInterrupt);

				if (isr == meen::ISR::Load)
				{
					if (screen_ == Screen::RomSelect)
					{
						// We can't save anything from the rom select screen
						if (loadSaveState_ == true)
						{
							// drop the interrupt
							isr = meen::ISR::NoInterrupt;
						}
					}
					else
					{
						// We can only load a save state from gameplay (as opposed to a rom), drop the interrupt
						if (loadSaveState_ == false)
						{
							isr = meen::ISR::NoInterrupt;
						}
					}
				}
				else if (isr == meen::ISR::Save)
				{
					// We can't save anything from rom select, drop the interrupt
					if (screen_ == Screen::RomSelect)
					{
						isr = meen::ISR::NoInterrupt;
					}
				}
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
				EventData eventData;
				Frame frame;

				if (screen_ == Screen::Gameplay)
				{
					isr = meen::ISR::Two;
				}

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

				if (runAsync_ == true)
				{
					{
        				std::lock_guard<std::mutex> lg(eventQMutex_);
						eventQ_.push_back(std::move(eventData));
					}

					eventQCv_.notify_one();
				}
				else
				{
					eventQ_.push_back(std::move(eventData));
				}
				break;
			}
			default:
			{
				assert(interrupt >= 0 && interrupt <= 2);
				break;
			}
		}

		return isr;
	}

	std::array<uint8_t, 16> SDLIoController::Uuid() const
	{
		return{ 0x22, 0x61, 0xC9, 0x53, 0x9A, 0x36, 0x4B, 0xD3, 0xB9, 0x68, 0x47, 0x67, 0x6F, 0x52, 0x6D, 0x48 };
	}

	bool SDLIoController::HandleEvent()
	{
		SDL_Event e;
		// We filter out all events execpt SDL_Quit
		if (SDL_PollEvent(&e))
		{
			return true;
		}

		EventData eventData;

		if (runAsync_ == true)
		{
			std::unique_lock<std::mutex> ul(eventQMutex_);
			eventQCv_.wait(ul, [this] { return !eventQ_.empty(); });
			eventData = std::move(eventQ_.front());
			eventQ_.pop_front();
		}
		else
		{
			if (eventQ_.empty() == false)
			{
				assert(eventQ_.size() == 1);
				eventData = std::move(eventQ_.front());
				eventQ_.pop_front();
			}
			else
			{
				// No event to process, nothing more to do
				return false;
			}
		}

		return std::visit(overloaded
		{
			[](const std::string& error)
			{
				printf("%s\n", error.c_str());
				return false;
			},
			[this, &e](uint16_t audioData)
			{
				// The audioData high 8 bits are the port and the low 8 bits the audio samples to play

				std::bitset<8> audio = audioData & 0xFF;
				// port will either be 3 or 5
				// when port is 3 index will be 0 and when it is 5 it will be 8
				// which will give the correct offset into the mixChunk_ array
				auto offset = ((audioData >> 8) - 3) << 2;

				for (int i = 0; i < 8; i++)
				{
					if (audio.test(i) == true)
					{
						[[maybe_unused]] auto busy = Mix_PlayChannel(i + offset /* use a dedicated channel */, mixChunk_[i + offset], 0 /* don't loop (play it once) */);
						// We have a channel for each sound, so busy should only be -1 when we don't have a sound for that channel
						assert(mixChunk_[i] == nullptr || busy != -1);
					}
				}

				return false;
			},
			[this, &e](Frame& frame)
			{
				if (sdlKbState_[SDL_SCANCODE_Q] != 0)
				{
					return true;
				}

				uint8_t* dst = nullptr;
				int rowBytes = 0;
				// For performance reasons we'll just run an assert on the following applicable SDL methods
				auto err = SDL_LockTexture(texture_, nullptr, std::bit_cast<void**>(&dst), &rowBytes);
				assert(err == 0);
				i8080ArcadeIO_->BlitVRAM(std::span(dst, dstRect_.h * rowBytes), dstRect_.w, rowBytes, std::span(*(frame.bitstream.get())), MemoryController::frameWidth);
				SDL_UnlockTexture(texture_);
				// We are done with the frame, return it immediately to the memory controller by explicitly setting it to nullptr
				frame.bitstream = nullptr;
				err = SDL_RenderCopy(renderer_, texture_, nullptr, &dstRect_);
				assert(err == 0);
				SDL_RenderPresent(renderer_);

				auto scrollIndex = [this](Uint8 key, Uint8 lastKey, int dir)
				{
					if (key ^ lastKey && key)
					{
						int romIndex = romIndex_;

						romIndex = (romIndex + dir) % romCount_;

						if (romIndex < 0)
						{
							romIndex = romCount_ - 1;
						}

						romIndex_ = romIndex;
					}

					return key;
				};

				// Halt all channels when returning to the rom select screen
				if (sdlKbState_[SDL_SCANCODE_ESCAPE] == SDL_TRUE)
				{
					// -1: since we don't set any channel tags, use the default
					Mix_HaltGroup(-1);
				}

				// Copy out the values that will be accessed from a different thread.
				// (Do it regardless in single threaded mode, its here for demo purposes only)
				kbState_[SDL_SCANCODE_C] = sdlKbState_[SDL_SCANCODE_C];
				kbState_[SDL_SCANCODE_1] = sdlKbState_[SDL_SCANCODE_1];
				kbState_[SDL_SCANCODE_2] = sdlKbState_[SDL_SCANCODE_2];
				kbState_[SDL_SCANCODE_A] = sdlKbState_[SDL_SCANCODE_A];
				kbState_[SDL_SCANCODE_S] = sdlKbState_[SDL_SCANCODE_S];
				kbState_[SDL_SCANCODE_D] = sdlKbState_[SDL_SCANCODE_D];
				kbState_[SDL_SCANCODE_3] = sdlKbState_[SDL_SCANCODE_3];
				kbState_[SDL_SCANCODE_4] = sdlKbState_[SDL_SCANCODE_4];
				kbState_[SDL_SCANCODE_5] = sdlKbState_[SDL_SCANCODE_5];
				kbState_[SDL_SCANCODE_6] = sdlKbState_[SDL_SCANCODE_6];
				kbState_[SDL_SCANCODE_T] = sdlKbState_[SDL_SCANCODE_T];
				kbState_[SDL_SCANCODE_E] = sdlKbState_[SDL_SCANCODE_E];
				kbState_[SDL_SCANCODE_J] = sdlKbState_[SDL_SCANCODE_J];
				kbState_[SDL_SCANCODE_K] = sdlKbState_[SDL_SCANCODE_K];
				kbState_[SDL_SCANCODE_L] = sdlKbState_[SDL_SCANCODE_L];
				kbState_[SDL_SCANCODE_I] = sdlKbState_[SDL_SCANCODE_I];
				kbState_[SDL_SCANCODE_R] = sdlKbState_[SDL_SCANCODE_R];
				kbState_[SDL_SCANCODE_Y] = sdlKbState_[SDL_SCANCODE_Y];
				kbState_[SDL_SCANCODE_ESCAPE] = sdlKbState_[SDL_SCANCODE_ESCAPE];

				lastUp_ = scrollIndex(sdlKbState_[SDL_SCANCODE_UP], lastUp_, 1);
				lastDown_ = scrollIndex(sdlKbState_[SDL_SCANCODE_DOWN], lastDown_, -1);
				// Check to see if the user wants to load a rom.
				// This will only be acknowledged in GenerateInterrupts if screen_ is RomSelect,
				// we could check screen_ for RomSelect here, but that would mean screen_ would have
				// to be atomic. There is logic in the ServiceInterrupts noInterrupt switch case to handle this.
				// One could remove that logic by making screen_ atomic.
				lastReturn_ = SetInterrupt(sdlKbState_[SDL_SCANCODE_RETURN], lastReturn_, meen::ISR::Load, false);
				return false;
			}
		}, eventData);
	}

	void SDLIoController::HandleError(std::string&& errorMsg)
	{
		if (runAsync_ == true)
		{
			std::unique_lock<std::mutex> ul(eventQMutex_);
			eventQ_.emplace_back(EventData{ errorMsg });
			// eventQCv_.notify_one();
		}
		else
		{
			eventQ_.emplace_back(EventData{ errorMsg });
		}
	}

	void SDLIoController::HandleLoadComplete()
	{
		// We successfully loaded the rom, transition into gameplay.
		screen_ = Screen::Gameplay;
		// Reset the internal state of the hardware
		i8080ArcadeIO_->Reset();

		// Based on what rom we have loaded, we may need to reconfigure the hardware,
		// for example, audio effects which repeat (ufo for space invaders) may not be the same for other roms,
		// need to confirm this.
		// i8080ArcadeIO_->SetOptions(R"({"repeat_samples":[1, 2, 4]})")
	}
} // namespace meen_i8080_arcade