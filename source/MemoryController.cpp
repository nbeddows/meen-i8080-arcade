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

#include <algorithm>
#include <cstring>
#include <fstream>

#include "i8080_arcade/MemoryController.h"

namespace i8080_arcade
{
	MemoryController::MemoryController(int framePoolSize)
		: memory_{ std::make_unique<uint8_t[]>(memorySize_) }
	{
		framePool_ = meen_hw::MH_ResourcePool<std::array<uint8_t, 7168>>();

		for(int i = 0; i < framePoolSize; i++)
		{
			framePool_.AddResource(new std::array<uint8_t, 7168>);
		}
	}

	meen_hw::MH_ResourcePool<std::array<uint8_t, 7168>>::ResourcePtr MemoryController::GetVideoFrame() const
	{
		auto frame = framePool_.GetResource();

		if(frame != nullptr)
		{
			std::copy_n(memory_.get() + 0x2400, frame->size(), frame->begin());
		}

		return frame;
	}

	size_t MemoryController::Size() const
	{
		return memorySize_;
	}

    void MemoryController::LoadRoms(const std::filesystem::path& romFilePath, const nlohmann::json& files)
	{
		for(const auto& file : files)
		{
			std::ifstream fin(romFilePath/file["name"].get<std::string>(), std::ios::binary | std::ios::ate);

			if (!fin)
			{
				throw std::runtime_error("The program file failed to open");
			}

			if (static_cast<size_t>(fin.tellg()) > memorySize_)
			{
				throw std::length_error("The length of the program is too big");
			}

			uint16_t size = static_cast<uint16_t>(fin.tellg());
			uint16_t offset = file["offset"].get<uint16_t>();

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

	std::array<uint8_t, 16> MemoryController::Uuid() const
	{
		return{ 0x5C, 0x64, 0x7C, 0xCB, 0x71, 0x2E, 0x4A, 0x0B, 0x8A, 0x26, 0x1D, 0xE2, 0x95, 0x44, 0xA1, 0xE9 };
	}
} // namespace i8080_arcade