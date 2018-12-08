#include "Console.h"
#include "I_Keypad.h"
#include "DisplayBuffer.h"
#include "..\LCD_UI\A_Top_UI.h"

namespace HardwareInterfaces {
	using namespace LCD_UI;

	Console::Console(I_Keypad & keyPad, DisplayBuffer_I & lcd_buffer, A_Top_UI & pageGenerator) :
		_keyPad(keyPad)
		, _lcd_UI(lcd_buffer)
		, _pageGenerator(pageGenerator)
	{}

	bool Console::processKeys() {
		bool doRefresh;
		auto keyPress = _keyPad.getKey();
		do {
			//Serial.print("Main processKeys: "); Serial.println(keyPress);
			doRefresh = true;
			switch (keyPress) {
			case 0:
				//Serial.println("GotKey Info");
				//newPage(mp_infoHome);
				break;
			case 1:
				//Serial.println("GotKey UP");
				_pageGenerator.rec_up_down(1);
				break;
			case 2:
				//Serial.println("GotKey Left");
				_pageGenerator.rec_left_right(-1);
				break;
			case 3:
				//Serial.println("GotKey Right");
				_pageGenerator.rec_left_right(1);
				break;
			case 4:
				//Serial.println("GotKey Down");
				_pageGenerator.rec_up_down(-1);
				break;
			case 5:
				//Serial.println("GotKey Back");
				_pageGenerator.rec_prevUI();
				break;
			case 6:
				//Serial.println("GotKey Select");
				_pageGenerator.rec_select();
				break;
			default:
				doRefresh = _keyPad.isTimeToRefresh();
#if defined (NO_TIMELINE) && defined (ZPSIM)
				{static bool test = true;// for testing, prevent refresh after first time through unless key pressed
				doRefresh = test;
				test = false;
				}
#endif
			}
			if (doRefresh) {
				//Edit::checkTimeInEdit(keyPress);
				//U4_byte	lastTick = micros();
				_pageGenerator.stream(_lcd_UI);
				_lcd_UI._lcd->sendToDisplay();
				//Serial.print("Main processKeys: streamToLCD took: "); Serial.println((micros()-lastTick)/1000);
			}
			keyPress = _keyPad.getKey();
		} while (keyPress != -1);
		return doRefresh;
	}
}
