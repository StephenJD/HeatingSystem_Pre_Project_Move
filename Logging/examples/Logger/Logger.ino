#include <I2C_Talk_ErrorCodes.h>
#include <I2C_Talk.h>
#include <I2C_SpeedTest.h>
#include <I2C_Scan.h>
#include <I2C_RecoverStrategy.h>
#include <I2C_RecoverRetest.h>
#include <I2C_Recover.h>
#include <I2C_Device.h>
#include <Clock.h>
#include <Logging.h>
#include <Date_Time.h>
#include <I2C_Talk.h>
#include <Conversions.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <Arduino.h>
#include <EEPROM.h>

#define RTC_ADDRESS 0x68
#define EEPROM_ADDR 0x50
#define EEPROM_CLOCK_ADDR 0

#define RTC_RESET 4
using namespace Date_Time;

I2C_Talk rtc{ Wire1, 100000 };
I_I2C_Scan scanner{ rtc };

#if defined(__SAM3X8E__)
#include <EEPROM.h>

EEPROMClass & eeprom() {
	static EEPROMClass_T<rtc> _eeprom_obj{ 0x50 };
	return _eeprom_obj;
}

EEPROMClass & EEPROM = eeprom();
#endif

Clock & clock_() {
  static Clock_I2C<rtc> _clock(RTC_ADDRESS);
  return _clock;
}

Logger & timelessLogger() {
  static Serial_Logger _log(9600);
  return _log;
}

Logger & nullLogger() {
	static Logger _log{};
	return _log;
}

Logger & logger() {
  static Serial_Logger _log(9600, clock_());
  return _log;
}

Logger & sdlogger() {
  static SD_Logger _log("testlog.txt", 9600, clock_());
  return _log;
}

//////////////////////////////// Start execution here ///////////////////////////////
void setup() {
  Serial.begin(9600); // NOTE! Serial.begin must be called before i2c_clock is constructed.
  logger() << " Setup Start\n";
  pinMode(RTC_RESET, OUTPUT);
  digitalWrite(RTC_RESET, LOW); // reset pin
  rtc.restart();
  scanner.show_all();

  logger() << "Notime Logger Message\n";
  logger() << L_time << "Timed Logger Message\n";
  sdlogger() << L_time << "SD Timed Logger Started\n";
  clock_().saveTime();
  clock_().loadTime();
}

void loop()
{
  delay(5000);
  sdlogger() << "SD Timed Logger...\n";
  timelessLogger() << "timelessLogger\n";
  nullLogger() << "Nothing";
  sdlogger() << L_time << L_tabs << "tabbed" << 5 << L_concat << "untabbed" << 6 << L_endl;
  sdlogger() << L_time << "not tabbed" << 5 << L_endl;
  sdlogger() << L_time << "log" << 5 << L_tabs << "tabbed" << 6 << L_endl;
  sdlogger() << L_cout << "cout" << 5 << L_tabs << "tabbed" << 6 << L_endl;
  sdlogger() << "log after cout" << 5 << L_tabs << "tabbed" << 6 << L_endl;
}