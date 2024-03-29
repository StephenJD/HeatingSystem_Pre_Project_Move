#include "TestDevices.h"
#include "Initialiser.h"
#include "HeatingSystemEnums.h"
#include "..\HeatingSystem.h"
#include "..\HardwareInterfaces\A__Constants.h"
#include "..\Client_DataStructures\Data_TempSensor.h"
#include <Logging.h>
#include <RDB.h>
#include <MemoryFree.h>

void reset_watchdog();

namespace HardwareInterfaces {
	using namespace RelationalDatabase;
	using namespace client_data_structures;
	using namespace Assembly;
	using namespace I2C_Talk_ErrorCodes;

	TestDevices::TestDevices(Initialiser & ini)
		: _ini(ini)
	{
		logger() << F("TestDevices Constructed") << L_endl;
	}

	HeatingSystem & TestDevices::hs() { return _ini.hs(); }

	int8_t TestDevices::showSpeedTestFailed(I_I2Cdevice_Recovery & device, const char * deviceName) {
		reset_watchdog();
		hs().mainDisplay.setCursor(5, 2);
		hs().mainDisplay.print("       0x");
		hs().mainDisplay.setCursor(5, 2);
		hs().mainDisplay.print(deviceName);
		hs().mainDisplay.setCursor(14, 2);
		hs().mainDisplay.print(device.getAddress(), HEX);
		hs().mainDisplay.print("    ");
		hs().mainDisplay.sendToDisplay();
		auto speedTest = I2C_SpeedTest{ device };
		speedTest.fastest();
		hs().mainDisplay.setCursor(17, 2);
		if (speedTest.error() != _OK) {
			hs().mainDisplay.print("Bad");
			hs().mainDisplay.sendToDisplay();
			logger() << F("\tspeedTest 0x") << L_hex << device.getAddress() << I2C_Talk::getStatusMsg(speedTest.error()) << L_endl;
#ifndef ZPSIM
			delay(2000);
#endif
		}
		else {
			if (speedTest.stopMargin() > _max_transferMargin_uS) _max_transferMargin_uS = speedTest.stopMargin();
			hs().mainDisplay.print(" OK");
			hs().mainDisplay.sendToDisplay();
			logger() << F("\tspeedTest 0x") << L_hex << device.getAddress() << F(". OK at ") << L_dec << device.runSpeed() << L_endl;
		}
		logger() << L_endl;
		return speedTest.error();
	}

	I_I2Cdevice_Recovery & TestDevices::getDevice(uint8_t deviceAddr) { return _ini.getDevice(deviceAddr); }

	uint8_t TestDevices::speedTestDevices() {// returns 0 for success or returns ERR_PORTS_FAILED / ERR_I2C_SPEED_FAIL
		enum { e_allPassed, e_mixValve, e_relays = 2, e_US_Remote = 4, e_FL_Remote = 8, e_DS_Remote = 16, e_TempSensors = 32 };
		hs().i2C.setTimeouts(5000, 2);
		hs()._recover.setTimeoutFn(&_ini._resetI2C.hardReset);
		int8_t returnVal = 0;
		hs().mainDisplay.setCursor(0, 0);
		if (logger().isWorking()) hs().mainDisplay.print("SD OK"); else hs().mainDisplay.print("SD Failed");
		hs().mainDisplay.setCursor(0, 2);
		hs().mainDisplay.print("Test ");
		logger() << L_endl << L_time << F("TestDevices::speedTestDevices has been called\n");
		logger() << F("\n\tTry Relay Port\n");
		if (showSpeedTestFailed(_ini.relayPort(), "Relay")) {
			returnVal = ERR_PORTS_FAILED;
		}

		logger() << F("\tTry Remotes") << L_endl;
		if (showSpeedTestFailed(hs().remDispl[D_Hall], "DS Rem")) {
			returnVal = ERR_I2C_READ_FAILED;
		}

		if (showSpeedTestFailed(hs().remDispl[D_Bedroom], "US Rem")) {
			returnVal = ERR_I2C_READ_FAILED;
		}

		if (showSpeedTestFailed(hs().remDispl[D_Flat], "FL Rem")) {
			returnVal = ERR_I2C_READ_FAILED;
		}

		for (auto & ts : hs()._tempController.tempSensorArr) {
		//for (int i = 0; i < sizeof(hs()._tempController.tempSensorArr) / sizeof(hs()._tempController.tempSensorArr[0]); ++i) {
		//auto ts = TempSensor{ hs().recoverObject(),0x29 };
			//auto & ts = hs()._tempController.tempSensorArr[i];
			//logger() << F("\tTry TS pos: ") << i << L_endl;
			//logger() << F("\tTry TS 0x") << L_hex << ts.getAddress() << L_endl;
			auto status = showSpeedTestFailed(ts, "TS");
			//logger() << L_time << "TS Error: " << status << L_endl;
		}

		logger() << F("\tTry Mix Valve") << L_endl;
		if (showSpeedTestFailed(hs()._tempController.mixValveControllerArr[0], "Mix V")) {
			returnVal = ERR_MIX_ARD_FAILED;
		}

		hs().mainDisplay.sendToDisplay();
		hs()._recover.setTimeoutFn(&_ini._resetI2C);
		hs().i2C.setTimeouts(5000, _max_transferMargin_uS);
		return returnVal;
	}

	uint8_t TestDevices::testRelays() {
		//******************** Cycle through ports *************************
		uint8_t returnVal = 0;
		uint8_t numberFailed = 0;
		logger() << F("Relay_Run::testRelays\n");
		returnVal = _ini.relayPort().testDevice();
		if (returnVal) {
			logger() << F("\tNo Relays") << I2C_Talk::getStatusMsg(returnVal) << L_endl;
			return NO_OF_RELAYS;
		}
		hs().mainDisplay.setCursor(0, 3);
		hs().mainDisplay.print("Relay Test: ");
		for (int relayNo = 0; relayNo < NO_OF_RELAYS; ++relayNo) { // 12 relays, but 1 is mix enable.
			hs()._tempController.relayArr[RELAY_ORDER[relayNo]].set(1);
			returnVal = _ini.relayPort().updateRelays();
			delay(200);
			hs()._tempController.relayArr[RELAY_ORDER[relayNo]].set(0);
			returnVal |= _ini.relayPort().updateRelays();
			numberFailed = numberFailed + (returnVal != 0);
			hs().mainDisplay.setCursor(12, 3);
			hs().mainDisplay.print(relayNo, DEC);
			if (returnVal) {
				 logger() << F("\tRelay Failed\tERR_PORTS_FAILED for Relay ") << relayNo << L_endl;
				hs().mainDisplay.print(" Fail");
			}
			else {
				hs().mainDisplay.print(" OK  ");
			}
			hs().mainDisplay.sendToDisplay();
			delay(200);
			returnVal = 0;
		}
		hs().mainDisplay.setCursor(12, 3);
		if (numberFailed > 0) {
			logger() << L_tabs << numberFailed << F(" Relays Failed") << L_endl;
			hs().mainDisplay.print(numberFailed);
			hs().mainDisplay.print("Failed");
		}
		else {
			logger() << F("All Relays OK\n") << L_endl;
			hs().mainDisplay.print("All OK  ");
			//I2C_OK = true;
		}
		hs().mainDisplay.sendToDisplay();
		delay(500);
		return numberFailed;
	}

}