#include <cache.h>

void NewCache::Init(std::unordered_map<u8, FT_Face> faces, int pixelsize){
	std::unordered_map<u8, FT_Face>::iterator iter;
	for (iter = faces.begin(); iter != faces.end(); iter++) {
		FT_Set_Pixel_Sizes(iter->second, 0, pixelsize);
		textCache.insert(std::make_pair(iter->second, new Cache()));
	}
}

NewCache::~NewCache(){
	for(std::unordered_map<FT_Face, Cache*>::iterator iter = textCache.begin();
		iter != textCache.end(); iter++) {
		delete iter->second;
	}
	textCache.clear();
}

void NewCache::ClearFace(FT_Face face){
	std::unordered_map<u16, FT_GlyphSlot>::iterator iter;   
	for(iter = textCache[face]->cacheMap.begin(); iter != textCache[face]->cacheMap.end(); iter++) {
		delete iter->second;
	}

	textCache[face]->cacheMap.clear();	
}

void NewCache::LoadIn(FT_Face face, u32 ucs, FT_GlyphSlot glyph){
	textCache[face]->cacheMap.insert(std::make_pair(ucs, glyph));
}

bool NewCache::LoadOut(FT_Face face, u32 ucs, FT_GlyphSlot * glyph){
	std::unordered_map<u16,FT_GlyphSlot>::iterator iter = textCache[face]->cacheMap.find(ucs);
	
	if (iter != textCache[face]->cacheMap.end()) {
		//stats_hits++;
		//hit = true;
		*glyph = iter->second; 
		return true;
	}else{
		return false;
	}

}
