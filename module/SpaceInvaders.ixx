export module SpaceInvaders;

import <chrono>;
import IController;
import Base;

using namespace std::chrono;
using namespace Emulator;

namespace SpaceInvaders
{
	/** MemoryController

		A custom memory controller targetting the Space Invaders arcade ROM.
	*/
	export class MemoryController final : public IMemoryController
	{
	private:
		size_t memorySize_{};
		std::unique_ptr<uint8_t[]> memory_;
	public:
		MemoryController(uint8_t addressBusSize);
		~MemoryController();

		//IMemoryContoller virtual overrides
		void Load(std::filesystem::path romFile, uint16_t offset) override final;
		size_t Size() const override final;

		//IController virtual overrides
		uint8_t Read(uint16_t address) override final;
		void Write(uint16_t address, uint8_t value) override final;
		ISR ServiceInterrupts(nanoseconds currTime) override final;
	};

	/** IoController
	
		A custom io controller targetting the Space Invaders arcade ROM.
	*/
	export class IoController final : public IController
	{
	private:
		ISR nextInterrupt_{ ISR::NoInterrupt };
		nanoseconds lastTime_{};
		uint8_t shiftIn_{};
		uint8_t shiftAmount_{};
		uint8_t shiftData_{};
	public:
		//IController virtual overrides
		uint8_t Read(uint16_t port) override final;
		void Write(uint16_t port, uint8_t data) override final;
		ISR ServiceInterrupts(nanoseconds currTime) override final;
	};
}