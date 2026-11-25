
#include "FreeTypeFontRasterizer.h"
#include "LayerBitmapIntf.h"
#include "FreeType.h"
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include "MsgIntf.h"
#include "FontSystem.h"
#include <complex>

extern void TVPUninitializeFreeFont();
extern FontSystem* TVPFontSystem;
extern const ttstr &TVPGetDefaultFontName();
tFreeTypeFace *FreeTypeFontRasterizer::GetFallbackFace()
{
	const ttstr &name = TVPGetDefaultFontName();
	if (Face->GetFontName() == name) return nullptr;
	if (!FaceFallback || FaceFallback->GetFontName() != name) {
		auto *next = new tFreeTypeFace(name, 0);
		delete FaceFallback;
		FaceFallback = next;
	}
	FaceFallback->SetHeight(Face->GetHeight());
	for (auto flag : {TVP_TF_ITALIC, TVP_TF_BOLD, TVP_TF_UNDERLINE, TVP_TF_STRIKEOUT,
	                  TVP_FACE_OPTIONS_NO_ANTIALIASING, TVP_FACE_OPTIONS_NO_HINTING}) {
		if (Face->GetOption(flag)) FaceFallback->SetOption(flag);
		else FaceFallback->ClearOption(flag);
	}
	return FaceFallback;
}

FreeTypeFontRasterizer::FreeTypeFontRasterizer() : RefCount(0), Face(NULL), LastBitmap(NULL) {
	AddRef();
}
FreeTypeFontRasterizer::~FreeTypeFontRasterizer() {
	if( Face ) delete Face;
	Face = NULL;
	if (FaceFallback) {
		delete FaceFallback;
		FaceFallback = nullptr;
	}
	TVPUninitializeFreeFont();
}
void FreeTypeFontRasterizer::AddRef() {
	RefCount++;
}
//---------------------------------------------------------------------------
void FreeTypeFontRasterizer::Release() {
	RefCount--;
	LastBitmap = NULL;
	if( RefCount == 0 ) {
		if( Face ) delete Face;
		Face = NULL;
		if (FaceFallback) {
			delete FaceFallback;
			FaceFallback = nullptr;
		}
		delete this;
	}
}
//---------------------------------------------------------------------------
void FreeTypeFontRasterizer::ApplyFont( class tTVPNativeBaseBitmap *bmp, bool force ) {
	if( bmp != LastBitmap || force ) {
		ApplyFont( bmp->GetFont() );
		LastBitmap = bmp;
	}
}
//---------------------------------------------------------------------------
void FreeTypeFontRasterizer::ApplyFont( const tTVPFont& font ) {
	ttstr stdname = TVPFontSystem->GetBeingFont(font.Face);
	// TVP_FACE_OPTIONS_NO_ANTIALIASING
	// TVP_FACE_OPTIONS_NO_HINTING
	// TVP_FACE_OPTIONS_FORCE_AUTO_HINTING
	tjs_uint32 opt = 0;
	opt |= (font.Flags & TVP_TF_ITALIC) ? TVP_TF_ITALIC : 0;
	opt |= (font.Flags & TVP_TF_BOLD) ? TVP_TF_BOLD : 0;
	opt |= (font.Flags & TVP_TF_UNDERLINE) ? TVP_TF_UNDERLINE : 0;
	opt |= (font.Flags & TVP_TF_STRIKEOUT) ? TVP_TF_STRIKEOUT : 0;
	opt |= (font.Flags & TVP_TF_FONTFILE) ? TVP_FACE_OPTIONS_FILE : 0;
	bool recreate = false;
	if( Face ) {
		if( Face->GetFontName() != stdname ) {
			delete Face;
			Face = new tFreeTypeFace( stdname, opt );
			recreate = true;
		}
	} else {
		Face = new tFreeTypeFace( stdname, opt );
		recreate = true;
	}
	Face->SetHeight( font.Height < 0 ? -font.Height : font.Height );
	if( recreate == false ) {
		if( font.Flags & TVP_TF_ITALIC ) {
			Face->SetOption(TVP_TF_ITALIC);
		} else {
			Face->ClearOption(TVP_TF_ITALIC);
		}
		if( font.Flags & TVP_TF_BOLD ) {
			Face->SetOption(TVP_TF_BOLD);
		} else {
			Face->ClearOption(TVP_TF_BOLD);
		}
		if( font.Flags & TVP_TF_UNDERLINE ) {
			Face->SetOption(TVP_TF_UNDERLINE);
		} else {
			Face->ClearOption(TVP_TF_UNDERLINE);
		}
		if( font.Flags & TVP_TF_STRIKEOUT ) {
			Face->SetOption(TVP_TF_STRIKEOUT);
		} else {
			Face->ClearOption(TVP_TF_STRIKEOUT);
		}
	}
	LastBitmap = NULL;
}
//---------------------------------------------------------------------------
void FreeTypeFontRasterizer::GetTextExtent(tjs_char ch, tjs_int &w, tjs_int &h) {
	tGlyphMetrics metrics{};
	if (Face && LoadWithFallback(ch, [&](tFreeTypeFace *face, tjs_char code) {
		return face->GetGlyphSizeFromCharcode(code, metrics);
	})) {
		w = metrics.CellIncX;
		h = metrics.CellIncY;
	} else {
		w = h = Face ? Face->GetHeight() : 0;
	}
}
//---------------------------------------------------------------------------
tjs_int FreeTypeFontRasterizer::GetAscentHeight() {
	if( Face ) return Face->GetAscent();
	return 0;
}
static bool isUnicodeSpace(char16_t ch)
{
	return  (ch >= 0x0009 && ch <= 0x000D) || ch == 0x0020 || ch == 0x0085 || ch == 0x00A0 || ch == 0x1680
		|| (ch >= 0x2000 && ch <= 0x200A) || ch == 0x2028 || ch == 0x2029 || ch == 0x202F
		|| ch == 0x205F || ch == 0x3000;
}

// All three callers try the same faces and replacement characters. Return
// the successful face so bounds and pixels can share the primary baseline.
template <typename LoadGlyph>
tFreeTypeFace *FreeTypeFontRasterizer::LoadWithFallback(tjs_char ch, LoadGlyph load) {
	if (load(Face, ch)) return Face;
	if (!isUnicodeSpace(ch)) {
		auto *fallback = GetFallbackFace();
		if (fallback && load(fallback, ch)) return fallback;
	}
	if (load(Face, Face->GetDefaultChar()) || load(Face, Face->GetFirstChar()))
		return Face;
	return nullptr;
}

//---------------------------------------------------------------------------
tTVPCharacterData* FreeTypeFontRasterizer::GetBitmap( const tTVPFontAndCharacterData & font, tjs_int aofsx, tjs_int aofsy ) {
	if( font.Antialiased ) {
		Face->ClearOption( TVP_FACE_OPTIONS_NO_ANTIALIASING );
	} else {
		Face->SetOption( TVP_FACE_OPTIONS_NO_ANTIALIASING );
	}
	if( font.Hinting ) {
		Face->ClearOption( TVP_FACE_OPTIONS_NO_HINTING );
		//Face->SetOption( TVP_FACE_OPTIONS_FORCE_AUTO_HINTING );
	} else {
		Face->SetOption( TVP_FACE_OPTIONS_NO_HINTING );
		//Face->ClearOption( TVP_FACE_OPTIONS_FORCE_AUTO_HINTING );
	}
	tTVPCharacterData *data = nullptr;
	auto *used = LoadWithFallback(font.Character, [&](tFreeTypeFace *face, tjs_char code) {
		data = face->GetGlyphFromCharcode(code);
		return data != nullptr;
	});
	if (!used) TVPThrowExceptionMessage(TVPFontRasterizeError);
	if (used != Face) data->OriginY += Face->GetAscent() - used->GetAscent();

	int cx = data->Metrics.CellIncX;
	int cy = data->Metrics.CellIncY;
	if( font.Font.Angle == 0 ) {
		data->Metrics.CellIncX = cx;
		data->Metrics.CellIncY = 0;
	} else if(font.Font.Angle == 2700) {
		data->Metrics.CellIncX = 0;
		data->Metrics.CellIncY = cx;
	} else {
		double angle = font.Font.Angle * (M_PI/1800);
		data->Metrics.CellIncX = static_cast<tjs_int>(  std::cos(angle) * cx);
		data->Metrics.CellIncY = static_cast<tjs_int>(- std::sin(angle) * cx);
	}

	data->Antialiased = font.Antialiased;
	data->FullColored = false;
	data->Blured = font.Blured;
	data->BlurWidth = font.BlurWidth;
	data->BlurLevel = font.BlurLevel;
	data->OriginX += aofsx; // for vertical text
//	data->OriginY += aofsy;

	// apply blur
	if(font.Blured) data->Blur(); // nasty ...
	return data;
}
//---------------------------------------------------------------------------
void FreeTypeFontRasterizer::GetGlyphDrawRect( const ttstr & text, tTVPRect& area ) {
	// アンチエイリアスとヒンティングは有効にする
	Face->ClearOption( TVP_FACE_OPTIONS_NO_ANTIALIASING );
	Face->ClearOption( TVP_FACE_OPTIONS_NO_HINTING );

	area.left = area.top = area.right = area.bottom = 0;
	tjs_int offsetx = 0;
	tjs_uint len = text.length();
	for( tjs_uint i = 0; i < len; i++ ) {
		tjs_char ch = text[i];
		tjs_int ax, ay;
		tTVPRect rt(0,0,0,0);
		auto *used = LoadWithFallback(ch, [&](tFreeTypeFace *face, tjs_char code) {
			return face->GetGlyphRectFromCharcode(rt, code, ax, ay);
		});
		if (used) {
			rt.add_offsets(offsetx, Face->GetAscent() - used->GetAscent());
			if( i != 0 ) {
				area.do_union( rt );
			} else {
				area = rt;
			}
		}
		offsetx += ax;
	}
}

