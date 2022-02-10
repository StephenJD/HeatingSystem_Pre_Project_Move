#include <Arduino.h>
//#include <I2C_Recover.h>
#include <I2C_RecoverRetest.h>

#include "OLED_Thick_Display.h"
#include "..\HeatingSystem\src\HardwareInterfaces\A__Constants.h"

#include <EEPROM_RE.h>

I2C_Talk& i2C() {
    static I2C_Talk _i2C{};
    return _i2C;
}

EEPROMClassRE& eeprom() {
  static EEPROMClassRE _eeprom;
	return _eeprom;
}

void ui_yield() {}

//I2C_Recovery::I2C_Recover i2c_recover(i2C());
I2C_Recovery::I2C_Recover_Retest i2c_recover(i2C());
unsigned long timeOfReset_mS;

#ifdef ZPSIM
    auto my_registers = i2c_registers::Registers<OLED_Thick_Display::R_DISPL_REG_SIZE, HardwareInterfaces::US_CONSOLE_I2C_ADDR>{i2C()};
#else
    auto my_registers = i2c_registers::Registers<OLED_Thick_Display::R_DISPL_REG_SIZE>{i2C()};
#endif

OLED_Thick_Display this_OLED_Thick_Display{i2c_recover, my_registers, HardwareInterfaces::PROGRAMMER_I2C_ADDR, 0, 0, &timeOfReset_mS };

void setup() {
  for (int pin = 0; pin < 18; ++pin) pinMode(pin, INPUT_PULLUP);
  i2C().onReceive(my_registers.receiveI2C);
  i2C().onRequest(my_registers.requestI2C);
  this_OLED_Thick_Display.begin();
}

void loop() {
	this_OLED_Thick_Display.processKeys();
}

void moveDisplay() {this_OLED_Thick_Display.moveDisplay();}
