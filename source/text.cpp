/* 

dslibris - an ebook reader for the Nintendo DS.

 Copyright (C) 2007-2008 Ray Haleblian

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

#include "text.h"

#include <iostream>
#include <sstream>
#include <sys/param.h>

#include "nds.h"
#include "fat.h"
#include "string.h"

#include "app.h"
#include "main.h"
#include "version.h"

#define PIXELSIZE 12
#define EXPERIMENTAL_KERNING
extern char msg[];
std::stringstream ss;

void Text::CopyScreen(u16 *src, u16 *dst) {
	memcpy(src, dst, display.width * display.height * sizeof(u16));
}

Text::Text()
{
	display.height = PAGE_HEIGHT;
	display.width = PAGE_WIDTH;
	filenames[TEXT_STYLE_REGULAR] = FONTREGULARFILE;
	filenames[TEXT_STYLE_BOLD] = FONTBOLDFILE;
	filenames[TEXT_STYLE_ITALIC] = FONTITALICFILE;
	filenames[TEXT_STYLE_BOLDITALIC] = FONTBOLDITALICFILE;
	filenames[TEXT_STYLE_BROWSER] = FONTBROWSERFILE;

	screenleft = (u16*)SCREENLEFT;
	screenright = (u16*)SCREENRIGHT;
	//offscreen = new u16[display.width * display.height];
	margin.left = MARGINLEFT;
	margin.right = MARGINRIGHT;
	margin.top = MARGINTOP;
	margin.bottom = MARGINBOTTOM;
	bgcolor.r = 15;
	bgcolor.g = 15;
	bgcolor.b = 15;
	usebgcolor = false;
	invert = false;
	justify = false;
	linespacing = 1;
	ftc = false;

	// Rendering state.
	hit = false;
	linebegan = false;
	codeprev = 0;
	bold = false;
	italic = false;
	style = TEXT_STYLE_REGULAR;
	pixelsize = PIXELSIZE;
	screen = screenleft;

	imagetype.face_id = (FTC_FaceID)&face_id;
	imagetype.flags = FT_LOAD_DEFAULT; 
	imagetype.height = pixelsize;
	imagetype.width = 0;

	// Statistics.
	stats_hits = 0;
	stats_misses = 0;

	//ClearScreen(offscreen, 255, 255, 255);
	ss.clear();
}

Text::~Text()
{
	// framebuffers
	//free(offscreen);
	
	// homemade cache
	ClearCache();
	for(std::map<FT_Face, Cache*>::iterator iter = textCache.begin();
		iter != textCache.end(); iter++) {
		delete iter->second;
	}
	textCache.clear();
	
	// FreeType
	for (std::map<u8, FT_Face>::iterator iter = faces.begin(); iter != faces.end(); iter++) {
		FT_Done_Face(iter->second);
	}
	FT_Done_FreeType(library);
}

void Text::UpdateMain() {
	dmaCopyAsynch(screenright, screenright + 256*256, 256*511+128);
}

static FT_Error
TextFaceRequester(    FTC_FaceID   face_id,
                      FT_Library   library,
                      FT_Pointer   request_data,
                      FT_Face     *aface )
{
	TextFace face = (TextFace) face_id;   // simple typecase
	return FT_New_Face( library, face->file_path, face->face_index, aface );
}

FT_Error Text::InitFreeTypeCache(void) {
	//! Use FreeType's cache manager. borken!

	auto error = FT_Init_FreeType(&library);
	if(error) return error;
	error = FTC_Manager_New(library,0,0,0,
		&TextFaceRequester,NULL,&cache.manager);
	if(error) return error;
	error = FTC_ImageCache_New(cache.manager,&cache.image);
	if(error) return error;
	error = FTC_SBitCache_New(cache.manager,&cache.sbit);
	if(error) return error;
	error = FTC_CMapCache_New(cache.manager,&cache.cmap);
	if(error) return error;

	sprintf(face_id.file_path, "%s/%s", app->fontdir.c_str(), filenames[TEXT_STYLE_REGULAR].c_str());
	face_id.face_index = 0;
	error =	FTC_Manager_LookupFace(cache.manager, (FTC_FaceID)&face_id, &faces[TEXT_STYLE_REGULAR]);
	if(error) return error;

	return 0;
}

FT_Error Text::CreateFace(int style) {
	std::string path = app->fontdir + "/" + filenames[style];
	error = FT_New_Face(library, path.c_str(), 0, &faces[style]);
	if (error) halt(path.c_str());
	
	error = FT_Select_Charmap(faces[style], FT_ENCODING_UNICODE);
	if (error) halt("error selecting charmap");

	auto size = pixelsize;
	if (style == TEXT_STYLE_BROWSER) size = 12;

#ifdef REQUEST_SIZE
	// This may provide more accurate sizeing.
	FT_Size_RequestRec request {
		.type = FT_SIZE_REQUEST_TYPE_NOMINAL,
		.width = size << 6,   // 26.6 points
		.height = size << 6,  // 26.6 points
		.horiResolution = 72 << 6, // 72 dpi
		.vertResolution = 72 << 6  // 72 dpi
	};
	FT_Request_Size(face, &request);
#else
	error = FT_Set_Pixel_Sizes(faces[style], 0, size);
	if (error) halt("error setting pixel size");
#endif

	textCache.insert(std::make_pair(faces[style], new Cache()));

	return error;
}

int Text::InitCache(void) {
	//! Use our own cheesey glyph cache.

	FT_Init_FreeType(&library);	
	error |= CreateFace(TEXT_STYLE_BROWSER);
	error |= CreateFace(TEXT_STYLE_REGULAR);
	error |= CreateFace(TEXT_STYLE_ITALIC);
	error |= CreateFace(TEXT_STYLE_BOLD);
	error |= CreateFace(TEXT_STYLE_BOLDITALIC);

	return error;
}

int Text::Init()
{
	if(ftc)
		return InitFreeTypeCache();
	else
		return InitCache();
}

void Text::ReportFace(FT_Face face)
{
	printf("%s\n", face->family_name);
	printf("%s\n", face->style_name);
	printf("faces %ld\n", face->num_faces);
	printf("glyphs %ld\n", face->num_glyphs);
	printf("fixed-sizes %d\n", face->num_fixed_sizes);
	for (int i=0;i<face->num_fixed_sizes;i++)
	{
		printf(" w %d h %d\n",
			face->available_sizes[i].width,
			face->available_sizes[i].height);
	}	
}

void Text::Begin()
{
	bold = false;
	italic = false;
	linebegan = false;
	stats_hits = 0;
	stats_misses = 0;
	hit = false;
}

void Text::End() {}

int Text::CacheGlyph(u32 ucs, FT_Face face)
{
	//! Cache glyph at ucs if there's space.

	//! Does not check if this is a duplicate entry;
	//! The caller should have checked first.

	if(textCache[face]->cacheMap.size() == CACHESIZE) return -1;
#ifdef LCD_VERTICAL
	FT_Load_Char(face, ucs,
		FT_LOAD_RENDER|FT_LOAD_TARGET_LCD);
#else
	FT_Load_Char(face, ucs,
		FT_LOAD_DEFAULT|FT_LOAD_TARGET_LIGHT);
	FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD_V);
#endif
	FT_GlyphSlot src = face->glyph;
	FT_GlyphSlot dst = new FT_GlyphSlotRec;
	int x = src->bitmap.rows;
	int y = abs(src->bitmap.pitch);
	dst->bitmap.buffer = new unsigned char[x*y];
	memcpy(dst->bitmap.buffer, src->bitmap.buffer, x*y);
	dst->bitmap.rows = src->bitmap.rows;
	dst->bitmap.width = src->bitmap.width;
	dst->bitmap_top = src->bitmap_top;
	dst->bitmap_left = src->bitmap_left;
	dst->bitmap.pitch = src->bitmap.pitch;
	dst->advance = src->advance;
	textCache[face]->cacheMap.insert(std::make_pair(ucs, dst));
	return ucs;
}

FT_UInt Text::GetGlyphIndex(u32 ucs)
{
	//! Given a UCS codepoint, return where it is in the charmap, by index.
	if (ftc)
		return FTC_CMapCache_Lookup(cache.cmap,
			(FTC_FaceID)&face_id, -1, ucs);
	else
		return FT_Get_Char_Index(faces[style], ucs);	
}

int Text::GetGlyphBitmap(u32 ucs, FTC_SBit *sbit, FTC_Node *anode)
{
	//! Given a UCS code, fills sbit and anode.
	//! Returns nonzero on error.
	if (!ftc) return 1;
	imagetype.face_id = (FTC_FaceID)&face_id;
	imagetype.height = pixelsize;
#ifdef LCD_VERTICAL
	imagetype.flags = FT_LOAD_RENDER;
#else
	imagetype.flags = FT_LOAD_DEFAULT;
#endif
	return FTC_SBitCache_Lookup(cache.sbit,&imagetype,
		GetGlyphIndex(ucs),sbit,anode);
}

FT_GlyphSlot Text::GetGlyph(u32 ucs, int flags, FT_Face face)
{
	if (ftc) halt("error: GetGlyph() called with ftc enabled");

	std::map<u16,FT_GlyphSlot>::iterator iter = textCache[face]->cacheMap.find(ucs);
	if (iter != textCache[face]->cacheMap.end()) {
		stats_hits++;
		hit = true;
		return iter->second;
	}

	// No cache hit, so load glyph.
	hit = false;
	stats_misses++;
	int i = CacheGlyph(ucs, face);
	if (i >= 0)
		return textCache[face]->cacheMap[ucs];

	// Cache is full, look up directly.
	FT_Load_Char(face, ucs, flags);
#ifdef LCD_VERTICAL
#else
	FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD_V);
#endif
	return face->glyph;
}

void Text::ClearCache()
{
	 for (std::map<u8, FT_Face>::iterator iter = faces.begin(); iter != faces.end(); iter++) {
		 ClearCache(iter->second);
	 }
}

void Text::ClearCache(u8 style)
{
	ClearCache(GetFace(style));
}

void Text::ClearCache(FT_Face face)
{
	for(std::map<u16, FT_GlyphSlot>::iterator iter = textCache[face]->cacheMap.begin();
		iter != textCache[face]->cacheMap.end();
		iter++) {
		delete iter->second;
	}
	textCache[face]->cacheMap.clear();
}

void Text::ClearScreen()
{
	if(invert) memset((void*)screen,0,256*511+128);
	else {
		memset((void*)screen,255,256*511+128);
		
		// alternately, off-white...
		// const u16 pixel = RGB15(29,29,29)|BIT(15);
		// for (int i=0; i<pixelcount; i++) screen[i] = pixel;
	}
}

void Text::ClearScreenSpecial()
{
	u16 clr_A = RGB15(12,7,2)|BIT(15);
	u16 clr_B = RGB15(7,2,0)|BIT(15);

	int halfheight = display.height >> 1;
	int halfwidth = display.width >> 1;

	for(int y = 0; y < display.height; y++){
		for(int x = 0; x < display.width; x++) {
			int a = x + y;
			int b = x - y;
		
			int d = abs(x - halfwidth) + abs(y - halfheight);

			if(d > 70) {
				if((x < halfwidth && y < halfheight) ||
				   (x > halfwidth && y > halfheight)) {
					if(a % 4) {
						screen[y*display.height+x] = clr_A;
					}else {
						screen[y*display.height+x] = clr_B;
					}
				}else {
					if(b % 4) {
						screen[y*display.height+x] = clr_A;
					}else {
						screen[y*display.height+x] = clr_B;
					}
				}
			}else {
				if(!(x % 4 && y % 4)) {
					screen[y*display.height+x] = clr_A;
				}else {
					screen[y*display.height+x] = clr_B;
				}

			}
		}
	}
}


void Text::ClearRect(u16 xl, u16 yl, u16 xh, u16 yh)
{
	u16 clearcolor;
	if(invert) clearcolor = RGB15(0,0,0) | BIT(15);
	else clearcolor = RGB15(31,31,31) | BIT(15);
	for(u16 y=yl; y<yh; y++) {
		for(u16 x=xl; x<xh; x++) {
			screen[y*display.height+x] = clearcolor;
		}
	}
}

u32 Text::GetStringWidth(const char *txt, u8 style)
{
	return GetStringWidth(txt, GetFace(style));
}

u32 Text::GetStringWidth(const char *txt, FT_Face face)
{
	//! Return total advance in pixels.
	u32 width = 0;
	const char *c;
	for(c = txt; c != NULL; c++)
	{
		u32 ucs = 0;
		GetCharCode(c, &ucs);
		width += GetAdvance(ucs, face);
	}
	return (width + 32) / 64;
}

u8 Text::GetCharCountInsideWidth(const char *txt, u8 style, u8 pixels) {
	u8 n = 0;
	u32 ucs = 0;
	u32 width = 0;
	for(const char *c = txt; *c != 0 && n <= strlen(txt);)
	{
		c += GetCharCode(c, &ucs);
		if (ucs == 0) continue;
		width += GetAdvance(ucs, faces[style]);
		if (width > 64 * pixels) return n;
		n++;
	}
	return n;
}

u8 Text::GetCharCode(const char *utf8, u32 *ucs) {
	//! Given a UTF-8 encoding, fill in the Unicode/UCS code point.
	//! Return the bytelength of the encoding, for advancing
	//! to the next character; 0 if encoding could not be translated.
	
	if (utf8[0] < 0x80) { // ASCII
		*ucs = utf8[0];
		return 1;

	} else if (utf8[0] > 0xc1 && utf8[0] < 0xe0) { // latin
		*ucs = ((utf8[0]-192)*64) + (utf8[1]-128);
		return 2;

	} else if (utf8[0] > 0xdf && utf8[0] < 0xf0) { // asian
		*ucs = (utf8[0]-224)*4096 + (utf8[1]-128)*64 + (utf8[2]-128);
		return 3;

	} else if (utf8[0] > 0xef) {
		*ucs = ((utf8[0] & 0x07) << 18) |
			((utf8[1] & 0x3F) << 12) |
			((utf8[2] & 0x3F) << 6) |
			(utf8[3] & 0x3F);
		return 4;
	}
	return 0;
}

std::string Text::GetFontName(u8 style) {
	return
		std::string(faces[style]->family_name)
		+ " "
		+ std::string(faces[style]->style_name);
}

u32 Text::GetHeight() {
	return (GetFace(style)->size->metrics.height);
}

void Text::GetPen(u16 *x, u16 *y) {
	*x = pen.x;
	*y = pen.y;
}

void Text::SetPen(u16 x, u16 y) {
	pen.x = x;
	pen.y = y;
}

void Text::GetPen(u16 &x, u16 &y) {
	x = pen.x;
	y = pen.y;
}

void Text::SetInvert(bool state) {
	invert = state;
}

bool Text::GetInvert() {
	return invert;
}

u32 Text::GetPenX() {
	return pen.x;
}

u32 Text::GetPenY() {
	return pen.y;
}

u8 Text::GetPixelSize()
{
	return pixelsize;
}

u16* Text::GetScreen()
{
	return screen;
}

void Text::SetPixelSize(u8 size)
{
	if (size == pixelsize) return;
	pixelsize = size;
	if (ftc)
	{
		imagetype.height = pixelsize;
		imagetype.width = pixelsize;
	}
	else
	{
		for (auto& it : faces)
		{
			// UI font stays at 12.
			if (it.first == TEXT_STYLE_BROWSER) continue;
			FT_Set_Pixel_Sizes(it.second, 0, pixelsize);
			ClearCache(it.second);
		}
	}
}

void Text::SetScreen(u16 *inscreen)
{
	screen = inscreen;
}

u32 Text::GetAdvance(u32 ucs, FT_Face face) {
	//! Return glyph advance in pixels.

	if (ftc)
	{
		auto gindex = GetGlyphIndex(ucs);

		FTC_SBit sbit;
		error = FTC_SBitCache_Lookup(cache.sbit, &imagetype, gindex, &sbit, NULL);
		if (!error) return sbit->xadvance * 64;

		FT_Glyph glyph;
		error = FTC_ImageCache_Lookup(cache.image, &imagetype, gindex, &glyph, NULL);
		if (!error) return (glyph->advance).x;
	}
	else
	{
#ifdef ADVANCE_NO_CACHE
		// Much slower, maybe less buggy.
		auto gindex = FT_Get_Char_Index(face, ucs);
		error = FT_Load_Glyph(face, gindex, FT_LOAD_DEFAULT);
		if (!error) return face->glyph->advance.x;
#else
		return GetGlyph(ucs, FT_LOAD_DEFAULT, face)->advance.x;
#endif
	}
	return 0;
}

int Text::GetStringAdvance(const char *s) {
	int advance = 0;
	for(unsigned int i=0;i<strlen(s);i++) {
		advance += GetAdvance(s[i]);
	}
	return advance;
}

bool Text::GetFontName(std::string &s) {
	const char *name = FT_Get_Postscript_Name(GetFace(style));
	if(!name)
		return false;
	else {
		s = name;
		return true;
	}
}

void Text::InitPen(void) {
	pen.x = (margin.left) * 64;
	pen.y = (margin.top) * 64 + GetHeight();
}

void Text::PrintChar(u32 ucs)
{
	PrintChar(ucs, GetFace(style));
}

void Text::PrintChar(u32 ucs, u8 astyle) {
	PrintChar(ucs, GetFace(astyle));
}

void Text::PrintChar(u32 ucs, FT_Face face) {
	// Draw a character for the given UCS codepoint,
	// into the current screen buffer at the current pen position.

	u16 bx, by, width, height, pitch = 0;
	FT_Byte *buffer = NULL;
	FT_UInt advance = 0;
	FTC_Node anode = nullptr;
	FT_Glyph glyph;

	if(ftc)
	{
	    auto glyph_index = FTC_CMapCache_Lookup(cache.cmap, (FTC_FaceID)&face_id, -1, ucs);
		// TODO set imagetype here

		FTC_SBit p = &sbit;
  		error = FTC_SBitCache_Lookup(cache.sbit,
                            	&imagetype,
								glyph_index,
                                &p,
                                &anode );
		if (error) return;

		// there will typically be no bitmap, only an outline
		if (!p)
		{
			error = FTC_ImageCache_Lookup(cache.image, &imagetype, glyph_index, &glyph, &anode);
			if (error) return;
		}
		// TODO rasterize bitmap

		buffer = sbit.buffer;
		bx = sbit.left;
		by = sbit.top;
		height = sbit.height;
		width = sbit.width;
		pitch = sbit.pitch;
		advance = sbit.xadvance * 64;
	}
	else
	{
		// Consult the cache for glyph data and cache it on a miss, if space is available.
		FT_GlyphSlot glyph = GetGlyph(ucs, FT_LOAD_RENDER|FT_LOAD_TARGET_LCD, face);

  		// extract glyph image
		FT_Bitmap bitmap = glyph->bitmap;
		bx = glyph->bitmap_left;
		by = glyph->bitmap_top;
		width = bitmap.width;
		height = bitmap.rows;
		pitch = bitmap.pitch;
		advance = glyph->advance.x;
		buffer = bitmap.buffer;
	}

#ifdef EXPERIMENTAL_KERNING
	// Fetch a kerning vector.
	if(codeprev) {
		FT_Vector kerning_vector;
		error = FT_Get_Kerning(face, GetGlyphIndex(codeprev), GetGlyphIndex(ucs), FT_KERNING_DEFAULT, &kerning_vector);
		if (error) halt("error getting kerning vector");

		//move pen by kerning vector
		pen.x += kerning_vector.x;
	}
#endif

	// Render to framebuffer.

#ifdef DRAW_PEN_POSITION
	// Mark the pen position.
	screen[pen.y*display.height+pen.x] = RGB15(0, 0, 0) | BIT(15);
#endif
#ifdef LCD_VERTICAL

	u8 offset = (u32(pen.x) % 64) / 22; //0, 1, or 2. number of subpixels to shift by.
	u16 gx, gy;
	for (gy=0; gy<height; gy++)
	{
		for (gx=0; gx<width/3; gx++)
		{
			//supersampling rather than rgb-ing. maybe ill do both in the future
			u8 ab = offset>0 ? 0 : buffer[gy*pitch+gx*3];
			u8 ag = offset>1 ? 0 : buffer[gy*pitch+gx*3+1 - offset];
			u8 ar = buffer[gy*pitch+gx*3+2 - offset];
			if (!(ab|ag|ar)) continue;
			//if (!invert) a = 256 - a;

			u16 sx = (pen.x/64+gx+bx);
			u16 sy = (pen.y/64+gy-by);
			u16 layerBottom = screen[sy*display.height+sx];
			int r,g,b;
			//DS seems to use BGR not RGB
			r = 0x1F & (layerBottom >> 0);
			g = 0x1F & (layerBottom >> 5);
			b = 0x1F & (layerBottom >> 10);

			if(invert) {
				b+=(int)ab>>3;
				g+=(int)ag>>3;
				r+=(int)ar>>3;
			}else {
				b-=(int)ab>>3;
				g-=(int)ag>>3;
				r-=(int)ar>>3;
			}

			r = r<0 ? 0 : r;
			g = g<0 ? 0 : g;
			b = b<0 ? 0 : b;
			
			r = r>31 ? 31 : r;
			g = g>31 ? 31 : g;
			b = b>31 ? 31 : b;

			u16 pixel = RGB15(r,g,b) | BIT(15);
			
#ifdef DRAW_CACHE_MISSES
			// if(!hit) pixel = RGB15(a>>3,0,0) | BIT(15);
#endif
			
			screen[sy*display.height+sx] = pixel;

			if (offset > 0){
				//supersampling rather than rgb-ing. maybe ill do both in the future

				ab = offset<=0 ? 0 : buffer[gy*pitch+gx*3+3 - offset];
				ag = offset<=1 ? 0 : buffer[gy*pitch+gx*3+4 - offset];
				ar = 0;

				if (!(ab|ag|ar)) continue;
				//if (!invert) a = 256 - a;

				sx = (pen.x/64+gx+bx+1);
				sy = (pen.y/64+gy-by);
				layerBottom = screen[sy*display.height+sx];
				int r,g,b;
				//DS seems to use BGR not RGB
				r = 0x1F & (layerBottom >> 0);
				g = 0x1F & (layerBottom >> 5);
				b = 0x1F & (layerBottom >> 10);
			
				
				if(invert) {
					b+=(int)ab>>3;
					g+=(int)ag>>3;
					r+=(int)ar>>3;
				}else {
					b-=(int)ab>>3;
					g-=(int)ag>>3;
					r-=(int)ar>>3;
				}
				r = r<0 ? 0 : r;
				g = g<0 ? 0 : g;
				b = b<0 ? 0 : b;
				
				r = r>31 ? 31 : r;
				g = g>31 ? 31 : g;
				b = b>31 ? 31 : b;

				pixel = RGB15(r,g,b) | BIT(15);
				
				screen[sy*display.height+sx] = pixel;
			}
		}
	}

	pen.x += advance;
	codeprev = ucs;

	if (ftc && anode)
		FTC_Node_Unref(anode, cache.manager);
#else
    u8 offset = (u32(pen.y) % 64) / 22; //0, 1, or 2. number of subpixels to shift by.
	u16 gx, gy;
	for (gy=0; gy<height/3; gy++)
	{
		for (gx=0; gx<width; gx++)
		{
			//supersampling rather than rgb-ing. maybe ill do both in the future
			u8 ar = offset>0 ? 0 : buffer[(gy*3)*pitch+gx];
			u8 ag = offset>1 ? 0 : buffer[(gy*3+1 - offset)*pitch+gx];
			u8 ab = buffer[(gy*3+2 - offset)*pitch+gx];
			if (!(ab|ag|ar)) continue;
			//if (!invert) a = 256 - a;

			u16 sx = (pen.x/64+gx+bx);
			u16 sy = (pen.y/64+gy-by);
			u16 layerBottom = screen[sy*display.height+sx];
			int r,g,b;
			//DS seems to use BGR not RGB
			r = 0x1F & (layerBottom >> 0);
			g = 0x1F & (layerBottom >> 5);
			b = 0x1F & (layerBottom >> 10);

			if(invert) {
				b+=(int)ab>>3;
				g+=(int)ag>>3;
				r+=(int)ar>>3;
			}else {
				b-=(int)ab>>3;
				g-=(int)ag>>3;
				r-=(int)ar>>3;
			}

			r = r<0 ? 0 : r;
			g = g<0 ? 0 : g;
			b = b<0 ? 0 : b;
			
			r = r>31 ? 31 : r;
			g = g>31 ? 31 : g;
			b = b>31 ? 31 : b;

			u16 pixel = RGB15(r,g,b) | BIT(15);
			
#ifdef DRAW_CACHE_MISSES
			// if(!hit) pixel = RGB15(a>>3,0,0) | BIT(15);
#endif
			
			screen[sy*display.height+sx] = pixel;

			if (offset > 0){
				//supersampling rather than rgb-ing. maybe ill do both in the future

				ar = offset<=0 ? 0 : buffer[(gy*3+3 - offset)*pitch+gx];
				ag = offset<=1 ? 0 : buffer[(gy*3+4 - offset)*pitch+gx];
				ab = 0;

				if (!(ab|ag|ar)) continue;
				//if (!invert) a = 256 - a;

				sx = (pen.x/64+gx+bx+1);
				sy = (pen.y/64+gy-by);
				layerBottom = screen[sy*display.height+sx];
				int r,g,b;
				//DS seems to use BGR not RGB
				r = 0x1F & (layerBottom >> 0);
				g = 0x1F & (layerBottom >> 5);
				b = 0x1F & (layerBottom >> 10);
			
				
				if(invert) {
					b+=(int)ab>>3;
					g+=(int)ag>>3;
					r+=(int)ar>>3;
				}else {
					b-=(int)ab>>3;
					g-=(int)ag>>3;
					r-=(int)ar>>3;
				}
				r = r<0 ? 0 : r;
				g = g<0 ? 0 : g;
				b = b<0 ? 0 : b;
				
				r = r>31 ? 31 : r;
				g = g>31 ? 31 : g;
				b = b>31 ? 31 : b;

				pixel = RGB15(r,g,b) | BIT(15);
				
				screen[sy*display.height+sx] = pixel;
			}
		}
	}

	pen.x += advance;
	codeprev = ucs;

	if (ftc && anode)
		FTC_Node_Unref(anode, cache.manager);    
#endif

}

bool Text::PrintNewLine(void) {
	//! Render a newline at the current position.
	pen.x = (margin.left) * 64;
	int height = GetHeight();
	int y = pen.y + height * linespacing;
	if (y > 64 * (display.height - margin.bottom)) {
		if (screen == screenleft)
		{
			screen = screenright;
			pen.y = 64 * margin.top + height;
			return true;
		}
		else
			return false;
	}
	else
	{
		pen.y += height * linespacing;
		return true;
	}
}

void Text::PrintString(const char *s) {
	//! Render a character string starting at the pen position.
	PrintString(s, style);
}

void Text::PrintString(const char *s, u8 style) {
	//! Render a character string starting at the pen position.
	PrintString(s, GetFace(style));
}

void Text::PrintString(const char *s, FT_Face face) {
	//! Render a character string starting at the pen position.
	u8 i=0;
	while(i<strlen((char*)s)) {
		u32 c = s[i];
		if (c == '\n') {
			PrintNewLine();
			i++;
		} else {
			i+=GetCharCode(&(s[i]),&c);
			PrintChar(c, face);
		}
	}
}

void Text::ClearScreen(u16 *screen, u8 r, u8 g, u8 b)
{
	for (int i=0;i<display.height*display.height;i++)
		screen[i] = RGB15(r,g,b) | BIT(15);
}

void Text::PrintSplash(u16 *screen)
{
	// push
	auto s = GetScreen();

	SetScreen(screen);
	drawstack(screen);
	// pop
	SetScreen(s);
}

void Text::SetFontFile(const char *path, u8 style)
{
	if (!strcmp(filenames[style].c_str(), path)) return;
	filenames[style] = std::string(path);
	CreateFace(style);
}

std::string Text::GetFontFile(u8 style)
{
	return filenames[style];
}
