#pragma once
#include "..\HardwareInterfaces\LCD_Display.h"

namespace LCD_UI {

	using CursorMode = HardwareInterfaces::LCD_Display::CursorMode;

	class UI_DisplayBuffer { // cannot derive from LCD_Display, because it needs to be a polymorphic pointer
	public:
		enum ListStatus { e_showingAll, e_notShowingStart, e_not_showingEnd = e_notShowingStart * 2, e_this_not_showing = e_not_showingEnd * 2 };
		
		UI_DisplayBuffer(HardwareInterfaces::LCD_Display & displayBuffer) : _lcd(&displayBuffer) {}
		
		//Queries
		const char * toCStr() const { return _lcd->buff(); }
		CursorMode cursorMode() { return _lcd->cursorMode(); }
		int cursorPos() { return _lcd->cursorPos(); }
		//bool requestingNewLine() {return toCStr()[strlen(toCStr()) - 1] == '~';}
		// Modifiers
		bool hasAdded(const char * stream, CursorMode cursorMode, int cursorOffset, int endPos, ListStatus listStatus);
		void newLine();
		void reset() {_lcd->reset();}
		void truncate(int newEnd) { _lcd->truncate(newEnd); }
		HardwareInterfaces::LCD_Display * _lcd;
	};
}