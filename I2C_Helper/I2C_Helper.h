/*
I2C_Helper.h
*/
#ifndef I2C_Helper_h
#define I2C_Helper_h

#include <Arduino.h>
#include <inttypes.h>
#include <Wire.h>

/**
Using this library requires a modified wire.cpp (Sam) or twi.c (AVR)
found in C:\Users\[UserName]\AppData\Roaming [ or Local] \Arduino15\packages\arduino\hardware\sam\1.6.4\libraries\Wire
or C:\Program Files (x86)\Arduino1.5\hardware\arduino\avr\libraries\Wire\
Small mods required so that in the event of a time-out Wire.endTransmission() returns error 1
and requestFrom() returns 0.
This in turn requires small mods to TWI_WaitTransferComplete(), TWI_WaitByteSent(), TWI_WaitByteReceived().
This library takes an optional void timoutFunction() as an argument to the setTimeoutFn method.
If supplied, the supplied function will get called whenever there is a time_out, allowing the I2C devices to be reset.
It should make a call to I2C_Helper.timeout_reset().
*/

#ifndef VARIANT_MCK
	#define VARIANT_MCK F_CPU
#endif

class I2C_Helper {
public:
	class I_I2Cdevice {
	public:
		I_I2Cdevice() = default;
		I_I2Cdevice(uint8_t addr) : _address(addr) {}
		I_I2Cdevice(I2C_Helper & i2C, uint8_t addr) : _address(addr) {_i2C = &i2C;}
		virtual uint8_t initialiseDevice() { return 0; }
		/// <summary>
		/// Function Operator performs device test
		/// </summary>	
		virtual uint8_t testDevice(I2C_Helper & i2c, int addr) = 0;
		uint8_t getAddress() const { return _address; }
		void setAddress(uint8_t addr) { _address = addr; }

		static void setI2Chelper(I2C_Helper & i2C) { _i2C = &i2C; }
	protected:
		static I2C_Helper * _i2C;
		uint8_t _address = 255;
	};

	class I_I2CresetFunctor {
	public:
		virtual uint8_t operator()(I2C_Helper & i2c, int addr) = 0;
	};

	typedef uint8_t (*TestFnPtr)(I2C_Helper &, int);
	
	// Basic Usage //
	enum error_codes {_OK, _Insufficient_data_returned, _NACK_during_address_send, _NACK_during_data_send, _NACK_during_complete, _NACK_receiving_data, _Timeout, _speedError, _slave_shouldnt_write, _I2C_not_created, _I2C_Device_Not_Found, _I2C_ReadDataWrong, _I2C_AddressOutOfRange};
	enum {_single_master = 255, _no_address = 255};

	I2C_Helper(TwoWire &wire_port = Wire, int32_t i2cFreq = 100000);
	
	uint8_t read(uint8_t deviceAddr, uint8_t registerAddress, uint8_t numberBytes, uint8_t *dataBuffer); // Return errCode. dataBuffer may not be written to if read fails.
	uint8_t read(uint8_t deviceAddr, uint8_t registerAddress, uint8_t numberBytes, char *dataBuffer) {return read(deviceAddr, registerAddress, numberBytes, (uint8_t *) dataBuffer);}
	uint8_t readEP(uint8_t deviceAddr, int pageAddress, uint8_t numberBytes, uint8_t *dataBuffer); // Return errCode. dataBuffer may not be written to if read fails.
	uint8_t write(const uint8_t *dataBuffer, uint8_t numberBytes); // Called by slave in response to request from a Master. Return errCode.
	uint8_t write(const char *dataBuffer) {return write((const uint8_t*)dataBuffer, (uint8_t)strlen(dataBuffer)+1);}// Called by slave in response to request from a Master. Return errCode.
	uint8_t write(uint8_t deviceAddr, uint8_t registerAddress, uint8_t numberBytes, const uint8_t *dataBuffer); // Return errCode.
	uint8_t write(uint8_t deviceAddr, uint8_t registerAddress, uint8_t data); // Return errCode.
	uint8_t writeEP(uint8_t deviceAddr, int pageAddress, uint8_t data); // Return errCode.
	
	static uint8_t notExists(I2C_Helper & i2c, int deviceAddr);
	uint8_t notExists(int deviceAddr);
	
	// Enhanced Usage //
	I2C_Helper(TwoWire &wire_port, int8_t zxPin, uint8_t retries, uint16_t zxDelay, I_I2CresetFunctor * timeoutFunction = 0, int32_t i2cFreq = 100000);
	uint8_t write_at_zero_cross(uint8_t deviceAddr, uint8_t registerAddress, uint8_t data); // Return errCode.
	
	int32_t setI2CFrequency(int32_t i2cFreq); // turns auto-speed off
	int32_t getI2CFrequency(); // returns current Frequency
	void useAutoFrequency() {_useAutoSpeed = true;}
	virtual int32_t setThisI2CFrequency(int8_t devAddr, int32_t i2cFreq) {return setI2Cfreq_retainAutoSpeed(i2cFreq);}
	virtual int32_t getThisI2CFrequency(int8_t devAddr) {return getI2CFrequency();}

	void setNoOfRetries(uint8_t retries);
	uint8_t getNoOfRetries();
	void setTimeoutFn (I_I2CresetFunctor * timeoutFnPtr);
	void setTimeoutFn (TestFnPtr timeoutFnPtr);
	I_I2CresetFunctor * getTimeoutFn() {return timeoutFunctor;}

	bool restart();
	bool slowdown_and_reset(int addr); // called by timoutFunction. Reduces I2C speed by 10% if last called within one second. Returns true if speed was reduced
	signed char adjustSpeedTillItWorksAgain(I_I2Cdevice * deviceFailTest, int32_t increment);
	
	void static setZeroCross(int8_t zxPin); // Arduino pin signalling zero-cross detected. 0 = disable, +ve = signal on rising edge, -ve = signal on falling edge
	void static setZeroCrossDelay(uint16_t zxDelay); // microseconds delay between signal on zero_cross_pin to next true zero-cross. Default = 700
	static const char * getError(int errorCode);
	
	// Slave Usage //
	// Where there is a single master, you can set the slave arduino using setAsSlave(), which prohibits it from initiating a write.
	// Where multiple arduinos need to be able to initiate writes (even if they are not talking to each other),
	// each must use the setAsMaster() function, providing a unique I2C address.
	// Multi_master mode enables each arduino to act as a master and also respond as a slave.
	// To switch off multi-master mode, call setAsMaster() without supplying an address.
	// To resond as a slave, the onReceive() and onRequest() functions must be set.
	// *** The onReceive() function must not do a Serial.print when it receives a register address prior to a read.
	I2C_Helper(int multiMaster_MyAddress, TwoWire &wire_port = Wire, int32_t i2cFreq = 100000);
	void setAsSlave(int slaveAddress) { _myAddress = slaveAddress; _canWrite = false; restart(); }
	void setAsMaster(int multiMaster_MyAddress = _single_master) {_myAddress = multiMaster_MyAddress; _canWrite = true; restart();}
	void onReceive(void(*fn)(int)) { wire_port.onReceive(fn); } // The supplied function is called when data is sent to a slave
	void onRequest(void(*fn)(void)){ wire_port.onRequest(fn); } // The supplied function is called when data is requested from a slave
	uint8_t receiveFromMaster(int howMany, uint8_t *dataBuffer); // Data is written to dataBuffer. Returns how many are written.
	uint8_t receiveFromMaster(int howMany, char *dataBuffer) {return receiveFromMaster(howMany, (uint8_t *) dataBuffer);}
	
	// I2C Testing //
	struct scanResult {
		scanResult();
		void reset();
		void prepareNextTest();
		int8_t foundDeviceAddr;	// -1 to 127
		uint8_t totalDevicesFound;
		uint8_t	error;
		int32_t maxSafeSpeed;
		int32_t minSafeSpeed;
		int32_t thisHighestFreq;
		int32_t thisLowestFreq;
	} result;
	
	template<bool non_stop, bool serial_out>
	bool scan();

	template<bool non_stop, bool serial_out>
	uint32_t speedTest(I_I2Cdevice * deviceFailTest = 0);
	
//typedef bool (*scanFnType)(scanResult);
//scanFnType const scanNext = & scan<false,false>;
//scanFnType const scanAll = & scan<true,false>;
//auto scanNext = scan<false,false>;
//auto scanAll = scan<true,true>;
#define scanAll scan<true,true>
#define scanNext scan<false,false> // returns false when no more found
#define scanNextS scan<false,true> // returns false when no more found
#define speedTestAll speedTest<true,true>
#define speedTestNext speedTest<false,false>
#define speedTestNextS speedTest<false,true>
/** Usage:
	I2C_Helper i2C;
	i2C.scanAll(); // scan all addresses and print results to serial port
	i2C.speedTestAll(); // test all and print results to serial port
	
	// or to LCD ...
	i2C.result.reset(); // reset to start scanning at 0
	while (i2C.scanNext()){ // return after each device found. Don't output to serial port.
		lcd->print(i2C.result.foundDeviceAddr,HEX); lcd->print(" ");
	}
*/
	// required by template, may as well be publicly available
	static const int32_t MAX_I2C_FREQ = VARIANT_MCK / 40; //100000; // 
	static const int32_t MIN_I2C_FREQ = VARIANT_MCK / 65288; //32644; //VARIANT_MCK / 65288; //36000; //
	uint8_t successAfterRetries;
protected:
	int32_t setI2Cfreq_retainAutoSpeed(int32_t i2cFreq);
	bool _useAutoSpeed;
private:
	bool restart(const char * name, int addr);
	uint8_t check_endTransmissionOK(int addr);
	void waitForZeroCross();
	void callTime_OutFn(int addr);
	uint8_t beginTransmission(uint8_t deviceAddr); // return false to inhibit access
	signed char testDevice(I_I2Cdevice * deviceFailTest, uint8_t addr, int noOfTests);
	signed char findAworkingSpeed(I_I2Cdevice * deviceFailTest);
	signed char findOptimumSpeed(I_I2Cdevice * deviceFailTest, int32_t & bestSpeed, int32_t limitSpeed);
	void useAutoSpeed(bool set) {_useAutoSpeed = set;}

	TwoWire & wire_port;
	uint8_t noOfRetries;

	I_I2CresetFunctor * timeoutFunctor;
	
	class I2Creset_Functor : public I_I2CresetFunctor {
	public:
		void set(TestFnPtr tfn) {_tfn = tfn;}
		uint8_t operator()(I2C_Helper & i2c, int addr) /*override*/ {return _tfn(i2c,addr);}
	private:
		TestFnPtr _tfn;
	};
	I2Creset_Functor _i2CresetFunctor; // data member functor to wrap free reset function
	//TestFnPtr timeoutFn;

	uint32_t relayDelay;
	uint32_t relayStart;
	static int8_t s_zeroCrossPin;
	static uint16_t s_zxSigToXdelay;
	int32_t _i2cFreq;
	int32_t _lastGoodi2cFreq;
	uint8_t _myAddress;
	bool _canWrite;
	unsigned long _lastRestartTime;

};

class I2C_Helper_Auto_Speed_Hoist : public I2C_Helper {
public:
	I2C_Helper_Auto_Speed_Hoist(TwoWire &wire_port = Wire, int32_t i2cFreq = 100000) : I2C_Helper(wire_port, i2cFreq){}
	I2C_Helper_Auto_Speed_Hoist(int multiMaster_MyAddress, TwoWire &wire_port = Wire, int32_t i2cFreq = 100000) : I2C_Helper(multiMaster_MyAddress, wire_port, i2cFreq){}
	I2C_Helper_Auto_Speed_Hoist(TwoWire &wire_port, signed char zxPin, uint8_t retries, uint16_t zxDelay, I_I2CresetFunctor * timeoutFunction, int32_t i2cFreq = 100000) : I2C_Helper(wire_port, zxPin, retries, zxDelay, timeoutFunction, i2cFreq){}

protected:
	int32_t _setI2CFrequency(int8_t devAddr, int32_t i2cFreq, int8_t * devAddrArr, int32_t * i2c_speedArr, int noOfDevices);
	int32_t _getI2CFrequency(int8_t devAddr, int8_t * devAddrArr, int32_t * i2c_speedArr, int noOfDevices);
};

template<int noOfDevices>
class I2C_Helper_Auto_Speed : public I2C_Helper_Auto_Speed_Hoist {
public:
	I2C_Helper_Auto_Speed(TwoWire & wire_port = Wire, int32_t i2cFreq = 100000) : I2C_Helper_Auto_Speed_Hoist(wire_port, i2cFreq){resetAddresses();}
	I2C_Helper_Auto_Speed(int multiMaster_MyAddress, TwoWire & wire_port = Wire, int32_t i2cFreq = 100000): I2C_Helper_Auto_Speed_Hoist(multiMaster_MyAddress, wire_port, i2cFreq){resetAddresses();}
	I2C_Helper_Auto_Speed(TwoWire & wire_port, signed char zxPin, uint8_t retries, uint16_t zxDelay, I_I2CresetFunctor * timeoutFunction, int32_t i2cFreq = 100000): I2C_Helper_Auto_Speed_Hoist(wire_port, zxPin, retries, zxDelay, timeoutFunction, i2cFreq){resetAddresses();}
	

	virtual int32_t setThisI2CFrequency(int8_t devAddr, int32_t i2cFreq) {return _setI2CFrequency(devAddr, i2cFreq, devAddrArr, i2c_speedArr, noOfDevices);}
	virtual int32_t getThisI2CFrequency(int8_t devAddr) {if (_useAutoSpeed) return _getI2CFrequency(devAddr, devAddrArr, i2c_speedArr, noOfDevices); else return getI2CFrequency();}
private:
	void resetAddresses();
	int8_t devAddrArr[noOfDevices];
	int32_t i2c_speedArr[noOfDevices];
};

template<int noOfDevices>
void I2C_Helper_Auto_Speed<noOfDevices>::resetAddresses() {
	for (int i=0; i< noOfDevices; ++i) {
		devAddrArr[i] = 0;
		i2c_speedArr[i] = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Template Function implementations //
template<bool non_stop, bool serial_out>
bool I2C_Helper::scan(){ 
	if (serial_out) {
		Serial.println("Start Scan");
	}
	while(scan<false,false>()) {
		if (serial_out) {
			if (result.error == _OK) {
				Serial.print("I2C Device at: 0x"); Serial.println(result.foundDeviceAddr,HEX);
			} else {
				Serial.print("I2C Error at: 0x"); 
				Serial.print(result.foundDeviceAddr, HEX);
				Serial.println(getError(result.error));
			}
		}
		if (!non_stop) return true;
	}
	if (serial_out) {
		Serial.print("Total I2C Devices: "); 
		Serial.println(result.totalDevicesFound,DEC);
		if (result.error != _OK) {
			Serial.print("Error: ");
			Serial.println(getError(result.error));
		}
	}
	return false;
}

template<> // specialization implemented in .cpp
bool I2C_Helper::scan<false,false>();

template<> // specialization implemented in .cpp
uint32_t I2C_Helper::speedTest<false,false>(I_I2Cdevice * deviceFailTest);

template<bool non_stop, bool serial_out>
uint32_t I2C_Helper::speedTest(I_I2Cdevice * deviceFailTest) {
	if (serial_out) {
		if (non_stop) {
			Serial.println("Start Speed Test...");
			scan<false,serial_out>();
		} else {
			//if (result.foundDeviceAddr > 127) {
			//	Serial.print("Device address out of range: 0x");
			//}
			//else {
				Serial.print("Test Device at: 0x");
			//}
			Serial.println(result.foundDeviceAddr,HEX);
		}
	}

	do  {
		speedTest<false,false>(deviceFailTest);
	} while(non_stop && scan<false,serial_out>());

	if (serial_out) {
		if (non_stop) {
			Serial.print("Overall Best Frequency: "); 
			Serial.println(result.maxSafeSpeed); Serial.println();
			Serial.print("Overall Min Frequency: "); 
			Serial.println(result.minSafeSpeed); Serial.println();
		} else {
			if (result.error == 0) {
				Serial.print(" Final Max Frequency: "); Serial.println(result.thisHighestFreq);
				Serial.print(" Final Min Frequency: "); Serial.println(result.thisLowestFreq); Serial.println();
			} else {
				Serial.println(" Test Failed");
				Serial.println(getError(result.error));
				Serial.println();
			}
		}
	}

	return non_stop ? result.maxSafeSpeed : result.thisHighestFreq;
}

#endif