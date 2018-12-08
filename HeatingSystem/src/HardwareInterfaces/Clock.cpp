#include "Clock.h"
#include "Logging.h"
#include "EEPROM.h"

namespace HeatingSystemSupport {
	int writer(int address, const void * data, int noOfBytes); // function requred by RDB, borrowed here
	int reader(int address, void * result, int noOfBytes); // function requred by RDB, borrowed here
}
using namespace HeatingSystemSupport;

namespace HardwareInterfaces {
	using namespace GP_LIB;
	using namespace Date_Time;

	uint8_t Clock::_mins1; // minute units
	uint8_t Clock::_secs;
	uint8_t Clock::_autoDST;
	uint8_t Clock::_dstHasBeenSet;
	Date_Time::DateTime Clock::_now;
	uint32_t Clock::_lastCheck_mS = millis();

	///////////////////////////////////////////////////
	//         Public Static Helper Functions        //
	///////////////////////////////////////////////////

	int Clock::secondsSinceLastCheck(uint32_t & lastCheck_mS) { // static function
		// move forward one second every 1000 milliseconds
		uint32_t elapsedTime = millisSince(lastCheck_mS);
		int seconds = 0;
		while (elapsedTime >= 1000) {
			lastCheck_mS += 1000;
			elapsedTime -= 1000;
			++seconds;
		}
		return seconds;
	}

	///////////////////////////////////////////////////
	//             Clock Member Functions            //
	///////////////////////////////////////////////////

	DateTime Clock::_dateTime() { 
		// called on every request for time/date.
		// Only needs to check anything every 10 minutes
		static uint8_t oldHr = _now.hrs() + 1; // force check first time through
		int newSecs = _secs + secondsSinceLastCheck(_lastCheck_mS);
		int newMins = _mins1 + newSecs / 60;
		_mins1 = newMins % 10;
		_secs = newSecs % 60;
		if (newMins < 10) return _now;

		_now.addOffset({ m10, newMins / 10 });
		_update();
		if (_now.hrs() != oldHr) {
			oldHr = _now.hrs();
			_adjustForDST();
		}
		return _now;
	}

	void Clock::setHrMin(int hr, int min) {
		_now.time() = TimeOnly(hr, min);
		setMinUnits(min % 10);
		saveTime();
	}

	void Clock::setTime(Date_Time::DateOnly date, Date_Time::TimeOnly time, int min) {
		_now = DateTime{ date, time };
		setMinUnits(min);
		setAutoDSThours(true);
		setSeconds(0);
		saveTime();
	}

	void Clock::_adjustForDST() {
		//Last Sunday in March and October. Go forward at 1 am, back at 2 am.
		if ((autoDSThours()) && (_now.month() == 3 || _now.month() == 10)) { // Mar/Oct
			if (_now.day() == 1 && dstHasBeenSet()) {
				dstHasBeenSet(false); // clear
				saveTime();
			}
			else if (_now.day() > 24) { // last week
				if (_now.weekDayNo() == SUNDAY) {
					if (!dstHasBeenSet() && _now.month() == 3 && _now.hrs() == 1) {
						_now.setHrs(autoDSThours()+1);
						dstHasBeenSet(true); // set
						saveTime();
					}
					else if (!dstHasBeenSet() && _now.month() == 10 && _now.hrs() == 2) {
						_now.setHrs(2-autoDSThours());
						dstHasBeenSet(true); // set
						saveTime();
					}
				}
			}
		}
	}

#ifdef ZPSIM
	void Clock::testAdd1Min() {
		if (minUnits() < 9) setMinUnits(minUnits() + 1);
		else {
			setMinUnits(0);
			_now.addOffset({ m10,1 });
		}
		_update();
	}
#endif
	
	///////////////////////////////////////////////////////////////
	//                     EEPROM_Clock                          //
	///////////////////////////////////////////////////////////////

	EEPROM_Clock::EEPROM_Clock(unsigned int addr) :_addr(addr) { loadTime(); }

	void EEPROM_Clock::saveTime() {
		auto nextAddr = writer(_addr, &_now, 4);
		nextAddr = writer(nextAddr, &_autoDST, 1);
		writer(nextAddr, &_dstHasBeenSet, 1);
	}
	
	void EEPROM_Clock::loadTime() {
		auto nextAddr = reader(_addr, &_now, 4);
		nextAddr = reader(nextAddr, &_autoDST, 1);
		nextAddr = reader(nextAddr, &_dstHasBeenSet, 1);
		if (year() == 0) {
			_setFromCompiler();
		}
	}

	void EEPROM_Clock::_update() { // called every 10 minutes
		saveTime();
	}


	///////////////////////////////////////////////////////////////
	//                     I2C_Clock                             //
	///////////////////////////////////////////////////////////////

	I2C_Clock::I2C_Clock(I2C_Helper & i2C, int addr) : I_I2Cdevice(i2C, addr) { loadTime(); }

	void I2C_Clock::loadTime() {
		// called by logToSD so must not recursivly call logToSD
		int errCode = 0;
#if !defined (ZPSIM)
		if (_i2C) {
			//		int tryAgain = 10;
			uint8_t data[9];
			//		do  {
			//data[1] = 0;
			//data[2] = 0;
			//data[4] = 0;
			//data[5] = 0;
			data[6] = 0;
			//errCode = _i2C->read(RTC_ADDRESS,0x08,9,data); // Last Compiler Time

			errCode = _i2C->read(_address, 0, 9, data);
			while ((errCode != 0 || data[6] == 255) && _i2C->getI2CFrequency() > _i2C->MIN_I2C_FREQ) {
				log().logToSD_notime("Error reading RTC : ", errCode);
				//_i2C->slowdown_and_reset(0);
				data[6] = 0;
				errCode = _i2C->read(_address, 0, 9, data);
			}
			if (errCode != 0) {
				log().logToSD_notime("RTC Unreadable");
			}
			else if (data[6] == 0) {
				_setFromCompiler();
			}
			else {
				_now.setMins10(data[1] >> 4);
				_now.setHrs(fromBCD(data[2]));
				_now.setDay(fromBCD(data[4]));
				_now.setMonth(fromBCD(data[5]));
				_now.setYear(fromBCD(data[6]));
				_autoDST = data[8] >> 1;
				_dstHasBeenSet = data[8] & 1;
				setMinUnits(data[1] & 15);
				setSeconds(fromBCD(data[0]));
				//logToSD_notime("Reading RTC year: ", year);
			}
			//		} while (data[6] == 0 && --tryAgain > 0);
		}
		else {
			_setFromCompiler();
			errCode = I2C_Helper::_I2C_Device_Not_Found;
		}
#endif	
	}

	void I2C_Clock::saveTime() {
		uint8_t data[9];
		data[0] = toBCD(_secs); // seconds
		data[1] = (_now.mins10() << 4) + minUnits();
		data[2] = toBCD(_now.hrs());
		data[3] = 0; // day of week
		data[4] = toBCD(_now.day());
		data[5] = toBCD(_now.month());
		data[6] = toBCD(_now.year());
		data[7] = 0; // disable SQW
		data[8] = _autoDST << 1 | _dstHasBeenSet; // in RAM
		uint8_t errCode = _i2C->write(_address, 0, 9, data);
		while (errCode != 0 && _i2C->getI2CFrequency() > _i2C->MIN_I2C_FREQ) {
			log().logToSD_notime("Error writing RTC : ", errCode);
			//_i2C->slowdown_and_reset(0);
			errCode = _i2C->write(_address, 0, 9, data);
		}
		if (errCode != 0) {
			log().logToSD_notime("Unable to write RTC:", errCode);
		}
		else {
			log().logToSD_notime("Saved CurrDateTime");
		}
		//#if defined ZPSIM
		//	CURR_TIME = currentTime();
		//#endif
	}

	void I2C_Clock::_update() { // called every 10 minutes
		loadTime();
	}

}