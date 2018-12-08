#include "Temp_Sensor.h"
#include "I2C_Helper.h"

iTemp_Sensor::iTemp_Sensor() :lastGood(30*256) {
	#ifdef ZPSIM
	 change = 256;
	#endif
}

int iTemp_Sensor::error = 0;

S1_byte iTemp_Sensor::get_temp() const {
	return (get_fractional_temp() + 128) / 256;
}

/////////// I2C_Temp_Sensor Specialisation ///////////////

const U1_byte DS75LX_Temp = 0x00;  // two bytes must be read. Temp is MS 9 bits, in 0.5 deg increments, with MSB indicating -ve temp.
const U1_byte DS75LX_Config = 0x01;

I2C_Helper * I2C_Temp_Sensor::i2C = 0;

void I2C_Temp_Sensor::setI2C(I2C_Helper & _i2C) {i2C = &_i2C;}

I2C_Temp_Sensor::I2C_Temp_Sensor(int address, bool is_hi_res)
	:address(address)
	{
		if (i2C != 0) {
			if (is_hi_res) i2C->write(address, DS75LX_Config, 0x60);
			Serial.println("I2C_Temp_Sensor created");
		} else error = I2C_Helper::_I2C_not_created;
	}

S2_byte I2C_Temp_Sensor::get_fractional_temp() const {
	U1_byte temp[2];
	if (i2C != 0) {
		error = i2C->read(address, DS75LX_Temp, 2, temp);
	} else error = I2C_Helper::_I2C_not_created;
#ifdef ZPSIM
	lastGood += change;
	temp[0] = lastGood / 256;
	if (lastGood < 7680) change = 256;
	if (lastGood > 17920) change = -256;
	temp[1] = 0;
#endif
	S2_byte returnVal;
	if (error) {
		returnVal = lastGood;
	} else {
		returnVal = (temp[0]<<8) | temp[1];
		lastGood = returnVal;
	}
#ifdef TEST_MIX_VALVE_CONTROLLER
	extern S2_byte tempSensors[2];
	return tempSensors[address];
#else
	return returnVal;
#endif
}