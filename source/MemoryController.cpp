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

#include <cstring>
#include <fstream>

#include "SpaceInvaders/MemoryController.h"

namespace SpaceInvaders
{
	MemoryController::MemoryController()
		: memory_{ std::make_unique<uint8_t[]>(memorySize_) }
	{
		for (int i = 0; i < 1 /* frame pool size */; i++)
		{
			framePool_.push_back({ std::make_unique<std::array<uint8_t, VideoFrame::size>>() });
		}
	}

	MemoryController::VideoFrame MemoryController::GetVideoFrame() const
	{
		VideoFrame frame;
		// This method is called from ServiceInterrupts so we don't 
		// want to block waiting for this mutex as we could stall the cpu,
		// if we don't get it, this frame will be dropped (host is too
		// slow, the machine clock resolution is too high or the function
		// call spuriously failed).
		auto locked = frameMutex_.try_lock();
	
		if (locked == true)
		{
			if (framePool_.empty() == false)
			{
				frame = std::move(framePool_.back());
				framePool_.pop_back();
			}

			frameMutex_.unlock();

			if (frame.vram != nullptr)
			{
				auto data = frame.vram.get()->data();
				auto size = frame.vram.get()->size();
				auto vram = memory_.get() + 0x2400;

				memcpy(data, vram, size);
			}
		}

		return frame;
	}

	void MemoryController::ReturnVideoFrame(VideoFrame&& frame)
	{
		// The resource has to be returned, otherwise the 'pipeline' will stall.
		std::lock_guard<std::mutex> lock(frameMutex_);
		framePool_.push_back(std::move(frame));
	}

	size_t MemoryController::Size() const
	{
		return memorySize_;
	}

	void MemoryController::Load(const char* romFile, uint16_t offset)
	{
		std::ifstream fin(romFile, std::ios::binary | std::ios::ate);

		if (!fin)
		{
			throw std::runtime_error("The program file failed to open");
		}

		if (static_cast<size_t>(fin.tellg()) > memorySize_)
		{
			throw std::length_error("The length of the program is too big");
		}

		uint16_t size = static_cast<uint16_t>(fin.tellg());

		if (size > memorySize_ - offset)
		{
			throw std::length_error("The length of the program is too big to fit at the specified offset");
		}

		fin.seekg(0, std::ios::beg);

		if (!(fin.read(reinterpret_cast<char*>(&memory_[offset]), size)))
		{
			throw std::invalid_argument("The program specified failed to load");
		}
	}

	uint8_t MemoryController::Read(uint16_t addr)
	{
		return memory_[addr];
	}

	void MemoryController::Write(uint16_t addr, uint8_t data)
	{
		memory_[addr] = data;
	}

	MachEmu::ISR MemoryController::ServiceInterrupts([[maybe_unused]] uint64_t currTime, [[maybe_unused]] uint64_t cycles)
	{
		return MachEmu::ISR::NoInterrupt;
	}
} // namespace SpaceInvaders