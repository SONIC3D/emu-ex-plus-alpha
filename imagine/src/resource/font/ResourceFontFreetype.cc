/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#define LOGTAG "ResFontFreetype"
#include <imagine/resource/font/ResourceFontFreetype.hh>
#include <imagine/gfx/Gfx.hh>
#include <imagine/util/strings.h>
#include <imagine/io/FileIO.hh>
#ifdef CONFIG_PACKAGE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

#ifdef CONFIG_PACKAGE_FONTCONFIG
static CallResult getFontFilenameWithPattern(FcPattern *pat, char *&filename, FcPattern *&matchPatOut)
{
	FcDefaultSubstitute(pat);
	if(!FcConfigSubstitute(nullptr, pat, FcMatchPattern))
	{
		logErr("error applying font substitutions");
		FcPatternDestroy(pat);
		return INVALID_PARAMETER;
	}
	FcResult result = FcResultMatch;
	auto matchPat = FcFontMatch(nullptr, pat, &result);
	FcPatternDestroy(pat);
	if(!matchPat || result == FcResultNoMatch)
	{
		logErr("fontconfig couldn't find a valid font");
		if(matchPat)
			FcPatternDestroy(matchPat);
		return INVALID_PARAMETER;
	}
	if(!FcPatternGetString(matchPat, FC_FILE, 0, (FcChar8**)&filename) == FcResultMatch)
	{
		logErr("fontconfig font missing file path");
		FcPatternDestroy(matchPat);
		return INVALID_PARAMETER;
	}
	matchPatOut = matchPat;
	return OK;
}

static bool addonSystemFontContainingChar(ResourceFontFreetype &font, int c)
{
	logMsg("looking for font with char: %c", c);
	auto pat = FcPatternCreate();
	if(!pat)
	{
		logErr("error allocating fontconfig pattern");
		return false;
	}
	auto charSet = FcCharSetCreate();
	FcCharSetAddChar(charSet, c);
	FcPatternAddCharSet(pat, FC_CHARSET, charSet);
	char *filename;
	FcPattern *matchPat;
	if(getFontFilenameWithPattern(pat, filename, matchPat) != OK)
	{
		FcCharSetDestroy(charSet);
		return false;
	}
	auto ret = font.loadIntoNextSlot((char*)filename);
	FcPatternDestroy(matchPat);
	FcCharSetDestroy(charSet);
	return ret == OK;
}
#endif

ResourceFontFreetype *ResourceFontFreetype::load()
{
	auto inst = new ResourceFontFreetype;
	if(!inst)
	{
		logErr("out of memory");
		return nullptr;
	}
	return inst;
}

ResourceFontFreetype *ResourceFontFreetype::loadWithIoWithName(GenericIO io, const char *name)
{
	if(!io)
		return nullptr;
	//logMsg("loadWithIoWithName");
	auto inst = new ResourceFontFreetype;
	if(!inst)
	{
		logErr("out of memory");
		return nullptr;
	}

	//logMsg("fontData_open");
	if(inst->f[0].open(std::move(io)) != OK)
	{
		logErr("error reading font");
		inst->free();
		return nullptr;
	}

	//logMsg("setting io addr %p", &i_io);

	if(inst->initWithName(name) != OK)
	{
		inst->free();
		return nullptr;
	}

	inst->usedCharSlots = 1;
	return inst;
}

ResourceFontFreetype *ResourceFontFreetype::load(GenericIO io)
{
	//logMsg("loadWithIo");
	return loadWithIoWithName(std::move(io), nullptr);
}

ResourceFontFreetype *ResourceFontFreetype::load(const char *name)
{
	//if(string_hasDotExtension(name, "ttf"))
	{
		//logMsg("suffix matches TT Font");
		FileIO io;
		io.open(name);
		if(!io)
		{
			logMsg("unable to open file");
			return nullptr;
		}
		return ResourceFontFreetype::loadWithIoWithName(io, name);
	}

	return nullptr;
}

ResourceFontFreetype *ResourceFontFreetype::loadAsset(const char *name)
{
	return load(openAppAssetIO(name));
}

CallResult ResourceFontFreetype::loadIntoSlot(GenericIO io, uint slot)
{
	if(f[slot].isOpen())
		f[slot].close(1);
	if(f[slot].open(std::move(io)) != OK)
	{
		logErr("error reading font");
		return IO_ERROR;
	}
	usedCharSlots = slot+1;
	return OK;
}

CallResult ResourceFontFreetype::loadIntoSlot(const char *name, uint slot)
{
	FileIO io;
	io.open(name);
	if(!io)
	{
		logMsg("unable to open file %s", name);
		return IO_ERROR;
	}
	auto res = loadIntoSlot(io, slot);
	if(res != OK)
	{
		return res;
	}
	return OK;
}

CallResult ResourceFontFreetype::loadIntoNextSlot(const char *name)
{
	if(usedCharSlots == MAX_FREETYPE_SLOTS)
		return NO_FREE_ENTRIES;
	return loadIntoSlot(name, usedCharSlots);
}

void ResourceFontFreetype::free()
{
	iterateTimes(MAX_FREETYPE_SLOTS, i)
	{
		f[i].close(1);
	}
	delete this;
}

void ResourceFontFreetype::setMetrics(const FreetypeFontData &fontData, GlyphMetrics &metrics)
{
	metrics.xSize = fontData.charBitmapWidth();
	metrics.ySize = fontData.charBitmapHeight();
	metrics.xOffset = fontData.getCurrentCharBitmapLeft();
	metrics.yOffset = fontData.getCurrentCharBitmapTop();
	metrics.xAdvance = fontData.getCurrentCharBitmapXAdvance();
}

IG::Pixmap ResourceFontFreetype::charBitmap()
{
	return f[currCharSlot].accessCharBitmap();
}

CallResult ResourceFontFreetype::activeChar(int idx, GlyphMetrics &metrics)
{
	//logMsg("active char: %c", idx);
	iterateTimes(usedCharSlots, i)
	{
		auto &font = f[i];
		if(!font.isOpen())
			continue;
		auto res = font.setActiveChar(idx);
		if(res != OK)
		{
			logMsg("glyph 0x%X not found in slot %d", idx, i);
			continue;
		}
		setMetrics(font, metrics);
		//logMsg("set metrics for index %d: %dx%d, %d %d %d", idx, metrics.xSize, metrics.ySize, metrics.xOffset, metrics.yOffset, metrics.xAdvance);
		currCharSlot = i;
		return OK;
	}

	#ifdef CONFIG_PACKAGE_FONTCONFIG
	// try to find a font with the missing char and load into next free slot
	if(usedCharSlots != MAX_FREETYPE_SLOTS && addonSystemFontContainingChar(*this, idx))
	{
		assert(activeFontSizeData);
		uint newSlot = usedCharSlots-1;
		auto &font = f[newSlot];
		if(font.newSize(activeFontSizeData->settings.pixelWidth, activeFontSizeData->settings.pixelHeight,
			&activeFontSizeData->size[newSlot]) != OK)
		{
			logErr("couldn't allocate font size");
			return NOT_FOUND;
		}
		auto res = font.setActiveChar(idx);
		if(res != OK)
		{
			logMsg("glyph 0x%X still not found", idx);
			return NOT_FOUND;
		}
		setMetrics(font, metrics);
		//logMsg("set metrics for index %d: %dx%d, %d %d %d", idx, metrics.xSize, metrics.ySize, metrics.xOffset, metrics.yOffset, metrics.xAdvance);
		currCharSlot = newSlot;
		return OK;
	}
	#endif

	return NOT_FOUND;
}

/*int ResourceFontFreetype::currentFaceDescender () const //+
{ return f.maxDescender(); }
int ResourceFontFreetype::currentFaceAscender () const //+
{ return f.maxAscender(); }*/

CallResult ResourceFontFreetype::newSize(const FontSettings &settings, FontSizeRef &sizeRef)
{
	freeSize(sizeRef);
	auto sizeData = new FontSizeData(settings);
	if(!sizeData)
	{
		logErr("couldn't allocate size data");
		return OUT_OF_MEMORY;
	}
	// create FT_Size objects for slots in use
	iterateTimes(usedCharSlots, i)
	{
		if(!f[i].isOpen())
		{
			//sizeRef.size[i] = nullptr;
			continue;
		}
		auto res = f[i].newSize(settings.pixelWidth, settings.pixelHeight, &sizeData->size[i]);
		if(res != OK)
		{
			// TODO: cleanup already allocated sizes
			return res;
		}
	}
	sizeRef.ptr = sizeData;
	return OK;
}

CallResult ResourceFontFreetype::applySize(FontSizeRef &sizeRef)
{
	auto &sizeData = *((FontSizeData*)sizeRef.ptr);
	iterateTimes(usedCharSlots, i)
	{
		if(!sizeData.size[i])
			continue;
		auto res = f[i].applySize(sizeData.size[i]);
		if(res != OK)
		{
			return res;
		}
	}
	activeFontSizeData = (FontSizeData*)sizeRef.ptr;
	return OK;
}

void ResourceFontFreetype::freeSize(FontSizeRef &sizeRef)
{
	if(!sizeRef.ptr)
		return;
	auto &sizeData = *((FontSizeData*)sizeRef.ptr);
	iterateTimes(usedCharSlots, i)
	{
		if(sizeData.size[i])
			f[i].freeSize(sizeData.size[i]);
	}
	if(&sizeData == activeFontSizeData)
		activeFontSizeData = nullptr;
	delete ((FontSizeData*)sizeRef.ptr);
	sizeRef.ptr = nullptr;
}
