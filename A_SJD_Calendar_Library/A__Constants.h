#if !defined (CONSTANTS__INCLUDED_)
#define CONSTANTS__INCLUDED_

#include "debgSwitch.h"
#include <Arduino.h>
#include "A__Typedefs.h"
#include "../MultiCrystal/MultiCrystal.h"

//#include <MemoryFree.h>

#define LOCAL_INT_PIN 18
#define RTC_RESET 4
#define REF_ANALOGUE 3

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  //Code in here will only be compiled if an Arduino Mega is used.
  #define KEYINT 5
#elif defined(__SAM3X8E__)
  #define KEYINT LOCAL_INT_PIN
#endif

extern long actualLoopTime;
extern long mixCheckTime;
extern long zoneCheckTime;
extern long storeCheckTime;



////////////////// Program Version /////////////////////
const short EEPROMsize = 4096;
#define VERSION  "07.6" // max size 6 characters. #defined to allow concatenation of adjacent literals
#define VERSION_SIZE 6
////////////////// EVENT CODES ////////////////////////
extern bool temp_sense_hasError;
//const S1_err TEMP_SENS_ERR_TEMP		= -30;

const S1_err ERR_PORTS_FAILED		= 1;
const S1_err ERR_READ_VALUE_WRONG	= 2;
const S1_err ERR_I2C_RESET			= 3;
const S1_err ERR_I2C_SPEED_FAIL		= 4;
const S1_err ERR_I2C_READ_FAILED	= 5;
const S1_err ERR_I2C_SPEED_RED_FAILED	= 6;
const S1_err EVT_I2C_SPEED_RED		= 7;
const S1_err ERR_MIX_ARD_FAILED		= 8;
const S1_err ERR_OP_NOT_LAT			= 9;
const S1_err EVT_THS_COND_CHANGE	= 10;
const S1_err EVT_TEMP_REQ_CHANGED	= 11;
const S1_err ERR_TEMP_SENS_READ		= 12;
const S1_err ERR_RTC_FAILED			= 13;
const S1_err EVT_GAS_TEMP			= 15;

//////////////////////////////////////////////////////
enum modes {e_onChange_read_LATafter, e_onChange_read_OUTafter, e_alwaysWrite_read_LATafter, e_alwaysCheck_read_OUTafter, e_alwaysCheck_read_latBeforeWrite, e_alwaysCheck_read_OutBeforeWrite, noOfSwModes};

////////////////////////// I2C Setup /////////////////////////////////////
//  I2C device address is 0 0 1 0   0 A2 A1 A0
//  Mine are at 010 0000 and 010 0111
// IO Device Addresses

const U1_byte I2C_MASTER_ADDR = 0x11;
const U1_byte MIX_VALVE_I2C_ADDR = 0x10;
const U1_byte I2C_DATA_PIN = 10;
const U1_byte RTC_ADDRESS = 0x68;
const U1_byte EEPROM_ADDRESS = 0x50; 
const U1_byte DS_REMOTE_ADDRESS = 0x26;
const U1_byte FL_REMOTE_ADDRESS = 0x25;
const U1_byte US_REMOTE_ADDRESS = 0x24;
const U1_byte REM_DISPL_ADDR[] = {0x24, 0x25, 0x26};
const U1_byte IO8_PORT_OptCoupl = 0x20;
const U1_byte IO8_PORT_MixValves =0x27;
const U1_byte IO8_PORT_OptCouplActive = 0;
const U1_byte IO8_PORT_MixValvesActive = 0;
const U1_byte MIX_VALVE_USED = 128|8|2|1;

// registers
const U1_byte REG_8PORT_IODIR = 0x00;
const U1_byte REG_8PORT_PullUp = 0x06;
const U1_byte REG_8PORT_OPORT = 0x09;
const U1_byte REG_8PORT_OLAT = 0x0A;
const U1_byte REG_16PORT_INTCAPA = 0x10;
const U1_byte REG_16PORT_GPIOA = 0x12; 

const U1_byte DS75LX_Temp = 0x00; // two bytes must be read. Temp is MS 9 bits, in 0.5 deg increments, with MSB indicating -ve temp.
const U1_byte DS75LX_Config = 0x01;
const U1_byte DS75LX_HYST_REG = 0x02;
const U1_byte DS75LX_LIMIT_REG = 0x03;

// Pin assignments & misc.
const S1_byte ZERO_CROSS_PIN = -15; // -ve = active falling edge.
const S1_byte RESET_OUT_PIN = -14;  // -ve = active low.
const S1_byte RESET_LED_PIN_P = 16;
const S1_byte RESET_LED_PIN_N = 19;
const U1_byte NO_OF_RETRIES = 3; //0xFF;
const uint16_t ZERO_CROSS_DELAY = 690;
/////////////////// Mixing Valves ///////////////////////////////
const S2_byte VALVE_FULL_TRANSIT_TIME = 140;
const U1_byte VALVE_WAIT_TIME = 40;

//////////////////// Display ////////////////////////////////
const U1_byte SECONDS_ON_VERSION_PAGE = 5;
const char NEW_LINE_CHR = '~'; // character used to indicate this field to start on a new line
const char CONTINUATION = '_'; // character used to indicate where a field can split over lines
const U1_byte DISPLAY_WAKE_TIME = 60;
const U1_byte MAX_LINE_LENGTH = 20;
const U1_byte MAX_NO_OF_ROWS = 4;
const U1_byte ADDITIONAL_BUFFER_SPACE = 3; // 1st char is cursor position, allow for space following string, last is a null.
const U1_byte DISPLAY_BUFFER_LENGTH = (MAX_LINE_LENGTH + ADDITIONAL_BUFFER_SPACE) * MAX_NO_OF_ROWS;
extern char SPACE_CHAR;// = ' ';  // temporarily made extern to Collection_Utilities.cpp so it can be redefined by gtest
const char LIST_LEFT_CHAR = '<';
const char LIST_RIGHT_CHAR = '>';
const char EDIT_SPC_CHAR = '|';
const U1_byte MIN_BL = 125;
const U1_byte MAX_BL = 255;
const S1_byte EDIT_TIME = 60;

////////////////////// Relay testing order ///////////////////
//								  0		 1		 2		3	   4	  5		6			
//name[RelayCnt][RELAY_N+1] =	{"Flat","FlTR",	"HsTR","UpSt","MFSt","Gas","DnSt"};
//port[RelayCnt] =			    { 6,	 1,		 0,		5,	   2,	  3,	4};

const U1_ind RELAY_ORDER[] = {1,2,4,5,6,3,0}; // relay indexes

////////////////////// Thermal Store Time-Temps ///////////////////
const U1_byte THERM_STORE_HYSTERESIS = 5;
const U1_byte TT_SHORTEN_TIME = 20;
const U1_byte TT_LOWER_TEMP = 5;
const U1_byte TT_NEW_TT_LENGTH = 10;
const U1_byte TT_PROXIMITY = 30; // should be more than TT_NEW_TT_LENGTH

////////////////////// Zones ///////////////////
const U1_byte MIN_ON_TEMP = 10;
const U1_byte MAX_REQ_TEMP = 80;
const U1_byte FLOW_TEMP_SENSITIVITY = 32; // no of degrees flow temp changes per degree room-temp error
const U1_byte MIN_FLOW_TEMP = 30; // Flow temp below which pump turns off.
const S2_byte MAX_MIN_TEMP_ERROR = 256;

#if defined (ZPSIM)
const long AVERAGING_PERIOD = 100; // seconds
#else
const long AVERAGING_PERIOD = 600; // seconds
#endif 

////////////////////// Towel Rails ///////////////////
const U1_byte TOWEL_RAIL_FLOW_TEMP = 50;
const U1_byte TWL_RAD_RISE_TIME = 10; // seconds
const U1_byte TWL_RAD_RISE_TEMP = 5; // degrees

////////////////////// Argument Constants ///////////////////
const S1_byte NOT_A_LIST = -2;
const S1_byte LIST_NOT_ACTIVE = -1;
const S1_newPage DO_EDIT = -1;
const S1_newPage VIEWED_DT = -2;
const S1_byte CHECK_FROM = -2;
const S1_byte CURRENT_DT = -3;
const U1_byte SPARE_TT = 200;
// Binary OR'ed flags set in Field Count to indicate field mode.
const U1_byte HIDDEN = 1;	// %2=1
const U1_byte NEW_LINE = 2;   // &2=1		
const U1_byte SELECTABLE = 4;	
const U1_byte CHANGEABLE = 8;
const U1_byte EDITABLE = 16;

////////////////// Strings //////////////////
const char NEW_DP_NAME[] = "New Prog";

extern MultiCrystal * mainLCD;
extern MultiCrystal * hallLCD;
extern MultiCrystal * usLCD;
extern MultiCrystal * flatLCD;
extern bool processReset;

void iniPrint(char * msg);
void iniPrint(char * msg, U4_byte val);
void logToSD();
void logToSD(const char * msg);
void logToSD(const char * msg, long val);
void logToSD_notime(const char * msg, long val = 0);
void logToSD(const char * msg, long val, const char * name, long val2 = 0xFFFFABCD);
void logToSD(const char * msg, long index, long call, long autocall=0, long offset=0, long outside=0, long setback=0, long req=0, int temp=0, bool isHeating=0);

//////////// here to avoid circular header dependancy
enum noItemsVals	{noDisplays, noTempSensors, noRelays, noMixValves, noOccasionalHts, noZones, noTowelRadCcts, noAllDPs, noAllDTs, noAllTTs,NO_COUNT};

enum StartAddr {INI_ST, INFO_ST, EVENT_ST,
				NO_OF_ST		= 6,
				NO_OF_DTA		= NO_OF_ST + 2, // = 8
				THM_ST_ST		= NO_OF_DTA + NO_COUNT, // = 18
				DISPLAY_ST		= THM_ST_ST+2,
				RELAY_ST		= DISPLAY_ST+2, // = 22
				MIXVALVE_ST		= RELAY_ST+2,
				TEMP_SNS_ST		= MIXVALVE_ST+2,
				OCC_HTR_ST		= TEMP_SNS_ST+2,
				ZONE_ST			= OCC_HTR_ST+2,
				TWLRAD_ST		= ZONE_ST+2, // = 32
				DATE_TIME_ST	= TWLRAD_ST+2,
				DAYLY_PROG_ST	= DATE_TIME_ST+2, // =36
				TIME_TEMP_ST	= DAYLY_PROG_ST+2}; // = 38

/* Globals
Name	Meaning									Declared			Initialised
rtc		I2C_Helper for Real-Time_Clock/EEprom	Setup_Loop			setup()
i2C		I2C_Helper for Relays/Temp Sensors		Relay_Run			Factory::initialiseProgrammer()
mainLCD	MultiCrystal for LCD					A__Constants		A__Constants
f		factoryObjects							D_Factory			createFactObject()
*/
/* Helper macros */

#define HEX__(n) 0x##n##LU
#define B8__(x) ((x&0x0000000FLU)?1:0) \
+((x&0x000000F0LU)?2:0) \
+((x&0x00000F00LU)?4:0) \
+((x&0x0000F000LU)?8:0) \
+((x&0x000F0000LU)?16:0) \
+((x&0x00F00000LU)?32:0) \
+((x&0x0F000000LU)?64:0) \
+((x&0xF0000000LU)?128:0)

/* User macros */
#define B8(d) ((unsigned char)B8__(HEX__(d)))
/*
const U1_byte UpDnChar[8] = {
	B00100,
	B01010,
	B10101,
	B00100,
	B00100,
	B10101,
	B01010,
	B00100
};

const U1_byte flameChar[8] = {
	B01000,
	B01100,
	B01101,
	B11101,
	B11011,
	B01111,
	B01110,
	B00110
};

const U1_byte cmdChar[8] = {
	B00000,
	B00000,
	B01111,
	B01111,
	B01111,
	B01111,
	B00000,
	B00000
};

*/












#endif