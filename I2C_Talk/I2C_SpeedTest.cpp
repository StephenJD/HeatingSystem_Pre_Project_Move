#include "I2C_SpeedTest.h"
#include <I2C_Recover.h>
#include <Logging.h>

using namespace I2C_Talk_ErrorCodes;

bool I2C_SpeedTest::_is_inSpeedTest = false;

error_codes I2C_SpeedTest::testDevice(int noOfTests, int allowableFailures) {
	return _i2c_device->recovery().testDevice(noOfTests, allowableFailures); 
}

void I2C_SpeedTest::prepareNextTest() {
	_error = _OK;
	_thisHighestFreq = (_i2c_device) ? _i2c_device->i2C().max_i2cFreq() : 1000000;
}

uint32_t I2C_SpeedTest::fastest(I_I2Cdevice_Recovery & i2c_device) {
	_i2c_device = &i2c_device;
	return fastest_T<false>();
}

uint32_t I2C_SpeedTest::show_fastest(I_I2Cdevice_Recovery & i2c_device) {
	_i2c_device = &i2c_device;
	return fastest_T<true>();
}

// implementation of fully specialized template to test one device
template<> 
uint32_t I2C_SpeedTest::fastest_T<false>() {
	if (!_i2c_device->isEnabled()) {
		_i2c_device->reset();
		logger() << F("\tRe-enabling disabled device");
	}	
	auto startFreq = _i2c_device->runSpeed(); // default 100000
	if (_is_inSpeedTest) return startFreq;
	_is_inSpeedTest = true;
	prepareNextTest();

	//Serial.println(startFreq);

	_thisHighestFreq = _i2c_device->i2C().setI2CFrequency(startFreq);
	_i2c_device->recovery().registerDevice(*_i2c_device);
#ifdef DEBUG_SPEED_TEST
	logger() << F("\tfastest_T Initial test...: ") << L_endl;
#endif	
	auto status = _i2c_device->recovery().testDevice(2,1); // Unfreeze is only Recovery strategy applied during speed-test
#ifdef DEBUG_SPEED_TEST
	logger() << F("\tfastest_T Initial 2 tests at: ") << _i2c_device->i2C().getI2CFrequency() << F(" Result:") << I2C_Talk::getStatusMsg(status) << L_endl;
#endif
#ifndef TRY_ALL_SPEEDS_
	if (status != _OK)
#endif
		status = _i2c_device->recovery().findAworkingSpeed();
	
	auto originalMargin = _i2c_device->i2C().stopMargin();
	auto marginLimit = originalMargin * 4;
	while (status == _StopMarginTimeout && _i2c_device->i2C().stopMargin() < marginLimit) { // try longer timeout margin
		_i2c_device->i2C().setTransferMargin(_i2c_device->i2C().stopMargin() + 1);
#ifdef DEBUG_SPEED_TEST
		logger() << F("\tTry increasing transfer Margin to ") << _i2c_device->i2C().stopMargin() << L_endl;
#endif
		_i2c_device->i2C().begin();
		status = _i2c_device->recovery().findAworkingSpeed();
	}

	if (status == _OK) {
		_stopMargin = _i2c_device->i2C().stopMargin();
		//Serial.println(" ** findMaxSpeed **");
		_thisHighestFreq = _i2c_device->i2C().getI2CFrequency();
		status = findOptimumSpeed(_thisHighestFreq, _i2c_device->i2C().max_i2cFreq() /*result.maxSafeSpeed*/);
	}

	_error = status;
	if (status != _OK) {
		_i2c_device->reset();
		_thisHighestFreq = _i2c_device->runSpeed();
	}
	_i2c_device->set_runSpeed(thisHighestFreq());

	//Serial.print("fastest_T<false,false> Finished");Serial.flush();
	_i2c_device->i2C().setTransferMargin(originalMargin);
	_is_inSpeedTest = false;
	return thisHighestFreq();
}

error_codes I2C_SpeedTest::findOptimumSpeed(int32_t & bestSpeed, int32_t limitSpeed ) {
	
	// lambdas
	//auto setToLimitSpeed = [bestSpeed, limitSpeed, this](auto & adjustBy) {
	//};

	
	// enters function at a working frequency
	int32_t adjustBy = (limitSpeed - bestSpeed) / 3;
	if (limitSpeed == bestSpeed) adjustBy = limitSpeed / 10;
	// 1st Try at limit frequency
	_i2c_device->i2C().setI2CFrequency(limitSpeed); 
	//logger() << F("** findOptimumSpeed start at ") << _i2c_device->i2C().getI2CFrequency() << F(" limit: ") << limitSpeed << F(" adjustBy: ") << adjustBy << L_endl;
	auto status = _i2c_device->recovery().testDevice(2,1);
	//logger() << F("\t** findOptimumSpeed 2-tests at ") << _i2c_device->i2C().getI2CFrequency() << I2C_Talk::getStatusMsg(status) << L_endl;
	if (status != _OK) {
		auto trySpeed = _i2c_device->i2C().setI2CFrequency(limitSpeed - adjustBy);
		status = _OK;
		do {
			//logger() << F("\n  Try best speed: ") << trySpeed << F(" adjustBy : ") << adjustBy << L_endl;
			while (status == _OK && (adjustBy > 0 ? trySpeed < limitSpeed : trySpeed > limitSpeed)) { // adjust speed 'till it breaks	
				status = _i2c_device->recovery().testDevice(2, 1); // I2C_RecoverRetest.cpp
#ifdef DEBUG_SPEED_TEST 
				logger() << F("    Tried at: ") << _i2c_device->i2C().getI2CFrequency() << I2C_Talk::getStatusMsg(status) << L_endl;
#endif
				if (status != _OK) {
					limitSpeed = trySpeed - adjustBy / 100;
					//logger() << F("\n Failed. NewLimit: ") << limitSpeed << L_endl;
				}
				else {
					//logger() << F(" OK. BestSpeed was:") << bestSpeed << L_endl;
					bestSpeed = trySpeed;
					trySpeed = _i2c_device->i2C().setI2CFrequency(trySpeed + adjustBy);
				}
			}
			trySpeed = _i2c_device->i2C().setI2CFrequency(trySpeed - adjustBy);
			adjustBy /= 2;
			status = _OK;
		} while (abs(adjustBy) > bestSpeed / 7);
	}
	status = adjustSpeedTillItWorksAgain((adjustBy > 0 ? -10 : 10));
	bestSpeed = _i2c_device->i2C().getI2CFrequency();
	return status != _OK ? _I2C_Device_Not_Found : _OK;
}

error_codes I2C_SpeedTest::adjustSpeedTillItWorksAgain(int32_t incrementRatio) {
	auto status = _OK;
	//Serial.print("\n ** Adjust I2C_Speed: "); Serial.println(i2C().getI2CFrequency(),DEC);Serial.flush();
	auto currFreq = _i2c_device->i2C().getI2CFrequency();
	do { // Adjust speed 'till it works reliably
		status = _i2c_device->recovery().testDevice(NO_OF_TESTS_MUST_PASS, 1);
#ifdef DEBUG_SPEED_TEST 
		logger() << F("\t** adjustSpeedTillItWorksAgain 5-tests at: ") << currFreq << I2C_Talk::getStatusMsg(status) << L_endl;
#endif
		if (status != _OK) {
			auto increment = currFreq / incrementRatio;
			if (increment > 0) {
				if (increment < 2000) increment = 2000;
			} else {
				if (increment > -2000) increment = -2000;
			}
			_i2c_device->i2C().setI2CFrequency(currFreq + increment);
			//logger() << F("\tAdjust I2C_Speed: ") << currFreq << F(" increment : ") << increment << L_endl;
		}
		currFreq = _i2c_device->i2C().getI2CFrequency();
	} while (status != _OK && currFreq > I2C_Talk::MIN_I2C_FREQ && currFreq < _i2c_device->i2C().max_i2cFreq());
	//Serial.print(" Adjust I2C_Speed "); Serial.print(status ? "failed at : " : "finished at : "); Serial.println(i2C().getI2CFrequency(),DEC);Serial.flush();
	return status != _OK ? _I2C_Device_Not_Found : _OK;
}

I_I2C_SpeedTestAll::I_I2C_SpeedTestAll(I2C_Talk & i2c, I2C_Recovery::I2C_Recover & recover) 
	: I_I2C_Scan(i2c, recover) {}

int8_t I_I2C_SpeedTestAll::prepareTestAll() {
	_maxSafeSpeed = scanner().i2C().max_i2cFreq();
	_speedTester.prepareNextTest();
	return 0;
}
