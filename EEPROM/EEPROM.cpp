/*
  EEPROM.cpp - EEPROM library
  Copyright (c) 2006 David A. Mellis.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/******************************************************************************
 * Includes
 ******************************************************************************/
#if defined(__SAM3X8E__) || defined ZPSIM

#include "EEPROM.h"
//#include "I2C_Helper.h"
#include "I2C_Talk.h"

/******************************************************************************
 * Constructors
 ******************************************************************************/

#if defined (ZPSIM)
	using namespace std;
	#include <iostream>
	#include <fstream>

	uint8_t EEPROMClass::myEEProm[4096];
	auto i2c = I2C_Talk();

	EEPROMClass fileEEPROM(i2c,0);

	void EEPROMClass::saveEEPROM() {
		auto myfile = ofstream("EEPROM.dat", ios::binary);
		if (myfile) {
			myfile.write((char*)myEEProm, 4096);
		}
		else cout << "Unable to open file";
	}

	EEPROMClass::~EEPROMClass() {
#if defined LOAD_EEPROM
		saveEEPROM();
#endif
	}
#endif

	EEPROMClass::EEPROMClass(I2C_Talk & i2C, int addr) : I_I2Cdevice(i2C, addr) {
#if defined LOAD_EEPROM
		ifstream myfile("EEPROM.dat", ios::binary);	// Input file stream
		if (myfile.is_open()) {
			myfile.read((char*)myEEProm, 4096);
			myfile.close();
		}
		else cout << "Unable to open file";
#endif	
	}

	EEPROMClass & EEPROM(fileEEPROM);

/******************************************************************************
 * User API
 ******************************************************************************/

//void EEPROMClass::setI2Chelper(I2C_Helper & i2C) {
//	_i2C = &i2C;
//	_i2C->result.reset();
//	_i2C->result.foundDeviceAddr = _eepromAddr;
//	_i2C->speedTestS();
//	_i2C->setThisI2CFrequency(_eepromAddr, 400000);
//}

uint8_t EEPROMClass::read(int iAddr)
{
	#if defined (ZPSIM)
		return myEEProm[iAddr];
	#else
		uint8_t result;
		readEP(iAddr,1,&result);
		return result;
	#endif
}

uint8_t EEPROMClass::write(int iAddr, uint8_t iVal)
{
	#if defined (ZPSIM)
		myEEProm[iAddr] = iVal;
		return 0;
	#else
	  return writeEP(iAddr, iVal);;
	#endif
}

uint8_t EEPROMClass::update(int iAddr, uint8_t iVal) {
	if (iVal != read(iAddr)) return write(iAddr,iVal);
	else return ++iAddr;
}

#endif