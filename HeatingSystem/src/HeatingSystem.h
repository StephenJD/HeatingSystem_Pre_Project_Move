#pragma once
#include "Assembly\HeatingSystemEnums.h"
#include "Assembly\TestDevices.h"
#include "Assembly\Initialiser.h"
#include "Assembly\MainConsolePageGenerator.h"

#include "Client_DataStructures\Data_TempSensor.h"
#include "Client_DataStructures\Data_MixValveControl.h"

#include "HardwareInterfaces\Relay.h"
#include "HardwareInterfaces\RemoteDisplay.h"
#include "HardwareInterfaces\I2C_Comms.h"
#include "HardwareInterfaces\Logging.h"
#include "HardwareInterfaces\LocalDisplay.h"
#include "HardwareInterfaces\LocalKeypad.h"
#include "HardwareInterfaces\Console.h"

#include <I2C_Helper.h>
#include <RDB.h>


//////////////////////////////////////////////////////////
//    Single Responsibility is to connect the parts     //
//////////////////////////////////////////////////////////
//namespace HeatingSystemN {
	class HeatingSystem {
	public:

		HeatingSystem();
		void serviceMainConsole();

		// Public Data Members
		HardwareInterfaces::LocalDisplay mainDisplay;
		HardwareInterfaces::LocalKeypad localKeypad;
		RelationalDatabase::RDB<Assembly::TB_NoOfTables> db;
		HardwareInterfaces::RelaysPort relaysPort;
		HardwareInterfaces::MixValveController mixValveController;
		I2C_Helper_Auto_Speed<27> i2C;

		HardwareInterfaces::I2C_Temp_Sensor tempSensorArr[Assembly::NO_OF_TEMP_SENSORS]; // Array of TempSensor provided by client
		HardwareInterfaces::Relay relayArr[Assembly::NO_OF_RELAYS]; // Array of Relay provided by client
		HardwareInterfaces::RemoteDisplay remDispl[Assembly::NO_OF_REMOTE_DISPLAYS];
	private:
		HardwareInterfaces::Null_Logger * _log;
		Assembly::Initialiser _initialiser;
		Assembly::MainConsolePageGenerator _mainPages;
		HardwareInterfaces::Console _mainConsole;

	};
//}
//using HeatingSystemN::HeatingSystem;