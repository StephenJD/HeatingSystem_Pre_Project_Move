#include "MixValveController.h"
#include "A__Constants.h"
#include "..\Assembly\HeatingSystemEnums.h"
#include <I2C_Talk_ErrorCodes.h>
#include <Logging.h>
//#include "..\Client_DataStructures\Data_TempSensor.h"
#include "..\Client_DataStructures\Data_Relay.h"
#include <Timer_mS_uS.h>
#include <Clock.h>
#include "..\HardwareInterfaces\I2C_Comms.h"

void ui_yield();

namespace arduino_logger {
	Logger& profileLogger();
}
using namespace arduino_logger;

namespace HardwareInterfaces {
	using namespace Assembly;
	using namespace I2C_Talk_ErrorCodes;
	using namespace i2c_registers;
//#if defined (ZPSIM)
//	using namespace std;
//	ofstream MixValveController::lf("LogFile.csv");
//
//#endif
	MixValveController::MixValveController(I2C_Recovery::I2C_Recover& recover, i2c_registers::I_Registers& local_registers) 
		: I2C_To_MicroController{recover, local_registers}
		, _slaveMode_flowTempSensor{ recover }
	{}

	void MixValveController::initialise(int index, int addr, UI_Bitwise_Relay * relayArr, int flowTS_addr, UI_TempSensor & storeTempSens, unsigned long& timeOfReset_mS) {
#ifdef ZPSIM
		uint8_t(&debugWire)[SIZE_OF_ALL_REGISTERS] = reinterpret_cast<uint8_t(&)[SIZE_OF_ALL_REGISTERS]>(TwoWire::i2CArr[PROGRAMMER_I2C_ADDR]);
#endif
		auto localRegOffset = index == M_UpStrs ? PROG_REG_MV0_OFFSET : PROG_REG_MV1_OFFSET;
		auto remoteRegOffset = index == M_UpStrs ? MV0_REG_OFFSET : MV1_REG_OFFSET;
		I2C_To_MicroController::initialise(addr, localRegOffset, remoteRegOffset, timeOfReset_mS);
		auto reg = registers();
		reg.set(Mix_Valve::R_REMOTE_REG_OFFSET, localRegOffset);
		_relayArr = relayArr;
		_flowTS_addr = flowTS_addr;
		_storeTempSens = &storeTempSens;
		_controlZoneRelay = index == M_DownStrs ? R_DnSt : R_UpSt;
		_slaveMode_flowTempSensor.setAddress(_flowTS_addr);
	}

	uint8_t MixValveController::sendSlaveIniData(uint8_t requestINI_flag) {
#ifdef ZPSIM
		uint8_t(&debugWire)[30] = reinterpret_cast<uint8_t(&)[30]>(TwoWire::i2CArr[getAddress()]);
		writeRegValue(Mix_Valve::R_MODE, Mix_Valve::e_Checking);
#endif
		waitForWarmUp();
		uint8_t errCode = reEnable(true);
		auto reg = registers();
		if (errCode == _OK) {
			errCode = writeReg(Mix_Valve::R_DEVICE_STATE);
			errCode |= writeReg(Mix_Valve::R_REMOTE_REG_OFFSET);
			errCode |= writeRegValue(Mix_Valve::R_TS_ADDRESS, _flowTS_addr);
			errCode |= writeRegValue(Mix_Valve::R_FULL_TRAVERSE_TIME, VALVE_TRANSIT_TIME);
			errCode |= writeRegValue(Mix_Valve::R_SETTLE_TIME, VALVE_WAIT_TIME);
			if (errCode) return errCode;

			reg.set(Mix_Valve::R_RATIO, 30);
			reg.set(Mix_Valve::R_FROM_TEMP, 55);
			reg.set(Mix_Valve::R_FLOW_TEMP, 55);
			reg.set(Mix_Valve::R_REQUEST_FLOW_TEMP, 25);
			errCode = writeRegSet(Mix_Valve::R_RATIO, Mix_Valve::MV_VOLATILE_REG_SIZE - Mix_Valve::R_RATIO);
			if (errCode == _OK) {
				auto rawReg = rawRegisters();
				auto newIniStatus = rawReg.get(R_SLAVE_REQUESTING_INITIALISATION);
				//logger() << L_time << "SendMixIni. IniStatus was:" << newIniStatus;
				newIniStatus &= ~requestINI_flag;
				//logger() << " New IniStatus:" << newIniStatus << L_endl;
				rawReg.set(R_SLAVE_REQUESTING_INITIALISATION, newIniStatus);
			}
			readRegistersFromValve();
		}
		logger() <<  F("MixValveController::sendSlaveIniData()") << I2C_Talk::getStatusMsg(errCode) << " State:" << reg.get(Mix_Valve::R_DEVICE_STATE) << L_endl;
		return errCode;
	}

	uint8_t MixValveController::flowTemp() const {
		return registers().get(Mix_Valve::R_FLOW_TEMP);
	}

	bool MixValveController::check() { // called once per second
		if (reEnable() == _OK) {
			auto mixIniStatus = rawRegisters().get(R_SLAVE_REQUESTING_INITIALISATION);
			if (mixIniStatus > ALL_REQUESTING) {
				logger() << L_time << "Bad Mix IniStatus : " << mixIniStatus << L_endl;
				mixIniStatus = ALL_REQUESTING;
				rawRegisters().set(R_SLAVE_REQUESTING_INITIALISATION, ALL_REQUESTING);
			}
			uint8_t requestINI_flag = MV_US_REQUESTING_INI << index();
			if (mixIniStatus & requestINI_flag) sendSlaveIniData(requestINI_flag);
			readRegistersFromValve();
			logMixValveOperation(false);
		}
		return true;
	}

	void MixValveController::monitorMode() {
		if (reEnable() == _OK) {
			_relayArr[_controlZoneRelay].set(); // turn call relay ON
			relayController().updateRelays();
			readRegistersFromValve();
			logMixValveOperation(true);
		}
	}

	void MixValveController::logMixValveOperation(bool logThis) {
//#ifndef ZPSIM
		auto reg = registers();
		auto algorithmMode = Mix_Valve::Mix_Valve::Mode(reg.get(Mix_Valve::R_MODE)); // e_Moving, e_Wait, e_Checking, e_Mutex, e_NewReq
		if (algorithmMode >= Mix_Valve::e_Error) {
			logger() << "MixValve Mode Error: " << algorithmMode << L_endl;
			uint8_t requestINI_flag = MV_US_REQUESTING_INI << index();
			sendSlaveIniData(requestINI_flag);
			if (reg.get(Mix_Valve::R_MODE) >= Mix_Valve::e_Error) {
				recovery().resetI2C();
				if (reg.get(Mix_Valve::R_MODE) >= Mix_Valve::e_Error) {
					HardReset::arduinoReset("MixValveController InvalidMode");
				}
			}
		}

		bool ignoreThis = (algorithmMode == Mix_Valve::e_Checking && flowTemp() == _mixCallTemp)
			|| algorithmMode == Mix_Valve::e_AtLimit;

		auto valveIndex = _localRegOffset ? 1 : 0;
		static bool wasLogging = false;
		if (!logThis) wasLogging = false;
		else if (!wasLogging  ) {
			wasLogging = true;
			profileLogger() << L_time << "Logging\t\tReq\tIs\tState\tTime\tPos\tRatio\tFromP\tFromT\n";
		}
		if (logThis || !ignoreThis || _previous_valveStatus[valveIndex] != algorithmMode) {
			_previous_valveStatus[valveIndex] = algorithmMode;
			profileLogger() << L_time << L_tabs 
				<< (valveIndex == M_UpStrs ? "US_Mix R/Is" : "DS_Mix R/Is") << _mixCallTemp << flowTemp()
				<< showState() << reg.get(Mix_Valve::R_COUNT) << reg.get(Mix_Valve::R_VALVE_POS) << reg.get(Mix_Valve::R_RATIO)
				<< reg.get(Mix_Valve::R_FROM_POS) << reg.get(Mix_Valve::R_FROM_TEMP) << L_endl;
		}
//#endif
	}

	const __FlashStringHelper* MixValveController::showState() const {
		auto reg = registers();
		switch (reg.get(Mix_Valve::R_MODE)) { // e_Moving, e_Wait, e_Checking, e_Mutex, e_NewReq, e_AtLimit, e_DontWantHeat
		case Mix_Valve::e_Wait: return F("Wt");
		case Mix_Valve::e_Checking: return F("Chk");
		case Mix_Valve::e_Mutex: return F("Mx");
		case Mix_Valve::e_NewReq: return F("Req");
		case Mix_Valve::e_AtLimit: return F("Lim");
		case Mix_Valve::e_DontWantHeat: return F("Min");
		case Mix_Valve::e_Moving:
			switch (int8_t(reg.get(Mix_Valve::R_MOTOR_ACTIVITY))) { // e_Moving_Coolest, e_Cooling = -1, e_Stop, e_Heating
			case Mix_Valve::e_Moving_Coolest: return F("Min");
			case Mix_Valve::e_Cooling: return F("Cl");
			case Mix_Valve::e_Heating: return F("Ht");
			default: return F("Stp"); // Now stopped!
			}
		default: return F("SEr");
		}
	}

	void MixValveController::sendRequestFlowTemp(uint8_t callTemp) {
		_mixCallTemp = callTemp;
		registers().set(Mix_Valve::R_REQUEST_FLOW_TEMP, _mixCallTemp);
		writeReg(Mix_Valve::R_REQUEST_FLOW_TEMP);
	}

	bool MixValveController::amControlZone(uint8_t newCallTemp, uint8_t maxTemp, uint8_t zoneRelayID) { // highest callflow temp
		// Lambdas
		auto relayName = [](int id) -> const char* { // only for error reporting
			switch (id) {
			case R_Flat: return "Flat-UFH";
			case R_FlTR: return "Fl-TR";
			case R_HsTR: return "House-TR";
			case R_UpSt: return "US-UFH";
			case R_DnSt: return "DS-UFH";
			default: return "";
			}
		};

		auto setNewControlZone = [maxTemp, this, zoneRelayID, relayName]() {
			profileLogger() << L_time << F("MixValveController: ") << relayName(zoneRelayID) << F(" is new ControlZone.\t New MaxTemp: ") << maxTemp << L_endl;
			_limitTemp = maxTemp;
			_controlZoneRelay = zoneRelayID;
		};

		auto hasLimitPriority = [maxTemp, this, newCallTemp]() -> bool {
			auto zoneIsHeating = newCallTemp > MIN_FLOW_TEMP;
			if (zoneIsHeating && maxTemp <= _limitTemp) {
				_limitTemp = maxTemp;
				if (_mixCallTemp > _limitTemp) _mixCallTemp = 0; // trigger take control
				return true;
			} else return false;
		};
		
		auto amControllingZone = [this,zoneRelayID]() -> bool {return _controlZoneRelay == zoneRelayID && _limitTemp < 100; };

		auto checkForNewControllingZone = [newCallTemp, this, amControllingZone, hasLimitPriority, setNewControlZone]() {
			// controlZone is one calling for heat with lowest max-temp and highest call-temp.
			if (!amControllingZone() && hasLimitPriority() && newCallTemp > _mixCallTemp) { // new control zone
				setNewControlZone();
			}
		};
				
		// Algorithm
		checkForNewControllingZone();
		bool amInControl = amControllingZone();
		if (amInControl) {
			auto mv_FlowTemp = flowTemp();
#if defined (ZPSIM)
			bool debug;
			if (zoneRelayID == 3)
				debug = true;
			if (_localRegOffset == 1) 
				debug = true;
			else { // upstairs
				debug = true;
				if (mv_FlowTemp >= _mixCallTemp)
					bool debug = true;
			}
#endif
			
			if (newCallTemp <= MIN_FLOW_TEMP) {
				profileLogger() << L_time << F("MixValveController: ") << relayName(zoneRelayID) << F(" Released Control.") << L_endl;
				newCallTemp = MIN_FLOW_TEMP;
				_relayArr[_controlZoneRelay].clear(); // turn call relay OFF
				_limitTemp = 100; // release control of limit temp.
				amInControl = false;
			}
			else {
				_relayArr[_controlZoneRelay].set(); // turn call relay ON
				if (newCallTemp > _limitTemp) newCallTemp = _limitTemp;
			}

			if (_mixCallTemp != newCallTemp) {
				sendRequestFlowTemp(newCallTemp);
			} 
		}
		return amInControl;
	}

	// Private Methods

	int8_t MixValveController::relayInControl() const {
		return _relayArr[_controlZoneRelay].logicalState() == 0 ? -1 : _controlZoneRelay;
	}

	bool MixValveController::needHeat(bool isHeating) {
		if (relayInControl() == -1) return false;

		int storeTempAtMixer = _storeTempSens->get_temp(); 
		//auto minStoreTemp = isHeating ? _mixCallTemp + THERM_STORE_HYSTERESIS : _mixCallTemp;
		if (registers().get(Mix_Valve::R_VALVE_POS) == VALVE_TRANSIT_TIME) {
			profileLogger() << L_time << (_localRegOffset == 1 ? "US" : "DS") << " Mix-Store Temp: " << storeTempAtMixer << L_endl;
			return true;
		}
		return false;
	}

	void MixValveController::readRegistersFromValve() {
#ifdef ZPSIM
		//uint8_t (&debugWire)[30] = reinterpret_cast<uint8_t (&)[30]>(TwoWire::i2CArr[getAddress()][0]);
#endif
		auto reg = registers();
		// Lambdas
		auto give_MixV_Bus = [this](Mix_Valve::I2C_Flags_Obj i2c_status) {
			rawRegisters().set(R_PROG_STATE, 1);
			i2c_status.set(Mix_Valve::F_I2C_NOW);
			writeRegValue(Mix_Valve::R_DEVICE_STATE, i2c_status);
		};

		auto read_flowTempSensors = [this, &reg]() {
			_slaveMode_flowTempSensor.readTemperature();
			auto flowTemp = _slaveMode_flowTempSensor.get_temp();
			reg.set(Mix_Valve::R_FLOW_TEMP, flowTemp);
			auto status = writeReg(Mix_Valve::R_FLOW_TEMP);
			if (status) {
				logger() << "Write MixVTempS " << flowTemp << I2C_Talk::getStatusMsg(status) << L_endl;
			}
		};

		// Algorithm
		uint8_t value = 0;
		auto status = reEnable(); // see if is disabled
		waitForWarmUp();
		if (status == _OK) {
			wait_DevicesToFinish(rawRegisters());
			auto i2c_status = Mix_Valve::I2C_Flags_Obj{ reg.get(Mix_Valve::R_DEVICE_STATE) };
			//logger() << L_time << "MV-Master[" << index() << "] Req Temp" << L_endl;
			give_MixV_Bus(i2c_status);
			wait_DevicesToFinish(rawRegisters());
			status = readRegSet(Mix_Valve::R_DEVICE_STATE, Mix_Valve::MV_NO_TO_READ); // recovery
			auto flowTemp = reg.get(Mix_Valve::R_FLOW_TEMP);
			if (flowTemp == 255) {
				logger() << L_time << F("read device 0x") << L_hex << getAddress() << I2C_Talk::getStatusMsg(status);
				logger() << L_dec << F(". FlowTemp reads 255 at freq: ") << runSpeed();
			}
			if (status) {
				logger() << L_time << F("MixValve Read device 0x") << L_hex << getAddress() << I2C_Talk::getStatusMsg(status) << " at freq: " << L_dec << runSpeed() << L_endl;
				readRegSet(Mix_Valve::R_DEVICE_STATE, Mix_Valve::MV_NO_TO_READ);
			}
		} else logger() << L_time << F("MixValve Read device 0x") << L_hex << getAddress() << F(" disabled") << L_endl;
		
		if (reg.get(Mix_Valve::R_REQUEST_FLOW_TEMP) == 0) { // Resend to confirm new temp request
			reg.set(Mix_Valve::R_REQUEST_FLOW_TEMP, _mixCallTemp);
			writeReg(Mix_Valve::R_REQUEST_FLOW_TEMP);
		} else if (reg.get(Mix_Valve::R_REQUEST_FLOW_TEMP) != _mixCallTemp) { // This gets called when R_REQUEST_FLOW_TEMP == 255. -- not sure why!
			logger() << "MV_Request_Reg :" << reg.get(Mix_Valve::R_REQUEST_FLOW_TEMP) << " Req: " << _mixCallTemp << L_endl;
			logger() << "MV_FlowT_Reg :" << reg.get(Mix_Valve::R_FLOW_TEMP) << L_endl;
			reg.set(Mix_Valve::R_REQUEST_FLOW_TEMP, _mixCallTemp);
			writeReg(Mix_Valve::R_REQUEST_FLOW_TEMP);
		}
	}
}