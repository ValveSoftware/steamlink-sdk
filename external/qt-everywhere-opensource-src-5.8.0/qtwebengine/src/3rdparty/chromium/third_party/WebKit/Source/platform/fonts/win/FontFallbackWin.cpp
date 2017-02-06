/*
 * Copyright (c) 2006, 2007, 2008, 2009, 2010, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/win/FontFallbackWin.h"

#include "platform/fonts/AcceptLanguagesResolver.h"
#include "platform/fonts/FontCache.h"
#include "SkFontMgr.h"
#include "SkTypeface.h"
#include "wtf/HashMap.h"
#include "wtf/StringExtras.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"
#include <limits>
#include <unicode/uchar.h>

namespace blink {

namespace {

static inline bool isFontPresent(const UChar* fontName, SkFontMgr* fontManager)
{
    String family = fontName;
    SkTypeface* typeface;
    typeface = fontManager->matchFamilyStyle(family.utf8().data(), SkFontStyle());

    if (!typeface)
        return false;

    RefPtr<SkTypeface> tf = adoptRef(typeface);
    SkTypeface::LocalizedStrings* actualFamilies = tf->createFamilyNameIterator();
    bool matchesRequestedFamily = false;
    SkTypeface::LocalizedString actualFamily;
    while (actualFamilies->next(&actualFamily)) {
        if (equalIgnoringCase(family, AtomicString::fromUTF8(actualFamily.fString.c_str()))) {
            matchesRequestedFamily = true;
            break;
        }
    }
    actualFamilies->unref();

    return matchesRequestedFamily;
}

// A simple mapping from UScriptCode to family name. This is a sparse array,
// which works well since the range of UScriptCode values is small.
typedef const UChar* ScriptToFontMap[USCRIPT_CODE_LIMIT];

void initializeScriptMonospaceFontMap(ScriptToFontMap& scriptFontMap, SkFontMgr* fontManager)
{
    struct FontMap {
        UScriptCode script;
        const UChar* family;
    };

    static const FontMap fontMap[] = {
        { USCRIPT_HEBREW, L"courier new" },
        { USCRIPT_ARABIC, L"courier new" },
    };

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(fontMap); ++i)
        scriptFontMap[fontMap[i].script] = fontMap[i].family;
}

void initializeScriptFontMap(ScriptToFontMap& scriptFontMap, SkFontMgr* fontManager)
{
    struct FontMap {
        UScriptCode script;
        const UChar* family;
    };

    static const FontMap fontMap[] = {
        { USCRIPT_LATIN, L"Times New Roman" },
        { USCRIPT_GREEK, L"Times New Roman" },
        { USCRIPT_CYRILLIC, L"Times New Roman" },
        // For USCRIPT_COMMON, we map blocks to scripts when
        // that makes sense.
    };

    struct ScriptToFontFamilies {
        UScriptCode script;
        const UChar** families;
    };

    // For the following scripts, multiple fonts may be listed. They are tried
    // in order. The first slot is preferred but the font may not be available,
    // if so the remaining slots are tried in order.
    // In general the order is the Windows 10 font follow by the 8.1, 8.0 and
    // finally the font for Windows 7.
    // For scripts where an optional or region specific font may be available
    // that should be listed before the generic one.
    // Based on the "Script and Font Support in Windows" MSDN documentation [1]
    // with overrides and additional fallbacks as needed.
    // 1: https://msdn.microsoft.com/en-us/goglobal/bb688099.aspx
    static const UChar* arabicFonts[] = { L"Tahoma", L"Segoe UI", 0 };
    static const UChar* armenianFonts[] = { L"Segoe UI", L"Sylfaen", 0 };
    static const UChar* bengaliFonts[] = { L"Nirmala UI", L"Vrinda", 0 };
    static const UChar* brahmiFonts[] = { L"Segoe UI Historic", 0 };
    static const UChar* brailleFonts[] = { L"Segoe UI Symbol", 0 };
    static const UChar* bugineseFonts[] = { L"Leelawadee UI", 0 };
    static const UChar* canadianAaboriginalFonts[] = { L"Gadugi",
        L"Euphemia", 0 };
    static const UChar* carianFonts[] = { L"Segoe UI Historic", 0 };
    static const UChar* cherokeeFonts[] = { L"Gadugi", L"Plantagenet", 0 };
    static const UChar* copticFonts[] = { L"Segoe UI Symbol", 0 };
    static const UChar* cuneiformFonts[] = { L"Segoe UI Historic", 0 };
    static const UChar* cypriotFonts[] = { L"Segoe UI Historic", 0 };
    static const UChar* deseretFonts[] = { L"Segoe UI Symbol", 0 };
    static const UChar* devanagariFonts[] = { L"Nirmala UI", L"Mangal", 0 };
    static const UChar* egyptianHieroglyphsFonts[] = { L"Segoe UI Historic",
        0 };
    static const UChar* ethiopicFonts[] = { L"Nyala", L"Abyssinica SIL",
        L"Ethiopia Jiret", L"Visual Geez Unicode", L"GF Zemen Unicode",
        L"Ebrima", 0 };
    static const UChar* georgianFonts[] = { L"Sylfaen", L"Segoe UI", 0 };
    static const UChar* glagoliticFonts[] = { L"Segoe UI Historic",
        L"Segoe UI Symbol", 0 };
    static const UChar* gothicFonts[] = { L"Segoe UI Historic",
        L"Segoe UI Symbol", 0 };
    static const UChar* gujaratiFonts[] = { L"Nirmala UI", L"Shruti", 0 };
    static const UChar* gurmukhiFonts[] = { L"Nirmala UI", L"Raavi", 0 };
    static const UChar* hangulFonts[] = { L"Malgun Gothic", L"Gulim", 0 };
    static const UChar* hebrewFonts[] = { L"David", L"Segoe UI", 0 };
    static const UChar* imperialAramaicFonts[] = { L"Segoe UI Historic", 0 };
    static const UChar* inscriptionalPahlaviFonts[] = { L"Segoe UI Historic",
        0 };
    static const UChar* inscriptionalParthianFonts[] = { L"Segoe UI Historic",
        0 };
    static const UChar* javaneseFonts[] = { L"Javanese Text", 0 };
    static const UChar* kannadaFonts[] = { L"Tunga", L"Nirmala UI", 0 };
    static const UChar* katakanaOrHiraganaFonts[] = { L"Meiryo", L"Yu Gothic",
        L"MS PGothic", L"Microsoft YaHei", 0 };
    static const UChar* kharoshthiFonts[] = { L"Segoe UI Historic", 0 };
    // Try Khmer OS before Vista fonts as it goes along better with Latin
    // and looks better/larger for the same size.
    static const UChar* khmerFonts[] = { L"Leelawadee UI", L"Khmer UI",
        L"Khmer OS", L"MoolBoran", L"DaunPenh", 0 };
    static const UChar* laoFonts[] = { L"Leelawadee UI", L"Lao UI",
        L"DokChampa", L"Saysettha OT", L"Phetsarath OT", L"Code2000", 0 };
    static const UChar* lisuFonts[] = { L"Segoe UI", 0 };
    static const UChar* lycianFonts[] = { L"Segoe UI Historic", 0 };
    static const UChar* lydianFonts[] = { L"Segoe UI Historic", 0 };
    static const UChar* malayalamFonts[] = { L"Nirmala UI", L"Kartika", 0 };
    static const UChar* meroiticCursiveFonts[] = { L"Segoe UI Historic",
        L"Segoe UI Symbol", 0 };
    static const UChar* mongolianFonts[] = { L"Mongolian Baiti", 0 };
    static const UChar* myanmarFonts[] = { L"Myanmar Text", L"Padauk",
        L"Parabaik", L"Myanmar3", L"Code2000", 0 };
    static const UChar* newTaiLueFonts[] = { L"Microsoft New Tai Lue", 0 };
    static const UChar* nkoFonts[] = { L"Ebrima", 0 };
    static const UChar* oghamFonts[] = { L"Segoe UI Historic",
        L"Segoe UI Symbol", 0 };
    static const UChar* olChikiFonts[] = { L"Nirmala UI", 0 };
    static const UChar* oldItalicFonts[] = { L"Segoe UI Historic",
        L"Segoe UI Symbol", 0 };
    static const UChar* oldPersianFonts[] = { L"Segoe UI Historic", 0 };
    static const UChar* oldSouthArabianFonts[] = { L"Segoe UI Historic", 0 };
    static const UChar* oriyaFonts[] = { L"Kalinga", L"ori1Uni",
        L"Lohit Oriya", L"Nirmala UI", 0 };
    static const UChar* orkhonFonts[] = { L"Segoe UI Historic",
        L"Segoe UI Symbol", 0 };
    static const UChar* osmanyaFonts[] = { L"Ebrima", 0 };
    static const UChar* phagsPaFonts[] = { L"Microsoft PhagsPa", 0 };
    static const UChar* runicFonts[] = { L"Segoe UI Historic",
        L"Segoe UI Symbol", 0 };
    static const UChar* shavianFonts[] = { L"Segoe UI Historic", 0 };
    static const UChar* simplifiedHanFonts[] = { L"simsun", L"Microsoft YaHei",
        0 };
    static const UChar* sinhalaFonts[] = { L"Iskoola Pota", L"AksharUnicode",
        L"Nirmala UI", 0 };
    static const UChar* soraSompengFonts[] = { L"Nirmala UI", 0 };
    static const UChar* symbolsFonts[] = { L"Segoe UI Symbol", 0 };
    static const UChar* syriacFonts[] = { L"Estrangelo Edessa",
        L"Estrangelo Nisibin", L"Code2000", 0 };
    static const UChar* taiLeFonts[] = { L"Microsoft Tai Le", 0 };
    static const UChar* tamilFonts[] = { L"Nirmala UI", L"Latha", 0 };
    static const UChar* teluguFonts[] = { L"Nirmala UI", L"Gautami", 0 };
    static const UChar* thaanaFonts[] = { L"MV Boli", 0 };
    static const UChar* thaiFonts[] = { L"Tahoma", L"Leelawadee UI",
        L"Leelawadee", 0 };
    static const UChar* tibetanFonts[] = { L"Microsoft Himalaya", L"Jomolhari",
        L"Tibetan Machine Uni", 0 };
    static const UChar* tifinaghFonts[] = { L"Ebrima", 0 };
    static const UChar* traditionalHanFonts[] = { L"pmingliu",
        L"Microsoft JhengHei", 0 };
    static const UChar* vaiFonts[] = { L"Ebrima", 0 };
    static const UChar* yiFonts[] = { L"Microsoft Yi Baiti", L"Nuosu SIL",
        L"Code2000", 0 };

    static const ScriptToFontFamilies scriptToFontFamilies[] = {
        { USCRIPT_ARABIC, arabicFonts },
        { USCRIPT_ARMENIAN, armenianFonts },
        { USCRIPT_BENGALI, bengaliFonts },
        { USCRIPT_BRAHMI, brahmiFonts },
        { USCRIPT_BRAILLE, brailleFonts },
        { USCRIPT_BUGINESE, bugineseFonts },
        { USCRIPT_CANADIAN_ABORIGINAL, canadianAaboriginalFonts },
        { USCRIPT_CARIAN, carianFonts },
        { USCRIPT_CHEROKEE, cherokeeFonts },
        { USCRIPT_COPTIC, copticFonts },
        { USCRIPT_CUNEIFORM, cuneiformFonts },
        { USCRIPT_CYPRIOT, cypriotFonts },
        { USCRIPT_DESERET, deseretFonts },
        { USCRIPT_DEVANAGARI, devanagariFonts },
        { USCRIPT_EGYPTIAN_HIEROGLYPHS, egyptianHieroglyphsFonts },
        { USCRIPT_ETHIOPIC, ethiopicFonts },
        { USCRIPT_GEORGIAN, georgianFonts },
        { USCRIPT_GLAGOLITIC, glagoliticFonts },
        { USCRIPT_GOTHIC, gothicFonts },
        { USCRIPT_GUJARATI, gujaratiFonts },
        { USCRIPT_GURMUKHI, gurmukhiFonts },
        { USCRIPT_HANGUL, hangulFonts },
        { USCRIPT_HEBREW, hebrewFonts },
        { USCRIPT_HIRAGANA, katakanaOrHiraganaFonts },
        { USCRIPT_IMPERIAL_ARAMAIC, imperialAramaicFonts },
        { USCRIPT_INSCRIPTIONAL_PAHLAVI, inscriptionalPahlaviFonts },
        { USCRIPT_INSCRIPTIONAL_PARTHIAN, inscriptionalParthianFonts },
        { USCRIPT_JAVANESE, javaneseFonts },
        { USCRIPT_KANNADA, kannadaFonts },
        { USCRIPT_KATAKANA, katakanaOrHiraganaFonts },
        { USCRIPT_KATAKANA_OR_HIRAGANA, katakanaOrHiraganaFonts },
        { USCRIPT_KHAROSHTHI, kharoshthiFonts },
        { USCRIPT_KHMER, khmerFonts },
        { USCRIPT_LAO, laoFonts },
        { USCRIPT_LISU, lisuFonts },
        { USCRIPT_LYCIAN, lycianFonts },
        { USCRIPT_LYDIAN, lydianFonts },
        { USCRIPT_MALAYALAM, malayalamFonts },
        { USCRIPT_MEROITIC_CURSIVE, meroiticCursiveFonts },
        { USCRIPT_MONGOLIAN, mongolianFonts },
        { USCRIPT_MYANMAR, myanmarFonts },
        { USCRIPT_NEW_TAI_LUE, newTaiLueFonts },
        { USCRIPT_NKO, nkoFonts },
        { USCRIPT_OGHAM, oghamFonts },
        { USCRIPT_OL_CHIKI, olChikiFonts },
        { USCRIPT_OLD_ITALIC, oldItalicFonts },
        { USCRIPT_OLD_PERSIAN, oldPersianFonts },
        { USCRIPT_OLD_SOUTH_ARABIAN, oldSouthArabianFonts },
        { USCRIPT_ORIYA, oriyaFonts },
        { USCRIPT_ORKHON, orkhonFonts },
        { USCRIPT_OSMANYA, osmanyaFonts },
        { USCRIPT_PHAGS_PA, phagsPaFonts },
        { USCRIPT_RUNIC, runicFonts },
        { USCRIPT_SHAVIAN, shavianFonts },
        { USCRIPT_SIMPLIFIED_HAN, simplifiedHanFonts },
        { USCRIPT_SINHALA, sinhalaFonts },
        { USCRIPT_SORA_SOMPENG, soraSompengFonts },
        { USCRIPT_SYMBOLS, symbolsFonts },
        { USCRIPT_SYRIAC, syriacFonts },
        { USCRIPT_TAI_LE, taiLeFonts },
        { USCRIPT_TAMIL, tamilFonts },
        { USCRIPT_TELUGU, teluguFonts },
        { USCRIPT_THAANA, thaanaFonts },
        { USCRIPT_THAI, thaiFonts },
        { USCRIPT_TIBETAN, tibetanFonts },
        { USCRIPT_TIFINAGH, tifinaghFonts },
        { USCRIPT_TRADITIONAL_HAN, traditionalHanFonts },
        { USCRIPT_VAI, vaiFonts },
        { USCRIPT_YI, yiFonts }
    };

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(fontMap); ++i)
        scriptFontMap[fontMap[i].script] = fontMap[i].family;

    // FIXME: Instead of scanning the hard-coded list, we have to
    // use EnumFont* to 'inspect' fonts to pick up fonts covering scripts
    // when it's possible (e.g. using OS/2 table). If we do that, this
    // had better be pulled out of here.
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(scriptToFontFamilies); ++i) {
        UScriptCode script = scriptToFontFamilies[i].script;
        scriptFontMap[script] = 0;
        const UChar** familyPtr = scriptToFontFamilies[i].families;
        while (*familyPtr) {
            if (isFontPresent(*familyPtr, fontManager)) {
                scriptFontMap[script] = *familyPtr;
                break;
            }
            ++familyPtr;
        }
    }

    // Initialize the locale-dependent mapping.
    // Since Chrome synchronizes the ICU default locale with its UI locale,
    // this ICU locale tells the current UI locale of Chrome.
    UScriptCode hanScript = scriptCodeForHanFromLocale(
        icu::Locale::getDefault().getName(), '_');
    // For other locales, use the simplified Chinese font for Han.
    const UChar* localeFamily = scriptFontMap[hanScript == USCRIPT_COMMON
        ? USCRIPT_SIMPLIFIED_HAN : hanScript];
    if (localeFamily)
        scriptFontMap[USCRIPT_HAN] = localeFamily;
}

static UScriptCode scriptForHan(UScriptCode contentScript,
    const AtomicString& contentLocale)
{
    UScriptCode script = scriptCodeForHanFromLocale(contentScript, contentLocale);
    if (script != USCRIPT_COMMON)
        return script;
    script = AcceptLanguagesResolver::preferredHanScript();
    if (script != USCRIPT_COMMON)
        return script;
    // Use UI locale. See initializeScriptFontMap().
    return USCRIPT_HAN;
}

// There are a lot of characters in USCRIPT_COMMON that can be covered
// by fonts for scripts closely related to them. See
// http://unicode.org/cldr/utility/list-unicodeset.jsp?a=[:Script=Common:]
// FIXME: make this more efficient with a wider coverage
UScriptCode getScriptBasedOnUnicodeBlock(int ucs4)
{
    UBlockCode block = ublock_getCode(ucs4);
    switch (block) {
    case UBLOCK_CJK_SYMBOLS_AND_PUNCTUATION:
        return USCRIPT_HAN;
    case UBLOCK_HIRAGANA:
    case UBLOCK_KATAKANA:
        return USCRIPT_KATAKANA_OR_HIRAGANA;
    case UBLOCK_ARABIC:
        return USCRIPT_ARABIC;
    case UBLOCK_THAI:
        return USCRIPT_THAI;
    case UBLOCK_GREEK:
        return USCRIPT_GREEK;
    case UBLOCK_DEVANAGARI:
        // For Danda and Double Danda (U+0964, U+0965), use a Devanagari
        // font for now although they're used by other scripts as well.
        // Without a context, we can't do any better.
        return USCRIPT_DEVANAGARI;
    case UBLOCK_ARMENIAN:
        return USCRIPT_ARMENIAN;
    case UBLOCK_GEORGIAN:
        return USCRIPT_GEORGIAN;
    case UBLOCK_KANNADA:
        return USCRIPT_KANNADA;
    case UBLOCK_GOTHIC:
        return USCRIPT_GOTHIC;
    default:
        return USCRIPT_COMMON;
    }
}

UScriptCode getScript(int ucs4)
{
    UErrorCode err = U_ZERO_ERROR;
    UScriptCode script = uscript_getScript(ucs4, &err);
    // If script is invalid, common or inherited or there's an error,
    // infer a script based on the unicode block of a character.
    if (script <= USCRIPT_INHERITED || U_FAILURE(err))
        script = getScriptBasedOnUnicodeBlock(ucs4);
    return script;
}

const UChar* getFontBasedOnUnicodeBlock(UBlockCode blockCode, SkFontMgr* fontManager)
{
    static const UChar* emojiFonts[] = {L"Segoe UI Emoji", L"Segoe UI Symbol"};
    static const UChar* mathFonts[] = {L"Cambria Math", L"Segoe UI Symbol", L"Code2000"};
    static const UChar* symbolFont = L"Segoe UI Symbol";
    static const UChar* emojiFont = 0;
    static const UChar* mathFont = 0;
    static bool initialized = false;
    if (!initialized) {
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(emojiFonts); i++) {
            if (isFontPresent(emojiFonts[i], fontManager)) {
                emojiFont = emojiFonts[i];
                break;
            }
        }
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(mathFonts); i++) {
            if (isFontPresent(mathFonts[i], fontManager)) {
                mathFont = mathFonts[i];
                break;
            }
        }
        initialized = true;
    }

    switch (blockCode) {
    case UBLOCK_EMOTICONS:
    case UBLOCK_ENCLOSED_ALPHANUMERIC_SUPPLEMENT:
        return emojiFont;
    case UBLOCK_PLAYING_CARDS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS_AND_ARROWS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS_AND_PICTOGRAPHS:
    case UBLOCK_TRANSPORT_AND_MAP_SYMBOLS:
    case UBLOCK_ALCHEMICAL_SYMBOLS:
    case UBLOCK_DINGBATS:
    case UBLOCK_GOTHIC:
        return symbolFont;
    case UBLOCK_ARROWS:
    case UBLOCK_MATHEMATICAL_OPERATORS:
    case UBLOCK_MISCELLANEOUS_TECHNICAL:
    case UBLOCK_GEOMETRIC_SHAPES:
    case UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_A:
    case UBLOCK_SUPPLEMENTAL_ARROWS_A:
    case UBLOCK_SUPPLEMENTAL_ARROWS_B:
    case UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_B:
    case UBLOCK_SUPPLEMENTAL_MATHEMATICAL_OPERATORS:
    case UBLOCK_MATHEMATICAL_ALPHANUMERIC_SYMBOLS:
    case UBLOCK_ARABIC_MATHEMATICAL_ALPHABETIC_SYMBOLS:
    case UBLOCK_GEOMETRIC_SHAPES_EXTENDED:
        return mathFont;
    default:
        return 0;
    };
}

} // namespace

// FIXME: this is font fallback code version 0.1
//  - Cover all the scripts
//  - Get the default font for each script/generic family from the
//    preference instead of hardcoding in the source.
//    (at least, read values from the registry for IE font settings).
//  - Support generic families (from FontDescription)
//  - If the default font for a script is not available,
//    try some more fonts known to support it. Finally, we can
//    use EnumFontFamilies or similar APIs to come up with a list of
//    fonts supporting the script and cache the result.
//  - Consider using UnicodeSet (or UnicodeMap) converted from
//    GLYPHSET (BMP) or directly read from truetype cmap tables to
//    keep track of which character is supported by which font
//  - Update script_font_cache in response to WM_FONTCHANGE

const UChar* getFontFamilyForScript(UScriptCode script,
    FontDescription::GenericFamilyType generic,
    SkFontMgr* fontManager)
{
    static ScriptToFontMap scriptFontMap;
    static ScriptToFontMap scriptMonospaceFontMap;
    static bool initialized = false;
    if (!initialized) {
        initializeScriptFontMap(scriptFontMap, fontManager);
        initializeScriptMonospaceFontMap(scriptMonospaceFontMap, fontManager);
        initialized = true;
    }
    if (script == USCRIPT_INVALID_CODE)
        return 0;
    ASSERT(script < USCRIPT_CODE_LIMIT);
    if (generic == FontDescription::MonospaceFamily && scriptMonospaceFontMap[script])
        return scriptMonospaceFontMap[script];
    return scriptFontMap[script];
}

// FIXME:
//  - Handle 'Inherited', 'Common' and 'Unknown'
//    (see http://www.unicode.org/reports/tr24/#Usage_Model )
//    For 'Inherited' and 'Common', perhaps we need to
//    accept another parameter indicating the previous family
//    and just return it.
//  - All the characters (or characters up to the point a single
//    font can cover) need to be taken into account
const UChar* getFallbackFamily(UChar32 character,
    FontDescription::GenericFamilyType generic,
    UScriptCode contentScript,
    const AtomicString& contentLocale,
    UScriptCode* scriptChecked,
    FontFallbackPriority fallbackPriority,
    SkFontMgr* fontManager)
{
    ASSERT(character);
    ASSERT(fontManager);
    UBlockCode block = fallbackPriority == FontFallbackPriority::EmojiEmoji ?
        UBLOCK_EMOTICONS : ublock_getCode(character);
    const UChar* family = getFontBasedOnUnicodeBlock(block, fontManager);
    if (family) {
        if (scriptChecked)
            *scriptChecked = USCRIPT_INVALID_CODE;
        return family;
    }

    UScriptCode script = getScript(character);

    // For the full-width ASCII characters (U+FF00 - U+FF5E), use the font for
    // Han (determined in a locale-dependent way above). Full-width ASCII
    // characters are rather widely used in Japanese and Chinese documents and
    // they're fully covered by Chinese, Japanese and Korean fonts.
    if (0xFF00 < character && character < 0xFF5F)
        script = USCRIPT_HAN;

    if (script == USCRIPT_COMMON)
        script = getScriptBasedOnUnicodeBlock(character);

    // For unified-Han scripts, try the lang attribute, system, or
    // accept-languages.
    if (script == USCRIPT_HAN)
        script = scriptForHan(contentScript, contentLocale);

    family = getFontFamilyForScript(script, generic, fontManager);
    // Another lame work-around to cover non-BMP characters.
    // If the font family for script is not found or the character is
    // not in BMP (> U+FFFF), we resort to the hard-coded list of
    // fallback fonts for now.
    if (!family || character > 0xFFFF) {
        int plane = character >> 16;
        switch (plane) {
        case 1:
            family = L"code2001";
            break;
        case 2:
            // Use a Traditional Chinese ExtB font if in Traditional Chinese locale.
            // Otherwise, use a Simplified Chinese ExtB font. Windows Japanese
            // fonts do support a small subset of ExtB (that are included in JIS X 0213),
            // but its coverage is rather sparse.
            // Eventually, this should be controlled by lang/xml:lang.
            if (icu::Locale::getDefault() == icu::Locale::getTraditionalChinese())
                family = L"pmingliu-extb";
            else
                family = L"simsun-extb";
            break;
        default:
            family = L"lucida sans unicode";
        }
    }

    if (scriptChecked)
        *scriptChecked = script;
    return family;
}

} // namespace blink
