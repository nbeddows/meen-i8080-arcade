/*
Copyright (c) 2021-2024 Nicolas Beddows <nicolas.beddows@gmail.com>

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

#include "i8080_arcade/SdlIoController.h"

namespace i8080_arcade
{
    SdlIoController::SdlIoController(const std::shared_ptr<MemoryController>& memoryController, const nlohmann::json& audioHardware, const nlohmann::json& videoHardware)
		: memoryController_{ memoryController }
	{
		SDL_SetMainReady();

		if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0)
		{
			throw std::runtime_error("Failed to initialise SDL");
		}

		window_ = SDL_CreateWindow("i8080 arcade",
								SDL_WINDOWPOS_UNDEFINED,
								SDL_WINDOWPOS_UNDEFINED,
								videoHardware["width"].get<int>(),
								videoHardware["height"].get<int>(),
								videoHardware["full-screen"].get<bool>() ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

		if (window_ == nullptr)
		{
			throw std::bad_alloc();
		}

		renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);

		if (renderer_ == nullptr)
		{
			printf("Failed to allocate an accelerated renderer, falling back to software\n");
			renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);

			if (renderer_ == nullptr)
			{
				throw std::runtime_error("Failed to allocate an SDL renderer");
			}
		}

		i8080ArcadeIO_ = meen_hw::MakeI8080ArcadeIO();

		if(i8080ArcadeIO_ == nullptr)
		{
			throw std::runtime_error("Failed to create i8080 arcade hardware");
		}

		if (Mix_OpenAudio(audioHardware["sample-rate"].get<int>(), 8 /* format (mono) */, audioHardware["channels"].get<int>(), audioHardware["sample-size"].get<int>()) < 0)
		{
			throw std::runtime_error("Failed to open SDL Mixer");
		}

		siEvent_ = SDL_RegisterEvents(1);

		if (siEvent_ == 0xFFFFFFFF)
		{
			throw std::runtime_error("Exhausted all user level events");
		}

		SDL_SetEventFilter([](void* eventType, SDL_Event* e)
		{
			// Ignore all events other than ours.
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

	SdlIoController::~SdlIoController()
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

	void SdlIoController::LoadAudioSamples(const std::filesystem::path& audioFilePath, const nlohmann::json& audio)
	{
		for(const auto& file : audio["file"])
		{
			auto name = file.get<std::string>();
			auto mixChunk = Mix_LoadWAV((audioFilePath/name).string().c_str());

			if(name.empty() == false && mixChunk == nullptr)
			{
				throw std::runtime_error("Failed to load audio sample");
			}

			mixChunk_.emplace_back(mixChunk);
		}
	}

	void SdlIoController::LoadVideoTextures(const nlohmann::json& videoTextures)
	{
		i8080ArcadeIO_->SetOptions(videoTextures.dump().c_str());
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, i8080ArcadeIO_->GetVRAMWidth(), i8080ArcadeIO_->GetVRAMHeight());

		if (texture_ == nullptr)
		{
			throw std::bad_alloc();
		}
	}

	uint8_t SdlIoController::Read(uint16_t port)
	{
		uint8_t ret = 0;

		if (quit_ == false)
		{
			ret = i8080ArcadeIO_->ReadPort(port);

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
					ret = p.get_future().get();
				}
			}
		}

		return ret;
	}

	void SdlIoController::Write(uint16_t port, uint8_t data)
	{
		if (quit_ == false)
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
	}

	MachEmu::ISR SdlIoController::ServiceInterrupts(uint64_t currTime, uint64_t cycles)
	{
		auto isr = MachEmu::ISR::Quit;

		if(quit_ == false)
		{
			auto interrupt = i8080ArcadeIO_->GenerateInterrupt(currTime, cycles);
	
			switch(interrupt)
			{
				case 0:
				{
					isr = loadSaveInterrupt_.exchange(MachEmu::ISR::NoInterrupt);				
					break;
				}
				case 1:
				{
					isr = MachEmu::ISR::One;
					break;
				}
				case 2:
				{
					isr = MachEmu::ISR::Two;
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
						videoFrameWrapper->videoFrame = memoryController_->GetVideoFrame();	
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
		}

		return isr;
	}

	std::array<uint8_t, 16> SdlIoController::Uuid() const
	{
		return{ 0x22, 0x61, 0xC9, 0x53, 0x9A, 0x36, 0x4B, 0xD3, 0xB9, 0x68, 0x47, 0x67, 0x6F, 0x52, 0x6D, 0x48 };
	}

	void SdlIoController::EventLoop()
	{
		SDL_Event e;
		Uint8 lastR = 0;
		Uint8 lastY = 0;
		const auto state = SDL_GetKeyboardState(nullptr);

		while (quit_ == false && SDL_WaitEvent(&e))
		{
			switch (e.type)
			{
				case SDL_QUIT:
				{
					quit_ = true;
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
											i8080ArcadeIO_->BlitVRAM(std::span(dst, i8080ArcadeIO_->GetVRAMWidth() * i8080ArcadeIO_->GetVRAMHeight()), rowBytes, std::span(*videoFrame));
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

								// Scan the keyboard for load and save requests, we'll lock this to
								// the renderer, ie; check for these requests 60 times per second
								auto setInterrupt = [this](Uint8 key, Uint8 lastKey, MachEmu::ISR isr)
								{
									if (key ^ lastKey && key)
									{
										loadSaveInterrupt_ = isr;
									}

									return key;
								};

								lastR = setInterrupt(state[SDL_SCANCODE_R], lastR, MachEmu::ISR::Load);
								lastY = setInterrupt(state[SDL_SCANCODE_Y], lastY, MachEmu::ISR::Save);
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
								quit_ = state[SDL_SCANCODE_Q];

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
	}
} // namespace i8080_arcade