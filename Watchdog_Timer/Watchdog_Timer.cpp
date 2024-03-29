#include <Watchdog_Timer.h>

#if defined(__SAM3X8E__)
	#include <Watchdog.h>
	// functions must be in cpp.
	void watchdogSetup() { watchdogEnable(16000);}

	void reset_watchdog() {watchdogReset(); }

	/// <summary>
	/// Max priod is 16000mS
	/// </summary>
	void set_watchdog_timeout_mS(int period_mS)  {
		watchdogEnable(period_mS);
		reset_watchdog();
	}

	void disable_watchdog() { watchdogDisable(); }

	//void WDT_Init();
	//{
	//	watchdogDisable();
	//}

#else
	//Code in here will only be compiled if an Arduino Mega is used.
	#ifdef ZPSIM
		void wdt_reset() {}
		void wdt_enable(int) {}
		void wdt_disable() {}
	#else
		#include <avr/wdt.h>
	#endif

	//WDTO_15MS   0
	//WDTO_30MS   1
	//WDTO_60MS   2
	//WDTO_120MS   3
	//WDTO_250MS   4
	//WDTO_500MS   5
	//WDTO_1S   6
	//WDTO_2S   7
	//WDTO_4S   8
	//WDTO_8S   9

	int toWD_Time(int period_mS) {
		if (period_mS > 8000) period_mS = 8000;
		period_mS /= 15 + 1;
		auto wd_time = 0;
		while (period_mS) {
			++wd_time;
			period_mS /= 2;
		}
		return wd_time;
	}

	void reset_watchdog() {
		wdt_reset();
	}

	void set_watchdog_timeout_mS(int period) {
		wdt_enable(toWD_Time(period));
		reset_watchdog();
	}

	void disable_watchdog() { wdt_disable(); }

	//// Function Implementation
	//void wdt_init(void) // ensures WDT is disabled on re-boot to prevent repeated re-boots after a timeout
	//{
	//	MCUSR = 0;
	//	wdt_disable();
	//	return;
	//}
#endif

