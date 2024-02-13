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

#include "SpaceInvaders/SdlIoController.h"

namespace SpaceInvaders
{
    SdlIoController::SdlIoController(const std::shared_ptr<MemoryController>& memoryController, const nlohmann::json& config)
		: IoController(memoryController, config)
	{
		SDL_SetMainReady();

		if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0)
		{
			throw std::runtime_error("Failed to initialise SDL");
		}

		window_ = SDL_CreateWindow("Space Invaders", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width_, height_, 0/*SDL_WINDOW_FULLSCREEN*/);

		if (window_ == nullptr)
		{
			throw std::bad_alloc();
		}

		renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);

		if (renderer_ == nullptr)
		{
			throw std::bad_alloc();
		}

		// Create the 8bpp texture
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, width_, height_);

		if (texture_ == nullptr)
		{
			throw std::bad_alloc();
		}

		if (Mix_OpenAudio(11025 /* frequency */, 8 /* format (mono) */, 1 /* channels */, 4096 /* sample size */) < 0)
		{
			throw std::runtime_error("Failed to open SDL Mixer");
		}

		// static_assert(wavFiles_.size() == mixChunk_.size());

		for (int i = 0; i < totalWavFiles_; i++)
		{
			mixChunk_[i] = Mix_LoadWAV(wavFiles_[i]);

			if (wavFiles_[i] && mixChunk_[i] == nullptr)
			{
				throw std::bad_alloc();
			}
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

	uint8_t SdlIoController::Read(uint16_t port)
	{
		auto ret = IoController::ReadFrom(port);

		if (ret == 0)
		{
			const auto state = SDL_GetKeyboardState(nullptr);
			auto quit = state[SDL_SCANCODE_Q];

			if (quit == false)
			{
				auto processKeyTable = [&](int scanCode, const char* str, uint8_t bit)
				{
					if (state[scanCode] != 0)
					{
						ret |= bit;
					}
				};

				if (port == 1)
				{
					ret = 0x08;
					processKeyTable(SDL_SCANCODE_C, "Credit", 0x01);
					processKeyTable(SDL_SCANCODE_1, "1P", 0x04);
					processKeyTable(SDL_SCANCODE_2, "2P", 0x02);
					processKeyTable(SDL_SCANCODE_A, "1P Left", 0x20);
					processKeyTable(SDL_SCANCODE_S, "1P Fire", 0x10);
					processKeyTable(SDL_SCANCODE_D, "1P Right", 0x40);
				}
				else if (port == 2)
				{
					processKeyTable(SDL_SCANCODE_3, "3 Ships", 0x00);
					processKeyTable(SDL_SCANCODE_4, "4 Ships", 0x01);
					processKeyTable(SDL_SCANCODE_5, "5 Ships", 0x02);
					processKeyTable(SDL_SCANCODE_6, "6 Ships", 0x03);
					processKeyTable(SDL_SCANCODE_T, "Tilt", 0x04);
					processKeyTable(SDL_SCANCODE_E, "Extra ship at", 0x08);
					processKeyTable(SDL_SCANCODE_J, "2P Left", 0x20);
					processKeyTable(SDL_SCANCODE_K, "2P Fire", 0x10);
					processKeyTable(SDL_SCANCODE_L, "2P Right", 0x40);
					processKeyTable(SDL_SCANCODE_I, "Show coin info", 0x80);
				}
				else
				{
					// force a failure
					assert(port == 0 || port == 3);
					//printf("Unknown device\n");
				}
			}
			else
			{
				SDL_Event e{ .type = SDL_QUIT };
				SDL_PushEvent(&e);
			}
		}

		return ret;
	}

	void SdlIoController::Write(uint16_t port, uint8_t data)
	{
		auto audio = IoController::WriteTo(port, data);

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

	MachEmu::ISR SdlIoController::ServiceInterrupts(uint64_t currTime, uint64_t cycles)
	{
		auto isr = IoController::ServiceInterrupts(currTime, cycles);

		if (isr == MachEmu::ISR::Two)
		{
			auto videoFrame = memoryController_->GetVideoFrame();

			SDL_Event e{};
			e.type = siEvent_;
			e.user.code = EventCode::RenderVideo;

			// Allow events where the vram is nullptr to be pushed so we can track
			// dropped frames in the main thread.
			if (videoFrame.vram != nullptr)
			{
				e.user.data1 = videoFrame.vram.release();
			}

			e.user.data2 = nullptr;
			SDL_PushEvent(&e);
		}

		return isr;
	}

	void SdlIoController::EventLoop()
	{
		SDL_Event e;

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
								auto* src = reinterpret_cast<std::array<uint8_t, MemoryController::VideoFrame::size>*>(e.user.data1);
								uint8_t * dst = nullptr;
								int rowBytes = 0;

								// If src is nullptr then the frame that this interrupt refers to has been dropped.
								if (src != nullptr)
								{
									if (SDL_LockTexture(texture_, nullptr, reinterpret_cast<void**>(&dst), &rowBytes) == 0)
									{
										IoController::Blit(dst, src->data(), rowBytes);
										SDL_UnlockTexture(texture_);
									}

									memoryController_->ReturnVideoFrame({ std::unique_ptr<std::array<uint8_t, MemoryController::VideoFrame::size>>(src) });
								}
								else
								{
									printf ("Dropped Video Frame\n");
								}

								SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
								SDL_RenderPresent(renderer_);
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
} // namespace SpaceInvaders