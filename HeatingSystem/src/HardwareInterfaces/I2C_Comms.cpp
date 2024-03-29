#include "I2C_Comms.h"
#include <Logging.h>
#include "A__Constants.h"
#include <I2C_Recover.h>
#include <LocalKeypad.h>

using namespace I2C_Recovery;
using namespace I2C_Talk_ErrorCodes;
namespace HardwareInterfaces {

	/////////////////////////////////////////////////////
	//              I2C Reset Support                  //
	/////////////////////////////////////////////////////

	ResetI2C::ResetI2C(I2C_Recover_Retest & recover, I_IniFunctor & resetFn, I_TestDevices & testDevices, Pin_Wag resetPinWag)
		:
		_recover(&recover)
		, hardReset(resetPinWag)
		, _postI2CResetInitialisation(&resetFn)
		, _testDevices(&testDevices)
		{
			_recover->setTimeoutFn(this);
		}

	error_codes ResetI2C::operator()(I2C_Talk & i2c, int addr) {
		static bool isInReset = false;
		if (isInReset) {
			logger() << F("\nTest: Recursive Reset... for 0x") << L_hex << addr << L_endl;
			return _OK;
		}

		isInReset = true;
		error_codes status = _OK;
		auto origFn = _recover->getTimeoutFn();
		_recover->setTimeoutFn(&hardReset);

		logger() << F("\t\tResetI2C... for 0x") << L_hex << addr << L_endl;
		hardReset(i2c, addr);
		if (!_recover->isRecovering()) {
			logger() << "\tResetI2C Doing retest" << L_endl;
			I_I2Cdevice_Recovery & device = _testDevices->getDevice(addr);
			status = device.testDevice();
			if (status == _OK) postResetInitialisation();
		}

		_recover->setTimeoutFn(origFn);
		isInReset = false;
		return status;
	}

	void ResetI2C::postResetInitialisation() { 
		if (hardReset.initialisationRequired && _postI2CResetInitialisation) {
			logger() << F("\t\tResetI2C... _postI2CResetInitialisation\n");
			if ((*_postI2CResetInitialisation)() != 0) return; // return 0 for OK. Resets hardReset.initialisationRequired to false.
		}
	};

	void HardReset::arduinoReset() {
		logger() << F("HardReset::arduinoReset called") << L_flush;
		pinMode(A4, OUTPUT);
		digitalWrite(A4, LOW);
	}

	error_codes HardReset::operator()(I2C_Talk & i2c, int addr) {
		LocalKeypad::indicatorLED().set();
		_resetPinWag.set();
		delay(128); // interrupts still serviced
		_resetPinWag.clear();
		i2c.begin();
		timeOfReset_mS = millis();
		logger() << L_time << F("Done Hard Reset for 0x") << L_hex << addr << L_endl;
		LocalKeypad::indicatorLED().clear();
		if (digitalRead(20) == LOW)
			logger() << "\tData stuck after reset" << L_endl;
		initialisationRequired = true;
		return _OK;
	}

}