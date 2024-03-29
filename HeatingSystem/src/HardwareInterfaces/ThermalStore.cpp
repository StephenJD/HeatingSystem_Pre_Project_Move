 #include "ThermalStore.h"
#include <Logging.h>
#include "BackBoiler.h"
#include "MixValveController.h"
#include "A__Constants.h"
#include "..\Client_DataStructures\Data_TempSensor.h" // relative path required by Arduino
#include <FlashStrings.h>

namespace HardwareInterfaces {

	ThermalStore::ThermalStore(UI_TempSensor * tempSensorArr, MixValveController(&mixValveControllerArr)[Assembly::NO_OF_MIX_VALVES], BackBoiler & backBoiler)
		: _tempSensorArr(tempSensorArr)
		, _mixValveControllerArr(mixValveControllerArr)
		, _backBoiler(backBoiler)
	{}

	void ThermalStore::initialise(client_data_structures::R_ThermalStore thermStoreData) {
		_thermStoreData = thermStoreData;
		calcCapacities();
	}

	//int16_t ThermalStore::getFractionalOutsideTemp() const {
	//	return f->tempSensorR(getVal(OutsideTS)).getFractionalSensTemp();
	//}

	uint8_t ThermalStore::getTopTemp() const {
		uint8_t temp = _tempSensorArr[_thermStoreData.OvrHeatTS].get_temp();
		if (UI_TempSensor::hasError()) temp = _tempSensorArr[_thermStoreData.DHWpreMixTS].get_temp();
		return temp;
	}

	//uint8_t ThermalStore::getOutsideTemp() const {
	//	return f->tempSensorR(getVal(OutsideTS)).getSensTemp();
	//}

	void ThermalStore::setLowestCWtemp(bool isFlowing) {
		if (isFlowing || (_groundT > getGroundTemp())) _groundT = getGroundTemp();
	}

	uint8_t ThermalStore::getGroundTemp() const {
		return  _tempSensorArr[_thermStoreData.GroundTS].get_temp();
	}

	uint8_t ThermalStore::getOutsideTemp() const {
		return  _tempSensorArr[_thermStoreData.OutsideTS].get_temp();
	}

	bool ThermalStore::dumpHeat() const {
		return getTopTemp() >= _thermStoreData.OvrHeatTemp;
	}

	//bool ThermalStore::check() { // checks zones, twl rads & thm store for heat requirement and sets relays accordingly
	//	--Zone_Run::z_period;
	//	uint8_t i;
	//	for (i = 0; i < f->towelRads.count(); ++i) { // takes 200mS, required every second
	//		f->towelRads[i].run().check(); // setFlowTemp, mixCall & switches relays
	//	}
	//
	//	for (i = 0; i < f->zones.count(); ++i) { // takes 850mS
	//		// DHW zone checks Mix Valves are hot enough & DHW is OK.
	//		f->zoneR(i).check(); // setZFlowTemp, mixCall & switches relays
	//	}
	//
	//	if (Zone_Run::z_period <= 0) Zone_Run::z_period = AVERAGING_PERIOD;
	//
	//	if (!temp_sense_hasError) {
	//		for (i = 0; i < f->mixingValves.count(); ++i) { // takes 104mS
	//			f->mixValveR(i).check(); // Check Mixing Valve flow temps
	//		}
	//	}
	//	return true;
	//}

	bool ThermalStore::needHeat(int currRequest, int nextRequest) {
		// called every Minute.
		setLowestCWtemp(false);
		bool needHeat = dhwNeedsHeat(currRequest, nextRequest);
		logger() << L_time << F("ThermalStore:: currRequest: ") << currRequest << F(" CurrDHW temp: ") << _theoreticalDeliveryTemp << (needHeat ? F(" Asking for heat.") : F(" Not asking for heat.")) <<  (_isHeating ? F(" Is heating") : F(" Not heating"))  << L_endl;
		if (!_isHeating && needHeat) logger() << L_time << F("DHW Needs Heat. CurrRequest: ") << currRequest << F(" Curr DHW temp: ") << _theoreticalDeliveryTemp << L_endl;
		// Check temp for each mix valve		
		auto mixNeedsHeat = false;
		for (auto & mixV : _mixValveControllerArr) {
			mixNeedsHeat = mixNeedsHeat || mixV.needHeat(_isHeating);
		}
		if (/*!_isHeating && */mixNeedsHeat) logger() << L_time << F("MixValve Needs Heat\n");
		needHeat |= mixNeedsHeat;
		if (_isHeating && !needHeat) {
			//f->eventS().newEvent(EVT_GAS_TEMP, f->tempSensorR(getVal(GasTS)).getSensTemp());
			logger() << F("\tGas Turned OFF   Gas Flow Temp: ") << _tempSensorArr[_thermStoreData.GasTS].get_temp();
			logger() << F("\n\tCurr DHW temp: ") << _theoreticalDeliveryTemp << L_endl;
		}
		else if (!_isHeating && needHeat) {
			logger() << F("\tGas Turned ON\n");
			logger() << L_tabs << F("GroundT:") << _groundT
				<< F("Top:") << getTopTemp()
				<< F("Mid:") << _tempSensorArr[_thermStoreData.MidDhwTS].get_temp()
				<< F("Lower") << _tempSensorArr[_thermStoreData.LowerDhwTS].get_temp() << L_endl;
		}
		_isHeating = needHeat;
		return needHeat;
	}

	// Private methods

	void ThermalStore::calcCapacities() {
		// Using conductionConstant, flowRate & coilFraction calculate exponent k at each level
		// Actual Data without gas-reheat on shallow bath: Top:-3C, Mid:-8C, Bot:-4C.
		uint8_t flowRate = _thermStoreData.DHWflowRate;

		float cond = _thermStoreData.Conductivity; //25; //
		float f_array[4]; // First calc length of Water Column measured by sensor
		f_array[0] = float(_thermStoreData.MidSensHeight - _thermStoreData.LowerSensHeight); // Bottom Column cm's
		f_array[1] = (_thermStoreData.TopSensHeight - _thermStoreData.LowerSensHeight) / 2.0F; // Mid Column
		f_array[2] = _thermStoreData.CylHeight - (_thermStoreData.TopSensHeight + _thermStoreData.MidSensHeight) / 2.F; // Top Column
		f_array[3] = f_array[0] + f_array[1] + f_array[2]; // Total Length of useful water column
		auto volPerM = pow((float)_thermStoreData.CylDia, 2) * 0.00071F;
		_bottomV = f_array[0] * volPerM; // vol in litres, allowing 10% for coil volume
		_midV = f_array[1] * volPerM; // vol in litres
		_upperV = f_array[2] * volPerM; // vol in litres
		// Now calc Fraction of heatexchanger at each level
		f_array[0] = f_array[0] / f_array[3];
		f_array[1] = f_array[1] / f_array[3];
		f_array[2] = f_array[2] / f_array[3];
		float k[3];
		auto conductivityConst = 0.046F - 0.0031F * cond;
		auto conductivityNumerator = .026F * cond - .032F;
		auto conductivityPowerConst = 1.0F - .001F * cond;
		auto conductivityPowerFact = .003F * cond - .025F;
		auto flowPerSec = flowRate / 60.0F;
		k[0] = conductivityConst * f_array[0] + conductivityNumerator *f_array[0] / pow(flowPerSec, conductivityPowerConst + conductivityPowerFact * f_array[0]);
		k[1] = conductivityConst * f_array[1] + conductivityNumerator *f_array[1] / pow(flowPerSec, conductivityPowerConst + conductivityPowerFact * f_array[1]);
		k[2] = conductivityConst * f_array[2] + conductivityNumerator *f_array[2] / pow(flowPerSec, conductivityPowerConst + conductivityPowerFact * f_array[2]);

		_bottomC = 1.0F - exp(-k[0]);
		_midC = 1.0F - exp(-k[1]);
		_upperC = 1.0F - exp(-k[2]);
		logger() << F("\nThermalStore::calcCapacities\t_upperC ") << _upperC << F(" _midC ") << _midC << F(" _bottomC ") << _bottomC << L_endl;
	}

	uint8_t ThermalStore::calcCurrDeliverTemp(int callTemp) const {
		float boilerPwr = _thermStoreData.BoilerPower;
		uint8_t duration = _thermStoreData.DHWflowTime;
		float groundT = _groundT;
		float topT = getTopTemp();
		float midT = _tempSensorArr[_thermStoreData.MidDhwTS].get_temp();
		float botT = _tempSensorArr[_thermStoreData.LowerDhwTS].get_temp();
		//logger() << F("\nThermalStore::calcCurrDeliverTemp\t_topT ") << topT << F(" mid ") << midT << F(" Bot ") << botT << F(" Grnd ") << _groundT << F(" Cond ") << _thermStoreData.Conductivity << L_endl;
		//logger() << F("\nThermalStore Capacities\t_upperC ") << _upperC << F(" _midC ") << _midC << F(" _bottomC ") << _bottomC << L_endl;

		// using Capacities, ground and store temps, calc HW temp at each level
		float HWtemp[3];
		HWtemp[0] = groundT + (botT - groundT) * _bottomC;
		HWtemp[1] = HWtemp[0] + (midT - HWtemp[0]) * _midC;
		HWtemp[2] = HWtemp[1] + (topT - HWtemp[1]) * _upperC;
		//logger() << F("\t_HWtemp[0] ") << HWtemp[0] << F(" HWtemp[1] ") << HWtemp[1] << F(" HWtemp[2] ") << HWtemp[2] << L_endl;
		// Using HWtemps, calculate share of energy for each section of store
		float share[3];
		share[0] = (HWtemp[0] - groundT) / (HWtemp[2] - groundT);
		share[1] = (HWtemp[1] - HWtemp[0]) / (HWtemp[2] - groundT);
		share[2] = (HWtemp[2] - HWtemp[1]) / (HWtemp[2] - groundT);
		//logger() << F("\t_share[0] ") << share[0] << F(" share[1] ") << share[1] << F(" share[2] ") << share[2] << L_endl;
		
		// Calc final store temps using ratio of DHW vol to storeVol
		float storeTemps[3];
		float factor = ((callTemp - groundT) * _thermStoreData.DHWflowRate * .07F - boilerPwr) * duration *1.43F;
		storeTemps[0] = botT - factor * share[0] / _bottomV;
		storeTemps[1] = midT - factor * share[1] / _midV;
		storeTemps[2] = topT - factor * share[2] / _upperV;
		//logger() << F("\t_storeTemps[0] ") << storeTemps[0] << F(" storeTemps[1] ") << storeTemps[1] << F(" storeTemps[2] ") << storeTemps[2] << L_endl;

		// Calc final HW temps
		HWtemp[0] = groundT + (storeTemps[0] - groundT) * _bottomC;
		HWtemp[1] = HWtemp[0] + (storeTemps[1] - HWtemp[0]) * _midC;
		HWtemp[2] = HWtemp[1] + (storeTemps[2] - HWtemp[1]) * _upperC + 0.5f;
		//logger() << F("\t_Final HWtemp[0] ") << HWtemp[0] << F(" HWtemp[1] ") << HWtemp[1] << F(" HWtemp[2] ") << HWtemp[2] << L_endl;
		return (uint8_t)HWtemp[2];
	}

	bool ThermalStore::dhwNeedsHeat(int callTemp, int nextRequest) {
		_theoreticalDeliveryTemp = calcCurrDeliverTemp(nextRequest > callTemp ? nextRequest : callTemp);
		//logger() << F("\nThermalStore::dhwNeedsHeat\t_theoreticalDeliveryTemp ") << _theoreticalDeliveryTemp << L_endl;
		// Note that because callTemp determins the rate of heat-extraction, currDeliverTemp, which is temperature AFTER filling a bath, will change if callTemp changes.
		static bool hasRequestedCondReduction = false;
		bool dhwTempOK = dhwDeliveryOK(callTemp);
		if (!hasRequestedCondReduction && _theoreticalDeliveryTemp >= callTemp && !dhwTempOK) { // reduce conductivity if claims to be hot enought, but isn't
			hasRequestedCondReduction = true;
			//f->eventS().newEvent(EVT_THS_COND_CHANGE,S1_byte(getVal(Conductivity)) - 1);
			logger() << F("\nThermalStore::dhwNeedsHeat\tPre-Mix temp too low - reduce cond?\t ") 
				<< F(" Cond: ") << _thermStoreData.Conductivity
				<< F(" CallTemp: ") << callTemp
				<< F(" NextCallTemp: ") << nextRequest
				<< F(" Calculated DHW: ") << _theoreticalDeliveryTemp
				<< F(" DHW-flowTemp: ") << _tempSensorArr[_thermStoreData.DHWFlowTS].get_temp()
				<< F(" DHW-preMixTemp: ") << L_fixed << _tempSensorArr[_thermStoreData.DHWpreMixTS].get_fractional_temp() / 256 << L_endl;
			//setVal(Conductivity, uint8_t(getVal(Conductivity)) - 1);
		}
		if (dhwTempOK) hasRequestedCondReduction = false;
		uint8_t addHysteresis = 0;
		if (!_backBoiler.check() && _isHeating) { // only add hysteresis if stove not on.
			addHysteresis = THERM_STORE_HYSTERESIS;
		}
		return _theoreticalDeliveryTemp < (callTemp + addHysteresis);	// If HWtemp[2] is not hot enough, we need heat.
	}

	bool ThermalStore::dhwDeliveryOK(int currRequest) const {
		/*
		Compare DHW temp each side of thermostatic valve to see if cold is being mixed in.
		If temps are the same, tank is too cool.
		*/
		bool tempError = false;
		auto dhwFlowTemp = _tempSensorArr[_thermStoreData.DHWFlowTS].get_temp();
		auto DHWpreMixTemp = _tempSensorArr[_thermStoreData.DHWpreMixTS].get_temp();
		if (DHWpreMixTemp < 0 || dhwFlowTemp > DHWpreMixTemp + 2) tempError = true;
		return (tempError || dhwFlowTemp >= currRequest || DHWpreMixTemp > dhwFlowTemp);
	}
}