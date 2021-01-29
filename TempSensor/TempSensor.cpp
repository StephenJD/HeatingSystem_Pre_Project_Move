#include "TempSensor.h"
#include <I2C_Recover.h>
#include "Logging.h"

using namespace I2C_Talk_ErrorCodes;

namespace HardwareInterfaces {
	const uint8_t DS75LX_Temp = 0x00;  // two bytes must be read. Temp is MS 9 bits, in 0.5 deg increments, with MSB indicating -ve temp.
	const uint8_t DS75LX_Config = 0x01;
	const uint8_t DS75LX_HYST_REG = 0x02;
	//Error_codes TempSensor::_error;

	TempSensor::TempSensor(I2C_Recovery::I2C_Recover & recover, int addr) : I_I2Cdevice_Recovery(recover, addr) {
		recover.i2C().setMax_i2cFreq(400000);
	} // initialiser for first array element 
	
	TempSensor::TempSensor(I2C_Recovery::I2C_Recover & recover) : I_I2Cdevice_Recovery(recover) {
		recover.i2C().setMax_i2cFreq(400000);
	}


	void TempSensor::initialise(int address) {
		setAddress(address);
	}

	uint8_t TempSensor::setHighRes() {
		_error = write(DS75LX_Config, 0x60);
		return _error;
	}

	int8_t TempSensor::get_temp() const {
		return (get_fractional_temp() + 128) / 256;
	}

	int16_t TempSensor::get_fractional_temp() const {
#ifdef TEST_MIX_VALVE_CONTROLLER
		extern S2_byte tempSensors[2];
		return tempSensors[address];
#else
		return _lastGood;
#endif
	}

	Error_codes TempSensor::readTemperature() {
		constexpr int CONSECUTIVE_COUNT = 2;
		constexpr int MAX_TRIES = 6;
		constexpr int DATA_MASK = 0xFFFF;
		uint8_t newTemp[2]; // MSB = units, LSB = fraction
		_error = read_verify_2bytes(DS75LX_Temp, newTemp, CONSECUTIVE_COUNT, MAX_TRIES, DATA_MASK); // Non-Recovery
		checkTemp((newTemp[0] << 8) + newTemp[1]);

#ifdef ZPSIM
		//  if (getAddress() == 0x70) _error = _NACK_during_data_send;
			//bool debug = true;
		//_lastGood += change;
		if (_lastGood < 2560) change = 256;
		if (_lastGood > 17920) change = -256;
#endif
		if (_error != _OK) {
			logger() << F("Temp Error for 0x") << L_hex << getAddress() << L_endl;
			_error = read(DS75LX_Temp, 2, newTemp); // Recovery
		}
		return _error;
	}

	auto TempSensor::checkTemp(int16_t newTemp)->I2C_Talk_ErrorCodes::Error_codes {
		if (_error == _OK) {
			auto lsNibble = newTemp & 0x000F;
			if (lsNibble) {
				logger() << F("Temp LSNibble Error for 0x") << L_hex << getAddress() << " Was: 0x" << lsNibble << L_endl;
				_error = _I2C_ReadDataWrong;
			} else {
				if (_lastGood != 0 && abs(_lastGood - newTemp) > 1280) {
					_error = _I2C_ReadDataWrong;
					logger() << F("Temp Jump Error for 0x") << L_hex << getAddress() << " Was: " << L_dec << _lastGood << F(" Now: ") << newTemp << L_endl;
				}
				_lastGood = newTemp;
			}
		}
		return _error;
	}

	Error_codes TempSensor::testDevice() { // non-recovery test
		uint8_t temp[2] = {75,0};
		_error = I_I2Cdevice::write_verify(DS75LX_HYST_REG, 2, temp);
		if (_error == _OK) {
			_error = I_I2Cdevice::read(DS75LX_Temp, 2, temp);
			checkTemp((temp[0] << 8) + temp[1]);
		}
		return _error;
	}
}