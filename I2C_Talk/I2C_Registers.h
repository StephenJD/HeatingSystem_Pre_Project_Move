#pragma once
#include <Arduino.h>
#include <I2C_Talk.h>
#include <MsTimer2.h>

//inline void stopTimer() { MsTimer2::stop(); digitalWrite(LED_BUILTIN, LOW); }
//inline void flashLED(int period) {
//	pinMode(LED_BUILTIN, OUTPUT);
//	digitalWrite(LED_BUILTIN, HIGH);
//	MsTimer2::set(period, stopTimer);
//	MsTimer2::start();
//}

namespace i2c_registers {
	struct Defaut_Tag_None {};

	// mono-state class - all static data. Each template instantiation gets its own data.
	template<I2C_Talk& i2C, int register_size, typename PurposeTag = Defaut_Tag_None>
	class Registers {
	public:
		void setRegister(int reg, uint8_t value) { _regArr[reg] = value; }
		void modifyRegister(int reg, uint8_t increment) { _regArr[reg] += increment; }
		uint8_t getRegister(int reg) { return _regArr[reg]; }
		int noOfRegisters() { return register_size; }
		uint8_t* reg_ptr(int reg) { return _regArr + reg; }

		// Called when data is sent by a Master, telling this slave how many bytes have been sent.
		static void receiveI2C(int howMany) {
			i2C.receiveFromMaster(1, &_regAddr); // first byte is reg-address
			if (--howMany) {
				//flashLED(10);
				if (_regAddr + howMany > register_size) howMany = register_size - _regAddr;
				auto noReceived = i2C.receiveFromMaster(howMany, _regArr + _regAddr);
				//_regAddr += howMany;
				//Serial.flush(); Serial.print(noReceived); Serial.print(F(" for RAdr: ")); Serial.print((int)_regAddr); Serial.print(F(" V: ")); Serial.println((int)_regArr[_regAddr]);
			}
		}

		// Called when data is requested by a Master from this slave, reading from the last register address sent.
		static void requestI2C() { 
			// The Master will send a NACK when it has received the requested number of bytes, so we should offer as many as could be requested.
			//flashLED(10);
			int bytesAvaiable = register_size - _regAddr;
			if (bytesAvaiable > 32) bytesAvaiable = 32;
			i2C.write(_regArr + _regAddr, bytesAvaiable);
			//Serial.flush(); Serial.print(F("SAdr:")); Serial.print((int)_regAddr); 
			//Serial.print(F(" Len:")); Serial.print((int)bytesAvaiable);
			//Serial.print(F(" Val:")); Serial.println((int)*(_regArr + _regAddr));
		}
	private:
		static uint8_t _regArr[register_size];
		static uint8_t _regAddr; // the register address sent in the request
	};

	template<I2C_Talk& i2C, int register_size, typename PurposeTag>
	uint8_t Registers<i2C, register_size, PurposeTag>::_regArr[register_size];

	template<I2C_Talk& i2C, int register_size, typename PurposeTag>
	uint8_t Registers<i2C, register_size, PurposeTag>::_regAddr;
}
