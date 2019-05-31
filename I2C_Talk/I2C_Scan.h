#pragma once
#include <Arduino.h>

/// <summary>
/// Usage:
///	<para>auto i2C = I2C_Talk{};</para>
///	<para>auto scanner = I2C_Scan(i2C);</para>
///	<para>scanner.show_all()// scan all addresses and print results to serial port</para>
///	
///	or to LCD ...
///	<para>scanner.reset(); // reset to start scanning at 0</para>
///	<para>while (scanner.next()){ // return after each device found. Don't output to serial port.</para>
///	<para>	lcd->print(scanner.foundDeviceAddr,HEX); lcd->print(" ");</para>
///	<para>}</para>
///	</summary>
class I2C_Scan {
public:
	I2C_Scan(I2C_Talk & i2C) : _i2C(&i2C) {}
	
	void reset();
	bool next() { return next_T<false, false>(); }
	bool show_all() { return next_T<true, true>(); }
	bool show_next() { return next_T<false, true>(); }
	
	uint8_t	error = I2C_Talk::_OK;
	int8_t foundDeviceAddr = -1;	// -1 to 127
	uint8_t totalDevicesFound = 0;	

private:
	template<bool non_stop, bool serial_out>
	bool next_T();

	bool nextDevice();
	I2C_Talk * _i2C;
};

//*************************************************************************************
// Template Function implementations //
template<bool non_stop, bool serial_out>
bool I2C_Scan::next_T(){
	if constexpr(serial_out) {
		Serial.println("\nResume Scan");
	}
	while(nextDevice()) {
		if constexpr(serial_out) {
			if (result.error == _OK) {
				Serial.print("I2C Device at: 0x"); Serial.println(result.foundDeviceAddr,HEX);
			} else {
				Serial.print("I2C Error at: 0x"); 
				Serial.print(result.foundDeviceAddr, HEX);
				Serial.println(getError(result.error));
			}
		}
		if constexpr(!non_stop) return true;
	}
	if constexpr(serial_out) {
		Serial.print("Total I2C Devices: "); 
		Serial.println(result.totalDevicesFound,DEC);
	}
	return false;
}

