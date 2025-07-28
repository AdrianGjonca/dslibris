#ifndef _cache_h
#define _cache_h

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_CACHE_H
#include <unordered_map>
#include <nds.h>

class Cache {
public:
	std::unordered_map<u16, FT_GlyphSlot> cacheMap;
};


class NewCache{
private:
	std::unordered_map<FT_Face, Cache*> textCache;
public:
	void LoadIn(FT_Face face, u32 ucs, FT_GlyphSlot glyph);
	bool LoadOut(FT_Face face, u32 ucs, FT_GlyphSlot * glyph);
	void ClearFace(FT_Face face);
	FT_GlyphSlot LoadOut(FT_Face face, u32 ucs);

	void Init(std::unordered_map<u8, FT_Face> faces, int pixelsize);
	~NewCache();
};

#endif
