#pragma once
#include "Assembly\HeatingSystemEnums.h"
#include "Assembly\TestDevices.h"
#include "Assembly\Initialiser.h"
#include "Assembly\MainConsolePageGenerator.h"
#include "Assembly\TemperatureController.h"
#include "Assembly\Database.h"

//#include "Client_DataStructures\Data_TempSensor.h"
//#include "Client_DataStructures\Data_MixValveControl.h"

#include "HardwareInterfaces\RemoteDisplay.h"
#include "HardwareInterfaces\I2C_Comms.h"
#include "HardwareInterfaces\LocalDisplay.h"
#include "HardwareInterfaces\LocalKeypad.h"
#include "HardwareInterfaces\Console.h"
#include "HardwareInterfaces\LocalDisplay.h"
#include "Assembly/Sequencer.h"
#include <I2C_Talk.h>
#include <I2C_RecoverRetest.h>
#include <RDB.h>


//////////////////////////////////////////////////////////
//    Single Responsibility is to connect the parts     //
//////////////////////////////////////////////////////////
namespace HeatingSystemSupport {
	void initialise_virtualROM();
}

	class HeatingSystem {
	public:

		HeatingSystem();
		void serviceConsoles();
		void serviceProfiles();
		void serviceTemperatureController() { _tempController.checkAndAdjust(); }

	private: // data-member ordering matters!
		I2C_Talk i2C;
		I2C_Recovery::I2C_Recover_Retest _recover;
		RelationalDatabase::RDB<Assembly::TB_NoOfTables> db;
		Assembly::Initialiser _initialiser; // Checks db
		Assembly::TemperatureController _tempController;
		Assembly::Database _hs_db;
	public:	
		// Public Data Members
		HardwareInterfaces::LocalDisplay mainDisplay;
		HardwareInterfaces::LocalKeypad localKeypad;
	private: 
		friend Assembly::Initialiser;
		friend HardwareInterfaces::TestDevices;
		// Run-time data arrays
		HardwareInterfaces::RemoteDisplay remDispl[Assembly::NO_OF_REMOTE_DISPLAYS];
		//Assembly::TemperatureController _tempController;
		Assembly::MainConsolePageGenerator _mainPages;
		Assembly::Sequencer _sequencer;
		HardwareInterfaces::Console _mainConsole;
	};
