export module SpaceInvaders;

import <chrono>;
import IController;
import Base;
import SystemBus;

using namespace std::chrono;
using namespace Emulator;

namespace SpaceInvaders
{
	/** IoController
	
		A custom io controller targetting the Space Invaders arcade ROM.
	*/
	export class IoController : public IController
	{
	private:
		ISR nextInterrupt_{ ISR::NoInterrupt };
		std::shared_ptr<ControlBus<8>> controlBus_;
		nanoseconds lastTime_;
		uint8_t shiftIn_{};
		uint8_t shiftAmount_{};
		uint8_t shiftData_{};
	public:
		IoController(std::shared_ptr<ControlBus<8>> controlBus);
		uint8_t Read(uint16_t port) override final;
		void Write(uint16_t port, uint8_t data) override final;
		ISR ServiceInterrupts(nanoseconds currTime) override final;
	};
}