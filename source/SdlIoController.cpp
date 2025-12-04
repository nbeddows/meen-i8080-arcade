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

#include <algorithm>
#include <assert.h>
#include <bitset>
#include <fstream>

#include "meen_i8080_arcade/MemoryController.h"
#include "meen_i8080_arcade/SdlIoController.h"
#include "meen/Base.h"

namespace meen_i8080_arcade
{
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

		if (sampleRate > 0)
		{
			// Fill out the desired output format
			SDL_AudioSpec desiredSpec{};
			desiredSpec.freq = sampleRate;    // sample rate
			desiredSpec.format = AUDIO_S16;   // 16-bit signed audio
			desiredSpec.channels = 2;         // stereo
			// Allocate a sample buffer large enough to span one video frame duration (video runs at 60Hz).
			// Note: depending on the sample rate this may not be a whole number and will be truncated,
			// hence it could be one sample less than a video frame duration (this should be fine).
			desiredSpec.samples = sampleRate / 60; // Internal sample buffer spanning a video frame duration (approx).

			audioDeviceId_ = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, &obtainedSpec_, 0);

			if (audioDeviceId_ == 0)
			{
				printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
			}

			// Check to make sure we got what we asked for
			if (desiredSpec.freq != obtainedSpec_.freq || desiredSpec.format != obtainedSpec_.format || desiredSpec.channels != obtainedSpec_.channels || desiredSpec.samples != obtainedSpec_.samples)
			{
				printf("Failed to open the audio device with the desired specifications\n");
			}
		}

		SDL_SetEventFilter([](void* eventType, SDL_Event* e)
		{
			// Ignore all events except quit.
			return static_cast<int>(e->type == SDL_QUIT);
		},
		nullptr);

		// Monitor key presses/releases
		sdlKbState_ = SDL_GetKeyboardState(nullptr);
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

		SDL_Quit();
	}

	std::tuple<bool, int> SDLIoController::GetRomIndex()
	{
		return std::tuple(loadSaveState_.exchange(false), romIndex_.load());
	}

	std::error_code SDLIoController::LoadAudioSamples(const JsonVariantConst audioSamples)
	{
		auto scheme = audioSamples["scheme"].as<std::string_view>();
		auto directory = audioSamples["directory"].as<std::string_view>();

		auto addChunk = [this](const uint8_t* wav, int len)
		{
			auto getUint32 = [](const uint8_t* ptr) -> uint32_t
			{
				return (static_cast<uint32_t>(ptr[3]) << 24) | (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[1]) << 8) | static_cast<uint32_t>(ptr[0]);
			};

			auto getUint16 = [](const uint8_t* ptr) -> uint16_t
			{
				return (static_cast<uint16_t>(ptr[1]) << 8) | static_cast<uint16_t>(ptr[0]);
			};

			if (len < 8)
			{
				return std::errc::protocol_not_supported;
			}

			// Check the 'RIFF' fourcc
			if (getUint32(wav) != 0x46464952)
			{
				return std::errc::protocol_not_supported;
			}

			// Make sure we have enough buffer
			if (len - 8 < getUint32(wav + 4))
			{
				return std::errc::value_too_large;
			}

			// Check for 'WAVE' fourcc
			if (getUint32(wav + 8) != 0x45564157)
			{
				return std::errc::protocol_not_supported;
			}

			// Check for 'fmt ' fourcc
			if (getUint32(wav + 12) != 0x20746D66)
			{
				return std::errc::protocol_not_supported;
			}

			// Not supporting extended data
			if (getUint32(wav + 16) != 16)
			{
				return std::errc::no_protocol_option;
			}

			// Only supporting PCM format
			if (getUint16(wav + 20) != 1)
			{
				return std::errc::no_protocol_option;
			}

			// Check for 'data' fourcc
			if (getUint32(wav + 36) != 0x61746164)
			{
				return std::errc::protocol_not_supported;
			}

			// The WAV header is 44 bytes, make sure we have enough buffer
			if (len - 44 < getUint32(wav + 40))
			{
				return std::errc::value_too_large;
			}

			auto nBlockAlign = getUint16(wav + 32);
			auto dataLen = getUint32(wav + 40);

			// We only support 8 bit mono, any other nBlockAlign indicates otherwise
			if (nBlockAlign != 1)
			{
				return std::errc::not_supported;
			}

			// The WAV data is not aligned correctly
			if (dataLen % nBlockAlign != 0)
			{
				return std::errc::illegal_byte_sequence;
			}

			audioChunks_.emplace_back
			(
				getUint16(wav + 22),  // channels
				getUint32(wav + 24),  // sample rate
				getUint32(wav + 28),  // bytes per second
				nBlockAlign,          // nblock align: channels * bitsPerSample / 8
				getUint16(wav + 34),  // bits per sample
				-1,                   // sample index
				std::vector<uint8_t>(wav + 44, wav + 44 + dataLen)
			);

			return std::errc{};
		};

		auto addChunkFromFile = [this, &addChunk](std::string_view directory, std::string_view resource)
		{
			if (resource.empty() == false)
			{
				std::ifstream fin(std::string(directory) + std::string(resource), std::ios::binary);

				if (fin.bad())
				{
					return std::errc::bad_file_descriptor;
				}

				fin.seekg(0, std::ios::end);
				int len = fin.tellg();
				fin.seekg(0, std::ios::beg);
				std::vector<uint8_t> wav(len);
				fin.read(std::bit_cast<char*>(wav.data()), len);
				return addChunk(wav.data(), len);
			}

			audioChunks_.emplace_back();
			return std::errc{};
		};

		auto addChunkFromMem = [this, &addChunk](std::string_view resource, int resourceSize)
		{
			if (resource.empty() == false)
			{
				uintptr_t value = 0;
				auto [ptr, ec] = std::from_chars(resource.data(), resource.data() + resource.size(), value, 10);

				if (ec != std::errc())
				{
					return ec;
				}

				if (ptr != resource.data() + resource.size())
				{
					return std::errc::illegal_byte_sequence;
				}

				return addChunk(std::bit_cast<const uint8_t*>(value), resourceSize);

			}

			audioChunks_.emplace_back();
			return std::errc{};
		};

		for (const auto& sample : audioSamples["sample"].as<JsonArrayConst>())
		{
			auto ec = std::errc{};
			auto dir = directory;
			auto resource = sample["bytes"].as<std::string_view>();

			if (resource.starts_with("mem://"))
			{
				resource.remove_prefix(strlen("mem://"));
				ec = addChunkFromMem(resource, sample["size"].as<int>());
			}
			else if (resource.starts_with("file://"))
			{
				resource.remove_prefix(strlen("file://"));
				// A resource starting with a scheme specifies the exact location of that resource
				dir = "";
				ec = addChunkFromFile(directory, resource);
			}
			else
			{
				if (scheme == "mem://")
				{
					ec = addChunkFromMem(resource, sample["size"].as<int>());
				}
				else if (scheme == "file://")
				{
					ec = addChunkFromFile(directory, resource);
				}
				else
				{
					return std::make_error_code(std::errc::not_supported);
				}
			}

			if (ec != std::errc{})
			{
				return std::make_error_code(ec);
			}
		}

		if (obtainedSpec_.samples > 0)
		{
			for (int i = 0; i < 2 /* total number of audio frames in the pool */; i++)
			{
				audioFramePool_.AddResource(new std::vector<int32_t>(obtainedSpec_.samples));
			}

			// Start audio playback
			SDL_PauseAudioDevice(audioDeviceId_, 0);
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
					// Drop any remaining audio
					SDL_ClearQueuedAudio(audioDeviceId_);
					// Clear all queued chunks and reset chunk->samples to -1
					while (audioMixChunks_.empty() == false)
					{
						auto audioMixChunk = audioMixChunks_.back();
						audioMixChunks_.pop_back();
						audioMixChunk->sampleIndex = -1;
					}

					if (runAsync_ == false)
					{
						// In single threaded mode we need to remove all outstanding i8080 arcade events and return the video frames
						// back to the memory controller so they can be cleared.
						while(eventQ_.empty() == false)
						{
							auto eventData = std::move(eventQ_.front());
							eventQ_.pop_front();

							if (std::holds_alternative<Frame<uint8_t>>(eventData))
							{
								auto& frame = std::get<Frame<uint8_t>>(eventData);
								frame.bitstream = nullptr;
							}
							else if (std::holds_alternative<Frame<int32_t>>(eventData))
							{
								auto& frame = std::get<Frame<int32_t>>(eventData);
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
		std::bitset<8> audio = i8080ArcadeIO_->WritePort(port, data);

		if (audio.count() > 0)
		{
			// port will either be 3 or 5
			// when port is 3 index will be 0 and when it is 5 it will be 8
			// which will give the correct offset into the mixChunk_ array
			auto offset = (port - 3) << 2;

			for (int i = 0; i < 8; i++)
			{
				if (i + offset < audioChunks_.size())
				{
					auto audioChunk = &audioChunks_[i + offset];

					// Drop the sample if it is still playing
					if (audio.test(i) == true && audioChunk->sampleIndex == -1)
					{
						if (audioChunk->samples.empty() == false)
						{
							audioChunk->sampleIndex = 0;
							// Push it to the list of chunks to be played
							audioMixChunks_.emplace_back(audioChunk);
						}
						else
						{
							{
								meen_hw::MH_LockGuard lg(eventQMutex_);
								eventQ_.emplace_back(EventData{ std::string("Audio chunk ") + std::to_string(i) + " is missing from the config file" });
							}

							eventQCv_.notify_one();
						}
					}
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
				Frame<uint8_t> videoFrame;

				if (screen_ == Screen::Gameplay)
				{
					isr = meen::ISR::Two;
				}

				auto mc = static_cast<MemoryController*>(memoryController);

				switch (screen_)
				{
					case Screen::RomSelect:
						videoFrame.bitstream = mc->GetRomSelectFrame(romIndex_, currTime);
						break;
					case Screen::Gameplay:
						videoFrame.bitstream = mc->GetGameplayFrame(currTime);
						break;
					default:
						break;
				}

				if (videoFrame.bitstream != nullptr)
				{
					videoFrame.timestamp = currTime;
					eventData = std::move(videoFrame);
				}
				else
				{
					eventData = "Failed to get the video frame from the memory controller, video frame dropped";
				}

				if (runAsync_ == true)
				{
					{
						meen_hw::MH_LockGuard lg(eventQMutex_);
						eventQ_.push_back(std::move(eventData));
					}

					eventQCv_.notify_one();
				}
				else
				{
					eventQ_.push_back(std::move(eventData));
				}

				// Mix a video frame duration worth of audio if we have any chunks pending
				if (audioMixChunks_.empty() == false)
				{
					Frame<int32_t> audioFrame{ .bitstream = audioFramePool_.GetResource(), .timestamp = currTime };

					if (audioFrame.bitstream != nullptr)
					{
						// Apply some very basic mixing
						// Only supports 8 bit mono samples for input and 16 bit stereo samples for output
						for(auto& sample : *audioFrame.bitstream)
						{
							sample = 0;

							if (audioMixChunks_.empty() == false)
							{
								for (auto audioMixChunk = audioMixChunks_.cbegin(); audioMixChunk != audioMixChunks_.cend();)
								{
									// convert unsigned 8bit sample to a signed 16bit sample mixing it with the current sample
									sample += ((*audioMixChunk)->samples[(*audioMixChunk)->sampleIndex++] - 128) * 256;// / audioMixChunks_.size();

									if ((*(audioMixChunk))->sampleIndex >= (*audioMixChunk)->samples.size())
									{
										// Reset the sample count for when this chunk is next played
										(*audioMixChunk)->sampleIndex = -1;
										// We are done with this chunk, remove it from the list
										audioMixChunk = audioMixChunks_.erase(audioMixChunk);
									}
									else
									{
										audioMixChunk++;
									}
								}

								// Clamp to 16bit to prevent overflow
								sample = std::clamp<int32_t>(sample, -32768, 32767);
								// Duplicate the 16bit mono sample on both channels
								sample = (sample << 16) | sample;
							}
						}

						eventData = std::move(audioFrame);
					}
					else
					{
						eventData = "Failed to get the audio frame from the audio device, audio frame dropped";
					}

					// Send the mixed audio samples to the main thread
					if (runAsync_ == true)
					{
						{
							meen_hw::MH_LockGuard lg(eventQMutex_);
							eventQ_.push_back(std::move(eventData));
						}

						eventQCv_.notify_one();
					}
					else
					{
						eventQ_.push_back(std::move(eventData));
					}
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
			meen_hw::MH_LockGuard lg(eventQMutex_);
			eventQCv_.wait(eventQMutex_, [this]{ return eventQ_.empty() == false; });
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
			[this](Frame<int32_t>& frame)
			{
				SDL_QueueAudio(audioDeviceId_, static_cast<const void*>(frame.bitstream->data()), obtainedSpec_.size);
				// We are done with the frame, return it immediately to the audio frame pool by explicitly setting it to nullptr
				frame.bitstream = nullptr;
				return false;
			},
			[this](Frame<uint8_t>& frame)
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
            meen_hw::MH_LockGuard lg (eventQMutex_);
            eventQ_.emplace_back(EventData{ errorMsg });
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