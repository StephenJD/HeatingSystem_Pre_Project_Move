#include "I2C_Talk.h"
//#include <FlashStrings.h>

#ifndef VARIANT_MCK
#define VARIANT_MCK F_CPU
#endif

#ifdef DEBUG_TALK
#include <Logging.h>
// For use when debugging twi.c
extern "C" void I2C_Talk_msg(const char * str) {
	logger() << str << L_endl;
}

extern "C" void I2C_Talk_msg2(const char * str, long val1, long val2) {
	logger() << str << val1 << F(" : ") << val2 << L_endl;
}
#endif

using namespace I2C_Talk_ErrorCodes;
using namespace I2C_Recovery;

// encapsulation is improved by using global vars and functions in .cpp file
// rather than declaring these as class statics in the header file,
// since any changes to these would "dirty" the header file unnecessarily.

// private global variables //
static const uint16_t HALF_MAINS_PERIOD = 10000; // in microseconds. 10000 for 50Hz, 8333 for 60Hz

int8_t I2C_Talk::TWI_BUFFER_SIZE = 32;

I2C_Talk::I2C_Talk(int multiMaster_MyAddress, TwoWire & wire_port, int32_t max_I2Cfreq) :
	_max_i2cFreq(max_I2Cfreq)
	,_i2cFreq(max_I2Cfreq)
	, _myAddress(multiMaster_MyAddress)
	, _wire_port(&wire_port)
{}

void I2C_Talk::ini(TwoWire & wire_port, int32_t max_I2Cfreq) {
	//logger() << F("I2C_Talk::ini") << L_endl;
	//logger() << F("I2C_Talk::wire_port") << long(&wire_port) << L_endl;
	_max_i2cFreq = max_I2Cfreq;
	_i2cFreq = max_I2Cfreq;
	_wire_port = &wire_port;
}


error_codes I2C_Talk::read(int deviceAddr, int registerAddress, int numberBytes, uint8_t *dataBuffer) {

	auto returnStatus = beginTransmission(deviceAddr);
	if (returnStatus == _OK) {
		_wire().write(registerAddress);
		returnStatus = endTransmission();
		if (returnStatus == _OK) returnStatus = getData(deviceAddr, numberBytes, dataBuffer);
#ifdef DEBUG_TALK
		else
			logger() << F(" I2C_Talk::read, sendRegister failed. 0x") << L_hex << deviceAddr << getStatusMsg(returnStatus) << L_endl;
#endif
	}
	return returnStatus;
}

error_codes I2C_Talk::readEP(int deviceAddr, int pageAddress, int numberBytes, uint8_t *dataBuffer) {
	//Serial.print("\treadEP Wire:"); Serial.print((long)&_wire_port); Serial.print(", addr:"); Serial.print(deviceAddr); Serial.print(", page:"); Serial.print(pageAddress);
	//Serial.print(", NoOfBytes:"); Serial.println(numberBytes);
	auto returnStatus = _OK;
	waitForEPready(deviceAddr);
	// NOTE: this puts it in slave mode. Must re-begin to send more data.
	while (numberBytes > 0) {
		uint8_t bytesOnThisPage = min(numberBytes, TWI_BUFFER_SIZE);
		beginTransmission(deviceAddr);
		if (returnStatus == _OK) {
			_wire().write(pageAddress >> 8);   // Address MSB
			_wire().write(pageAddress & 0xff); // Address LSB
			returnStatus = endTransmission();
			if (returnStatus == _OK) returnStatus = getData(deviceAddr, bytesOnThisPage, dataBuffer);
			//Serial.print("ReadEP: status"); Serial.print(getStatusMsg(returnStatus)); Serial.print(" Bytes to read: "); Serial.println(bytesOnThisPage);
		}
		else return returnStatus;
		pageAddress += bytesOnThisPage;
		dataBuffer += bytesOnThisPage;
		numberBytes -= bytesOnThisPage;
	}
	return returnStatus;
}

error_codes I2C_Talk::getData(int deviceAddr, int numberBytes, uint8_t *dataBuffer) {
	// Register address must be loaded into write buffer before entry...
	//retuns 0=_OK, 1=_Insufficient_data_returned, 2=_NACK_during_address_send, 3=_NACK_during_data_send, 4=_NACK_during_complete, 5=_NACK_receiving_data, 6=_Timeout, 7=_slave_shouldnt_write, 8=_I2C_not_created
	//logger() << F(" I2C_Talk::getData start") << L_endl;

	uint8_t returnStatus = (_wire().requestFrom((int)deviceAddr, (int)numberBytes) != numberBytes);
	if (returnStatus != _OK) { // returned error
		returnStatus = _wire().read(); // retrieve error code
#ifdef DEBUG_TALK
		logger() << F(" I2C_Talk::getData Error for: 0x") << L_hex << deviceAddr << F(" ") << returnStatus << getStatusMsg(returnStatus) << L_endl;
#endif
	}
	else {
		//logger() << F(" I2C_Talk::getData OK") << L_endl;
		uint8_t i = 0;
		for (; _wire().available(); ++i) {
			dataBuffer[i] = _wire().read();
		}
	}

	return static_cast<error_codes>(returnStatus);
}

error_codes I2C_Talk::write(int deviceAddr, int registerAddress, int numberBytes, const uint8_t * dataBuffer) {
	//logger() << F(" I2C_Talk::write...") << L_endl; 
	auto returnStatus = beginTransmission(deviceAddr);
	if (returnStatus == _OK) {
		_wire().write(registerAddress);
		_wire().write(dataBuffer, uint8_t(numberBytes));
		synchroniseWrite();
		//logger() << F(" I2C_Talk::write send data..") << L_endl; 
		returnStatus = endTransmission();
		setProcessTime();	
#ifdef DEBUG_TALK
		//if (returnStatus) {
		//	logger() << F(" I2C_Talk::write endDone 0x") << L_hex << deviceAddr << getStatusMsg(returnStatus) << L_endl; 
		//}
#endif
	}
	return returnStatus;
}

error_codes I2C_Talk::write_verify(int deviceAddr, int registerAddress, int numberBytes, const uint8_t *dataBuffer) {
	uint8_t verifyBuffer[32] = {0xAA,0xAA};
	auto status = write(deviceAddr, registerAddress, numberBytes, dataBuffer);
	if (status == _OK) status = read(deviceAddr, registerAddress, numberBytes, verifyBuffer);
	int i = 0;
	while (status == _OK && i < numberBytes) {
		status |= (verifyBuffer[i] == dataBuffer[i] ? _OK : _I2C_ReadDataWrong);
		++i;
	}
	return status;
}

error_codes I2C_Talk::writeEP(int deviceAddr, int pageAddress, int numberBytes, const uint8_t * dataBuffer) {
	auto returnStatus = _OK; 
	// Lambda
	auto _writeEP_block = [returnStatus, deviceAddr, this](int pageAddress, uint16_t numberBytes, const uint8_t * dataBuffer) mutable {
		waitForEPready(deviceAddr);
		_wire().beginTransmission((uint8_t)deviceAddr);
		_wire().write(pageAddress >> 8);   // Address MSB
		_wire().write(pageAddress & 0xff); // Address LSB
		_wire().write(dataBuffer, uint8_t(numberBytes));
		returnStatus = endTransmission();
		_lastWrite = micros();
		return returnStatus;
	};

	auto calcBytesOnThisPage = [](int pageAddress, uint16_t numberBytes) {
		uint8_t bytesUntilPageBoundary = I2C_EEPROM_PAGESIZE - pageAddress % I2C_EEPROM_PAGESIZE;
		uint8_t bytesOnThisPage = min(numberBytes, bytesUntilPageBoundary);
		return min(bytesOnThisPage, TWI_BUFFER_SIZE - 2);
	};
	//Serial.print("\twriteEP Wire:"); Serial.print((long)&_wire_port); Serial.print(", addr:"); Serial.print(deviceAddr); Serial.print(", page:"); Serial.print(pageAddress);
	//Serial.print(", NoOfBytes:"); Serial.println(numberBytes); 
	//Serial.print(" BuffSize:"); Serial.print(TWI_BUFFER_SIZE);
	//Serial.print(", PageSize:"); Serial.println(I2C_EEPROM_PAGESIZE);

	while (returnStatus == _OK && numberBytes > 0) {
		uint8_t bytesOnThisPage = calcBytesOnThisPage(pageAddress, numberBytes);
		returnStatus = _writeEP_block(pageAddress, numberBytes, dataBuffer);
		pageAddress += bytesOnThisPage;
		dataBuffer += bytesOnThisPage;
		numberBytes -= bytesOnThisPage;
	}
	return returnStatus;
}

error_codes I2C_Talk::status(int deviceAddr) // Returns in slave mode.
{
	auto status = beginTransmission(deviceAddr);
	if (status == _OK) {
		status = endTransmission();
		// NOTE: this puts it in slave mode. Must re-begin to send more data.
	}
	return status;
}

void I2C_Talk::waitForEPready(int deviceAddr) {
	constexpr uint32_t I2C_WRITEDELAY = 5000;
	// If writing again within 5mS of last write, wait until EEPROM gives ACK again.
	// this is a bit faster than the hardcoded 5 milliSeconds
	while ((micros() - _lastWrite) <= I2C_WRITEDELAY) {
		_wire().beginTransmission((uint8_t)deviceAddr);
		// NOTE: this puts it in slave mode. Must re-begin to send more data.
		if (endTransmission() == _OK) break;
		yield(); // may be defined for co-routines
	}
}

void I2C_Talk::setMax_i2cFreq(int32_t max_I2Cfreq) { 
	if (max_I2Cfreq > VARIANT_MCK / 36) max_I2Cfreq = VARIANT_MCK / 36;
	if (max_I2Cfreq < _max_i2cFreq) _max_i2cFreq = max_I2Cfreq;
	// Must NOT setI2CFrequency() as Wire may not yet be constructed.
}

int32_t I2C_Talk::setI2CFrequency(int32_t i2cFreq) {
	if (_i2cFreq != i2cFreq) {
		_i2cFreq = i2cFreq;
		if (i2cFreq < MIN_I2C_FREQ) _i2cFreq = MIN_I2C_FREQ;
		if (i2cFreq > _max_i2cFreq) _i2cFreq = _max_i2cFreq;
		_wire().setClock(_i2cFreq);
	}
	return _i2cFreq;
}

void I2C_Talk::setTimeouts(uint32_t slaveByteProcess_uS, int stopMargin_uS, uint32_t busRelease_uS) {
	_slaveByteProcess_uS = slaveByteProcess_uS;
	_stopMargin_uS = stopMargin_uS;
	_busRelease_uS = busRelease_uS;
}

void I2C_Talk::extendTimeouts(uint32_t slaveByteProcess_uS, int stopMargin_uS, uint32_t busRelease_uS) {
	if (slaveByteProcess_uS > _slaveByteProcess_uS) _slaveByteProcess_uS = slaveByteProcess_uS;
	if (stopMargin_uS > _stopMargin_uS) _stopMargin_uS = stopMargin_uS;
	if (busRelease_uS > _busRelease_uS) _busRelease_uS = busRelease_uS;
}

const __FlashStringHelper * I2C_Talk::getStatusMsg(int errorCode) {
	switch (errorCode) {
	case _OK:	return F(" No Error");
	case _Insufficient_data_returned:	return F(" Insufficient data returned");
	case _NACK_during_address_send:	return F(" NACK address send");
	case _NACK_during_data_send:	return F(" NACK data send");
	case _NACK_during_complete:	return F(" NACK during complete");
	case _NACK_receiving_data:	return F(" NACK receiving data");
	case _Timeout:	return F(" Timeout");
	case _StopMarginTimeout: return F(" Stop Margin Timeout");
	case _SlaveByteProcessTimeout: return F(" Slave Byte Process Timeout");
	case _BusReleaseTimeout: return F(" Bus Release Timeout (Data hung low)");
	case _I2C_ClockHungLow: return F(" Clock Hung Low");
	case _disabledDevice:	return F(" Device Disabled. Enable with set_runSpeed()");
	case _slave_shouldnt_write:	return F(" Slave Shouldn't Write");
	case _I2C_Device_Not_Found: return F(" I2C Device not found");
	case _I2C_ReadDataWrong: return F(" I2C Read Data Wrong");
	case _I2C_AddressOutOfRange: return F(" I2C Address Out of Range");
	case 0xFF: return F(" Need modified Wire.cpp/twi.c");
	default: return F(" Not known");
	}
}

// Slave response
error_codes I2C_Talk::write(const uint8_t *dataBuffer, int numberBytes) {// Called by slave in response to request from a Master. Return errCode.
	return static_cast<error_codes>(_wire().write(dataBuffer, uint8_t(numberBytes)));
} 

uint8_t I2C_Talk::receiveFromMaster(int howMany, uint8_t *dataBuffer) {
	uint8_t noReceived = 0;
	while (_wire().available() && noReceived < howMany ) {
		dataBuffer[noReceived] = _wire().read();
		++noReceived;
	}
	return noReceived;
}

// Private Functions
error_codes I2C_Talk::beginTransmission(int deviceAddr) { // return false to inhibit access
	auto status = validAddressStatus(deviceAddr);
	if (status == _OK) _wire().beginTransmission((uint8_t)deviceAddr); // Puts in Master Mode.
	return status;
}

error_codes I2C_Talk::endTransmission() {
	auto status = static_cast<I2C_Talk_ErrorCodes::error_codes>(_wire().endTransmission());
#ifdef DEBUG_TALK
	if (status >= _Timeout) {
		logger() << F("_wire().endTransmission() returned ") << status << getStatusMsg(status) << L_endl;
	}
#endif
	return status;
}

bool I2C_Talk::begin() {
	wireBegin();
	_wire().setTimeouts(_slaveByteProcess_uS, _stopMargin_uS, _busRelease_uS);
	TWI_BUFFER_SIZE = getTWIbufferSize();
	return true;
}

uint8_t I2C_Talk::getTWIbufferSize() {
	uint8_t junk[1];
	_wire().beginTransmission(1);
	return uint8_t(_wire().write(junk, 100));
}

void I2C_Talk_ZX::setProcessTime() {relayDelay = uint16_t(micros() - relayStart); }

void I2C_Talk_ZX::synchroniseWrite() {
#if !defined (ZPSIM)
	if (_waitForZeroCross) {
		if (s_zeroCrossPinWatch.port() != 0) { //////////// Zero Cross Detection function ////////////////////////
			uint32_t zeroCrossSignalTime = micros();
			//logger() << "Wait for zLow" << L_endl;
			while (s_zeroCrossPinWatch.logicalState() != false && int32_t(micros() - zeroCrossSignalTime) < HALF_MAINS_PERIOD); // We need non-active state.
			zeroCrossSignalTime = micros();
			//logger() << "Wait for zHigh" << L_endl;
			while (s_zeroCrossPinWatch.logicalState() != true && int32_t(micros() - zeroCrossSignalTime) < HALF_MAINS_PERIOD); // Now wait for active state.
			zeroCrossSignalTime = micros();
			int32_t fireDelay = s_zxSigToXdelay - relayDelay;
			if (fireDelay < 0) fireDelay += HALF_MAINS_PERIOD;
			//logger() << "Delay for zzz " << fireDelay << L_endl;
			while (int32_t(micros() - zeroCrossSignalTime) < fireDelay) {
				//logger() << "so far... " << int32_t(micros() - zeroCrossSignalTime) << L_endl;; // wait for fireTime.
			}
			relayStart = micros(); // time relay delay
		}
		_waitForZeroCross = false;
	}
#endif
}