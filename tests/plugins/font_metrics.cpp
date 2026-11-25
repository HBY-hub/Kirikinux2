// SPDX-License-Identifier: AGPL-3.0-only
// Exercise the production TextRender, FreeType and character bitmap paths.
// Only the host font registry, file streams and script host are supplied here.
#include "ncbind.hpp"
#include "FreeTypeFontRasterizer.h"
#include "FontSystem.h"
#include "FontImpl.h"
#include "ComplexRect.h"
#include "MsgIntf.h"
#include "UtilStreams.h"
#include "tvpgl.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

static tTJS *engine;
static std::vector<char> primaryBytes, fallbackBytes;
static const ttstr primary(TJS_W("Primary"));
static const ttstr fallback(TJS_W("Fallback"));
static FreeTypeFontRasterizer rasterizer;

FontSystem::FontSystem() = default;
ttstr FontSystem::GetBeingFont(ttstr name) { return name == primary ? primary : fallback; }
static FontSystem fontSystem;
FontSystem *TVPFontSystem = &fontSystem;
const ttstr &TVPGetDefaultFontName() { return fallback; }
TVPFontNamePathInfo *TVPFindFont(const ttstr &) { return nullptr; } // both files use face 0
tTJSBinaryStream *TVPCreateFontStream(const ttstr &face) {
    const auto &data = face == primary ? primaryBytes : fallbackBytes;
    return new tTVPMemoryStream(data.data(), data.size());
}
bool TVPEncodeUTF8ToUTF16(ttstr &out, const std::string &in) {
    out = ttstr(in);
    return true;
}

void TVPAddLog(const ttstr &message) { std::cerr << message.AsStdString() << '\n'; }
FontRasterizer *GetCurrentRasterizer() { return &rasterizer; }
iTJSDispatch2 *TVPGetScriptDispatch() { return engine->GetGlobal(); }
ttstr TVPFormatMessage(const tjs_char *msg, const ttstr &) { return ttstr(msg); }
ttstr TVPFormatMessage(const tjs_char *msg, const ttstr &, const ttstr &) { return ttstr(msg); }
void TVPThrowExceptionMessage(const tjs_char *msg) { TJS_eTJSError(msg); }
void TVPThrowExceptionMessage(const tjs_char *msg, const ttstr &arg) { TJS_eTJSError(ttstr(msg) + arg); }
tTJSMessageHolder TVPCannotOpenFontFile(nullptr, TJS_W("Cannot open font"), false);
tTJSMessageHolder TVPFontCannotBeUsed(nullptr, TJS_W("Cannot use font"), false);
tTJSMessageHolder TVPFontRasterizeError(nullptr, TJS_W("Rasterization failed"), false);

// Optional TVPGL operations from CharacterData.cpp. This test draws unblurred 8-bit glyphs.
decltype(TVPBLExpand1BitTo8BitPal) TVPBLExpand1BitTo8BitPal;
decltype(TVPChBlurMulCopy65) TVPChBlurMulCopy65;
decltype(TVPChBlurMulCopy) TVPChBlurMulCopy;
decltype(TVPChBlurCopy65) TVPChBlurCopy65;
decltype(TVPChBlurCopy) TVPChBlurCopy;

static bool checkFallback() {
    tTVPFontAndCharacterData request{};
    request.Character = u'ｶ';
    request.Antialiased = request.Hinting = true;
    request.Font = {24, 0, 0, fallback};
    rasterizer.ApplyFont(request.Font);
    tTVPRect expected;
    rasterizer.GetGlyphDrawRect(ttstr(TJS_W("ｶ")), expected);
    const int fallbackAscent = rasterizer.GetAscentHeight();
    request.Font.Face = primary;
    rasterizer.ApplyFont(request.Font);
    expected.add_offsets(0, rasterizer.GetAscentHeight() - fallbackAscent);
    int width = 0, height = 0;
    rasterizer.GetTextExtent(request.Character, width, height);
    tTVPRect actual;
    rasterizer.GetGlyphDrawRect(ttstr(TJS_W("ｶ")), actual);
    auto *bitmap = rasterizer.GetBitmap(request, 0, 0);
    const bool ok = width == 12 && bitmap->Metrics.CellIncX == width &&
        actual == expected && bitmap->OriginY == actual.top && bitmap->BlackBoxX > 0;
    std::cout << "Half-width kana: measured " << width << ", drawn " << bitmap->Metrics.CellIncX
              << ", bounds y=" << actual.top << ", bitmap y=" << bitmap->OriginY << '\n';
    bitmap->Release();
    return ok;
}

static bool checkTextRender() {
    engine = new tTJS();
    ncbAutoRegister::AllRegist();
    if (!ncbAutoRegister::LoadModule(TJS_W("TextRender.dll"))) return false;
    // Measure the post-switch width independently, including bold synthesis.
    rasterizer.ApplyFont(tTVPFont{24, TVP_TF_BOLD, 0, fallback});
    int expected, height;
    rasterizer.GetTextExtent(u'i', expected, height);
    engine->ExecScript(ttstr(TJS_W(
        "var r = new TextRenderBase(); r.defaultFace='Primary'; r.setRenderSize(200,100);"
        "r.render('i%fFallback;%b1ii',0,0,1,false); r.done(); var chars=r.getCharacters(0,0);")));
    tTJSVariant width, step;
    engine->EvalExpression(ttstr(TJS_W("chars[1].cw")), &width);
    engine->EvalExpression(ttstr(TJS_W("chars[2].x - chars[1].x")), &step);
    std::cout << "Switched font: measured " << width.AsInteger() << ", step " << step.AsInteger()
              << ", expected " << expected << '\n';
    const bool switched = width.AsInteger() == expected && step.AsInteger() == expected;

    // A half-width kana and a full-width kana exactly fill a 36px line.
    // The second instance also changes the shared rasterizer between calls.
    engine->ExecScript(ttstr(TJS_W(
        "r.setRenderSize(36,100); r.render('ｶ',0,0,1,false);"
        "var other = new TextRenderBase(); other.defaultFontSize=48; other.setRenderSize(200,200);"
        "r.render('カｶ',0,0,1,false); r.done(); chars=r.getCharacters(0,0);")));
    tTJSVariant wrapped;
    engine->EvalExpression(ttstr(TJS_W(
        "chars.count == 3 && chars[0].cw == 12 && chars[1].cw == 24 &&"
        "chars[1].x == 12 && chars[1].y == 0 && chars[2].x == 0 && chars[2].y > 0")), &wrapped);
    if (!wrapped.AsInteger()) std::cerr << "Unexpected mixed-width line break\n";
    return switched && wrapped.AsInteger();
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    std::ifstream first(argv[1], std::ios::binary), second(argv[2], std::ios::binary);
    primaryBytes.assign(std::istreambuf_iterator<char>(first), {});
    fallbackBytes.assign(std::istreambuf_iterator<char>(second), {});
    if (primaryBytes.empty() || fallbackBytes.empty()) {
        std::cerr << "Unable to read font fixtures: " << argv[1] << ", " << argv[2] << '\n';
        return 2;
    }
    try {
        bool fallbackOK = checkFallback();
        return checkTextRender() && fallbackOK ? 0 : 1;
    } catch (const eTJS &error) {
        std::cerr << error.GetMessage().AsStdString() << '\n';
        return 1;
    }
}
