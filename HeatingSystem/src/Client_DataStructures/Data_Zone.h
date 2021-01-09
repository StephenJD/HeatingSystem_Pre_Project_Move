#pragma once
#include "..\LCD_UI\I_Record_Interface.h"
#include "..\LCD_UI\UI_Primitives.h"
#include "..\LCD_UI\I_Data_Formatter.h"
#include <RDB.h>

namespace HardwareInterfaces {
	class Zone;
}

namespace client_data_structures {
	using namespace LCD_UI;

	//***************************************************
	//              Zone RDB Tables
	//***************************************************

	struct R_Zone {
		char name[7];
		char abbrev[4];
		RecordID callTempSens;
		RecordID callRelay;
		RecordID mixValve;
		uint8_t maxFlowTemp;
		int8_t offsetT;
		uint8_t autoRatio; // heat-in/heat-out : 20 X (flowT-RoomT)/(RoomT-OS_T) 
		uint8_t autoTimeC; 
		uint8_t autoQuality; // Longest Averaging Period(mins/8). If == 0, use manual.
		uint8_t autoDelay;
		bool operator < (R_Zone rhs) const { return false; }
		bool operator == (R_Zone rhs) const { return true; }
	};

	inline Logger & operator << (Logger & stream, const R_Zone & zone) {
		return stream << F("Zone: ") << zone.name;
	}

	struct R_DwellingZone {
		RecordID dwellingID;
		RecordID zoneID;
		RecordID field(int fieldIndex) {
			if (fieldIndex == 0) return dwellingID; else return zoneID;
		}
		bool operator < (R_DwellingZone rhs) const { return false; }
		bool operator == (R_DwellingZone rhs) const { return true; }
	};

	inline Logger & operator << (Logger & stream, const R_DwellingZone & dwellingZone) {
		return stream << F("DwellingZone DwID: ") << (int)dwellingZone.dwellingID << F(" ZnID: ") << (int)dwellingZone.zoneID;
	}

	//***************************************************
	//              RecInt_Zone
	//***************************************************

	/// <summary>
	/// DB Interface to all Zone Data
	/// Provides streamable fields which may be populated by a database or the runtime-data.
	/// A single object may be used to stream and edit any of the fields via getFieldAt
	/// </summary>
	class RecInt_Zone : public Record_Interface<R_Zone> {
	public:
		enum streamable { e_name, e_abbrev, e_reqTemp, e_offset, e_isTemp, e_isHeating, e_ratio, e_timeConst, e_quality, e_delay	};
		RecInt_Zone(VolatileData* runtimeData);
		VolatileData* runTimeData() override { return _runTimeData; }
		I_Data_Formatter * getField(int _fieldID) override;
		bool setNewValue(int _fieldID, const I_Data_Formatter * val) override;
		bool actionOn_LR(int _fieldID, int moveBy) override;
		bool back(int fieldID) override;

		HardwareInterfaces::Zone& zone(int index);
	private:
		VolatileData* _runTimeData;
		StrWrapper _name;
		StrWrapper _abbrev;
		IntWrapper _requestTemp;
		IntWrapper _tempOffset;
		IntWrapper _isTemp;
		StrWrapper _isHeating;
		IntWrapper _autoRatio;
		IntWrapper _autoTimeC;
		IntWrapper _autoQuality;
		IntWrapper _autoDelay;
	};
}