/* 

dslibris - an ebook reader for the Nintendo DS.

 Copyright (C) 2007-2008 Ray Haleblian (ray23@sourceforge.net)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*/

#include <nds.h>
#include <stdio.h>
#include "button.h"

Button::Button() {}

Button::Button(Text *t)
{
	Init(t);
}

void Button::Init(Text *typesetter) {
	origin.x = 0;
	origin.y = 0;
	extent.x = 192;
	extent.y = 34;
	style = BUTTON_STYLE_BOOK;
	text.pixelsize = 12;
	text.style = TEXT_STYLE_BROWSER;
	text1.clear();
	text2.clear();
	ts = typesetter;
}

void Button::Label(const char *s) {
	std::string str = s;
	SetLabel(str);
}

void Button::SetLabel1(std::string s) {
	text1 = s;
}

void Button::SetLabel2(std::string s) {
	text2 = s;
}

void Button::Move(u16 x, u16 y) {
	origin.x = x;
	origin.y = y;
}

void Button::Resize(u16 x, u16 y) {
	extent.x = x;
	extent.y = y;
}

void Button::Draw(u16 *screen, bool highlight, bool ui) {
	// push state
	auto save_screen = ts->GetScreen();
	auto save_style = ts->GetStyle();

	u16 x, y;
	coord_t ul, lr;
	ul.x = origin.x;
	ul.y = origin.y;
	lr.x = origin.x + extent.x;
	lr.y = origin.y + extent.y;
	int w = ts->display.height;  // no really

	if (screen == nullptr) screen = ts->screen;

	ts->SetScreen(screen);
	ts->SetStyle(text.style);

	for (y=ul.y+3;y<lr.y-3;y++) {
		int g = 100 - 100 * (y - ul.y) / (lr.y - ul.y);
		
		if(highlight) g = 100 - g;

		int gi = (31-5) + g * 5 / 100;
		int gi_d2 = gi >> 1;

		for (x=ul.x+2;x<lr.x-2;x++) {
			if(highlight)
				screen[y*w + x] = RGB15(gi_d2-1,gi-2,gi_d2-3)|BIT(15);
			else if(!ui)
				screen[y*w + x] = RGB15(gi-2,gi_d2-1,gi_d2-3)|BIT(15);
			else 
				screen[y*w + x] = RGB15(gi-2, gi-2, gi)|BIT(15);

		}
	}
	
	u16 bordercolor = RGB15(20,10,10)|BIT(15);
	if(ui) {
		bordercolor = RGB15(8,8,8)|BIT(15);
	}
	if(highlight) {
		bordercolor = RGB15(3,3,3)|BIT(15);
	}
	for (int x=ul.x+2;x<lr.x-2;x++) {
		screen[(ul.y+3)*w + x] = bordercolor;
		screen[(lr.y-3)*w + x] = bordercolor;
	}
	for (int y=ul.y+4;y<lr.y-3;y++) {
		screen[y*w + ul.x+1] = bordercolor;
		screen[y*w + lr.x-2] = bordercolor;
	}

	
	if (text1.length()) {
		ts->SetPen((ul.x+6) * 64, (ul.y) * 64+ts->GetHeight());
		u8 len = ts->GetCharCountInsideWidth(text1.c_str(),
			text.style, lr.x-ul.x-4);
		ts->PrintString(text1.substr(0, len).c_str(),
			text.style);
	}

	if (text2.length()) {
		ts->SetPen((ul.x+6) * 64, ts->GetPenY()+ts->GetHeight());
		ts->PrintString(text2.c_str(), text.style);
	}

	// pop state
	ts->SetScreen(save_screen);
	ts->SetStyle(save_style);
}

bool Button::EnclosesPoint(u16 x, u16 y)
{
	if (x > origin.x && 
		y > origin.y && 
		x < origin.x + extent.x && 
		y < origin.y + extent.y) return true;
	return false;
}
