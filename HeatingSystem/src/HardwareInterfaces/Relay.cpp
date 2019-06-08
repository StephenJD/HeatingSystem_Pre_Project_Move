#include "Relay.h"
#include "A__Constants.h"
#include <Logging.h>

namespace HardwareInterfaces {

	/////////////////////////////////////
	//       RelaysPort Functions      //
	/////////////////////////////////////

	RelaysPort::RelaysPort(I2C_Talk & i2C, int addr, int zeroCrossPin, int resetPin)
		: I_I2Cdevice{ i2C, addr }
		, _zeroCrossPin( zeroCrossPin )
		, _resetPin( resetPin )
	{
		pinMode(abs(_zeroCrossPin), INPUT);
		logger().log("RelaysPort::setup()");
		digitalWrite(abs(_zeroCrossPin), HIGH); // turn on pullup resistors
		pinMode(abs(_resetPin), OUTPUT);
		digitalWrite(abs(_resetPin), (_resetPin < 0) ? HIGH : LOW);
	}

	uint8_t RelaysPort::initialiseDevice() {
		uint8_t pullUp_out[] = { 0 };
		uint8_t hasFailed;
		//uint8_t hasFailed = _i2C->i2C_is_frozen(_address);
		//if (hasFailed) {
		//	logger().log("  Initialise RelaysPort() is stuck - call hard reset...");
		//	//bareResetI2C(*i_2C, _address);
		//}
		//else {
			hasFailed = write_verify(REG_8PORT_PullUp, 1, pullUp_out); // clear all pull-up resistors
			//if (hasFailed) {
			//	logger().log("Initialise RelaysPort() write-verify failed at Freq:", _i2C->getI2CFrequency());
			//}
			//else {
				hasFailed = write_verify(REG_8PORT_OLAT, 1, &relayRegister); // set latches
				writeAtZeroCross();
				hasFailed |= write_verify(REG_8PORT_IODIR, 1, pullUp_out); // set all as outputs
				//if (hasFailed) logger().log("Initialise RelaysPort() lat-write failed at Freq:", _i2C->getI2CFrequency());
				//else logger().log("Initialise RelaysPort() succeeded at Freq:", _i2C->getI2CFrequency());
			//}
		//}
		return hasFailed;
	}

	uint8_t RelaysPort::setAndTestRegister() {
		uint8_t ANDmask = 0x7F;
		uint8_t error;
		//if (_i2C->notExists(_address)) error = I2C_Talk::_I2C_Device_Not_Found;
		//else {
			writeAtZeroCross();
			error = write_verify(REG_8PORT_OLAT & ANDmask, 1, &relayRegister);
		//}
		//logger().log("RelaysPort::setAndTestRegister() addr:", _address,_i2C->getStatusMsg(error));
		return error;
	}

	/////////////////////////////////////
	//       Relay Functions      //
	/////////////////////////////////////

	bool Relay::getRelayState() const {
		uint8_t myBit = RelaysPort::relayRegister & (1 << port());
		return !(myBit^activeState()); // Relay state
	}

	bool Relay::setRelay(uint8_t state) { // returns true if state is changed
		uint8_t myBitMask = 1 << port();
		uint8_t myBit = (RelaysPort::relayRegister  & myBitMask) != 0;
		uint8_t currState = !(myBit^activeState());

		myBit = !(state^activeState()); // Required bit state 
		if (!myBit) { // clear bit
			RelaysPort::relayRegister &= ~myBitMask;
		}
		else { // set this bit
			RelaysPort::relayRegister |= myBitMask;
		}
		return currState != state; // returns true if state is changed
	}
}