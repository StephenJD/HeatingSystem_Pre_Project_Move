
// *********************** Tests *********************
#include <catch.hpp>

#include "HeatingSystemEnums.h"
#include "UI_Cmd.h"
#include "Client_Cmd.h"
#include "A_Top_UI.h"
#include "Data_Dwelling.h"
#include "Data_Zone.h"
#include "Data_Program.h"
#include "Data_Spell.h"
#include "Data_Profile.h"
#include "Data_TimeTemp.h"
#include "DateTimeWrapper.h"
#include "Data_CurrentTime.h"
#include "UI_FieldData.h"
#include <EEPROM\EEPROM.h>
#include <Clock\Clock.h>
#include <Conversions.h>
#include <RDB.h>
#include "LocalDisplay.h"
#include <Logging.h>
#include "UI_Primitives.h"
#include "Initialiser.h"
#include "MainConsoleChapters.h"
#include "FactoryDefaults.h"
#include "..\Client_DataStructures\Data_MixValveControl.h"
#include "..\Client_DataStructures\Data_Relay.h"
#include "TempSensor.h"
#include "HeatingSystem.h"
#include <iostream>
#include <iomanip>

#define DATABASE
#define UI_DB_DISPLAY_VIEW_ONE
#define UI_DB_DISPLAY_VIEW_ALL
#define UI_DB_SHORT_LISTS
#define EDIT_NAMES_NUMS
#define BACK_TRACKING
////////#define EDIT_INTS
////
////////#define EDIT_FORMATTED_INTS
////
#define EDIT_DECIMAL
#define EDIT_DATES
//*#define EDIT_CURRENT_DATETIME
//#define EDIT_RUN
//
//#define VIEW_ONE_NESTED_CALENDAR_PAGE
//#define VIEW_ONE_NESTED_PROFILE_PAGE
//#define CONTRAST
//#define TIME_TEMP_EDIT
//#define MAIN_CONSOLE_PAGES
//#define INFO_CONSOLE_PAGES

//#define TEST_RELAYS
//#define CMD_MENU

using namespace LCD_UI;
using namespace RelationalDatabase;
using namespace HardwareInterfaces;
using namespace GP_LIB;
using namespace Date_Time;
using namespace std;

uint8_t BRIGHNESS_PWM = 5;
uint8_t CONTRAST_PWM = 6;
unsigned char PHOTO_ANALOGUE = A1;
uint8_t LOCAL_INT_PIN = 18;

//using namespace OiginalQueries;

HeatingSystem * heating_system = 0;

class ServiceConsoles {
public:
	void setHS(HeatingSystem & hs) {
		logger() << L_time << "Start setHS for ServiceConsoles...\n";
		heating_system = &hs;
		heating_system->serviceConsoles();
		heating_system->notifyDataIsEdited();
		heating_system->serviceProfiles();
		heating_system->serviceTemperatureController();
		logger() << L_time << "Done setHS for ServiceConsoles...\n";
	}
	void operator()() {
		if (heating_system) heating_system->serviceConsoles();
	}
private:
} serviceConsoles;

void ui_yield() {
	static bool inYield = false;
	if (inYield) {
		//logger() << "\n*******  Recursive ui_yield call *******\n\n";
		return;
	}
	inYield = true;
	serviceConsoles();
	inYield = false;
}

namespace LCD_UI {
	void notifyDataIsEdited() { // global function for notifying anyone who needs to know
		// Checks Zone Temps, then sets each zone.nextEvent to now.
		if (heating_system) heating_system->notifyDataIsEdited();
	}
}

std::string	test_stream(UI_DisplayBuffer & buffer) {
	std::string result = buffer.toCStr();
	if (buffer.cursorPos() >= (int)result.length()) {
		cout << "\nERROR in test_stream : cursorPos beyond last field\n\n";
		return result + " ** ERROR : cursorPos beyond last field **";
	}
	switch (buffer.cursorMode()) {
	case LCD_Display::e_selected:
		result.insert(buffer.cursorPos(), "_");
		break;
	case LCD_Display::e_inEdit:
		result.insert(buffer.cursorPos(), "#");
		break;
	default:
		break;
	}
	return result;
}

namespace HeatingSystemSupport {
	int writer(int address, const void * data, int noOfBytes);
	//{
	//	const unsigned char * byteData = static_cast<const unsigned char *>(data);
	//	for (noOfBytes += address; address < noOfBytes; ++byteData, ++address) {
	//		eeprom().update(address, *byteData);
	//	}
	//	return address;
	//}

	int reader(int address, void * result, int noOfBytes);
	//{
	//	uint8_t * byteData = static_cast<uint8_t *>(result);
	//	for (noOfBytes += address; address < noOfBytes; ++byteData, ++address) {
	//		*byteData = eeprom().read(address);
	//	}
	//	return address;
	//}
}
using namespace HeatingSystemSupport;

LocalLCD_Pinset localLCDpins() {
	if (analogRead(0) < 10) {
		return{ 26, 46, 44, 42, 40, 38, 36, 34, 32, 30, 28 }; // old board
	}
	else {
		return{ 46, 44, 42, 40, 38, 36, 34, 32, 30, 28, 26 }; // new board
	}
}

Logger & logger() {
	static Serial_Logger _log(9600, clock_());
	return _log;
}

Clock & clock_() {
	static Clock_EEPROM _clock(EEPROM_CLOCK_ADDR);
	return _clock;
}

I2C_Talk i2C;
I2C_Recovery::I2C_Recover_Retest recover{ i2C };

I2C_Talk rtc{ Wire1, 100000 };

EEPROMClass & eeprom() {
	static EEPROMClass_T<rtc> _eeprom_obj{ 0x50 };
	return _eeprom_obj;
}

EEPROMClass & EEPROM = eeprom();

namespace HardwareInterfaces {
	Bitwise_RelayController & relayController() {
		static RelaysPort _relaysPort(0x7F, heating_system->recoverObject(), IO8_PORT_OptCoupl);
		return _relaysPort;
	}
}

#ifdef DATABASE
//bool debugStop;
SCENARIO("Create a Database", "[Database]") {
	using namespace client_data_structures;
	using namespace Assembly;

	constexpr R_ThermalStore thermalStore_f[] = {
		{R_Gas
		,T_GasF
		,T_TkTop
		,T_TkUs
		,T_TkDs
		,T_CWin
		,T_Pdhw
		,T_DHW
		,T_OS
		,85,21,75,25,60,180,169,138,97,25}
	};

	constexpr R_MixValveControl mixValveControl_f[] = {
	{"DnS", T_DUfh, T_TkDs}
	,{"UpS", T_UUfh, T_TkUs}
	};

	constexpr R_Display displays_f[] = {
	{"Main", 0, 16, 20, 16, 250, 70, 30}
	,{"DS", 0x26, 0, 0, 0, 0, 0, 0}
	,{"US", 0x24, 0, 0, 0, 0, 0, 0}
	,{"Flat", 0x25, 0, 0, 0, 0, 0, 0}
	};

	constexpr R_Relay relays_f[] = {
	{ "Flat",6 << 1 }
	,{ "FlTR",1 << 1 }
	,{ "HsTR",0 << 1 }
	,{ "UpSt",5 << 1 }
	,{ "MFSt",2 << 1 }
	,{ "Gas",3 << 1 }
	,{ "DnSt",4 << 1 }
	};

	constexpr R_TempSensor tempSensors_f[] = {
		{ "Flat",0x70 },
		{ "FlTR",0x72 },
		{ "HsTR",0x71 },
		{ "EnST",0x76 },
		{ "UpSt",0x36 },
		{ "DnSt",0x74 },
		{ "OutS",0x2B }, // { "OutS",0x2B }, // 29
		{ "Grnd",0x35 },

		{ "Pdhw",0x37 },
		{ "DHot",0x28 },
		{ "US-F",0x2C },
		{ "DS-F",0x4F },
		{ "TkMf",0x75 },
		{ "TkDs",0x77 },
		{ "TkUs",0x2E },
		{ "TkTp",0x2D },

		{ "GasF",0x4B },
		{ "MFBF",0x2F }
	};

	constexpr R_TowelRail towelRails_f[] = {
		  { "EnSuite", T_ETrS, R_HsTR, M_UpStrs, 55, 160 }
		, { "Family", T_HTrS, R_HsTR, M_UpStrs, 45, 60 }
		, { "Flat", T_FTrS, R_FlTR, M_UpStrs, 40, 30 }
	};

	RDB<TB_NoOfTables> db(RDB_START_ADDR, EEPROM_SIZE, writer, reader, VERSION);

	db.createTable<>(thermalStore_f);
	db.createTable(mixValveControl_f);
	db.createTable(displays_f);
	db.createTable(relays_f);
	db.createTable(tempSensors_f);
	db.createTable(towelRails_f);
	auto tq_dwelling = db.createTable<R_Dwelling>(2);
	auto tq_zone = db.createTable<R_Zone>(4);
	auto tq_dwellingZone = db.createTable<R_DwellingZone>(6);
	auto tq_program = db.createTable<R_Program>(5);
	auto tq_profile = db.createTable<R_Profile>(36);
	auto tq_timeTemp = db.createTable<R_TimeTemp>(8, i_09_orderedInsert);
	auto tq_spell = db.createTable<R_Spell>(8, i_09_orderedInsert);

	tq_dwelling.insert(R_Dwelling{ "House" });
	tq_dwelling.insert(R_Dwelling{ "HolAppt" });

	R_TimeTemp timeTemps[] = { 
	{ 0,120L },
	{ 1,121L },
	{ 2,122L },
	{ 3,123L },
	{ 4,124L },
	{ 5,125L },
	{ 6,126L },
	{ 7,127L },
	{ 8,128L },
	{ 9,129L },
	{ 10,130L },
	{ 11,131L },
	{ 12,132L },
	{ 13,133L },
	{ 0,140L },
	{ 1,141L },
	{ 2,142L },
	{ 3,143L },
	{ 4,144L },
	{ 5,145L },
	{ 6,146L },
	{ 7,147L } };

	tq_timeTemp.insert(timeTemps, sizeof(timeTemps) / sizeof(R_TimeTemp));

	R_Zone allZones[] = { 
		{ "UpStrs","US",T_UR,R_UpSt,M_UpStrs,55,0,25,12,1,60 }
		,{ "DnStrs","DS",T_DR,R_DnSt,M_DownStrs,45,0,25,12,1,60 }
		,{ "DHW","DHW",T_GasF,R_Gas,M_UpStrs,80,0,60,12,1,3 }
		,{ "Flat","Flt",T_FR,R_Flat,M_UpStrs,55,0,25,12,1,60 }
	};
	tq_zone.insert(allZones, sizeof(allZones) / sizeof(R_Zone));
	R_DwellingZone allDwellingZones[] = { { 0,0 },{ 0,1 },{ 0,2 },{ 1,3 },{ 1,2 } };
	tq_dwellingZone.insert(allDwellingZones, sizeof(allDwellingZones) / sizeof(R_DwellingZone));

	R_Program allPrograms[] = { 
		{ "At Home",0 }
		,{ "At Work",0 } 
		,{ "Away",0 }
		,{ "Occup'd",1 }
		,{ "Empty",1 }
	};
	tq_program.insert(allPrograms, sizeof(allPrograms) / sizeof(R_Program));

	R_Spell allSpells[] = { // date , program
		{ DateTime({ 31,7,17, },{ 15,20 }),0 },
		{ DateTime({ 12,9,17, },{ 7,30 }),1 },
		{ DateTime({ 3,9,17, },{ 17,30 }),2 },
		{ DateTime({ 30,6, 17, },{ 10,0 }),3 },
		{ DateTime({ 22,9,17, },{ 15,0 }),2 },
		{ DateTime({ 30,9,17, },{ 10,0 }),4 } };
	tq_spell.insert(allSpells, sizeof(allSpells) / sizeof(R_Spell)); // Cause small table to grow.
	
	R_Profile allProfiles[] = {
		{ 0,0,100 }, // At Home
		{ 0,1,101 },
		{ 0,2,102 },
		{ 2,0,103 }, // Away
		{ 2,1,104 },
		{ 2,2,105 },
		{ 1,0,106 },  // At Work - W,SS 
		{ 1,0,107 }, // MT,TF

		{ 1,1,108 },
		{ 1,2,109 },
		{ 3,3,110 }, // Occup'd
		{ 3,2,111 },
		{ 4,3,112 }, // Empty
		{ 4,2,113 } };
	tq_profile.insert(allProfiles, sizeof(allProfiles) / sizeof(R_Profile)); // capacity = 36
	CHECK(string(Answer_R<R_Program>(tq_program[4]).rec().name) == "Empty");
	CHECK(Answer_R<R_Profile>(tq_profile[13]).rec().days == 113);

	auto q_dwellingSpells = QueryLF_T<R_Spell, R_Program>{ db.tableQuery(TB_Spell), db.tableQuery(TB_Program), 1, 1 };

	for (Answer_R<R_Dwelling> dwellng : db.tableQuery(TB_Dwelling)) {
		cout << dwellng.rec().name << endl;
	}

	for (Answer_R<R_Spell> spell : q_dwellingSpells) {
		cout << dec << spell.rec().date.day() << "/" << spell.rec().date.getMonthStr() << endl;
	}

}
#endif

#ifdef UI_DB_DISPLAY_VIEW_ONE
SCENARIO("Simple Page Scrolling", "[Chapter]") {
	// On each run through a TEST_CASE, Catch2 executes one SECTION/WHEN/THEN and skips the others.Next time, it executes the second section, and so on.
	// Sections may be nested. A section with no inner section is a LEAF and is executed just once.
	// To cascade tests (one follows the previous) they must be nested in the previous test.
	using namespace client_data_structures;
	using namespace Assembly;

	LCD_Display_Buffer<20, 4> lcd;
	UI_DisplayBuffer tb(lcd);

	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);
	if (!db.checkPW(VERSION)) { cout << "Password missmatch\n"; return; }
	//cout << "\tand some Queries are created" << endl;
	auto q_dwellings = db.tableQuery(TB_Dwelling);
	auto q_dwellingZones = QueryFL_T<R_DwellingZone>{ db.tableQuery(TB_DwellingZone), db.tableQuery(TB_Zone), 0, 1 };
	auto q_dwellingProgs = QueryF_T<R_Program>{ db.tableQuery(TB_Program) , 1 };

	//cout << " **** Next create DB Record Interface ****\n";
	auto rec_dwelling = Dataset_Dwelling(q_dwellings, noVolData, 0);
	auto rec_dwZone = Dataset_Zone(q_dwellingZones, noVolData, &rec_dwelling);
	auto rec_dwProgs = Dataset_Program(q_dwellingProgs, noVolData, &rec_dwelling);

	//cout << "\n **** Next create DB UIs ****\n";
	auto dwellNameUI_c = UI_FieldData(&rec_dwelling, Dataset_Dwelling::e_name, {V+S+V1+UD_A+R});
	auto zoneNameUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_name, {V+S+V1+UD_A+R});
	auto progNameUI_c = UI_FieldData(&rec_dwProgs, Dataset_Program::e_name); // non-recyclable

	// UI Elements
	UI_Label L1("L1");
	UI_Cmd C3("C3", 0, Behaviour{ V + S + L + V1 + UD_A});

	// UI Element Arays / Collections
	ui_Objects()[(long)&dwellNameUI_c] = "dwellNameUI_c";
	ui_Objects()[(long)&zoneNameUI_c] = "zoneNameUI_c";
	ui_Objects()[(long)&progNameUI_c] = "progNameUI_c";
	ui_Objects()[(long)&C3] = "C3";
	ui_Objects()[(long)&L1] = "L1";

	auto page1_c = makeCollection(L1, dwellNameUI_c, zoneNameUI_c, progNameUI_c, C3);
	auto display1_c = makeChapter(page1_c);
	auto display1_h = A_Top_UI(display1_c);

	ui_Objects()[(long)&display1_c] = "display1_c";
	ui_Objects()[(long)&page1_c] = "page1_c";

	GIVEN("There is only one Page") {
		REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs   At Home             C3");
		WHEN("Scrolling Unselected Page Up/Down Nothing changes") {
			display1_h.rec_up_down(1); // up-down no other pages, so stay put
			REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs   At Home             C3");
			display1_h.rec_up_down(-1); // up-down no other pages, so stay put
			REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs   At Home             C3");
			THEN("Selecting the page gets the focus") {
				display1_h.rec_select();
				REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs   At Home             C3");
				AND_THEN("Back-Key looses focus") {
					display1_h.rec_prevUI();
					REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs   At Home             C3");
				}
			}
		}

		WHEN("Right-Key from unselected then page gets focus") {
			display1_h.rec_left_right(1);
			REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs   At Home             C3");
			THEN("Right-Key from selected focus moves to next field") {
				cout << test_stream(display1_h.stream(tb)) << endl;
				display1_h.rec_left_right(1);
				REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s   At Home             C3");
				AND_THEN("focus recycles from the end") {
					display1_h.rec_left_right(1);
					REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs   At Hom_e             C3");
					display1_h.rec_left_right(1);
					REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs   At Home             C_3");
					display1_h.rec_left_right(1);
					REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs   At Home             C3");
				}
			}
		}

		WHEN("Left-Key from selected focus recycles to the end") {
			display1_h.rec_select();
			display1_h.rec_left_right(-1);
			REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs   At Home             C_3");
			THEN("focus moves towards the beginning") {
				display1_h.rec_left_right(-1);
				REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs   At Hom_e             C3");
				display1_h.rec_left_right(-1);
				REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s   At Home             C3");
				display1_h.rec_left_right(-1);
				REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs   At Home             C3");
			}
		}

		WHEN("Down-Key on first selected") {
			display1_h.rec_select();
			THEN("Next Member shown") {
				display1_h.rec_up_down(-1);
				REQUIRE(test_stream(display1_h.stream(tb)) == "L1 HolApp_t Flat     Occup'd             C3");
				AND_THEN("Recycles") {
					display1_h.rec_up_down(-1);
					REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs   At Home             C3");
				}
			}
		}

		WHEN("Up-Key on first selected") {
			display1_h.rec_select();
			THEN("Next Member shown") {
				display1_h.rec_up_down(1);
				REQUIRE(test_stream(display1_h.stream(tb)) == "L1 HolApp_t Flat     Occup'd             C3");
				AND_THEN("Recycles") {
					display1_h.rec_up_down(1);
					REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs   At Home             C3");
				}
			}
		}

		WHEN("Down-Key on second selected") {
			display1_h.rec_select();
			display1_h.rec_left_right(1);
			THEN("Next Member shown") {
				display1_h.rec_up_down(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   DH_W      At Home             C3");
				AND_THEN("Next Member shown") {
					display1_h.rec_up_down(-1);
					CHECK(test_stream(display1_h.stream(tb)) == "L1 House   DnStr_s   At Home             C3");
					AND_THEN("Recycles") {
						display1_h.rec_up_down(-1);
						CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s   At Home             C3");
					}
				}
			}
		}

		WHEN("Up-Key on second selected") {
			display1_h.rec_select();
			display1_h.rec_left_right(1);
			THEN("Next Member shown") {
				display1_h.rec_up_down(1);
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   DnStr_s   At Home             C3");
				AND_THEN("Next Member shown") {
					display1_h.rec_up_down(1);
					CHECK(test_stream(display1_h.stream(tb)) == "L1 House   DH_W      At Home             C3");
					AND_THEN("Recycles") {
						display1_h.rec_up_down(1);
						REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s   At Home             C3");
					}
				}
			}
		}
	}
}
#endif

#ifdef UI_DB_DISPLAY_VIEW_ALL
SCENARIO("View-All scroll and edit", "[Chapter]") {
	cout << "\n*********************************\n**** View-All with Names ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;

	LCD_Display_Buffer<80, 1> lcd;
	UI_DisplayBuffer tb(lcd);

	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	cout << "\tand some Queries are created" << endl;
	auto q_dwellings = db.tableQuery(TB_Dwelling);
	auto q_dwellingZones = QueryFL_T<R_DwellingZone>{ db.tableQuery(TB_DwellingZone), db.tableQuery(TB_Zone), 0, 1 };
	auto q_dwellingProgs = QueryF_T<R_Program>{ db.tableQuery(TB_Program) , 1 };

	cout << " **** Next create DB Record Interface ****\n";
	auto rec_dwelling = Dataset_Dwelling(q_dwellings, noVolData, 0);
	auto rec_dwZone = Dataset_Zone(q_dwellingZones, noVolData, &rec_dwelling);
	auto rec_dwProgs = Dataset_Program(q_dwellingProgs, noVolData, &rec_dwelling);

	cout << "\n **** Next create DB UIs ****\n";
	auto dwellNameUI_c = UI_FieldData(&rec_dwelling, Dataset_Dwelling::e_name);
	auto zoneNameUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_name, {V+S+VnLR+R0+ER0 });
	auto progNameUI_c = UI_FieldData(&rec_dwProgs, Dataset_Program::e_name, { V + S + VnLR + R0 + ER0 });

	// UI Elements
	UI_Label L1("L1");
	UI_Cmd C3("C3", 0);

	// UI Element Arrays / Collections
	cout << "\npage1 Collection\n";
	auto page1_c = makeCollection(L1, dwellNameUI_c, zoneNameUI_c, progNameUI_c, C3);
	cout << "\nDisplay     Collection\n";
	auto display1_c = makeChapter(page1_c);
	auto display1_h = A_Top_UI(display1_c);

	ui_Objects()[(long)&dwellNameUI_c] = "dwellNameUI_c";
	ui_Objects()[(long)&zoneNameUI_c] = "zoneNameUI_c";
	ui_Objects()[(long)&progNameUI_c] = "progNameUI_c";
	ui_Objects()[(long)&C3] = "C3";
	ui_Objects()[(long)&L1] = "L1";
	ui_Objects()[(long)&display1_c] = "display1_c";
	ui_Objects()[(long)&page1_c] = "page1_c";

	cout << "\n **** All Constructed ****\n\n";
	GIVEN("Moving Focus on ViewAll Names") {
		display1_h.rec_select();
		cout << test_stream(display1_h.stream(tb)) << endl;
		REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs DnStrs DHW    At Home At Work Away    C3");
		//                                           012345678901234567890123456789012345678901234567890123456789
		WHEN("Right, focus moves to first ViewAll name") {
			display1_h.rec_left_right(1); // moves page focus to zones
			CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s DnStrs DHW    At Home At Work Away    C3");
			THEN("Left moves back to View-one name") {
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs DnStrs DHW    At Home At Work Away    C3");
				AND_THEN("Up displays next ViewAll names") {
					display1_h.rec_up_down(1);
					CHECK(test_stream(display1_h.stream(tb)) == "L1 HolApp_t Flat   DHW    Occup'd Empty   C3");
					AND_THEN("Right moves to first ViewAll name") {
						display1_h.rec_left_right(1); // moves page focus to zones
						REQUIRE(test_stream(display1_h.stream(tb)) == "L1 HolAppt Fla_t   DHW    Occup'd Empty   C3");
					}
				}
			}
		}

		WHEN("Right on first ViewAll name, focus moves to next ViewAll name") {
			display1_h.rec_left_right(1); // moves page focus to zones
			REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s DnStrs DHW    At Home At Work Away    C3");
			display1_h.rec_left_right(1); // moves Zone focus to 1
			display1_h.stream(tb);
			CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStr_s DHW    At Home At Work Away    C3");
			display1_h.rec_left_right(1); // moves Zone focus to 2
			CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStrs DH_W    At Home At Work Away    C3");
			THEN("Right on last ViewAll name, focus moves to next ViewAll name") {
				display1_h.rec_left_right(1); // moves page focus to 3
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStrs DHW    At Hom_e At Work Away    C3");
				display1_h.rec_left_right(1); // moves prog focus to 1
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStrs DHW    At Home At Wor_k Away    C3");
				display1_h.rec_left_right(1); // moves prog focus to 1
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStrs DHW    At Home At Work Awa_y    C3");
				display1_h.rec_left_right(1); // moves page focus to 4
				REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStrs DHW    At Home At Work Away    C_3");
			}
		}

		WHEN("Left on first ViewAll name, focus moves to last ViewAll name") {
			display1_h.rec_left_right(-1);
			REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStrs DHW    At Home At Work Away    C_3");
			display1_h.rec_left_right(-1);
			CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStrs DHW    At Home At Work Awa_y    C3");
			THEN("Left moves back through the ViewAll names to the start") {
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStrs DHW    At Home At Wor_k Away    C3");
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStrs DHW    At Hom_e At Work Away    C3");
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStrs DH_W    At Home At Work Away    C3");
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStr_s DHW    At Home At Work Away    C3");
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s DnStrs DHW    At Home At Work Away    C3");
				display1_h.rec_left_right(-1);
				REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs DnStrs DHW    At Home At Work Away    C3");
			}
		}
	}
	GIVEN("View-One names Edit on SELECT") {
		display1_h.rec_select();
		REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs DnStrs DHW    At Home At Work Away    C3");
		display1_h.rec_select();
		CHECK(test_stream(display1_h.stream(tb)) == "L1 #House|| UpStrs DnStrs DHW    At Home At Work Away    C3");
		display1_h.rec_up_down(-1);
		REQUIRE(test_stream(display1_h.stream(tb)) == "L1 #Iouse|| UpStrs DnStrs DHW    At Home At Work Away    C3");
		THEN("Left does not recycle") {
			display1_h.rec_left_right(-1);
			CHECK(test_stream(display1_h.stream(tb)) == "L1 #Iouse|| UpStrs DnStrs DHW    At Home At Work Away    C3");
			THEN("Right moves to next edit position") {
				display1_h.rec_left_right(1);
				CHECK(test_stream(display1_h.stream(tb)) == "L1 I#ouse|| UpStrs DnStrs DHW    At Home At Work Away    C3");
				THEN("Up upper-cases the character") {
					display1_h.rec_up_down(-1);
					CHECK(test_stream(display1_h.stream(tb)) == "L1 I#Ouse|| UpStrs DnStrs DHW    At Home At Work Away    C3");
					THEN("Up moves to next character") {
						display1_h.rec_up_down(-1);
						CHECK(test_stream(display1_h.stream(tb)) == "L1 I#Puse|| UpStrs DnStrs DHW    At Home At Work Away    C3");
						THEN("Down lower-cases the character") {
							display1_h.rec_up_down(1);
							CHECK(test_stream(display1_h.stream(tb)) == "L1 I#puse|| UpStrs DnStrs DHW    At Home At Work Away    C3");
							THEN("Down moves the prev character") {
								display1_h.rec_up_down(1);
								CHECK(test_stream(display1_h.stream(tb)) == "L1 I#ouse|| UpStrs DnStrs DHW    At Home At Work Away    C3");
								THEN("Right moves edit pos to blank") {
									display1_h.rec_left_right(1);
									CHECK(test_stream(display1_h.stream(tb)) == "L1 Io#use|| UpStrs DnStrs DHW    At Home At Work Away    C3");
									display1_h.rec_left_right(1);
									CHECK(test_stream(display1_h.stream(tb)) == "L1 Iou#se|| UpStrs DnStrs DHW    At Home At Work Away    C3");
									display1_h.rec_left_right(1);
									CHECK(test_stream(display1_h.stream(tb)) == "L1 Ious#e|| UpStrs DnStrs DHW    At Home At Work Away    C3");
									display1_h.rec_left_right(1);
									CHECK(test_stream(display1_h.stream(tb)) == "L1 Iouse#|| UpStrs DnStrs DHW    At Home At Work Away    C3");
									display1_h.rec_left_right(1);
									CHECK(test_stream(display1_h.stream(tb)) == "L1 Iouse|#| UpStrs DnStrs DHW    At Home At Work Away    C3");
									THEN("Up starts at A") {
										display1_h.rec_up_down(-1);
										CHECK(test_stream(display1_h.stream(tb)) == "L1 Iouse #A UpStrs DnStrs DHW    At Home At Work Away    C3");
										AND_THEN("Back cancels edit") {
											display1_h.rec_prevUI(); // cancel edit
											REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs DnStrs DHW    At Home At Work Away    C3");
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	GIVEN("ViewAll names Edit on SELECT") {
		display1_h.rec_select();
		cout << test_stream(display1_h.stream(tb)) << endl;
		display1_h.rec_left_right(1);
		display1_h.rec_left_right(1);
		display1_h.rec_select();
		REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #DnStrs DHW    At Home At Work Away    C3");
		THEN("Left does not move") {
			display1_h.rec_left_right(-1);
			CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #DnStrs DHW    At Home At Work Away    C3");
			THEN("Right-Left moves edit pos") {
				display1_h.rec_left_right(1);
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs D#nStrs DHW    At Home At Work Away    C3");
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #DnStrs DHW    At Home At Work Away    C3");
				AND_THEN("Up changes to next upper-char") {
					display1_h.rec_up_down(-1);
					CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #EnStrs DHW    At Home At Work Away    C3");
					THEN("Save comes out of edit") {
						display1_h.rec_select();
						CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs EnStr_s DHW    At Home At Work Away    C3");
						AND_THEN("SELECT re-edits") {
							display1_h.rec_select();
							CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #EnStrs DHW    At Home At Work Away    C3");
							THEN("re-edit and save") {
								display1_h.rec_up_down(1);
								display1_h.rec_up_down(1);
								display1_h.rec_up_down(-1);
								display1_h.rec_select();
								REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs DnStr_s DHW    At Home At Work Away    C3");
							}
						}
					}
				}
			}
		}
	}
}
#endif

#ifdef UI_DB_SHORT_LISTS
SCENARIO("Multiple pages scroll with Short List Items", "[Chapter]") {
	cout << "\n*********************************\n**** Short List Names ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;

	LCD_Display_Buffer<80, 1> lcd;
	UI_DisplayBuffer tb(lcd);

	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	auto q_dwellings = db.tableQuery(TB_Dwelling);
	auto q_dwellingZones = QueryFL_T<R_DwellingZone>{ db.tableQuery(TB_DwellingZone), db.tableQuery(TB_Zone), 0, 1 };
	auto q_dwellingProgs = QueryF_T<R_Program>{ db.tableQuery(TB_Program) , 1 };
	auto q_dwellingSpells = QueryF_T<R_Spell>{ db.tableQuery(TB_Spell), 1 };
	auto q_ProgProfiles = QueryF_T<R_Profile>(db.tableQuery(TB_Profile), 0);
	auto q_ProfileTimeTemps = QueryF_T<R_TimeTemp>(db.tableQuery(TB_TimeTemp), 0);

	//cout << " **** Next create DB Record Interfaces ****\n";
	auto rec_dwelling = Dataset_Dwelling(q_dwellings, noVolData, 0);
	auto rec_dwZone = Dataset_Zone(q_dwellingZones, noVolData, &rec_dwelling);
	auto rec_dwProgs = Dataset_Program(q_dwellingProgs, noVolData, &rec_dwelling);

	// UI Elements
	UI_Label L1("L1");
	UI_Cmd C1("C1", 0), C2("C2", 0), C3("C3", 0), C4("C4", 0);
	cout << "\n **** Next create DB UIs ****\n";
	auto dwellNameUI_c = UI_FieldData(&rec_dwelling, Dataset_Dwelling::e_name);
	auto zoneNameUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_name);
	auto progNameUI_c = UI_FieldData(&rec_dwProgs, Dataset_Program::e_name);
	ui_Objects()[(long)&dwellNameUI_c] = "dwellNameUI_c";
	ui_Objects()[(long)&zoneNameUI_c] = "zoneNameUI_c";
	ui_Objects()[(long)&progNameUI_c] = "progNameUI_c";

	auto label_c = makeCollection(C1, C2, C3, C4);
	ui_Objects()[(long)&label_c] = "label_c";

	//cout << "\n **** Next create Short-collection wrappers ****\n";
	auto iterated_zoneName = UI_IteratedCollection{ 19, makeCollection(zoneNameUI_c) };
	auto iterated_progName = UI_IteratedCollection{ 30, makeCollection(progNameUI_c) };
	auto iterated_label = UI_IteratedCollection{ 3, makeCollection(label_c) };
	ui_Objects()[(long)&iterated_zoneName] = "iterated_zoneName";
	ui_Objects()[(long)&iterated_progName] = "iterated_progName";
	ui_Objects()[(long)&iterated_label] = "iterated_label";

	// UI Element Arays / Collections	cout << "\npage1 Collection\n";
	auto page1_c = makeCollection(L1, dwellNameUI_c, iterated_zoneName, iterated_progName);
	auto page2_c = makeCollection(iterated_label, dwellNameUI_c, iterated_zoneName, iterated_progName);
	auto page3_c = UI_IteratedCollection(7, makeCollection(C1, C2, C3, C4));
	//cout << "\nDisplay     Collection\n";
	auto display1_c = makeChapter(page1_c, page2_c, page3_c);
	auto display1_h = A_Top_UI(display1_c);
	ui_Objects()[(long)&page1_c] = "page1_c";
	ui_Objects()[(long)&page2_c] = "page2_c";
	ui_Objects()[(long)&page3_c] = "page3_c";
	ui_Objects()[(long)&display1_c] = "display1_c";

	//cout << " **** All Constructed ****\n\n";

	GIVEN("First page shows initially") {
		REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs> At Home>");
		THEN("UP/DN changes page and recycles") {
			display1_h.rec_up_down(1); // next page
			CHECK(test_stream(display1_h.stream(tb)) == "C1> House   UpStrs> At Home>");
			display1_h.rec_up_down(1); // next page
			CHECK(test_stream(display1_h.stream(tb)) == "C1 C2>");
			display1_h.rec_up_down(1); // next page
			CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs> At Home>");
			GIVEN("First page has view-one and two iterated names") {
				display1_h.rec_select();
				CHECK(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs> At Home>");
				THEN("RIGHT moves into iterated name") {
					display1_h.rec_left_right(1); // moves focus
					CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s> At Home>");
					THEN("RIGHT shows next member of iterated name") {
						display1_h.rec_left_right(1); // moves focus
						CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DnStr_s> At Home>");
						//											 01234567890123456 78901234567890123456789
						CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DnStr_s> At Home>");
						THEN("RIGHT shows last member of iterated name") {
							display1_h.rec_left_right(1); // moves focus
							CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DH_W    At Home>");
							CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DH_W    At Home>");
							THEN("RIGHT shows first member of next iterated name") {
								display1_h.rec_left_right(1); // moves focus
								CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DHW    At Hom_e>");
								CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DHW    At Hom_e>");
								cout << endl << test_stream(display1_h.stream(tb)) << endl;
								THEN("RIGHT shows next member of iterated name") {
									display1_h.rec_left_right(1); // moves focus
									CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DHW    <At Wor_k>");
									CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DHW    <At Wor_k>");
									THEN("RIGHT shows last member of iterated name") {
										display1_h.rec_left_right(1); // moves focus
										CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DHW    <Awa_y   ");
										CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DHW    <Awa_y   ");
										THEN("RIGHT recycles to first field") {
											display1_h.rec_left_right(1); // moves focus
											CHECK(test_stream(display1_h.stream(tb)) == "L1 Hous_e   <DHW    <Away   ");
											THEN("RIGHT moves from last to first iterated name") {
												display1_h.rec_left_right(1); // moves focus
												CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s> <Away   ");
												display1_h.rec_left_right(1); // moves focus
												CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DnStr_s> <Away   ");
												display1_h.rec_left_right(1); // moves focus
												CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DH_W    <Away   ");
												display1_h.rec_left_right(1); // moves focus
												CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DHW    At Hom_e>");												
												THEN("LEFT moves through iterated members") {
													display1_h.rec_left_right(-1); // moves focus
													CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DH_W    At Home>");
													display1_h.rec_left_right(-1); // moves focus
													CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DnStr_s> At Home>");
													display1_h.rec_left_right(-1); // moves focus
													CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s> At Home>");
													display1_h.rec_left_right(-1); // moves focus
													CHECK(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs> At Home>");
													THEN("LEFT moves from first to last iterated member") {
														display1_h.rec_left_right(-1); // moves focus
														CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs> <Awa_y   ");
														display1_h.rec_left_right(-1); // moves focus
														CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs> <At Wor_k>");
														display1_h.rec_left_right(-1); // moves focus
														CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs> At Hom_e>");
														display1_h.rec_left_right(-1); // moves focus
														CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DH_W    At Home>");
														display1_h.rec_left_right(-1); // moves focus
														CHECK(test_stream(display1_h.stream(tb)) == "L1 House   <DnStr_s> At Home>");
														display1_h.rec_left_right(-1); // moves focus
														CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s> At Home>");
														THEN("UP shows next members") {
															display1_h.rec_left_right(-1); // moves focus
															display1_h.rec_up_down(1); // next dwelling
															CHECK(test_stream(display1_h.stream(tb)) == "L1 HolApp_t Flat  > Occup'd>");
															THEN("RIGHT shows first iterated name") {
																display1_h.rec_left_right(1); // moves focus
																display1_h.stream(tb);
																CHECK(test_stream(display1_h.stream(tb)) == "L1 HolAppt Fla_t  > Occup'd>");
																THEN("RIGHT shows last member of iterated name") {
																	display1_h.rec_left_right(1); // moves focus
																	REQUIRE(test_stream(display1_h.stream(tb)) == "L1 HolAppt <DH_W    Occup'd>");
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
#endif

#ifdef EDIT_NAMES_NUMS
TEST_CASE("Edit Strings", "[Chapter]") {
	cout << "\n*********************************\n**** Edit String Values ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;

	LCD_Display_Buffer<80,1> lcd;
	UI_DisplayBuffer tb(lcd);

	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	auto q_dwellings = db.tableQuery(TB_Dwelling);
	auto q_dwellingZones = QueryFL_T<R_DwellingZone>{ db.tableQuery(TB_DwellingZone), db.tableQuery(TB_Zone), 0, 1 };
	auto q_dwellingProgs = QueryF_T<R_Program>{ db.tableQuery(TB_Program) , 1 };

	auto rec_dwelling = Dataset_Dwelling(q_dwellings, noVolData, 0);
	auto rec_dwZone = Dataset_Zone(q_dwellingZones, noVolData, &rec_dwelling);
	auto rec_dwProgs = Dataset_Program(q_dwellingProgs, noVolData, &rec_dwelling);

	auto dwellNameUI_c = UI_FieldData(&rec_dwelling, Dataset_Dwelling::e_name);
	auto zoneNameUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_name, {V+S+V1+R0+UD_A+ER});
	auto progNameUI_c = UI_FieldData(&rec_dwProgs, Dataset_Program::e_name);

	UI_Label L1("L1");
	UI_Cmd C3("C3", 0);

	auto page1_c = makeCollection(L1, dwellNameUI_c, zoneNameUI_c, progNameUI_c, C3);
	auto display1_c = makeChapter(page1_c);
	auto display1_h = A_Top_UI(display1_c);

	display1_h.rec_select();
	display1_h.rec_left_right(1); // moves focus
	REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s At Home C3");
	display1_h.rec_select(); // should make Edit_Char_h the recipient, i.e. in edit mode
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   #UpStrs At Home C3");
	display1_h.rec_left_right(1); // moves focus within zoneNameUI to next char
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   U#pStrs At Home C3");
	display1_h.rec_up_down(-1); // select next permissible char at this position
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   U#PStrs At Home C3");
	display1_h.rec_up_down(-1); // select next permissible char at this position
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   U#QStrs At Home C3");
	display1_h.rec_up_down(1); // select next permissible char at this position
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   U#qStrs At Home C3");
	display1_h.rec_left_right(2); // moves focus within zoneNameUI to nect char
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS#trs At Home C3");
	display1_h.rec_up_down(-1); // T
	display1_h.rec_up_down(-1); // U
	display1_h.rec_up_down(-1); // V
	display1_h.rec_up_down(-1); // W
	display1_h.rec_up_down(-1); // X
	display1_h.rec_up_down(-1); // Y
	display1_h.rec_up_down(-1); // Z
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS#Zrs At Home C3");
	display1_h.rec_up_down(-1); // -
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS#-rs At Home C3");
	display1_h.rec_up_down(-1); // /
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS#/rs At Home C3");
	display1_h.rec_up_down(-1); // 0
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS#0rs At Home C3");
	display1_h.rec_up_down(-1); // 1
	display1_h.rec_up_down(-1); // 2
	display1_h.rec_up_down(-1); // 3
	display1_h.rec_up_down(-1); // 4
	display1_h.rec_up_down(-1); // 5
	display1_h.rec_up_down(-1); // 6
	display1_h.rec_up_down(-1); // 7
	display1_h.rec_up_down(-1); // 8
	display1_h.rec_up_down(-1); // 9
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS#9rs At Home C3");
	display1_h.rec_up_down(-1); // |
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS#||| At Home C3");
	display1_h.rec_up_down(-1); //  
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS# rs At Home C3");
	display1_h.rec_up_down(1); //  
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS#||| At Home C3");
	display1_h.rec_left_right(1); // moves focus within zoneNameUI to next char
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS|#|| At Home C3");
	display1_h.rec_up_down(-1); // select next permissible char at this position
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS #A| At Home C3");
	display1_h.rec_select(); // should save zoneNameUI edits and come out of edit
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS _A  At Home C3");
	display1_h.rec_select(); 
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   #UqS A| At Home C3");
	THEN("Move left recycles wiyhin edit") {
		display1_h.rec_left_right(-1); // moves focus within zoneNameUI to nect char
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UqS A#| At Home C3");
		display1_h.rec_left_right(-4); // moves focus within zoneNameUI to nect char
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   U#qS A| At Home C3");
		display1_h.rec_up_down(1);
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   U#pS A| At Home C3");
		display1_h.rec_left_right(2); // moves focus within zoneNameUI to nect char
		display1_h.rec_up_down(1);
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpS#||| At Home C3");
		display1_h.rec_up_down(-1);
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpS# A| At Home C3");
		display1_h.rec_up_down(-1);
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpS#AA| At Home C3");
		display1_h.rec_up_down(-1); // B
		display1_h.rec_up_down(-1); // C
		display1_h.rec_up_down(-1); // D
		display1_h.rec_up_down(-1); // E
		display1_h.rec_up_down(-1); // F
		display1_h.rec_up_down(-1); // G
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpS#GA| At Home C3");
		display1_h.rec_up_down(-1); // select prev permissible char at this position
		display1_h.rec_up_down(-1);
		display1_h.rec_up_down(-1);
		display1_h.rec_up_down(-1);
		display1_h.rec_up_down(-1);
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpS#LA| At Home C3");
		display1_h.rec_up_down(-1); // select prev permissible char at this position
		display1_h.rec_up_down(-1);
		display1_h.rec_up_down(-1);
		display1_h.rec_up_down(-1);
		display1_h.rec_up_down(-1);
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpS#QA| At Home C3");
		display1_h.rec_up_down(-1); // select prev permissible char at this position
		display1_h.rec_up_down(-1);
		display1_h.rec_up_down(-1);
		display1_h.rec_up_down(1);
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpS#tA| At Home C3");
		display1_h.rec_left_right(1); // moves focus within zoneNameUI to next char
		display1_h.rec_up_down(-1); // B 
		display1_h.rec_up_down(-1); // C
		display1_h.rec_up_down(-1); // D 
		display1_h.rec_up_down(-1); // E 
		display1_h.rec_up_down(-1); // F 
		display1_h.rec_up_down(-1); // G 
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpSt#G| At Home C3");
		display1_h.rec_up_down(-1); // H 
		display1_h.rec_up_down(-1); // I
		display1_h.rec_up_down(-1); // J 
		display1_h.rec_up_down(-1); // K 
		display1_h.rec_up_down(-1); // L 
		display1_h.rec_up_down(-1); // M 
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpSt#M| At Home C3");
		display1_h.rec_up_down(-1); // N 
		display1_h.rec_up_down(-1); // O 
		display1_h.rec_up_down(-1); // P
		display1_h.rec_up_down(-1); // Q 
		display1_h.rec_up_down(-1); // R 
		display1_h.rec_up_down(1); // r 
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpSt#r| At Home C3");
		display1_h.rec_left_right(1); // moves focus within zoneNameUI to next char
		display1_h.rec_up_down(-1); // r 
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr#r At Home C3");
		display1_h.rec_up_down(-1); // R 
		display1_h.rec_up_down(-1); // S 
		display1_h.rec_up_down(1); // s 
		CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr#s At Home C3");
		display1_h.rec_select();
		REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s At Home C3");
	}
}
#endif

#ifdef EDIT_INTS

TEST_CASE("Edit Integer Data", "[Display]") {
	cout << "\n*********************************\n**** Edit Integer Data ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;

	LCD_Display_Buffer<80,1> lcd;
	UI_DisplayBuffer tb(lcd);

	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	cout << "\tand some Queries are created" << endl;
	auto q_dwellings = db.tableQuery(TB_Dwelling);
	auto q_dwellingZones = QueryFL_T<R_DwellingZone>{ db.tableQuery(TB_DwellingZone), db.tableQuery(TB_Zone), 0, 1 };
	auto q_dwellingProgs = QueryF_T<R_Program>{ db.tableQuery(TB_Program) , 1 };

	cout << " **** Next create DB Record Interfaces ****\n";
	auto rec_dwelling = Dataset_Dwelling(q_dwellings, noVolData, 0);
	auto rec_dwZone = Dataset_Zone(q_dwellingZones, noVolData, &rec_dwelling);
	auto rec_dwProgs = Dataset_Program(q_dwellingProgs, noVolData, &rec_dwelling);

	cout << " **** Next create DB UIs ****\n";
	auto dwellNameUI_c = UI_FieldData(&rec_dwelling, Dataset_Dwelling::e_name);
	auto zoneNameUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_name);
	auto progNameUI_c = UI_FieldData(&rec_dwProgs, Dataset_Program::e_name);
	auto zoneReqTempUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_reqTemp, Behaviour{V+S+V1+UD_A+R}, editNonRecycle());
	
	// UI Elements
	UI_Label L1("L1");
	UI_Cmd C3("C3", 0);

	// UI Collections
	cout << "\npage1 Collection\n";
	auto page1_c = makeCollection(L1, dwellNameUI_c, zoneNameUI_c, zoneReqTempUI_c, progNameUI_c, C3);

	cout << "\nDisplay     Collection\n";
	auto display1_c = makeChapter(page1_c);
	auto display1_h = A_Top_UI(display1_c);

	cout << " **** All Constructed ****\n\n";
	display1_h.rec_select();
	display1_h.stream(tb);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs 90 At Home C3");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s 90 At Home C3");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 9_0 At Home C3");
	display1_h.rec_select();
	cout << "***** Have made first select into Edit *****\n";
	cout << test_stream(display1_h.stream(tb));
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 9#0 At Home C3");
	display1_h.rec_up_down(1); // decrement 1
	cout << test_stream(display1_h.stream(tb));
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 8#9 At Home C3");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #89 At Home C3");
	display1_h.rec_up_down(-1); // increment 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #99 At Home C3");
	display1_h.rec_up_down(-1); // increment 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 1#00 At Home C3");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #100 At Home C3");
	display1_h.rec_up_down(1); // decrement 100
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #10 At Home C3");
	display1_h.rec_up_down(-1); // increment 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #20 At Home C3");
	display1_h.rec_left_right(-1); // should not recycle or move to next field
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #20 At Home C3");
	display1_h.rec_up_down(-7); // increment 70
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #90 At Home C3");
	display1_h.rec_up_down(-1); // increment 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 1#00 At Home C3");
	display1_h.rec_select();
	cout << "***** Have made first exit from Edit *****\n";
	REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 10_0 At Home C3");
	cout << test_stream(display1_h.stream(tb)) << endl;
}

#endif

#ifdef EDIT_FORMATTED_INTS
TEST_CASE("Edit Formatted Integer Data", "[Display]") {
	cout << "\n*********************************\n**** Edit Formatted Integer Data ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;

	struct R_TestInts {
		//int8_t edit1short;
		//int8_t edit1shortPlus;
		//int8_t edit1Fixed3;
		//int8_t edit1Fixed3Plus;
		//int8_t editAllShort;
		//int8_t editAllShortPlus;
		//int8_t editAllFixed3;
		//int8_t editAllFixed3Plus;
	};

	class Test_Ints : public Record_Interface<R_TestInts> {
	public:
		enum streamable { e_edit1short, e_edit1shortPlus, e_edit1Fixed3, e_edit1Fixed3Plus,
			e_editAllShort, e_editAllShortPlus, e_editAllFixed3, e_editAllFixed3Plus
		};
				
		I_Data_Formatter * getField(int fieldID) override {
			switch (fieldID) {
			case e_edit1short:
				return &_edit1short;
			case e_edit1shortPlus:
				return &_edit1shortPlus;
			case e_edit1Fixed3:
				return &_edit1Fixed3;
			case e_edit1Fixed3Plus:
				return &_edit1Fixed3Plus;
			default: return 0;
			};
		}

		I_Data_Formatter * getFieldAt(int fieldID, int elementIndex) {
			return getField(fieldID);
		}

		bool setNewValue(int _fieldID, const I_Data_Formatter * val) override { return false; }
	private:
		IntWrapper _edit1short;
		IntWrapper _edit1shortPlus;
		IntWrapper _edit1Fixed3;
		IntWrapper _edit1Fixed3Plus;
		IntWrapper _editAllShort;
		IntWrapper editAllShortPlus;
		IntWrapper editAllFixed3;
		IntWrapper editAllFixed3Plus;
	} testInts;

	LCD_Display_Buffer<80, 1> lcd;
	UI_DisplayBuffer tb(lcd);

	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	cout << "\tand some Queries are created" << endl;
	auto q_dwellings = db.tableQuery(TB_Dwelling);
	auto q_dwellingZones = QueryFL_T<R_DwellingZone>{ &db.tableQuery(TB_DwellingZone), &db.tableQuery(TB_Zone), 0, 1 };
	auto q_dwellingProgs = QueryF_T<R_Program>{ &db.tableQuery(TB_Program) , 1 };

	cout << " **** Next create DB Record Interfaces ****\n";
	auto rec_dwelling = Dataset_Dwelling(q_dwellings, noVolData, 0);
	auto rec_dwZone = Dataset_Zone(q_dwellingZones, noVolData, &rec_dwelling);
	auto rec_dwProgs = Dataset_Program(q_dwellingProgs, noVolData, &rec_dwelling);

	cout << " **** Next create DB UIs ****\n";
	auto dwellNameUI_c = UI_FieldData(&rec_dwelling, Dataset_Dwelling::e_name);
	auto zoneNameUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_name);
	auto progNameUI_c = UI_FieldData(&rec_dwProgs, Dataset_Program::e_name);
	auto zoneReqTempUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_reqTemp);

	// UI Elements
	UI_Label L1("L1");
	UI_Cmd C3("C3", 0);

	// UI Collections
	cout << "\npage1 Collection\n";
	auto page1_c = makeCollection(L1, , , , , C3);

	cout << "\nDisplay     Collection\n";
	auto display1_c = makeChapter(page1_c);
	auto display1_h = A_Top_UI(display1_c);

	cout << " **** All Constructed ****\n\n";
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs 90 At Home C3");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s 90 At Home C3");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 9_0 At Home C3");
	display1_h.rec_select();
	cout << "***** Have made first select into Edit *****\n";
	cout << test_stream(display1_h.stream(tb));
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 9#0 At Home C3");
	display1_h.rec_up_down(1); // decrement 1
	cout << test_stream(display1_h.stream(tb));
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 8#9 At Home C3");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #89 At Home C3");
	display1_h.rec_up_down(-1); // increment 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #99 At Home C3");
	display1_h.rec_up_down(-1); // increment 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 1#00 At Home C3");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #100 At Home C3");
	display1_h.rec_up_down(1); // decrement 100
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #10 At Home C3");
	display1_h.rec_up_down(-1); // increment 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #20 At Home C3");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #20 At Home C3");
	display1_h.rec_up_down(7); // increment 70
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs #90 At Home C3");
	display1_h.rec_up_down(-1); // increment 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 1#00 At Home C3");
	display1_h.rec_select();
	cout << "***** Have made first exit from Edit *****\n";
	REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs 10_0 At Home C3");
	cout << test_stream(display1_h.stream(tb)) << endl;
}

#endif
#ifdef EDIT_DECIMAL
TEST_CASE("Edit Decimal Data", "[Display]") {
	cout << "\n*********************************\n**** Edit Decimal Data ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;

	LCD_Display_Buffer<80,1> lcd;
	UI_DisplayBuffer tb(lcd);

	//enum tableIndex { TB_Dwelling, TB_Program, TB_DwellingZone, TB_TimeTemp, TB_Spell, TB_Profile, TB_Zone, TB_NoOfTables };
	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	cout << "\tand some Queries are created" << endl;
	auto q_dwellings = db.tableQuery(TB_Dwelling);
	auto q_dwellingZones = QueryFL_T<R_DwellingZone>{ db.tableQuery(TB_DwellingZone), db.tableQuery(TB_Zone), 0, 1 };
	auto q_dwellingProgs = QueryF_T<R_Program>{ db.tableQuery(TB_Program) , 1 };

	cout << " **** Next create DB Record Interfaces ****\n";
	auto rec_dwelling = Dataset_Dwelling(q_dwellings, noVolData, 0);
	auto rec_dwZone = Dataset_Zone(q_dwellingZones, noVolData, &rec_dwelling);
	auto rec_dwProgs = Dataset_Program(q_dwellingProgs, noVolData, &rec_dwelling);

	cout << " **** Next create DB UIs ****\n";
	auto dwellNameUI_c = UI_FieldData(&rec_dwelling, Dataset_Dwelling::e_name);
	auto zoneNameUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_name);
	auto progNameUI_c = UI_FieldData(&rec_dwProgs, Dataset_Program::e_name);
	auto zoneFactorUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_factor);
	
	ui_Objects()[(long)&dwellNameUI_c] = "dwellNameUI_c";
	ui_Objects()[(long)&zoneNameUI_c] = "zoneNameUI_c";
	ui_Objects()[(long)&progNameUI_c] = "progNameUI_c";
	ui_Objects()[(long)&zoneFactorUI_c] = "zoneFactorUI_c";

	// UI Elements
	UI_Label L1("L1");
	UI_Cmd C3("C3", 0);
	ui_Objects()[(long)&L1] = "L1";
	ui_Objects()[(long)&C3] = "C3";

	// UI Collections
	cout << "\npage1 Collection\n";
	auto page1_c = makeCollection(L1, dwellNameUI_c, zoneNameUI_c, zoneFactorUI_c, progNameUI_c, C3);

	cout << "\nDisplay     Collection\n";
	auto display1_c = makeChapter(page1_c);
	auto display1_h = A_Top_UI(display1_c);

	ui_Objects()[(long)&page1_c] = "page1_c";
	ui_Objects()[(long)&display1_c] = "display1_c";

	cout << " **** All Constructed ****\n\n";
	display1_h.rec_select();
	cout << test_stream(display1_h.stream(tb));
	REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs  +1.2 At Home C3");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStr_s  +1.2 At Home C3");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs  +1._2 At Home C3");
	display1_h.rec_select();
	cout << "***** Have made first select into Edit *****\n";
	cout << test_stream(display1_h.stream(tb));
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs  +1.#2 At Home C3");
	display1_h.rec_up_down(-1); // increment 1
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs  +1.#3 At Home C3");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs  +#1.3 At Home C3");
	display1_h.rec_up_down(1); // decrement 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs  +#0.3 At Home C3");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs  #+0.3 At Home C3");
	display1_h.rec_up_down(-1); // decrement 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs +#10.0 At Home C3");
	display1_h.rec_up_down(1); // decrement 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs  #+0.0 At Home C3");
	display1_h.rec_up_down(1); // decrement 10
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -#10.0 At Home C3");
	display1_h.rec_select();
	cout << "***** Have made first exit from Edit *****\n";
	REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10._0 At Home C3");
	cout << test_stream(display1_h.stream(tb));
}
#endif
///////////////////////////////////////////////////////////////////////////////////////
#ifdef EDIT_DATES
TEST_CASE("Edit Date Data", "[Chapter]") {
	cout << "\n*********************************\n**** Edit Date Data ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;

	clock_().setTime({ 31,8,17 }, { 15,20 }, 0);

	LCD_Display_Buffer<80,1> lcd;
	UI_DisplayBuffer tb(lcd);

	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	cout << "\tand some Queries are created" << endl;
	auto q_dwellings = db.tableQuery(TB_Dwelling);
	auto q_dwellingZones = QueryFL_T<R_DwellingZone>{ db.tableQuery(TB_DwellingZone), db.tableQuery(TB_Zone), 0, 1 };
	auto q_dwellingProgs = QueryF_T<R_Program>{ db.tableQuery(TB_Program) , 1 };
	auto q_dwellingSpells = QueryF_T<R_Spell>{ db.tableQuery(TB_Spell), 1 };

	cout << " **** Next create DB Record Interfaces ****\n";
	auto rec_dwelling = Dataset_Dwelling(q_dwellings, noVolData, 0);
	auto rec_dwZone = Dataset_Zone(q_dwellingZones, noVolData, &rec_dwelling);
	auto rec_dwProgs = Dataset_Program(q_dwellingProgs, noVolData, &rec_dwelling);
	auto rec_dwSpells = Dataset_Spell(q_dwellingSpells, noVolData, &rec_dwelling);

	cout << " **** Next create DB UIs ****\n";
	auto dwellNameUI_c = UI_FieldData(&rec_dwelling, Dataset_Dwelling::e_name);
	auto zoneNameUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_name);
	auto progNameUI_c = UI_FieldData(&rec_dwProgs, Dataset_Program::e_name);
	auto zoneFactorUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_factor);
	auto dwellSpellUI_c = UI_FieldData(&rec_dwSpells, Dataset_Spell::e_date);

	// UI Elements
	UI_Label L1("L1");

	// UI Collections
	cout << "\npage1 Collection\n";
	auto page1_c = makeCollection(L1, dwellNameUI_c, zoneNameUI_c, zoneFactorUI_c, progNameUI_c, dwellSpellUI_c);

	cout << "\nDisplay     Collection\n";
	auto display1_c = makeChapter(page1_c);
	auto display1_h = A_Top_UI(display1_c);

	cout << "Size of UI_FieldData is " << dec << sizeof(dwellSpellUI_c) << endl;
	cout << "Size of Field_StreamingTool_h is " << dec << sizeof(Field_StreamingTool_h) << endl;
	cout << "Size of LazyCollection is " << dec << sizeof(LazyCollection) << endl;
	cout << "Size of I_SafeCollection is " << dec << sizeof(I_SafeCollection) << endl;
	cout << "Size of Collection_Hndl is " << dec << sizeof(Collection_Hndl) << endl;
	cout << "Size of UI_Object is " << dec << sizeof(UI_Object) << endl;
	cout << "Size of int is " << dec << sizeof(int) << endl;

	cout << " **** All Constructed ****\n\n";
	display1_h.rec_select();
	display1_h.stream(tb);
	REQUIRE(test_stream(display1_h.stream(tb)) == "L1 Hous_e   UpStrs -10.0 At Home Now");
	display1_h.rec_left_right(1); // moves focus
	display1_h.rec_left_right(1); // moves focus
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Hom_e Now");
	display1_h.rec_left_right(1); // moves focus
	display1_h.stream(tb);
	cout << test_stream(display1_h.stream(tb));
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home No_w");
	clock_().setTime({ 31,7,17 }, { 8,10 }, 0);

	display1_h.rec_select();
	cout << "***** Have made first select into Edit *****\n";
	cout << test_stream(display1_h.stream(tb));
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 03:20pm #Today");
	display1_h.rec_up_down(-1); // increment 1 day
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 03:20pm #Tomor'w");
	display1_h.rec_up_down(-1); // increment 1 day
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 03:20pm 0#2Aug");
	display1_h.rec_up_down(-1); // increment 1 day
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 03:20pm 0#3Aug");
	display1_h.rec_up_down(1); // increment 1 day
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 03:20pm 0#2Aug");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 03:20pm 02#Aug");
	display1_h.rec_up_down(-1); // increment 1 month
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 03:20pm 02#Sep");
	display1_h.rec_up_down(1); // decrement 1 month
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 03:20pm 02#Aug");
	display1_h.rec_up_down(1); // decrement 1 month
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home #Now");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home No_w"); // 31st July
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home #Now");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home #Now");
	display1_h.rec_up_down(-1); // increment 1 day	
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am #Tomor'w"); // 1st Aug
	display1_h.rec_left_right(1); // should not result in incrementing month
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am #Tomor'w");
	display1_h.rec_up_down(-1); // increment 1 day
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am 0#2Aug");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am 02#Aug");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am 02#Sep");
	display1_h.rec_left_right(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am 0#2Sep");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am 0#1Sep");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am 3#1Aug");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am 31#Aug");
	display1_h.rec_up_down(-1); // increment 1 month
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am 31#Sep");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am 31#Aug");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am 3#1Aug");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am #31Aug");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10#am 31Aug");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:#10am 31Aug");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 0#8:10am 31Aug");
	display1_h.rec_up_down(-2); // increment 2 hrs
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 1#0:10am 31Aug");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 10:#10am 31Aug");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 10:10#am 31Aug");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 10:10am #31Aug");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 10:10am 3#1Aug");
	display1_h.rec_up_down(-2); // increment 2 days
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 10:10am 0#2Sep");
	display1_h.rec_left_right(1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 10:10am 02#Sep");
	display1_h.rec_up_down(1); // decrement 1 month
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 10:10am 02#Aug");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 10:10am 0#2Aug");
	display1_h.rec_up_down(2); // decrement 2 days
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 10:10am #Today");
	display1_h.rec_up_down(1); // decrement 1 days
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home #Now");
	display1_h.rec_up_down(-1); // increment 1 days
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am #Tomor'w");
	display1_h.rec_select();
	cout << "***** Have made save from Edit *****\n";
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am Tomor'_w");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10am #Tomor'w");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10#am Tomor'w");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:10#pm Tomor'w");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:#10pm Tomor'w");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 08:#20pm Tomor'w");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 0#8:20pm Tomor'w");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 0#9:20pm Tomor'w");
	display1_h.rec_left_right(-1); // moves focus
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 0#9:20pm Tomor'w");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 1#0:20pm Tomor'w");
	display1_h.rec_select();
	REQUIRE(test_stream(display1_h.stream(tb)) == "L1 House   UpStrs -10.0 At Home 10:20pm Tomor'_w");
	cout << test_stream(display1_h.stream(tb));
}
#endif

#ifdef EDIT_CURRENT_DATETIME
SCENARIO("Edit on UP/DOWN", "[Chapter]") {
	cout << "\n*********************************\n**** Edit CurrentDateTime ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;

	LCD_Display_Buffer<20, 4> lcd;
	UI_DisplayBuffer tb(lcd);
	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	auto rec_currTime = Dataset_WithoutQuery();

	auto currTimeUI_c = UI_FieldData(&rec_currTime, Dataset_WithoutQuery::e_currTime, { V + S + V1 + UD_E + R0 });
	auto currDateUI_c = UI_FieldData(&rec_currTime, Dataset_WithoutQuery::e_currDate, { V + S + V1 + UD_E + R0 });
	auto dstUI_c = UI_FieldData(&rec_currTime, Dataset_WithoutQuery::e_dst, {V+S+V1+UD_E+R0});
	// UI Elements
	UI_Label L1("DST Hours:");
	
	// UI Collections
	auto page1_c = makeCollection(currTimeUI_c, currDateUI_c, L1, dstUI_c);
	auto display1_c = makeChapter(page1_c);
	auto display1_h = A_Top_UI(display1_c);

	ui_Objects()[(long)&currTimeUI_c] = "currTimeUI_c";
	ui_Objects()[(long)&currDateUI_c] = "currDateUI_c";
	ui_Objects()[(long)&dstUI_c] = "dstUI_c";
	ui_Objects()[(long)&L1] = "L1";
	ui_Objects()[(long)&page1_c] = "page1_c";
	ui_Objects()[(long)&display1_c] = "display1_c";

	clock_().setTime({ 31,7,17 }, { 8,10 }, 5);
	GIVEN("Custom UI_Wrapper with Edit-on-up-down") {
		display1_h.rec_select();
		CHECK(test_stream(display1_h.stream(tb)) == "08:15:00a_m          Mon 31/Jul/2017     DST Hours: 1");
		display1_h.rec_left_right(1); // moves focus
		CHECK(test_stream(display1_h.stream(tb)) == "08:15:00am          Mon 31/Jul/201_7     DST Hours: 1");
		THEN("On up-down we start edit") {
			clock_().setTime({ 31,7,17 }, { 8,10 }, 5);
			clock_().refresh();
			display1_h.rec_up_down(-1);
			CHECK(test_stream(display1_h.stream(tb)) == "08:15:00am          Tue 31/Jul/201#8     DST Hours: 1");
			AND_THEN("On Select we save") {
				display1_h.rec_select();
				CHECK(test_stream(display1_h.stream(tb)) == "08:15:00am          Tue 31/Jul/201_8     DST Hours: 1");
				THEN("Also edits on SELECT") {
					display1_h.rec_left_right(1); // moves focus
					CHECK(test_stream(display1_h.stream(tb)) == "08:15:00am          Tue 31/Jul/2018     DST Hours: _1");
					display1_h.rec_select();
					CHECK(test_stream(display1_h.stream(tb)) == "08:15:00am          Tue 31/Jul/2018     DST Hours: #1");
					display1_h.rec_up_down(1);
					CHECK(test_stream(display1_h.stream(tb)) == "08:15:00am          Tue 31/Jul/2018     DST Hours: #0");
					display1_h.rec_select();
					REQUIRE(test_stream(display1_h.stream(tb)) == "08:15:00am          Tue 31/Jul/2018     DST Hours: _0");
				}
			}
		}
	}
}
#endif

#ifdef EDIT_RUN
SCENARIO("Non-iterated View-all giving Edit on UD", "[Chapter]") {
	cout << "\n*********************************\n**** Display / Edit Run Data ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;
	using namespace HardwareInterfaces;

	LCD_Display_Buffer<20,4> lcd;
	UI_DisplayBuffer tb(lcd);

	HardwareInterfaces::UI_TempSensor callTS[] = { {recover,10,18},{recover,11,19},{recover,12,55},{recover,13,21} };
	HardwareInterfaces::UI_Bitwise_Relay relays[4];
	Zone zoneArr[] = { {callTS[0],17, relays[0]},{callTS[1],20,relays[1]},{callTS[2],45,relays[2]},{callTS[3],21,relays[3]} };

	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	auto q_zones = db.tableQuery(TB_Zone);

	auto rec_zones = Dataset_Zone(q_zones, zoneArr, 0);
	
	auto zoneReqTempUI_c = UI_FieldData(&rec_zones, Dataset_Zone::e_reqIsTemp, Behaviour{V+S+VnLR+UD_E+R0});
	// UI Collections
	auto page1_c = makeCollection(zoneReqTempUI_c);
	auto display1_c = makeChapter(page1_c);
	auto display1_h = A_Top_UI(display1_c);

	ui_Objects()[(long)&zoneReqTempUI_c] = "zoneReqTempUI_c";
	ui_Objects()[(long)&page1_c] = "page1_c";
	ui_Objects()[(long)&display1_c] = "display1_c";

	//for (auto & z : zoneArr) z.setFlowTemp(); // Can't setflowtemp and get heating requirement unless constructed a thermstore and backboiler...
	display1_h.rec_select();
	GIVEN("Self-iterating View-all can be scrolled through") {
		cout << test_stream(display1_h.stream(tb)) << endl;
		REQUIRE(test_stream(display1_h.stream(tb)) == "UpStrs Req$1_7 is:18 DnStrs Req$20 is:19 DHW    Req$45 is:55 Flat   Req$21 is:21 ");
		display1_h.rec_left_right(1); // moves focus
		CHECK(test_stream(display1_h.stream(tb)) == "UpStrs Req$17 is:18 DnStrs Req$2_0 is:19 DHW    Req$45 is:55 Flat   Req$21 is:21 ");
		display1_h.rec_left_right(1); // moves focus
		CHECK(test_stream(display1_h.stream(tb)) == "UpStrs Req$17 is:18 DnStrs Req$20 is:19 DHW    Req$4_5 is:55 Flat   Req$21 is:21 ");
		display1_h.rec_left_right(1); // moves focus
		CHECK(test_stream(display1_h.stream(tb)) == "UpStrs Req$17 is:18 DnStrs Req$20 is:19 DHW    Req$45 is:55 Flat   Req$2_1 is:21 ");
		THEN("Recycles focus") {
			display1_h.rec_left_right(1); // moves focus
			CHECK(test_stream(display1_h.stream(tb)) == "UpStrs Req$1_7 is:18 DnStrs Req$20 is:19 DHW    Req$45 is:55 Flat   Req$21 is:21 ");
			AND_THEN("Edits on UP") {
				display1_h.rec_up_down(-1); // increment 1 degree puts into edit
				CHECK(test_stream(display1_h.stream(tb)) == "UpStrs Req$1#8 is:18 DnStrs Req$20 is:19 DHW    Req$45 is:55 Flat   Req$21 is:21 ");
				THEN("Allows Edit of 10's") {
					display1_h.rec_left_right(-1); // moves focus
					CHECK(test_stream(display1_h.stream(tb)) == "UpStrs Req$#18 is:18 DnStrs Req$20 is:19 DHW    Req$45 is:55 Flat   Req$21 is:21 ");
					AND_THEN("On decrementing 10 shows minimum") {
						display1_h.rec_up_down(1); // decrement 10 degree
						CHECK(test_stream(display1_h.stream(tb)) == "UpStrs Req$#10 is:18 DnStrs Req$20 is:19 DHW    Req$45 is:55 Flat   Req$21 is:21 ");
						AND_THEN("Allows incrementing 10") {
							display1_h.rec_up_down(-1); // increment 10 degree
							CHECK(test_stream(display1_h.stream(tb)) == "UpStrs Req$#20 is:18 DnStrs Req$20 is:19 DHW    Req$45 is:55 Flat   Req$21 is:21 ");
							AND_THEN("Left saves and edits last iteration") {
								display1_h.rec_left_right(-1); // moves focus - save current and move to next field
								CHECK(test_stream(display1_h.stream(tb)) == "UpStrs Req$20 is:18 DnStrs Req$20 is:19 DHW    Req$45 is:55 Flat   Req$2#1 is:21 ");
								THEN("Will not allow more than one degree increase") {
									display1_h.rec_up_down(-1); // increment 1 degree
									CHECK(test_stream(display1_h.stream(tb)) == "UpStrs Req$20 is:18 DnStrs Req$20 is:19 DHW    Req$45 is:55 Flat   Req$2#2 is:21 ");
									display1_h.rec_left_right(-1); // moves focus
									CHECK(test_stream(display1_h.stream(tb)) == "UpStrs Req$20 is:18 DnStrs Req$20 is:19 DHW    Req$45 is:55 Flat   Req$#22 is:21 ");
									THEN("SELECT to save") {
										display1_h.rec_select();
										REQUIRE(test_stream(display1_h.stream(tb)) == "UpStrs Req$20 is:18 DnStrs Req$20 is:19 DHW    Req$45 is:55 Flat   Req$2_2 is:21 ");
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
#endif

#ifdef CMD_MENU
TEST_CASE("Cmd-Menu", "[Display]") {
	cout << "\n*********************************\n**** Cmd-Menu ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;

	LCD_Display_Buffer<80, 1> lcd;
	UI_DisplayBuffer tb(lcd);
	clock_().setTime({ 31,7,17 }, { 8,10 }, 0);

	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	cout << "\tand some Queries are created" << endl;
	auto q_dwellings = db.tableQuery(TB_Dwelling);
	auto q_dwellingZones = QueryFL_T<R_DwellingZone>{ &db.tableQuery(TB_DwellingZone), &db.tableQuery(TB_Zone), 0, 1 };
	auto q_dwellingProgs = QueryFL_T<R_Program>{ &db.tableQuery(TB_Program) , 1 };
	auto q_dwellingSpells = QueryFL_T<R_Spell>{ &db.tableQuery(TB_Spell), 1 };

	cout << " **** Next create DB Record Interface ****\n";
	auto rec_dwelling = Dataset_Dwelling(q_dwellings, noVolData, 0);
	auto rec_dwZone = Dataset_Zone(q_dwellingZones, noVolData, &rec_dwelling);
	auto rec_dwProgs = Dataset_Program(q_dwellingProgs, noVolData, &rec_dwelling);
	auto rec_dwSpells = Dataset_Spell(q_dwellingSpells, noVolData, &rec_dwelling);

	cout << "\n **** Next create DB UI LazyCollections ****\n";
	auto dwellNameUI_c = UI_FieldData(&rec_dwelling, Dataset_Dwelling::e_name);
	auto zoneNameUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_name);
	auto progNameUI_c = UI_FieldData(&rec_dwProgs, Dataset_Program::e_name);
	auto dwellSpellUI_c = UI_FieldData(&rec_dwSpells, Dataset_Spell::e_date);

	// UI Element Arays / Collections
	cout << "\npage2 hidden Elements Collection\n";
	auto page2_hidden_c = makeCollection(zoneNameUI_c, dwellSpellUI_c, progNameUI_c);
	
	// UI Elements
	LCD_UI::UI_Cmd _dwellingZoneCmd = { "Zones",0 }, _dwellingCalendarCmd = { "Calendar",0 }, _dwellingProgCmd = { "Programs",0 };
	cout << "\ndwellingCmd Collection\n";
	auto _dwellingCmd_c = UI_MenuCollection{ page2_hidden_c, makeCollection(_dwellingZoneCmd,_dwellingCalendarCmd,_dwellingProgCmd) };

	cout << "\npage1 Collection\n";
	auto page1_c = makeCollection(dwellNameUI_c, zoneNameUI_c, progNameUI_c);
	cout << "\npage2 Collection\n";
	auto page2_c = makeCollection(dwellNameUI_c, _dwellingCmd_c, page2_hidden_c);
	_dwellingCmd_c.set_cmd_h(page2_c.item(1));

	cout << "\nDisplay     Collection\n";
	auto display1_c = makeChapter(page1_c, page2_c);
	auto display1_h = A_Top_UI(display1_c);

	cout << "\n **** All Constructed ****\n\n";
	display1_h.stream(tb);
	cout << test_stream(display1_h.stream(tb)) << endl;;
	CHECK(test_stream(display1_h.stream(tb)) == "House UpStrs At Home ");
	display1_h.rec_up_down(-1); // next page
	display1_h.stream(tb);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Zones UpStrs DnStrs DHW   ");
	display1_h.rec_up_down(-1); // next page
	CHECK(test_stream(display1_h.stream(tb)) == "House UpStrs At Home ");
	display1_h.rec_up_down(-1); // next page
	CHECK(test_stream(display1_h.stream(tb)) == "House   Zones UpStrs DnStrs DHW   ");
	display1_h.rec_left_right(1); // left-right to select page
	CHECK(test_stream(display1_h.stream(tb)) == "Hous_e   Zones UpStrs DnStrs DHW   ");
	display1_h.rec_left_right(1); // right
	CHECK(test_stream(display1_h.stream(tb)) == "House   Zone_s UpStrs DnStrs DHW   ");
	display1_h.rec_up_down(-1); // next Menu
	CHECK(test_stream(display1_h.stream(tb)) == "House   Calenda_r 10:20pm Tomor'w");
	display1_h.rec_up_down(-1); // next Menu
	CHECK(test_stream(display1_h.stream(tb)) == "House   Program_s At Home At Work Away   ");
	display1_h.rec_up_down(-1); // next Menu
	CHECK(test_stream(display1_h.stream(tb)) == "House   Zone_s UpStrs DnStrs DHW   ");
}
#endif

#ifdef VIEW_ONE_NESTED_CALENDAR_PAGE
SCENARIO("View-one nested Calendar element", "[Display]") {
	cout << "\n*********************************\n**** View-one nested Calendar element ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;
	using namespace LCD_UI;

	LCD_Display_Buffer<20, 4> lcd;
	UI_DisplayBuffer tb(lcd);
	clock_().setTime({ 31,7,17 }, { 18,10 }, 0);

	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	auto q_dwellings = db.tableQuery(TB_Dwelling);
	auto q_dwellingZones = QueryFL_T<R_DwellingZone>{ db.tableQuery(TB_DwellingZone), db.tableQuery(TB_Zone), 0, 1 };
	auto q_dwellingProgs = QueryF_T<R_Program>{ db.tableQuery(TB_Program) , 1 };
	auto q_dwellingSpells = QueryLF_T<R_Spell, R_Program>{ db.tableQuery(TB_Spell), db.tableQuery(TB_Program), 1, 1 };
	auto q_spellProgs = QueryLinkF_T<R_Spell, R_Program> { q_dwellingSpells, db.tableQuery(TB_Program), 1 ,1 };
	auto q_spellProg = QueryML_T<R_Spell>{ db.tableQuery(TB_Spell), q_spellProgs, 0 };

	auto rec_dwelling = Dataset_Dwelling(q_dwellings, noVolData, 0);
	auto rec_dwZone = Dataset_Zone(q_dwellingZones, noVolData, &rec_dwelling);
	auto rec_dwProgs = Dataset_Program(q_dwellingProgs, noVolData, &rec_dwelling);
	auto rec_dwSpells = Dataset_Spell(q_dwellingSpells, noVolData, &rec_dwelling);
	auto rec_spellProg = Dataset_Program(q_spellProg, noVolData, &rec_dwSpells);

	auto dwellNameUI_c = UI_FieldData(&rec_dwelling, Dataset_Dwelling::e_name);
	auto zoneNameUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_name,Behaviour{V+S+L+VnLR+UD_0+R0});
	auto progNameUI_c = UI_FieldData(&rec_dwProgs, Dataset_Program::e_name, Behaviour{V + S + L + VnLR + UD_0 + R0});
	auto dwellSpellUI_c = UI_FieldData(&rec_dwSpells, Dataset_Spell::e_date, Behaviour{ V + S + V1 + UD_E });
	auto spellProgUI_c = UI_FieldData(&rec_spellProg, Dataset_Program::e_name, Behaviour{ V + S +L+ V1+ER });

	ui_Objects()[(long)&dwellNameUI_c] = "dwellNameUI_c";
	ui_Objects()[(long)&zoneNameUI_c] = "zoneNameUI_c";
	ui_Objects()[(long)&progNameUI_c] = "progNameUI_c";
	ui_Objects()[(long)&dwellSpellUI_c] = "dwellSpellUI_c";
	ui_Objects()[(long)&spellProgUI_c] = "spellProgUI_c";

	// UI Elements
	//auto iteratedZoneName = UI_IteratedCollection<1>{ 60,zoneNameUI_c};
	auto iteratedZoneName = UI_IteratedCollection{ 60,makeCollection(zoneNameUI_c) };
	auto iteratedProgName = UI_IteratedCollection{ 60,makeCollection(progNameUI_c) };
	UI_Cmd _dwellingZoneCmd = { "Zones",0 }, _dwellingCalendarCmd = { "Calendar",0 }, _dwellingProgCmd = { "Programs",0 };
	InsertSpell_Cmd _fromCmd = { "From", 0, Behaviour{V + S + L+ UD_A} };
	UI_Label _insert = { "Insert-Prog", Behaviour{H+L0} };

	ui_Objects()[(long)&iteratedZoneName] = "iteratedZoneName";
	ui_Objects()[(long)&iteratedProgName] = "iteratedProgName";
	ui_Objects()[(long)&_dwellingZoneCmd] = "_dwellingZoneCmd";
	ui_Objects()[(long)&_dwellingCalendarCmd] = "_dwellingCalendarCmd";
	ui_Objects()[(long)&_dwellingProgCmd] = "_dwellingProgCmd";
	ui_Objects()[(long)&_fromCmd] = "_fromCmd";
	ui_Objects()[(long)&_insert] = "_insert";

	// UI Element Arays / Collections
	auto zone_subpage_c = makeCollection(_dwellingZoneCmd, iteratedZoneName);
	auto calendar_subpage_c = makeCollection(_dwellingCalendarCmd, _insert, _fromCmd, dwellSpellUI_c, spellProgUI_c);
	auto prog_subpage_c = makeCollection(_dwellingProgCmd, iteratedProgName);
	zone_subpage_c.behaviour().make_noRecycle();
	calendar_subpage_c.behaviour().make_noRecycle();
	prog_subpage_c.behaviour().make_noRecycle();
	
	ui_Objects()[(long)&zone_subpage_c] = "zone_subpage_c";
	ui_Objects()[(long)&calendar_subpage_c] = "calendar_subpage_c";
	ui_Objects()[(long)&prog_subpage_c] = "prog_subpage_c";

	_fromCmd.set_UpDn_Target(calendar_subpage_c.item(3));

	auto _page_dwellingMembers_subpage_c = makeCollection(zone_subpage_c, calendar_subpage_c, prog_subpage_c);
	_page_dwellingMembers_subpage_c.set(Behaviour{V+S+V1+UD_A+R});
	_fromCmd.set_OnSelFn_TargetUI(_page_dwellingMembers_subpage_c.item(1));

	auto _page_dwellingMembers_c = makeCollection(dwellNameUI_c, _page_dwellingMembers_subpage_c);
	_dwellingZoneCmd.set_UpDn_Target(_page_dwellingMembers_c.item(1));
	_dwellingCalendarCmd.set_UpDn_Target(_page_dwellingMembers_c.item(1));
	_dwellingProgCmd.set_UpDn_Target(_page_dwellingMembers_c.item(1));

	auto display1_c = makeChapter(_page_dwellingMembers_c);
	auto display1_h = A_Top_UI(display1_c);

	ui_Objects()[(long)&_page_dwellingMembers_subpage_c] = "_page_dwellingMembers_subpage_c";
	ui_Objects()[(long)&_page_dwellingMembers_c] = "_page_dwellingMembers_c";
	ui_Objects()[(long)&display1_c] = "display1_c";

	for (Answer_R<R_Program>program : q_dwellingProgs) {
		cout << (int)program.id() << " " << program.rec().name << endl;
	}

	for (Answer_R<R_Spell> spell : q_dwellingSpells) {
		logger() << (int)spell.id() << ": " << spell.rec() << L_endl;
	}
	cout << endl;

	GIVEN("Sub-Page with Cmd functions") {
		display1_h.stream(tb);
		cout << test_stream(display1_h.stream(tb)) << endl;;
		REQUIRE(test_stream(display1_h.stream(tb)) == "House   Zones       UpStrs DnStrs DHW   ");
		//											 0123456789012345678901234567890123456789
		display1_h.rec_left_right(1); // left-right so select page
		CHECK(test_stream(display1_h.stream(tb)) == "Hous_e   Zones       UpStrs DnStrs DHW   ");
		THEN("Command accepts focus and selection does nothing") {
			display1_h.rec_left_right(1); // right
			CHECK(test_stream(display1_h.stream(tb)) == "House   Zone_s       UpStrs DnStrs DHW   ");
			display1_h.rec_select();
			CHECK(test_stream(display1_h.stream(tb)) == "House   Zone_s       UpStrs DnStrs DHW   ");
			AND_THEN("UP selects next sub-page and recycles") {
				display1_h.rec_up_down(-1); // next Menu
				CHECK(test_stream(display1_h.stream(tb)) == "House   Program_s    At Home At Work     Away   ");
				display1_h.rec_up_down(-1); // next Menu
				CHECK(test_stream(display1_h.stream(tb)) == "House   Calenda_r    From 10:20pm Tomor'wAt Home");
				display1_h.rec_up_down(-1); // next Menu
				CHECK(test_stream(display1_h.stream(tb)) == "House   Zone_s       UpStrs DnStrs DHW   ");
				THEN("The sub-page can be scrolled into") {
					display1_h.rec_left_right(1); // right
					CHECK(test_stream(display1_h.stream(tb)) == "House   Zones       UpStr_s DnStrs DHW   ");
					display1_h.rec_left_right(1); // right
					CHECK(test_stream(display1_h.stream(tb)) == "House   Zones       UpStrs DnStr_s DHW   ");
					display1_h.rec_left_right(1); // right
					CHECK(test_stream(display1_h.stream(tb)) == "House   Zones       UpStrs DnStrs DH_W   ");
					display1_h.rec_left_right(1); // right
					REQUIRE(test_stream(display1_h.stream(tb)) == "Hous_e   Zones       UpStrs DnStrs DHW   ");
				}
			}
		}
	}

	GIVEN("Calendar Sub-Page") {
		display1_h.rec_left_right(1); // left-right so select page
		display1_h.rec_left_right(1);
		display1_h.rec_up_down(1); // Calendar Subpage
		display1_h.rec_left_right(1);
		display1_h.rec_left_right(1);
		REQUIRE(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm Tomor'_wAt Home");

		// Modify first spell
		display1_h.rec_up_down(-1);
		display1_h.rec_up_down(-1);
		CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 0#3Aug  At Home");
		display1_h.rec_select();

		cout << "\n **** Saved Modified first spell ****\n\n";
		for (Answer_R<R_Spell> spell : q_dwellingSpells) {
			logger() << (int)spell.id() << ": " << spell.rec() << L_endl;
		}
		cout << "\n **** Spell-Progs ****\n\n";
		for (Answer_R<R_Program> prog : q_spellProg) {
			logger() << (int)prog.id() << ": " << prog.rec() << L_endl;
		}

		CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 0_3Aug  At Home");
		THEN("Spells can be scrolled on From") {
			display1_h.rec_left_right(-1);
			CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 10:20pm 03Aug  At Home");
			display1_h.rec_up_down(1);
			CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 05:30pm 03Sep  Away   ");
			display1_h.rec_up_down(1);
			CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 07:30am 12Sep  At Work");
			display1_h.rec_up_down(1);
			CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 03:00pm 22Sep  Away   ");
			display1_h.rec_up_down(1);
			CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 10:20pm 03Aug  At Home");
			THEN("SELECT on From inserts a spell") {
				display1_h.rec_select();
				CHECK(test_stream(display1_h.stream(tb)) == "House   Insert-Prog From 10:20pm 0#3Aug  At Home");
				display1_h.rec_up_down(1);
				CHECK(test_stream(display1_h.stream(tb)) == "House   Insert-Prog From 10:20pm 0#2Aug  At Home");
				AND_THEN("BACK cancels the insert") {
					display1_h.rec_prevUI();
					CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 10:20pm 03Aug  At Home");
					cout << "\n **** Cancelled insert spell ****\n\n";
					for (Answer_R<R_Spell> spell : q_dwellingSpells) {
						logger() << (int)spell.id() << ": " << spell.rec() << L_endl;
					}
					THEN("Selecting 'From' allows insertion of a new spell before the current displayed spell") {
						display1_h.rec_select();
						CHECK(test_stream(display1_h.stream(tb)) == "House   Insert-Prog From 10:20pm 0#3Aug  At Home");
						display1_h.rec_up_down(1);
						CHECK(test_stream(display1_h.stream(tb)) == "House   Insert-Prog From 10:20pm 0#2Aug  At Home");
						display1_h.rec_select();
						CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 10:20pm 02Aug  At Home");
						AND_THEN("we can change the selected program"){
							display1_h.rec_left_right(1);
							display1_h.rec_left_right(1);
							display1_h.rec_select();
							CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 02Aug  #At Home");
							display1_h.rec_up_down(-1);
							CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 02Aug  #At Work");
							display1_h.rec_select();
							CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 02Aug  At Wor_k");
							THEN("We can insert new spell after first") {
								display1_h.rec_left_right(-1);
								display1_h.rec_left_right(-1);
								display1_h.rec_select();
								display1_h.rec_up_down(-1);
								display1_h.rec_up_down(-1);
								CHECK(test_stream(display1_h.stream(tb)) == "House   Insert-Prog From 10:20pm 0#4Aug  At Work");

								cout << "\n Before save Insert new spell after first...\n";
								for (Answer_R<R_Spell> spell : q_dwellingSpells) {
									logger() << (int)spell.id() << ": " << spell.rec() << L_endl;
								}

								display1_h.rec_select();
								cout << "\n After save Insert new spell after first...\n";
								for (Answer_R<R_Spell> spell : q_dwellingSpells) {
									logger() << (int)spell.id() << ": " << spell.rec() << L_endl;
								}

								display1_h.stream(tb);
								CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 10:20pm 04Aug  At Work");
								display1_h.rec_up_down(1);
								CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 05:30pm 03Sep  Away   ");
								display1_h.rec_up_down(1);
								CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 07:30am 12Sep  At Work");
								display1_h.rec_up_down(1);
								CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 03:00pm 22Sep  Away   ");
								THEN("And we can Edit last spell") {
									display1_h.rec_left_right(1);
									display1_h.rec_select();
									CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 03:00pm 2#2Sep  Away   ");
									display1_h.rec_left_right(1);
									display1_h.rec_up_down(-1);
									CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 03:00pm 22#Oct  Away   ");
									display1_h.rec_select();

									cout << "\n Edit last spell...\n";
									for (Answer_R<R_Spell> spell : q_dwellingSpells) {
										logger() << (int)spell.id() << ": " << spell.rec() << L_endl;
									}
									cout << "\n SpellProgs...\n";
									for (Answer_R<R_Program> prog : q_spellProg) {
										logger() << (int)prog.id() << ": " << prog.rec() << L_endl;
									}

									CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 03:00pm 2_2Oct  Away   ");

									THEN("and Insert new spell before last") {
										display1_h.rec_left_right(-1);
										CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 03:00pm 22Oct  Away   ");
										display1_h.rec_select();
										display1_h.rec_up_down(1);
										display1_h.rec_up_down(1);
										CHECK(test_stream(display1_h.stream(tb)) == "House   Insert-Prog From 03:00pm 2#0Oct  Away   ");
										display1_h.rec_select();
										CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 03:00pm 20Oct  Away   ");

										cout << "\n Insert new spell before last...\n";
										for (Answer_R<R_Spell> spell : q_dwellingSpells) {
											logger() << (int)spell.id() << ": " << spell.rec() << L_endl;
										}

										THEN("and Insert new spell after last") {
											display1_h.rec_up_down(1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 03:00pm 22Oct  Away   ");
											display1_h.rec_select();
											display1_h.rec_up_down(-1);
											display1_h.rec_up_down(-1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Insert-Prog From 03:00pm 2#4Oct  Away   ");
											display1_h.rec_select();
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 03:00pm 24Oct  Away   ");

											cout << "\n All spells after last...\n";
											for (Answer_R<R_Spell> spell : q_dwellingSpells) {
												logger() << (int)spell.id() << ": " << spell.rec() << L_endl;
											}
											cout << test_stream(display1_h.stream(tb)) << endl;
											cout << clock_().now() << endl;
											// Check all entries correct
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 03:00pm 24Oct  Away   ");
											display1_h.rec_up_down(1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 10:20pm 02Aug  At Work");
											display1_h.rec_up_down(1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 10:20pm 03Aug  At Home");
											display1_h.rec_up_down(1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 10:20pm 04Aug  At Work");
											display1_h.rec_up_down(1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 05:30pm 03Sep  Away   ");
											display1_h.rec_up_down(1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 07:30am 12Sep  At Work");
											display1_h.rec_up_down(1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 03:00pm 20Oct  Away   ");
											display1_h.rec_up_down(1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 03:00pm 22Oct  Away   ");
											display1_h.rec_up_down(1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 03:00pm 24Oct  Away   ");
											display1_h.rec_up_down(1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 10:20pm 02Aug  At Work");
											display1_h.rec_left_right(-1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   Calenda_r    From 10:20pm 02Aug  At Work");
											display1_h.rec_left_right(-1);
											CHECK(test_stream(display1_h.stream(tb)) == "Hous_e   Calendar    From 10:20pm 02Aug  At Work");
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	
	GIVEN("Program on Calendar Sub-Page") {
		display1_h.rec_left_right(1); // left-right so select page
		display1_h.rec_left_right(1);
		display1_h.rec_up_down(1); // Calendar Subpage
		REQUIRE(test_stream(display1_h.stream(tb)) == "House   Calenda_r    From 10:20pm 02Aug  At Work");
		display1_h.rec_left_right(1);
		CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m 10:20pm 02Aug  At Work");
		display1_h.rec_left_right(1);
		CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 0_2Aug  At Work");

		THEN("up-dn on the program has no effect") {
			display1_h.rec_left_right(1);
			CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 02Aug  At Wor_k");
			display1_h.rec_up_down(-1);
			CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 02Aug  At Wor_k");
			display1_h.rec_up_down(1);
			CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 02Aug  At Wor_k");
			THEN("SELECT program allows programs to be cycled through") {
				display1_h.rec_select();
				CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 02Aug  #At Work");
				display1_h.rec_up_down(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 02Aug  #Away   ");
				display1_h.rec_up_down(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 02Aug  #At Home");
				AND_THEN("BACK restores original program") {
					display1_h.rec_prevUI();
					CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From 10:20pm 02Aug  At Wor_k");
				}
			}
		}
	}
	
	GIVEN("Program on Flat Calendar Sub-Page ") {
		display1_h.rec_left_right(1); // left-right so select page
		display1_h.rec_up_down(1); // next Dwelling
		CHECK(test_stream(display1_h.stream(tb)) == "HolApp_t Zones       Flat   DHW   ");
		display1_h.rec_left_right(1);
		display1_h.rec_up_down(1); // Calendar Subpage
		CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calenda_r    From Now            Occup'd");
		display1_h.rec_left_right(1); // From field
		CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    Fro_m Now            Occup'd");
		cout << test_stream(display1_h.stream(tb)) << endl;
		cout << test_stream(display1_h.stream(tb)) << endl;
		display1_h.rec_left_right(1); // Spell field
		CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    From No_w            Occup'd");
		display1_h.rec_left_right(1); // Prog field
		CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    From Now            Occup'_d");

		cout << "\n Appt spells  Before Change program...\n";
		for (Answer_R<R_Spell> spell : q_dwellingSpells) {
			logger() << (int)spell.id() << ": " << spell.rec() << L_endl;
		}

		THEN("Program can be changed") {
			// Change   Program
			REQUIRE(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    From Now            Occup'_d");
			display1_h.rec_select();
			CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    From Now            #Occup'd");
			display1_h.rec_up_down(1);
			CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    From Now            #Empty  ");
			display1_h.rec_select();
			CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    From Now            Empt_y  ");

			cout << "\n Appt spells After Change program...\n";
			for (Answer_R<R_Spell> spell : q_dwellingSpells) {
				logger() << (int)spell.id() << ": " << spell.rec() << L_endl;
			}
			cout << "\n All spells After Change program...\n";
			for (Answer_R<R_Spell> spell : db.tableQuery(TB_Spell)) {
				logger() << (int)spell.id() << ": " << spell.rec() << L_endl;
			}
			AND_THEN("the spells can by cycled") {
				CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    From Now            Empt_y  ");
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    From No_w            Empty  ");
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    Fro_m Now            Empty  ");
				display1_h.rec_up_down(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    Fro_m 10:00am 30Sep  Empty  ");

				AND_THEN("the program can by changed again") {
					display1_h.rec_left_right(1);
					display1_h.rec_left_right(1);
					CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    From 10:00am 30Sep  Empt_y  ");
					display1_h.rec_select();
					CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    From 10:00am 30Sep  #Empty  ");
					display1_h.rec_up_down(-1);
					CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    From 10:00am 30Sep  #Occup'd");
					display1_h.rec_select();
					AND_THEN("the spells can by cycled") {
						display1_h.rec_left_right(-1);
						display1_h.rec_left_right(-1);
						display1_h.rec_up_down(-1);
						CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    Fro_m Now            Empty  ");
						display1_h.rec_up_down(-1);
						CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    Fro_m 10:00am 30Sep  Occup'd");
						display1_h.rec_up_down(-1);
						CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Calendar    Fro_m Now            Empty  ");
					}
				}
			}
		}
	}
}
#endif

#ifdef VIEW_ONE_NESTED_PROFILE_PAGE
SCENARIO("View-one nested Profile element", "[Display]") {
	cout << "\n*********************************\n**** View-one nested Profile element ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;
	using namespace LCD_UI;

	LCD_Display_Buffer<20, 4> lcd;
	UI_DisplayBuffer tb(lcd);
	clock_().setTime({ 31,7,17 }, { 8,10 }, 0);

	RDB<TB_NoOfTables> db(RDB_START_ADDR, writer, reader, VERSION);

	cout << "\tand some Queries are created" << endl;
	auto q_dwellings = db.tableQuery(TB_Dwelling);
	auto q_dwellingZones = QueryFL_T<R_DwellingZone>{ db.tableQuery(TB_DwellingZone), db.tableQuery(TB_Zone), 0, 1 };
	auto q_dwellingProgs = QueryF_T<R_Program>{ db.tableQuery(TB_Program) , 1 };
	auto q_zoneProfiles = QueryF_T<R_Profile>{ db.tableQuery(TB_Profile), 1 };
	auto q_progProfiles = QueryF_T<R_Profile>(q_zoneProfiles, 0);

	cout << " **** Next create DB Record Interface ****\n";
	auto rec_dwelling = Dataset_Dwelling(q_dwellings, noVolData, 0);
	auto rec_dwZone = Dataset_Zone(q_dwellingZones, noVolData, &rec_dwelling);
	auto rec_dwProgs = Dataset_Program(q_dwellingProgs, noVolData, &rec_dwelling);
	auto rec_profile = Dataset_ProfileDays(q_progProfiles, noVolData, &rec_dwProgs, &rec_dwZone);

	cout << "\n **** Next create DB UI LazyCollections ****\n";
	auto dwellNameUI_c = UI_FieldData(&rec_dwelling, Dataset_Dwelling::e_name);
	auto zoneAbbrevUI_c = UI_FieldData(&rec_dwZone, Dataset_Zone::e_abbrev, Behaviour{V+S+V1+UD_A+R});
	auto progNameUI_c = UI_FieldData(&rec_dwProgs, Dataset_Program::e_name, Behaviour{V+S+V1+UD_A+R});
	auto profileDaysUI_c = UI_FieldData(&rec_profile, Dataset_ProfileDays::e_days, Behaviour{V+S+V1+UD_A+R + ER}, &progNameUI_c, Dataset_Program::e_id);

	// UI Elements
	UI_Label _prog = { "Prog:", {V + L0} };
	UI_Label _zone = { "Zone:" };

	// UI Element Arays / Collections
	cout << "\nprofile_page Elements Collection\n";
	auto profile_page_c = makeCollection(dwellNameUI_c, _prog, progNameUI_c, _zone, zoneAbbrevUI_c, profileDaysUI_c);

	cout << "\nDisplay     Collection\n";
	auto display1_c = makeChapter(profile_page_c);
	auto display1_h = A_Top_UI(display1_c);

	cout << "\n **** All Constructed ****\n\n";
	display1_h.stream(tb);
	cout << test_stream(display1_h.stream(tb)) << endl;
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prog:       At Home             Zone: US  MT--F--");
	//											 0123456789012345678901234567890123456789
}
#endif

#ifdef CONTRAST
TEST_CASE("Contrast", "[Display]") {
	cout << "\n*********************************\n**** MainConsoleChapters ****\n********************************\n\n";

	using namespace client_data_structures;
	using namespace Assembly;
	using namespace LCD_UI;

	LCD_Display_Buffer<20, 4> lcd;
	UI_DisplayBuffer tb(lcd);
	clock_().setTime({ 31,7,17 }, { 8,10 }, 0);

	RDB<TB_NoOfTables>_db(RDB_START_ADDR, EEPROM_SIZE, writer, reader, VERSION);
	setFactoryDefaults(_db, VERSION);
	RelationalDatabase::TableQuery _q_displays(_db.tableQuery(TB_Display));
	HardwareInterfaces::LocalDisplay mainDisplay(&_q_displays);

	// Basic UI Elements
	client_data_structures::Contrast_Brightness_Cmd _contrastCmd{ "Contrast",0, Behaviour{V + S + L+V1 + UD_A} };
	_contrastCmd.setDisplay(mainDisplay);
	// Pages - Collections of UI handles
	cout << "\ntt_page Elements Collection\n";
	auto _page_contrast_c{ makeCollection(_contrastCmd) };

	cout << "\nDisplay     Collection\n";
	auto display1_c = makeChapter(_page_contrast_c);
	_contrastCmd.set_UpDn_Target(_contrastCmd.function(Contrast_Brightness_Cmd::e_contrast));

	auto display1_h = A_Top_UI(display1_c);

	cout << "\n **** All Constructed ****\n\n";
	display1_h.stream(tb);
	cout << test_stream(display1_h.stream(tb)) << endl;
	CHECK(test_stream(display1_h.stream(tb)) == "Contrast");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "Contras_t");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "Contras_t");
	display1_h.rec_prevUI();
	CHECK(test_stream(display1_h.stream(tb)) == "Contrast");
}
#endif

#ifdef TIME_TEMP_EDIT
SCENARIO("TimeTemps", "[Display]") {
	cout << "\n*********************************\n**** MainConsoleChapters ****\n********************************\n\n";

	using namespace client_data_structures;
	using namespace Assembly;
	using namespace LCD_UI;

	LCD_Display_Buffer<20, 4> lcd;
	UI_DisplayBuffer tb(lcd);
	clock_().setTime({ 31,7,17 }, { 8,10 }, 0);

	RDB<TB_NoOfTables>_db(RDB_START_ADDR, EEPROM_SIZE, writer, reader, VERSION);
	setFactoryDefaults(_db, VERSION);

	cout << "\tand some Queries are created" << endl;
	auto _q_dwellings = _db.tableQuery(TB_Dwelling);
	auto _q_dwellingProgs = QueryF_T<client_data_structures::R_Program>{ _db.tableQuery(TB_Program), 1 };
	auto _q_dwellingZones = QueryFL_T<client_data_structures::R_DwellingZone>{ _db.tableQuery(TB_DwellingZone), _db.tableQuery(TB_Zone), 0, 1 };
	auto _q_zoneProfiles = QueryF_T<client_data_structures::R_Profile>{ _db.tableQuery(TB_Profile), 1 };
	auto _q_profile = QueryF_T<client_data_structures::R_Profile>{ _q_zoneProfiles, 0 };
	auto _q_timeTemps = QueryF_T<R_TimeTemp>{ _db.tableQuery(TB_TimeTemp) , 0 };

	cout << " **** Next create DB Record Interface ****\n";
	auto _rec_dwelling = Dataset_Dwelling(_q_dwellings, noVolData, 0);
	auto _rec_dwProgs = Dataset_Program{ _q_dwellingProgs, noVolData, &_rec_dwelling };
	auto _rec_dwZones = Dataset_Zone{ _q_dwellingZones, 0, &_rec_dwelling };
	auto _rec_profile = Dataset_ProfileDays{ _q_profile, noVolData, &_rec_dwProgs, &_rec_dwZones };
	auto _rec_timeTemps = Dataset_TimeTemp(_q_timeTemps, noVolData, &_rec_profile);

	cout << "\n **** Next create DB UI LazyCollections ****\n";
	cout << "\n\tdwelling\n";
	auto _dwellNameUI_c = UI_FieldData{ &_rec_dwelling, Dataset_Dwelling::e_name };
	cout << "\tprogram\n";
	auto _progNameUI_c = UI_FieldData{ &_rec_dwProgs, Dataset_Program::e_name, Behaviour{V + S + V1 + UD_A + R} };
	cout << "\tzone\n";
	auto _zoneAbbrevUI_c = UI_FieldData{ &_rec_dwZones, Dataset_Zone::e_abbrev, Behaviour{V + S + V1 + UD_A + R} };
	cout << "\tprofile\n";
	auto _profileDaysUI_c = UI_FieldData{ &_rec_profile, Dataset_ProfileDays::e_days, Behaviour{V + S + V1 + UD_A + R + ER}};
	cout << "\ttimeTemp\n";
	auto _timeTempUI_c = UI_FieldData(&_rec_timeTemps, Dataset_TimeTemp::e_TimeTemp, Behaviour{ V + L + S + VnLR + UD_E + R0 +ER0 }, 0, 0, { static_cast<Collection_Hndl * (Collection_Hndl::*)(int)>(&InsertTimeTemp_Cmd::enableCmds), InsertTimeTemp_Cmd::e_allCmds });
	auto _iterated_timeTempUI = UI_IteratedCollection{ 80, makeCollection(_timeTempUI_c) };
	//auto _iterated_timeTempUI = UI_IteratedCollection<1>{ 80, _timeTempUI_c};

	InsertTimeTemp_Cmd _deleteTTCmd = { "Delete", 0, Behaviour{H + L + S + VnLR} };
	InsertTimeTemp_Cmd _editTTCmd = { "Edit", 0, Behaviour{H + S + VnLR} };
	InsertTimeTemp_Cmd _newTTCmd = { "New", 0, Behaviour{H + S} };

	// Pages & sub-pages - Collections of UI handles
	cout << "\ntt_page Elements Collection\n";
	auto _tt_SubPage_c{ makeCollection(_deleteTTCmd, _editTTCmd, _newTTCmd, _iterated_timeTempUI) };
	auto _page_profile_c{ makeCollection(_dwellNameUI_c, _progNameUI_c, _zoneAbbrevUI_c, _profileDaysUI_c, _tt_SubPage_c) };

	cout << "\nDisplay     Collection\n";
	auto display1_c = makeChapter(_page_profile_c);
	_deleteTTCmd.set_OnSelFn_TargetUI(&_editTTCmd);
	_editTTCmd.set_OnSelFn_TargetUI(_page_profile_c.item(4));
	_newTTCmd.set_OnSelFn_TargetUI(&_editTTCmd);
	_timeTempUI_c.set_OnSelFn_TargetUI(&_editTTCmd);

	auto display1_h = A_Top_UI(display1_c);

	ui_Objects()[(long)&_dwellNameUI_c] = "dwellNameUI_c";
	ui_Objects()[(long)&_zoneAbbrevUI_c] = "_zoneAbbrevUI_c";
	ui_Objects()[(long)&_progNameUI_c] = "progNameUI_c";
	ui_Objects()[(long)&_profileDaysUI_c] = "_profileDaysUI_c";
	ui_Objects()[(long)&_timeTempUI_c] = "_timeTempUI_c";
	ui_Objects()[(long)&_iterated_timeTempUI] = "_iterated_timeTempUI";
	ui_Objects()[(long)&_deleteTTCmd] = "_deleteTTCmd";
	ui_Objects()[(long)&_editTTCmd] = "_editTTCmd";
	ui_Objects()[(long)&_newTTCmd] = "_newTTCmd";
	ui_Objects()[(long)&_tt_SubPage_c] = "_tt_SubPage_c";
	ui_Objects()[(long)&_page_profile_c] = "_page_profile_c";
	ui_Objects()[(long)&display1_c] = "display1_c";

	for (Answer_R<R_TimeTemp> tt : _q_timeTemps) {
		logger() << (int)tt.id() << " : " << tt.rec() << L_endl;
	}

	cout << "\n **** All Constructed ****\n\n";
	GIVEN("we can scroll into a TT") {
		display1_h.stream(tb);
		cout << test_stream(display1_h.stream(tb)) << endl;
		REQUIRE(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a15 0900p18");
		display1_h.rec_left_right(1);
		display1_h.rec_left_right(1);
		display1_h.rec_left_right(1);
		display1_h.rec_left_right(1);
		CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFS_S             0730a15 0900p18");
		display1_h.rec_left_right(1);
		CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a1_5 0900p18");
		THEN("up-dn edits the temp") {
			display1_h.rec_up_down(-1);
			CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a1#6 0900p18");
			AND_THEN("LR saves and edits the next temp") {
				display1_h.rec_left_right(1);
				CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p1#8");
				AND_THEN("SELECT when in EDIT saves the temp") {
					display1_h.rec_select();
					CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p1_8");
					AND_THEN("SELECT offers Delete/Edit/New") {
						display1_h.rec_select();
						CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             Delete Edi_t New     0900p18");
						THEN("RIGHT immediatly inserts new") {
							display1_h.rec_left_right(1);
							CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             New                 0#900p18");
							cout << test_stream(display1_h.stream(tb)) << endl;
							display1_h.rec_up_down(-1);
							CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             New                 1#000p18");
							THEN("SAVE returns to TT list") {
								display1_h.rec_select();
								display1_h.stream(tb);
								cout << test_stream(display1_h.stream(tb)) << endl;
								CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p18     1000p1_8");
								display1_h.rec_up_down(-1);
								CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p18     1000p1#9");
								CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p18     1000p1#9");
								display1_h.rec_select();
								CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p18     1000p1_9");
								THEN("Cancelled NEW deletes the inserted TT") {
									display1_h.rec_select();
									CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             Delete Edi_t New     1000p19");
									display1_h.rec_left_right(1);
									CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             New                 1#000p19");
									display1_h.rec_up_down(1);
									CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             New                 0#900p19");
									display1_h.rec_prevUI();
									CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p18     1000p1_9");
									AND_THEN("DELETE removes seected TT") {
										display1_h.rec_select();
										CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             Delete Edi_t New     1000p19");
										display1_h.rec_left_right(-1);
										CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             Delet_e              1000p19");
										display1_h.rec_select();
										CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p1_8");
										THEN("EDIT and DELETE can be cancelled") {
											display1_h.rec_select();
											CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             Delete Edi_t New     0900p18");
											display1_h.rec_prevUI();
											CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p1_8");
											display1_h.rec_select();
											CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             Delete Edi_t New     0900p18");
											display1_h.rec_left_right(-1);
											CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             Delet_e              0900p18");
											display1_h.rec_prevUI();
											CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p1_8");
											AND_THEN("New inserts repeatedly") {
												display1_h.rec_select();
												display1_h.rec_left_right(1);
												CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             New                 0#900p18");
												display1_h.rec_up_down(-1);
												display1_h.rec_select();
												display1_h.rec_select();
												display1_h.rec_left_right(1);
												CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             New                 1#000p18");
												display1_h.rec_up_down(-1);
												display1_h.rec_select();
												display1_h.stream(tb);
												CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p18     1000p18 1100p1_8");
												display1_h.rec_select();
												display1_h.rec_left_right(1);
												display1_h.rec_up_down(-1);
												CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             New                 1#200p18");
												display1_h.rec_select();
												THEN("Multiple TTs can be scrolled through") {
													CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             <0900p18 1000p18    1100p18 1200p1_8");
													display1_h.rec_left_right(1);
													CHECK(test_stream(display1_h.stream(tb)) == "Hous_e   At Home US  MTWTFSS             <0900p18 1000p18    1100p18 1200p18");
													display1_h.rec_left_right(-1);
													CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             <0900p18 1000p18    1100p18 1200p1_8");
													display1_h.rec_left_right(-1);
													CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             <0900p18 1000p18    1100p1_8 1200p18");
													display1_h.rec_left_right(-1);
													CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             <0900p18 1000p1_8    1100p18 1200p18");
													display1_h.rec_left_right(-1);
													CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             <0900p1_8 1000p18    1100p18 1200p18");
													display1_h.rec_left_right(-1);
													CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a1_6 0900p18     1000p18 1100p18>    ");
													display1_h.rec_left_right(1);
													display1_h.rec_left_right(1);
													display1_h.rec_left_right(1);
													CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             0730a16 0900p18     1000p18 1100p1_8>    ");
													display1_h.rec_left_right(1);
													CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             <0900p18 1000p18    1100p18 1200p1_8");
													THEN("More TTs inserted and any selected TT edited") {
														display1_h.rec_select();
														display1_h.rec_left_right(1);
														CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             New                 1#200p18");
														display1_h.rec_up_down(-1);
														CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             New                 0#100p18");
														display1_h.rec_select();
														display1_h.stream(tb);
														CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             <1000p18 1100p18    1200p18 0100p1_8");
														display1_h.rec_select();
														CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             Delete Edi_t New     0100p18");
														display1_h.rec_select();
														CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             Edit                0#100p18");
														display1_h.rec_prevUI();
														CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             <1000p18 1100p18    1200p18 0100p1_8");
														display1_h.rec_select();
														CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             Delete Edi_t New     0100p18");
														display1_h.rec_select();
														CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             Edit                0#100p18");
														display1_h.rec_prevUI();
														CHECK(test_stream(display1_h.stream(tb)) == "House   At Home US  MTWTFSS             <1000p18 1100p18    1200p18 0100p1_8");
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
#endif

#ifdef MAIN_CONSOLE_PAGES
TEST_CASE("MainConsoleChapters", "[Display]") {
	cout << "\n*********************************\n**** MainConsoleChapters ****\n********************************\n\n";

	
	using namespace client_data_structures;
	using namespace Assembly;
	using namespace LCD_UI;

	LCD_Display_Buffer<20, 4> lcd;
	UI_DisplayBuffer tb(lcd);
	clock_().setTime({ 31,7,19 }, { 16,10 }, 0);
	unsigned long timeOfReset_mS = 0;

	HeatingSystem hs{};
	setFactoryDefaults(hs.getDB(), 1);

	auto q_Profiles = hs.getDB().tableQuery(TB_Profile);
	for (Answer_R<R_Profile> profile : q_Profiles) {
		logger() << profile.rec() << L_endl;
	}
	logger() << "Table Capacity: " << q_Profiles.last().tableNavigator().table().maxRecordsInTable() << L_endl;
	logger() << "Chunk Capacity: " << q_Profiles.last().tableNavigator().chunkCapacity() << L_endl;
	logger() << "chunkIsExtended(): " << q_Profiles.last().tableNavigator().chunkIsExtended() << L_endl;
	

	auto & display1_h = hs.mainConsoleChapters()(0);
	cout << test_stream(display1_h.stream(tb)) << endl;
	clock_().setTime({ 31,7,19 }, {16,10 }, 0);
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "04:10:00p_m SD OK    Wed 31/Jul/2019     DST Hours: 1        Backlight Contrast");
	display1_h.rec_left_right(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "04:10:00pm SD OK    Wed 31/Jul/2019     DST Hours: 1        Backlight Contras_t");
	display1_h.rec_up_down(-1);
	display1_h.rec_prevUI();
	CHECK(test_stream(display1_h.stream(tb)) == "04:10:00pm SD OK    Wed 31/Jul/2019     DST Hours: 1        Backlight Contrast");

	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: US  Ds: MTWTFSS0730a15 0900p18");
	//											 01234567890123456789012345678901234567890123456789012345678901234567890123456789
	//											 Line[0]			 Line[1]			 Line[2]			 Line[3]
	cout << test_stream(display1_h.stream(tb)) << endl;
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From Now            At Home");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "Hous_e   Calendar    From Now            At Home");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Calenda_r    From Now            At Home");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    Fro_m Now            At Home");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From No_w            At Home");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From Now            At Hom_e");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From Now            #At Home");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From Now            #At Work");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Calendar    From Now            At Wor_k");
	display1_h.rec_left_right(1);
	display1_h.rec_left_right(1);
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Zone_s       UpStrs DnStrs DHW   ");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Program_s    At Home At Work     Away   ");
	display1_h.rec_prevUI();
	display1_h.rec_up_down(1);

	auto q_TimeTemps = hs.getQueries()._q_timeTemps;
	q_TimeTemps.setMatchArg(0);
	cout << "House, At Home, US, TT's\n";
	for (Answer_R<R_TimeTemp> tt : q_TimeTemps) {
		logger() << tt.id() << ": " << tt.rec() << L_endl;
	}	
	
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: US  Ds: MTWTFSS0730a15 0900p18");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "Hous_e   Prg: At HomeZne: US  Ds: MTWTFSS0730a15 0900p18");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "HolApp_t Prg: Occup'dZne: Flt Ds: MTWTFSS0700a20 1100p18");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "Hous_e   Prg: At HomeZne: US  Ds: MTWTFSS0730a15 0900p18");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At Hom_eZne: US  Ds: MTWTFSS0730a15 0900p18");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At Hom_eZne: US  Ds: MTWTFSS0730a15 0900p18");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At Hom_eZne: US  Ds: MTWTFSS0730a15 0900p18");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: U_S  Ds: MTWTFSS0730a15 0900p18");
	display1_h.rec_up_down(-1);
 	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DHW Ds_: MTWTF--0630a45 0900a30     0330p45 1030p30");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DHW Ds_: -----SS0730a45 0930a30     0300p45 1030p30");
	display1_h.rec_left_right(-1);
	display1_h.rec_left_right(-1);
	display1_h.rec_left_right(-1);
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "HolApp_t Prg: Occup'dZne: Flt Ds: MTWTFSS0700a20 1100p18");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "Hous_e   Prg: At HomeZne: US  Ds: MTWTFSS0730a15 0900p18");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "HolApp_t Prg: Occup'dZne: Flt Ds: MTWTFSS0700a20 1100p18");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Prg: Occup'_dZne: Flt Ds: MTWTFSS0700a20 1100p18");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Prg: Occup'dZne: Fl_t Ds: MTWTFSS0700a20 1100p18");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "HolAppt Prg: Occup'dZne: DH_W Ds: MTWTFSS0700a45 1000a30     0400p45 1100p30");
	display1_h.rec_left_right(-1);
	display1_h.rec_left_right(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "HolApp_t Prg: Occup'dZne: DHW Ds: MTWTFSS0700a45 1000a30     0400p45 1100p30");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "Hous_e   Prg: At HomeZne: US  Ds: MTWTFSS0730a15 0900p18");
	display1_h.rec_left_right(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: US  Ds: MTWTFSS0730a15 0900p1_8");
	display1_h.rec_left_right(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: US  Ds: MTWTFSS0730a1_5 0900p18");
	display1_h.rec_left_right(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: US  Ds: MTWTFS_S0730a15 0900p18");
	display1_h.rec_left_right(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: US  Ds_: MTWTFSS0730a15 0900p18");
	display1_h.rec_left_right(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: U_S  Ds: MTWTFSS0730a15 0900p18");
	display1_h.rec_up_down(1);
	display1_h.rec_left_right(1);
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: MTWTF-_-0740a19 1100p16");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M#TWTF--0740a19 1100p16");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M#-WTF--0740a19 1100p16");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M-WTF-_-0740a19 1100p16");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T---S_S0800a19 1050p16");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T#---SS0800a19 1050p16");
	display1_h.rec_left_right(1);
	display1_h.rec_left_right(1);
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T---#SS0800a19 1050p16");
	display1_h.rec_up_down(1);
	
	display1_h.rec_select(); // remove Saturday, create a new profile.

	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T----_S0800a19 1050p16");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -----S_-0700a18");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M-WTF-_-0740a19 1100p16");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M#-WTF--0740a19 1100p16");
	display1_h.rec_left_right(-1); // Steal Sunday from later profile
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M-WTF-#-0740a19 1100p16");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M-WTF-#S0740a19 1100p16");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M-WTF-_S0740a19 1100p16");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T----_-0800a19 1050p16");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -----S_-0700a18");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T----_-0800a19 1050p16");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T#-----0800a19 1050p16");
	display1_h.rec_left_right(-1); 	// Steal Saturday from later profile - deletes that profile
	display1_h.rec_left_right(-1); 
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T---#S-0800a19 1050p16");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T---S_-0800a19 1050p16");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M-WTF-_S0740a19 1100p16");
	display1_h.rec_up_down(1); 
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T---S_-0800a19 1050p16");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M-WTF-_S0740a19 1100p16");
	display1_h.rec_up_down(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T---S_-0800a19 1050p16");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T#---S-0800a19 1050p16");
	display1_h.rec_up_down(1); // Steal Wednesday from earlier profile
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -T#W--S-0800a19 1050p16");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: -TW--S_-0800a19 1050p16");
	display1_h.rec_up_down(-1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M--TF-_S0740a19 1100p16");
	display1_h.rec_left_right(1);
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M--TF-S0740a1_9 1100p16");
	display1_h.rec_select();
	CHECK(test_stream(display1_h.stream(tb)) == "House   Prg: At HomeZne: DS  Ds: M--TF-SDelete Edi_t New     0740a19");
  }
#endif

#ifdef INFO_CONSOLE_PAGES
SCENARIO("InfoConsoleChapters", "[Display]") {
	cout << "\n*********************************\n**** MainConsoleChapters ****\n********************************\n\n";


	using namespace client_data_structures;
	using namespace Assembly;
	using namespace LCD_UI;

	LCD_Display_Buffer<20, 4> lcd;
	UI_DisplayBuffer tb(lcd);
	clock_().setTime({ 31,7,17 }, { 8,10 }, 0);
	unsigned long timeOfReset_mS = 0;

	HeatingSystem hs{};
	setFactoryDefaults(hs.getDB(), 1);

	auto showActive = [](auto ui) {
		for (auto & obj : *ui->get()->collection()) {
			auto fieldData = static_cast<UI_FieldData &>(obj);
			cout << ui_Objects()[(long)&fieldData] << "\tFocus: " << fieldData.focusIndex() << " RecID: " << fieldData.data()->recordID() << endl;
		}
	};

	auto & display1_h = hs.mainConsoleChapters()(1);
	cout << test_stream(display1_h.stream(tb)) << endl;
	clock_().setTime({ 31,7,17 }, { 8,10 }, 0);
	GIVEN("Multi-member iterated collection") {
		CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   Family   51 061 0   Flat     52 062 0");
		display1_h.rec_up_down(1);
		cout << test_stream(display1_h.stream(tb)) << endl;
		CHECK(test_stream(display1_h.stream(tb)) == "UpSt :16 DnSt :16   Flat :16 HsTR :16   EnST :16 FlTR :16   GasF :16 OutS :16>  ");
		display1_h.rec_up_down(1);
		cout << test_stream(display1_h.stream(tb)) << endl;
		CHECK(test_stream(display1_h.stream(tb)) == "Flat   0 FlTR   0   HsTR   0 UpSt   0   MFSt   0 Gas    0   DnSt   0");
		display1_h.rec_up_down(1);
		cout << test_stream(display1_h.stream(tb)) << endl;
		CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   Family   51 061 0   Flat     52 062 0");
		THEN("first row can be scrolled RIGHT with LR") {
			display1_h.rec_left_right(1);
			showActive(display1_h._leftRightBackUI); // 0,0,0 - 0,2,2
			CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuit_e  50 060 0   Family   51 061 0   Flat     52 062 0");
			showActive(display1_h._leftRightBackUI); // 0,0,0 - 2,2,2
			display1_h.rec_left_right(1);
			showActive(display1_h._leftRightBackUI); // 0,0,0 - 2,2,2
			CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  5_0 060 0   Family   51 061 0   Flat     52 062 0");
			showActive(display1_h._leftRightBackUI); // 0,0,0 - 2,2,2
			display1_h.rec_left_right(1);
			CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 06_0 0   Family   51 061 0   Flat     52 062 0");
			showActive(display1_h._leftRightBackUI); // 0,0,0 - 2,2,2
			display1_h.rec_left_right(1);
			CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuit_e  50 060 0   Family   51 061 0   Flat     52 062 0");
			showActive(display1_h._leftRightBackUI); // 0,0,0 - 2,2,2
			AND_THEN("first row can be scrolled LEFT with LR") {
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 06_0 0   Family   51 061 0   Flat     52 062 0");
				showActive(display1_h._leftRightBackUI); // 0,0,0 - 2,2,2
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  5_0 060 0   Family   51 061 0   Flat     52 062 0");
				showActive(display1_h._leftRightBackUI); // 0,0,0 - 2,2,2
				display1_h.rec_left_right(-1);
				CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuit_e  50 060 0   Family   51 061 0   Flat     52 062 0");
				showActive(display1_h._leftRightBackUI); // 0,0,0 - 2,2,2
				AND_THEN("the rows can be scrolled with UD") {
					display1_h.rec_up_down(1);
					CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   Famil_y   51 061 0   Flat     52 062 0");
					showActive(display1_h._leftRightBackUI); // 1,1,1 - 2,2,2			
					display1_h.rec_up_down(1);
					CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   Family   51 061 0   Fla_t     52 062 0");
					showActive(display1_h._leftRightBackUI); // 2,2,2 - 2,2,2
					display1_h.rec_up_down(1);
					CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuit_e  50 060 0   Family   51 061 0   Flat     52 062 0");
					showActive(display1_h._leftRightBackUI); // 0,0,0 - 2,2,2
					AND_THEN("last row can also be scrolled with LR") {
						display1_h.rec_up_down(-1);
						CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   Family   51 061 0   Fla_t     52 062 0");
						showActive(display1_h._leftRightBackUI);
						display1_h.rec_left_right(1);
						CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   Family   51 061 0   Flat     5_2 062 0");
						display1_h.rec_left_right(1);
						showActive(display1_h._leftRightBackUI); // 2,2,2 - 2,2,2
						CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   Family   51 061 0   Flat     52 06_2 0");
						AND_THEN("can scroll to middle row from time field") {
							display1_h.rec_up_down(-1);
							showActive(display1_h._leftRightBackUI); // 1,1,1 - 1,1,1					
							CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   Family   51 06_1 0   Flat     52 062 0");
							showActive(display1_h._leftRightBackUI); // 1,1,1 - 2,2,2					
							AND_THEN("middle row can also be scrolled with LR") {
								display1_h.rec_left_right(-1);
								showActive(display1_h._leftRightBackUI); // 1,2,1 - 2,2,2				
								CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   Family   5_1 061 0   Flat     52 062 0");
								showActive(display1_h._leftRightBackUI); // 1,2,1 - 2,2,2				
								display1_h.rec_left_right(-1);
								CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   Famil_y   51 061 0   Flat     52 062 0");
								showActive(display1_h._leftRightBackUI);
								AND_THEN("middle row name can be edited") {
									display1_h.rec_select();
									CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   #Family|| 51 061 0   Flat     52 062 0");
									display1_h.rec_up_down(1);
									display1_h.rec_select();
									//showActive(display1_h._leftRightBackUI);
									CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   famil_y   51 061 0   Flat     52 062 0");
									AND_THEN("middle row temp can be edited") {
										//showActive(display1_h._leftRightBackUI);
										display1_h.rec_left_right(1);
										//showActive(display1_h._leftRightBackUI); // 1,1,1, - 2,-1,-1
										CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   family   5_1 061 0   Flat     52 062 0");
										//showActive(display1_h._leftRightBackUI); // 1,1,1 - 2,2,2 
										display1_h.rec_select();
										CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   family   5#1 061 0   Flat     52 062 0");
										display1_h.rec_up_down(-1);
										display1_h.rec_up_down(-1);
										display1_h.rec_select();
										CHECK(test_stream(display1_h.stream(tb)) == "Room Temp OnFor ToGoEnSuite  50 060 0   family   5_3 061 0   Flat     52 062 0");
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
#endif

#ifdef TEST_RELAYS

#include "FactoryDefaults.h"
#include "Initialiser.h"
#include "Data_Relay.h"
#include "Data_TempSensor.h"
#include "I2C_Talk.h"

TEST_CASE("HeatingSystem Test Relays & TS", "[HeatingSystem]") {
	cout << "\n*********************************\n**** HeatingSystem Test Relays & TS ****\n********************************\n\n";
	using namespace client_data_structures;
	using namespace Assembly;

	HeatingSystem hs{};
	//HardwareInterfaces::localKeypad = &hs.localKeypad;  // to be defined by the client (required by interrupt handler)

	clock_().setTime({ 31,7,17 }, { 8,10 }, 0);


	auto q_relays = makeTable_Query(hs.db.table<R_Relay>(TB_Relay));
	auto thisRelay = q_relays.firstMatch();
	bool onOff = false;
	while (q_relays.more()) {
		int relayID = thisRelay->id();
		auto relayRec = thisRelay->rec();
		auto relay = relayRec.obj(relayID);
		relay.set(onOff);
		CHECK(relay.logicalState() == onOff);
		cout << thisRelay->rec().name << " Port: " << int(thisRelay->rec().port>>1) << (relay.logicalState() == false ? " Off" : " On") << endl;
		onOff = !onOff;
		thisRelay = q_relays.next(1);
	}
	cout << endl;

	hs.relaysPort.setAndTestRegister();
	thisRelay = q_relays.firstMatch();
	while (q_relays.more()) {
		int relayID = thisRelay->id();
		auto relay = thisRelay->rec().obj(relayID);
		onOff = !onOff;
		CHECK(relay.logicalState() == onOff);
		cout << thisRelay->rec().name << " Port: " << int(thisRelay->rec().port>>1) << (relay.logicalState() == false ? " Off" : " On") << endl;
		thisRelay = q_relays.next(1);
	}

	cout << endl;

	auto q_tempSensors = makeTable_Query(hs.db.table<R_TempSensor>(TB_TempSensor));
	auto thisTempSensor = q_tempSensors.firstMatch();
	while (q_tempSensors.more()) {
		int tempSensorID = thisTempSensor->id();
		CHECK((int)thisTempSensor->rec().obj(tempSensorID).get_temp() == 101);
		cout << thisTempSensor->rec().name << " addr: " << int(thisTempSensor->rec().address) << endl;
		thisTempSensor = q_tempSensors.next(1);
	}

	hs. serviceConsoles();
}
#endif


