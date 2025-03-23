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
#include <future>

#include "i8080_arcade/MemoryController.h"
#include "i8080_arcade/SdlIoController.h"
#include "meen/Base.h"

namespace i8080_arcade
{
    SDLIoController::SDLIoController(const JsonVariant& audioHardware, const JsonVariant& videoHardware)
	{
		SDL_SetMainReady();

		if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0)
		{
			printf("Failed to initialise SDL");
		}

		window_ = SDL_CreateWindow("i8080 arcade",
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

		if (Mix_OpenAudio(audioHardware["sampleRate"].as<int>(), 8 /* format (mono) */, audioHardware["channels"].as<int>(), audioHardware["sampleSize"].as<int>()) < 0)
		{
			printf("Failed to open SDL Mixer");
		}

		siEvent_ = SDL_RegisterEvents(1);

		if (siEvent_ == 0xFFFFFFFF)
		{
			printf("Exhausted all user level events");
		}

		SDL_SetEventFilter([](void* eventType, SDL_Event* e)
		{
			// Ignore all events except ours.
			if (reinterpret_cast<uint64_t>(eventType) == e->type || e->type == SDL_QUIT)
			{
				return 1;
			}
			else
			{
				return 0;
			}
		},
		reinterpret_cast<void*>(siEvent_));

		for(int i = 0; i < 1; i++)
		{
			videoFrameWrapperPool_.emplace_back(std::make_unique<VideoFrameWrapper>());
		}
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

		SDL_Quit();
	}

	std::tuple<bool, int> SDLIoController::GetRomIndex(int maxSize)
	{
		return std::tuple(loadSaveState_.exchange(false), 0);
	}

	std::error_code SDLIoController::LoadAudioSamples(const JsonVariant& audio)
	{
		auto scheme = audio["scheme"].as<std::string>();
		auto directory = audio["directory"].as<std::string>();

		auto addChunk = [&mixChunk = mixChunk_](const std::string& directory, const std::string& resource)
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

		for(const auto& sample : audio["sample"].as<JsonArray>())
		{
			auto dir = directory;
			auto resource = sample.as<std::string>();
			
			if (resource.starts_with("file://"))
			{
				resource.erase(strlen("file://"));
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

	std::error_code SDLIoController::LoadVideoTextures(const JsonVariant& videoTextures)
	{
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

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		texture_ = SDL_CreateTexture(renderer_, pf, SDL_TEXTUREACCESS_STREAMING, i8080ArcadeIO_->GetVRAMWidth(), i8080ArcadeIO_->GetVRAMHeight());

		if (texture_ == nullptr)
		{
			return std::make_error_code (std::errc::not_enough_memory);
		}

		return std::error_code{};
	}

	uint8_t SDLIoController::Read(uint16_t port, [[maybe_unused]] meen::IController* controller)
	{
		auto ret = i8080ArcadeIO_->ReadPort(port);

		if (ret == 0)
		{
			if (port == 1 || port == 2)
			{
				std::promise<uint8_t> p;
				SDL_Event e{};
				e.type = siEvent_;
				e.user.code = EventCode::ReadInput;
				e.user.data1 = reinterpret_cast<void*>(port);
				e.user.data2 = reinterpret_cast<void*>(&p);
				SDL_PushEvent(&e);					
				
				if (quit_ == false)
				{
					ret = p.get_future().get();
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
			SDL_Event e{};
			e.type = siEvent_;
			e.user.code = EventCode::RenderAudio;
			e.user.data1 = reinterpret_cast<void*>(port);
			e.user.data2 = reinterpret_cast<void*>(audio);
			SDL_PushEvent(&e);
		}
	}

	meen::ISR SDLIoController::ServiceInterrupts(uint64_t currTime, uint64_t cycles, meen::IController* memoryController)
	{
		meen::ISR isr{};

		auto interrupt = i8080ArcadeIO_->GenerateInterrupt(currTime, cycles);

		switch(interrupt)
		{
			case 0:
			{
				isr = loadSaveInterrupt_.exchange(meen::ISR::NoInterrupt);
				break;
			}
			case 1:
			{
				isr = meen::ISR::One;
				break;
			}
			case 2:
			{
				isr = meen::ISR::Two;
				VideoFrameWrapper* videoFrameWrapper = nullptr; 

				{
					std::lock_guard<std::mutex> lg(videoFrameWrapperMutex_);

					if(videoFrameWrapperPool_.empty() == false)
					{
						videoFrameWrapper = videoFrameWrapperPool_.back().release();
					}
				}

				if(videoFrameWrapper != nullptr)
				{
					// GetVideoFrame accepts a parameter for rom select screen or game play, memory controller will generate the next rom select frame or game play frame
					videoFrameWrapper->videoFrame = static_cast<MemoryController*>(memoryController)->GetVideoFrame();
				}

				SDL_Event e{};
				e.type = siEvent_;
				e.user.code = EventCode::RenderVideo;
				// Allow events where the vram is nullptr to be pushed so we can track
				// dropped frames in the main thread.
				e.user.data1 = videoFrameWrapper;
				e.user.data2 = nullptr;
				SDL_PushEvent(&e);
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
		bool quit = false;
		const auto state = SDL_GetKeyboardState(nullptr);

		if (SDL_WaitEvent(&e))
		{
			// Scan the keyboard for load and save requests
			auto setInterrupt = [this](Uint8 key, Uint8 lastKey, meen::ISR isr, bool loadSaveState)
			{
				if (key ^ lastKey && key)
				{
					loadSaveInterrupt_ = isr;
					loadSaveState_ = loadSaveState;
				}

				return key;
			};

			switch (e.type)
			{
				case SDL_QUIT:
				{
					quit_ = quit = true;
					break;
				}
				default:
				{
					if(e.type == siEvent_)
					{
						switch (e.user.code)
						{
							case EventCode::RenderVideo:
							{
								quit = state[SDL_SCANCODE_Q];

								if (quit == true)
								{
									quit_ = true;
									
									if (SDL_PollEvent(&e))
									{
										if (e.type == siEvent_ && e.user.code == EventCode::ReadInput)
										{
											static_cast<std::promise<uint8_t>*>(e.user.data2)->set_value(0);
										}
									}
									break;
								}

								auto videoFrameWrapper = std::bit_cast<VideoFrameWrapper*>(e.user.data1);

								if (videoFrameWrapper != nullptr)
								{
									// Move the frame out of the wrapper. This must be done as it allows the
									// video frame to be returned back to the memory controller, alternatively,
									// one could set videoFrameWrapper->videoFrame to nullptr once it is no longer
									// needed.
									auto videoFrame = std::move(videoFrameWrapper->videoFrame);
									// We are done with the wrapper, return it back to the wrapper pool
									{
										std::lock_guard<std::mutex> lg(videoFrameWrapperMutex_);
										videoFrameWrapperPool_.push_back(std::unique_ptr<VideoFrameWrapper>(videoFrameWrapper));
									}

									if (videoFrame != nullptr)
									{
										uint8_t* dst = nullptr;
										int rowBytes = 0;

										if (SDL_LockTexture(texture_, nullptr, std::bit_cast<void**>(&dst), &rowBytes) == 0)
										{
											i8080ArcadeIO_->BlitVRAM(std::span(dst, i8080ArcadeIO_->GetVRAMHeight() * rowBytes), rowBytes, std::span(*videoFrame));
											SDL_UnlockTexture(texture_);
										}
										else
										{
											printf("Failed to lock texture, video frame dropped\n");
										}
									}
									else
									{
										printf("Video frame dropped\n");
									}
								}
								else
								{
									printf("Wrapper empty, video frame dropped\n");
								}

								SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
								SDL_RenderPresent(renderer_);

								// Check to see if the user wants to load a rom
								lastR_ = setInterrupt(state[SDL_SCANCODE_U], lastR_, meen::ISR::Load, false);
								break;
							}
							case EventCode::RenderAudio:
							{
								uint8_t port = reinterpret_cast<uint64_t>(e.user.data1);
								std::bitset<8> audio = reinterpret_cast<uint64_t>(e.user.data2);
								// port will either be 3 or 5
								// when port is 3 index will be 0 and when it is 5 it will be 8
								// which will give the correct offset into the mixChunk_ array
								auto offset = (port - 3) << 2;

								for (int i = 0; i < 8; i++)
								{
									if (audio.test(i) == true)
									{
										[[maybe_unused]] auto busy = Mix_PlayChannel(-1 /* use the next available channel */, mixChunk_[i + offset], 0 /* don't loop (play it once) */);
										// We are playing 8 (default maximum) samples at the same time, this should not happen!
										// We are trying to play a track which isn't loaded (an unknown data bit is set?!?!)
										//assert(mixChunk_[i] == nullptr || busy != -1);
									}
								}
								break;
							}
							case EventCode::ReadInput:
							{
								uint8_t port = reinterpret_cast<uint64_t>(e.user.data1);
								auto p = static_cast<std::promise<uint8_t>*>(e.user.data2);
								uint8_t value = 0;

								// We can only load from a save file or save if a rom is currently running (engine will generate an error otherwise)
								// so we only allow the user to perform these operations when a rom is running only.
								// (the ReadInput event is only triggered when a rom is running)
								lastU_ = setInterrupt(state[SDL_SCANCODE_R], lastR_, meen::ISR::Load, true);
								lastY_ = setInterrupt(state[SDL_SCANCODE_Y], lastY_, meen::ISR::Save, false);

								if (port == 1)
								{
									value = 0x08;
									value |= (state[SDL_SCANCODE_C] * 0x01); // Credit
									value |= (state[SDL_SCANCODE_1] * 0x04); // 1P
									value |= (state[SDL_SCANCODE_2] * 0x02); // 2P
									value |= (state[SDL_SCANCODE_A] * 0x20); // 1P Left
									value |= (state[SDL_SCANCODE_S] * 0x10); // 1P Fire
									value |= (state[SDL_SCANCODE_D] * 0x40); // 1P Right
								}
								else if (port == 2)
								{
									value |= (state[SDL_SCANCODE_3] * 0x00); // 3 Ships
									value |= (state[SDL_SCANCODE_4] * 0x01); // 4 Ships
									value |= (state[SDL_SCANCODE_5] * 0x02); // 5 Ships
									value |= (state[SDL_SCANCODE_6] * 0x03); // 6 Ships
									value |= (state[SDL_SCANCODE_T] * 0x04); // Tilt
									value |= (state[SDL_SCANCODE_E] * 0x08); // Extra Ship at
									value |= (state[SDL_SCANCODE_J] * 0x20); // 2P Left
									value |= (state[SDL_SCANCODE_K] * 0x10); // 2P Fire
									value |= (state[SDL_SCANCODE_L] * 0x40); // 2P Right
									value |= (state[SDL_SCANCODE_I] * 0x80); // Show coin info
								}
								else
								{
									printf("Invalid Read Port: %d\n", port);
								}

								p->set_value(value);
								break;
							}
							default:
							{
								break;
							}
						}
					}
					break;
				}
			}
		}

		return quit;
	}

	void SDLIoController::HandleError(std::string&& errorMsg)
	{
		printf(errorMsg.c_str());
	}

} // namespace i8080_arcade