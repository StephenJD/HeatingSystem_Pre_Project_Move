#include "LocalDisplay.h"
#include <Logging.h>
#include "A__Constants.h"
#include <MemoryFree.h>

using namespace RelationalDatabase;
extern uint8_t BRIGHNESS_PWM;
extern uint8_t CONTRAST_PWM;
extern uint8_t PHOTO_ANALOGUE;

namespace HardwareInterfaces {

	LocalDisplay::LocalDisplay(RelationalDatabase::Query * query) : LCD_Display_Buffer(query), _lcd(localLCDpins().pins)
	{
		_lcd.begin(20, 4);
		setBackLight(true);
		logger() << F("\nLocalDisplay Constructed");
	}

	void LocalDisplay::blinkCursor(bool isAwake) {
		if (cursorMode() == e_selected) {
			if (!_wasBlinking) { // show cursor immediatly when selecting page
				//logger() << F("page selected\n");
				_wasBlinking = true;
				_cursorOn = false;
			}
			_cursorOn = !_cursorOn;
			if (_cursorOn) _lcd.cursor(); else _lcd.noCursor();
		} else _wasBlinking = false;
	}


	void LocalDisplay::sendToDisplay() {
		//logger() << F(" *** sendToDisplay() *** ") << buff() << L_endl;
		_lcd.clear();
		//delay(200);
		//logger() << buff());
		///0////////9/////////9/////////9/////////9/////////9/////////9///////////////////////////////////
		// 03:59:50 pm         Sat 12-Jan-2019     DST Hours: 0        Backlight Contrast
		_lcd.print(buff());
		_lcd.setCursor(cursorCol(), cursorRow());
		switch (cursorMode()) {
		case e_unselected: _lcd.noCursor(); _lcd.noBlink(); break;
		case e_selected:
#ifdef ZPSIM
			_lcd.cursor();
#endif
			_lcd.noBlink();
			break;
		case e_inEdit:  _lcd.noCursor(); _lcd.blink(); break;
		default:;
		}
	}

	uint8_t LocalDisplay::ambientLight() const { // return 5th root of photo-cell reading * 10. Returns 15 (bright) to 38 (dim).
		uint16_t photo = analogRead(PHOTO_ANALOGUE);
		return uint8_t((1024 - photo) >> 2);
	}

	void LocalDisplay::setBackLight(bool wake) {
		//logger() << F("SetBackLight Query set:") << (long)_query << L_endl;
		if (_query) {
			auto displayDataRS = _query->begin();
			Answer_R<R_Display> displayDataA = *_query->begin();
			R_Display displayData = displayDataA.rec();
			uint16_t minBL = displayData.backlight_dim;  // val 0-25
			uint8_t blRange = displayData.backlight_bright - minBL;// val 0-25
			uint8_t minPhoto = displayData.photo_dim; // val 5th root of Dim photo-cell reading * 10, typically = 40
			uint8_t photoRange = displayData.photo_bright;// val 5th root of Bright photo-cell reading * 10, typically = 20
			photoRange = (minPhoto > photoRange) ? minPhoto - photoRange : photoRange - minPhoto;
			//logger() << F("Backlight_dim: ") << minBL << F(" Backlight_bright: ") << displayData.backlight_bright <<  F(" Contrast: ") << displayData.contrast << L_endl;

			float lightFactor = photoRange > 1 ? float(ambientLight() - minPhoto) / photoRange : 1.f;
			uint16_t brightness = uint16_t(minBL + blRange * lightFactor); // compensate for light
			if (!wake) brightness = (brightness + minBL) / 2; // compensate for sleep

			brightness = (brightness * (MAX_BL - MIN_BL) / 25) + MIN_BL; // convert to analogue write val.
			if (brightness > MAX_BL) brightness = MAX_BL;
			if (brightness < MIN_BL) brightness = MIN_BL;

			analogWrite(BRIGHNESS_PWM, brightness); // Useful range is 255 (bright) to 200 (dim)
			analogWrite(CONTRAST_PWM, displayData.contrast * 3); // Useful range is 0 (max) to 70 (min)
		}
	}

	void LocalDisplay::changeContrast(int changeBy) {
		// range 0-25
		if (_query) {
			Answer_R<R_Display> displayData = *_query->begin();
			int contrast = displayData.rec().contrast + changeBy * 4;
			if (contrast > 0 && contrast < 70) {
				displayData.rec().contrast = uint8_t(contrast);
				displayData.update();
				logger() << F("\n\tChange Contrast : ") << displayData.rec().contrast;
			}
			setBackLight(true);
		}
	}
	
	void LocalDisplay::changeBacklight(int changeBy) {
		// range 0-25
		if (_query) {
			logger() << F("\nChange Backlight ChangeBy : ") << changeBy;
			Answer_R<R_Display> displayData = *_query->begin();
			auto ambient = ambientLight();
			auto midAmbient = (displayData.rec().photo_bright + displayData.rec().photo_dim) / 2;

			if (ambient > midAmbient) {
				uint8_t backLight = displayData.rec().backlight_bright - changeBy;
				if (backLight > 0 && backLight < 25) {
					displayData.rec().backlight_bright = backLight;
				}
				displayData.rec().photo_bright = ambient;
				logger() << F("\n\tChange Bright Backlight : ") << displayData.rec().backlight_bright;
				logger() << F("\n\tPhoto Bright : ") << displayData.rec().photo_bright;
			}
			else {
				uint8_t backLight = displayData.rec().backlight_dim - changeBy;
				if (backLight > 0 && backLight < 25) {
					displayData.rec().backlight_dim = backLight;
				}
				uint8_t	dsBrightPhotoVal = displayData.rec().photo_bright;
				//if (ambient > dsBrightPhotoVal) { // swap bright/dim photo vals
				//	logger() << F("\tPrevious was dimmer! Swap bright/dim photo vals") << L_endl;
				//	uint8_t temp = ambient;
				//	ambient = dsBrightPhotoVal;
				//	dsBrightPhotoVal = temp;
				//	displayData.rec().photo_bright = dsBrightPhotoVal;
				//}
				displayData.rec().photo_dim = ambient;
				logger() << F("\n\tChange Dim Backlight : ") << displayData.rec().backlight_dim;
				logger() << F("\n\tPhoto Dim : ") << displayData.rec().photo_dim;
			}
			displayData.update();
			setBackLight(true);
		}
	}
}