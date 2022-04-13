#include "Clock.h"
#include <Timer_mS_uS.h>

///////////////////////////////////////////////////
//         Public Global Functions        //
///////////////////////////////////////////////////

using namespace GP_LIB;
using namespace Date_Time;

	uint8_t Clock::_mins1; // minute units
	uint8_t Clock::_secs;
	uint8_t Clock::_autoDST = 1;
	uint8_t Clock::_dstHasBeenSet;
	Date_Time::DateTime Clock::_now;
	uint32_t Clock::_lastCheck_mS = millis() /*- uint32_t(60ul * 1000ul * 11ul)*/;

	///////////////////////////////////////////////////
	//             Clock Member Functions            //
	///////////////////////////////////////////////////

	DateTime Clock::_dateTime() const { 
		// called on every request for time/date.
		// Only needs to check anything every 10 minutes
		// Updates _lastCheck_mS to last second
		static uint8_t oldHr = _now.hrs() + 1; // force check first time through
		int newSecs = _secs + secondsSinceLastCheck(_lastCheck_mS);
		_secs = newSecs % 60;
		int newMins = _mins1 + newSecs / 60;
		_mins1 = newMins % 10;
		if (newMins >= 10) {
			Clock & modifierRef = const_cast<Clock &>(*this);
			_now.addOffset({ m10, newMins / 10 });
			modifierRef._update();
			if (_now.hrs() != oldHr) {
				oldHr = _now.hrs();
				modifierRef._adjustForDST();
			}
		}
		//Serial.print(" Refresh: "); Serial.println(_now.hrs());
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

	bool Clock::isNewSecond(uint8_t& oldSecond) {
		refresh();
		bool isNewSec = oldSecond != seconds();
#ifdef ZPSIM
		isNewSec = true;
#endif
		if (isNewSec) oldSecond = seconds(); 
		return isNewSec; 
	}

	bool Clock::isNewMinute(uint8_t& oldMin) {
		refresh();
		bool isNewMin = oldMin != minUnits();
		if (isNewMin) oldMin = minUnits();
		return isNewMin;
	}

	bool Clock::isNew10Min(uint8_t& oldMin) {
		refresh();
		bool isNewMin = oldMin != mins10();
		if (isNewMin) oldMin = mins10();
		return isNewMin;
	}	
	
	//Clock::NewPeriod Clock::isNewPeriod(uint32_t& lastCheck_mS) {
	//	refresh();
	//	NewPeriod newPeriod = NOT_NEW;
	//	if (lastCheck_mS / 1000 != _lastCheck_mS / 1000) {
	//		newPeriod = NEW_SEC;
	//		if (lastCheck_mS / 10000 != _lastCheck_mS / 10000) {
	//			newPeriod = NEW_SEC10;
	//			if (lastCheck_mS / 60000 != _lastCheck_mS / 60000) {
	//				newPeriod = NEW_MIN;
	//				if (lastCheck_mS / 600000 != _lastCheck_mS / 600000) {
	//					newPeriod = NEW_MIN10;
	//					if (lastCheck_mS / 3600000 != _lastCheck_mS / 3600000) {
	//						newPeriod = NEW_HR;
	//						if (time().asInt() == 0) newPeriod = NEW_DAY;
	//					}
	//				}
	//			}
	//		}
	//	}
	//	lastCheck_mS = _lastCheck_mS;
	//	return newPeriod;
	//}
	
	Clock::NewPeriod Clock::isNewPeriod(uint8_t& lastCheck_S) {
		refresh();
		NewPeriod newPeriod = NOT_NEW;
		if (lastCheck_S != seconds()) {
			newPeriod = NEW_SEC;
			if (seconds() / 10 != lastCheck_S / 10) {
				newPeriod = NEW_SEC10;
				if (seconds() < lastCheck_S) {
					newPeriod = NEW_MIN;
					if (minUnits() % 10 == 0) {
						newPeriod = NEW_MIN10;
						if (minUnits() == 0) {
							newPeriod = NEW_HR;
							if (time().asInt() == 0) newPeriod = NEW_DAY;
						}
					}
				}
			}
			lastCheck_S = seconds();
		}
		return newPeriod;
	}

	uint8_t Clock::loadTime() {
		int compilerMins1, compilerSecs;
		_now = _timeFromCompiler(compilerMins1, compilerSecs);
		setMinUnits(compilerMins1);
		setSeconds(compilerSecs);
		return 0;
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