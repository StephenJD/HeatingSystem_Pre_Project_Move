#include <Arduino.h>
#include <I2C_Talk.h>
#include <I2C_Scan.h>
#include <I2C_SpeedTest.h>

#include <Wire.h>
//#include <Logging.h>
//#include <SD.h>
//#include <spi.h>
//#include <Clock.h>
//#include <Date_Time.h>
//#include <EEPROM.h>
//#include <Conversions.h>

#define I2C_RESET 14

//////////////////////////////// Start execution here ///////////////////////////////
I2C_Talk i2C{};
I2C_Scan scanner{i2C};
I2C_SpeedTest speedTest{i2C};

void setup() {
	Serial.begin(9600);
	Serial.println("Serial Begun"); Serial.flush();
	pinMode(I2C_RESET, OUTPUT);
	digitalWrite(I2C_RESET, HIGH); // reset pin
	i2C.restart();
}

void loop() {
	uint8_t temp;
	i2C.read(0x29, 0, 1, &temp);
	Serial.println(temp, DEC); Serial.flush();

	auto result = scanner.show_all();
	Serial.print(" Show-all. Error:"); Serial.println(scanner.getStatusMsg());
	Serial.print(" Show-all. Result:"); Serial.println(result, HEX);
	Serial.print(" Show-all. Addr:"); Serial.println(scanner.foundDeviceAddr, HEX);
	Serial.print(" Show-all. Count:"); Serial.println(scanner.totalDevicesFound);

	Serial.println();
	scanner.scanFromZero();
	result = scanner.show_next();
	Serial.print(" Show-next. Error:"); Serial.println(scanner.getStatusMsg());
	Serial.print(" Show-next. Result:"); Serial.println(result, HEX);
	Serial.print(" Show-next. Addr:"); Serial.println(scanner.foundDeviceAddr, HEX);
	Serial.print(" Show-next. Count:"); Serial.println(scanner.totalDevicesFound);
	Serial.println();
	result = scanner.show_next();
	Serial.print(" Show-next again. Error:"); Serial.println(scanner.getStatusMsg());
	Serial.print(" Show-next again. Result:"); Serial.println(result, HEX);
	Serial.print(" Show-next again. Addr:"); Serial.println(scanner.foundDeviceAddr, HEX);
	Serial.print(" Show-next again. Count:"); Serial.println(scanner.totalDevicesFound);
	
	scanner.scanFromZero();
	result = scanner.next();
	Serial.println();
	Serial.print(" next. Error:"); Serial.println(scanner.getStatusMsg());
	Serial.print(" next. Result:"); Serial.println(result, HEX);
	Serial.print(" next. Addr:"); Serial.println(scanner.foundDeviceAddr, HEX);
	Serial.print(" next. Count:"); Serial.println(scanner.totalDevicesFound);
	auto addr = scanner.foundDeviceAddr;

	result = scanner.next();
	Serial.println();	
	Serial.print(" next again. Error:"); Serial.println(scanner.getStatusMsg());
	Serial.print(" next again. Result:"); Serial.println(result, HEX);
	Serial.print(" next again. Addr:"); Serial.println(scanner.foundDeviceAddr, HEX);
	Serial.print(" next again. Count:"); Serial.println(scanner.totalDevicesFound);

	speedTest.scanFromZero();
	addr = speedTest.next();
	auto speed = speedTest.show_fastest(addr);
	Serial.println();	
	Serial.print(" show_fastest. Error:"); Serial.println(speedTest.getStatusMsg());
	Serial.print(" show_fastest. Speed:"); Serial.println(speed);


	speed = speedTest.showAll_fastest();
	Serial.println();	
	Serial.print(" showAll_fastest. Error:"); Serial.println(speedTest.getStatusMsg());
	Serial.print(" showAll_fastest. Speed:"); Serial.println(speed);

	speed = speedTest.fastest(addr);
	Serial.println();	
	Serial.print(" fastest. Error:"); Serial.println(speedTest.getStatusMsg());
	Serial.print(" fastest. Speed:"); Serial.println(speed);

	delay(1000);
}