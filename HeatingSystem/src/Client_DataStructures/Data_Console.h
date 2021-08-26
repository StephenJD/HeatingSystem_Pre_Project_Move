#pragma once
#include "..\LCD_UI\I_Record_Interface.h"
#include "..\LCD_UI\UI_Primitives.h"
#include "..\LCD_UI\I_Data_Formatter.h"
#include <RemoteKeypad.h>

namespace client_data_structures {
	using namespace LCD_UI;
	using namespace HardwareInterfaces;

	//***************************************************
	//              Consoles RDB Tables
	//***************************************************

	struct R_Display {
		char name[5];
		uint8_t addr;
		uint8_t contrast;
		uint8_t backlight_bright;
		uint8_t backlight_dim;
		uint8_t photo_bright;
		uint8_t photo_dim;
		uint8_t timeout; // 0 == disabled keypad

		bool operator < (R_Display rhs) const { return false; }
		bool operator == (R_Display rhs) const { return true; }
	};

	inline arduino_logger::Logger& operator << (arduino_logger::Logger& stream, const R_Display& display) {
		return stream << F("Display ") << (int)display.name;
	}

//***************************************************
//              RecInt_Console
//***************************************************

	/// <summary>
	/// DB Interface to all Console Data
	/// Provides streamable fields which may be populated by a database or the runtime-data.
	/// A single object may be used to stream and edit any of the fields via getFieldAt
	/// </summary>
	class RecInt_Console : public Record_Interface<R_Display> {
	public:
		enum streamable { e_name, e_keypad_enabled	};
		RecInt_Console(HardwareInterfaces::RemoteKeypad* remoteKeypadArr);
		I_Data_Formatter* getField(int _fieldID) override;
		bool setNewValue(int _fieldID, const I_Data_Formatter* val) override;
	private:
		HardwareInterfaces::RemoteKeypad* _remoteKeypadArr;
		StrWrapper _name;
		IntWrapper _keypad_enabled;
	};
}
