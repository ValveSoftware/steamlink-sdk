/*
 * Copyright (c) 2006, 2007, 2008, 2009, 2010, 2012 Google Inc. All rights
 * reserved.
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

static inline bool isFontPresent(const UChar* fontName,
                                 SkFontMgr* fontManager) {
  String family = fontName;
  sk_sp<SkTypeface> tf(
      fontManager->matchFamilyStyle(family.utf8().data(), SkFontStyle()));
  if (!tf)
    return false;

  SkTypeface::LocalizedStrings* actualFamilies = tf->createFamilyNameIterator();
  bool matchesRequestedFamily = false;
  SkTypeface::LocalizedString actualFamily;
  while (actualFamilies->next(&actualFamily)) {
    if (equalIgnoringCase(
            family, AtomicString::fromUTF8(actualFamily.fString.c_str()))) {
      matchesRequestedFamily = true;
      break;
    }
  }
  actualFamilies->unref();

  return matchesRequestedFamily;
}

struct FontMapping {
  const UChar* familyName;
  const UChar* const* candidateFamilyNames;
};
// A simple mapping from UScriptCode to family name. This is a sparse array,
// which works well since the range of UScriptCode values is small.
typedef FontMapping ScriptToFontMap[USCRIPT_CODE_LIMIT];

const UChar* findMonospaceFontForScript(UScriptCode script) {
  struct FontMap {
    UScriptCode script;
    const UChar* family;
  };

  static const FontMap fontMap[] = {
      {USCRIPT_HEBREW, L"courier new"}, {USCRIPT_ARABIC, L"courier new"},
  };

  for (const auto& fontFamily : fontMap) {
    if (fontFamily.script == script)
      return fontFamily.family;
  }
  return nullptr;
}

void initializeScriptFontMap(ScriptToFontMap& scriptFontMap) {
  struct ScriptToFontFamilies {
    UScriptCode script;
    const UChar* const* families;
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
  static const UChar* const arabicFonts[] = {L"Tahoma", L"Segoe UI", 0};
  static const UChar* const armenianFonts[] = {L"Segoe UI", L"Sylfaen", 0};
  static const UChar* const bengaliFonts[] = {L"Nirmala UI", L"Vrinda", 0};
  static const UChar* const brahmiFonts[] = {L"Segoe UI Historic", 0};
  static const UChar* const brailleFonts[] = {L"Segoe UI Symbol", 0};
  static const UChar* const bugineseFonts[] = {L"Leelawadee UI", 0};
  static const UChar* const canadianAaboriginalFonts[] = {L"Gadugi",
                                                          L"Euphemia", 0};
  static const UChar* const carianFonts[] = {L"Segoe UI Historic", 0};
  static const UChar* const cherokeeFonts[] = {L"Gadugi", L"Plantagenet", 0};
  static const UChar* const copticFonts[] = {L"Segoe UI Symbol", 0};
  static const UChar* const cuneiformFonts[] = {L"Segoe UI Historic", 0};
  static const UChar* const cypriotFonts[] = {L"Segoe UI Historic", 0};
  static const UChar* const cyrillicFonts[] = {L"Times New Roman", 0};
  static const UChar* const deseretFonts[] = {L"Segoe UI Symbol", 0};
  static const UChar* const devanagariFonts[] = {L"Nirmala UI", L"Mangal", 0};
  static const UChar* const egyptianHieroglyphsFonts[] = {L"Segoe UI Historic",
                                                          0};
  static const UChar* const ethiopicFonts[] = {L"Nyala",
                                               L"Abyssinica SIL",
                                               L"Ethiopia Jiret",
                                               L"Visual Geez Unicode",
                                               L"GF Zemen Unicode",
                                               L"Ebrima",
                                               0};
  static const UChar* const georgianFonts[] = {L"Sylfaen", L"Segoe UI", 0};
  static const UChar* const glagoliticFonts[] = {L"Segoe UI Historic",
                                                 L"Segoe UI Symbol", 0};
  static const UChar* const gothicFonts[] = {L"Segoe UI Historic",
                                             L"Segoe UI Symbol", 0};
  static const UChar* const greekFonts[] = {L"Times New Roman", 0};
  static const UChar* const gujaratiFonts[] = {L"Nirmala UI", L"Shruti", 0};
  static const UChar* const gurmukhiFonts[] = {L"Nirmala UI", L"Raavi", 0};
  static const UChar* const hangulFonts[] = {L"Malgun Gothic", L"Gulim", 0};
  static const UChar* const hebrewFonts[] = {L"David", L"Segoe UI", 0};
  static const UChar* const imperialAramaicFonts[] = {L"Segoe UI Historic", 0};
  static const UChar* const inscriptionalPahlaviFonts[] = {L"Segoe UI Historic",
                                                           0};
  static const UChar* const inscriptionalParthianFonts[] = {
      L"Segoe UI Historic", 0};
  static const UChar* const javaneseFonts[] = {L"Javanese Text", 0};
  static const UChar* const kannadaFonts[] = {L"Tunga", L"Nirmala UI", 0};
  static const UChar* const katakanaOrHiraganaFonts[] = {
      L"Meiryo", L"Yu Gothic", L"MS PGothic", L"Microsoft YaHei", 0};
  static const UChar* const kharoshthiFonts[] = {L"Segoe UI Historic", 0};
  // Try Khmer OS before Vista fonts as it goes along better with Latin
  // and looks better/larger for the same size.
  static const UChar* const khmerFonts[] = {
      L"Leelawadee UI", L"Khmer UI", L"Khmer OS", L"MoolBoran", L"DaunPenh", 0};
  static const UChar* const laoFonts[] = {L"Leelawadee UI",
                                          L"Lao UI",
                                          L"DokChampa",
                                          L"Saysettha OT",
                                          L"Phetsarath OT",
                                          L"Code2000",
                                          0};
  static const UChar* const latinFonts[] = {L"Times New Roman", 0};
  static const UChar* const lisuFonts[] = {L"Segoe UI", 0};
  static const UChar* const lycianFonts[] = {L"Segoe UI Historic", 0};
  static const UChar* const lydianFonts[] = {L"Segoe UI Historic", 0};
  static const UChar* const malayalamFonts[] = {L"Nirmala UI", L"Kartika", 0};
  static const UChar* const meroiticCursiveFonts[] = {L"Segoe UI Historic",
                                                      L"Segoe UI Symbol", 0};
  static const UChar* const mongolianFonts[] = {L"Mongolian Baiti", 0};
  static const UChar* const myanmarFonts[] = {
      L"Myanmar Text", L"Padauk", L"Parabaik", L"Myanmar3", L"Code2000", 0};
  static const UChar* const newTaiLueFonts[] = {L"Microsoft New Tai Lue", 0};
  static const UChar* const nkoFonts[] = {L"Ebrima", 0};
  static const UChar* const oghamFonts[] = {L"Segoe UI Historic",
                                            L"Segoe UI Symbol", 0};
  static const UChar* const olChikiFonts[] = {L"Nirmala UI", 0};
  static const UChar* const oldItalicFonts[] = {L"Segoe UI Historic",
                                                L"Segoe UI Symbol", 0};
  static const UChar* const oldPersianFonts[] = {L"Segoe UI Historic", 0};
  static const UChar* const oldSouthArabianFonts[] = {L"Segoe UI Historic", 0};
  static const UChar* const oriyaFonts[] = {L"Kalinga", L"ori1Uni",
                                            L"Lohit Oriya", L"Nirmala UI", 0};
  static const UChar* const orkhonFonts[] = {L"Segoe UI Historic",
                                             L"Segoe UI Symbol", 0};
  static const UChar* const osmanyaFonts[] = {L"Ebrima", 0};
  static const UChar* const phagsPaFonts[] = {L"Microsoft PhagsPa", 0};
  static const UChar* const runicFonts[] = {L"Segoe UI Historic",
                                            L"Segoe UI Symbol", 0};
  static const UChar* const shavianFonts[] = {L"Segoe UI Historic", 0};
  static const UChar* const simplifiedHanFonts[] = {L"simsun",
                                                    L"Microsoft YaHei", 0};
  static const UChar* const sinhalaFonts[] = {L"Iskoola Pota", L"AksharUnicode",
                                              L"Nirmala UI", 0};
  static const UChar* const soraSompengFonts[] = {L"Nirmala UI", 0};
  static const UChar* const symbolsFonts[] = {L"Segoe UI Symbol", 0};
  static const UChar* const syriacFonts[] = {
      L"Estrangelo Edessa", L"Estrangelo Nisibin", L"Code2000", 0};
  static const UChar* const taiLeFonts[] = {L"Microsoft Tai Le", 0};
  static const UChar* const tamilFonts[] = {L"Nirmala UI", L"Latha", 0};
  static const UChar* const teluguFonts[] = {L"Nirmala UI", L"Gautami", 0};
  static const UChar* const thaanaFonts[] = {L"MV Boli", 0};
  static const UChar* const thaiFonts[] = {L"Tahoma", L"Leelawadee UI",
                                           L"Leelawadee", 0};
  static const UChar* const tibetanFonts[] = {
      L"Microsoft Himalaya", L"Jomolhari", L"Tibetan Machine Uni", 0};
  static const UChar* const tifinaghFonts[] = {L"Ebrima", 0};
  static const UChar* const traditionalHanFonts[] = {L"pmingliu",
                                                     L"Microsoft JhengHei", 0};
  static const UChar* const vaiFonts[] = {L"Ebrima", 0};
  static const UChar* const yiFonts[] = {L"Microsoft Yi Baiti", L"Nuosu SIL",
                                         L"Code2000", 0};

  static const ScriptToFontFamilies scriptToFontFamilies[] = {
      {USCRIPT_ARABIC, arabicFonts},
      {USCRIPT_ARMENIAN, armenianFonts},
      {USCRIPT_BENGALI, bengaliFonts},
      {USCRIPT_BRAHMI, brahmiFonts},
      {USCRIPT_BRAILLE, brailleFonts},
      {USCRIPT_BUGINESE, bugineseFonts},
      {USCRIPT_CANADIAN_ABORIGINAL, canadianAaboriginalFonts},
      {USCRIPT_CARIAN, carianFonts},
      {USCRIPT_CHEROKEE, cherokeeFonts},
      {USCRIPT_COPTIC, copticFonts},
      {USCRIPT_CUNEIFORM, cuneiformFonts},
      {USCRIPT_CYPRIOT, cypriotFonts},
      {USCRIPT_CYRILLIC, cyrillicFonts},
      {USCRIPT_DESERET, deseretFonts},
      {USCRIPT_DEVANAGARI, devanagariFonts},
      {USCRIPT_EGYPTIAN_HIEROGLYPHS, egyptianHieroglyphsFonts},
      {USCRIPT_ETHIOPIC, ethiopicFonts},
      {USCRIPT_GEORGIAN, georgianFonts},
      {USCRIPT_GLAGOLITIC, glagoliticFonts},
      {USCRIPT_GOTHIC, gothicFonts},
      {USCRIPT_GREEK, greekFonts},
      {USCRIPT_GUJARATI, gujaratiFonts},
      {USCRIPT_GURMUKHI, gurmukhiFonts},
      {USCRIPT_HANGUL, hangulFonts},
      {USCRIPT_HEBREW, hebrewFonts},
      {USCRIPT_HIRAGANA, katakanaOrHiraganaFonts},
      {USCRIPT_IMPERIAL_ARAMAIC, imperialAramaicFonts},
      {USCRIPT_INSCRIPTIONAL_PAHLAVI, inscriptionalPahlaviFonts},
      {USCRIPT_INSCRIPTIONAL_PARTHIAN, inscriptionalParthianFonts},
      {USCRIPT_JAVANESE, javaneseFonts},
      {USCRIPT_KANNADA, kannadaFonts},
      {USCRIPT_KATAKANA, katakanaOrHiraganaFonts},
      {USCRIPT_KATAKANA_OR_HIRAGANA, katakanaOrHiraganaFonts},
      {USCRIPT_KHAROSHTHI, kharoshthiFonts},
      {USCRIPT_KHMER, khmerFonts},
      {USCRIPT_LAO, laoFonts},
      {USCRIPT_LATIN, latinFonts},
      {USCRIPT_LISU, lisuFonts},
      {USCRIPT_LYCIAN, lycianFonts},
      {USCRIPT_LYDIAN, lydianFonts},
      {USCRIPT_MALAYALAM, malayalamFonts},
      {USCRIPT_MEROITIC_CURSIVE, meroiticCursiveFonts},
      {USCRIPT_MONGOLIAN, mongolianFonts},
      {USCRIPT_MYANMAR, myanmarFonts},
      {USCRIPT_NEW_TAI_LUE, newTaiLueFonts},
      {USCRIPT_NKO, nkoFonts},
      {USCRIPT_OGHAM, oghamFonts},
      {USCRIPT_OL_CHIKI, olChikiFonts},
      {USCRIPT_OLD_ITALIC, oldItalicFonts},
      {USCRIPT_OLD_PERSIAN, oldPersianFonts},
      {USCRIPT_OLD_SOUTH_ARABIAN, oldSouthArabianFonts},
      {USCRIPT_ORIYA, oriyaFonts},
      {USCRIPT_ORKHON, orkhonFonts},
      {USCRIPT_OSMANYA, osmanyaFonts},
      {USCRIPT_PHAGS_PA, phagsPaFonts},
      {USCRIPT_RUNIC, runicFonts},
      {USCRIPT_SHAVIAN, shavianFonts},
      {USCRIPT_SIMPLIFIED_HAN, simplifiedHanFonts},
      {USCRIPT_SINHALA, sinhalaFonts},
      {USCRIPT_SORA_SOMPENG, soraSompengFonts},
      {USCRIPT_SYMBOLS, symbolsFonts},
      {USCRIPT_SYRIAC, syriacFonts},
      {USCRIPT_TAI_LE, taiLeFonts},
      {USCRIPT_TAMIL, tamilFonts},
      {USCRIPT_TELUGU, teluguFonts},
      {USCRIPT_THAANA, thaanaFonts},
      {USCRIPT_THAI, thaiFonts},
      {USCRIPT_TIBETAN, tibetanFonts},
      {USCRIPT_TIFINAGH, tifinaghFonts},
      {USCRIPT_TRADITIONAL_HAN, traditionalHanFonts},
      {USCRIPT_VAI, vaiFonts},
      {USCRIPT_YI, yiFonts}};

  for (const auto& fontFamily : scriptToFontFamilies) {
    scriptFontMap[fontFamily.script].candidateFamilyNames = fontFamily.families;
  }

  // Initialize the locale-dependent mapping from system locale.
  UScriptCode hanScript = LayoutLocale::getSystem().scriptForHan();
  DCHECK(hanScript != USCRIPT_HAN);
  if (scriptFontMap[hanScript].candidateFamilyNames) {
    scriptFontMap[USCRIPT_HAN].candidateFamilyNames =
        scriptFontMap[hanScript].candidateFamilyNames;
  }
}

void findFirstExistingCandidateFont(FontMapping& scriptFontMapping,
                                    SkFontMgr* fontManager) {
  for (const UChar* const* familyPtr = scriptFontMapping.candidateFamilyNames;
       *familyPtr; familyPtr++) {
    if (isFontPresent(*familyPtr, fontManager)) {
      scriptFontMapping.familyName = *familyPtr;
      break;
    }
  }
  scriptFontMapping.candidateFamilyNames = nullptr;
}

// There are a lot of characters in USCRIPT_COMMON that can be covered
// by fonts for scripts closely related to them. See
// http://unicode.org/cldr/utility/list-unicodeset.jsp?a=[:Script=Common:]
// FIXME: make this more efficient with a wider coverage
UScriptCode getScriptBasedOnUnicodeBlock(int ucs4) {
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

UScriptCode getScript(int ucs4) {
  UErrorCode err = U_ZERO_ERROR;
  UScriptCode script = uscript_getScript(ucs4, &err);
  // If script is invalid, common or inherited or there's an error,
  // infer a script based on the unicode block of a character.
  if (script <= USCRIPT_INHERITED || U_FAILURE(err))
    script = getScriptBasedOnUnicodeBlock(ucs4);
  return script;
}

const UChar* getFontBasedOnUnicodeBlock(UBlockCode blockCode,
                                        SkFontMgr* fontManager) {
  static const UChar* const emojiFonts[] = {L"Segoe UI Emoji",
                                            L"Segoe UI Symbol"};
  static const UChar* const mathFonts[] = {L"Cambria Math", L"Segoe UI Symbol",
                                           L"Code2000"};
  static const UChar* const symbolFont = L"Segoe UI Symbol";
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

}  // namespace

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
                                    SkFontMgr* fontManager) {
  static ScriptToFontMap scriptFontMap;
  static bool initialized = false;
  if (!initialized) {
    initializeScriptFontMap(scriptFontMap);
    initialized = true;
  }

  if (script == USCRIPT_INVALID_CODE)
    return 0;
  ASSERT(script < USCRIPT_CODE_LIMIT);
  if (generic == FontDescription::MonospaceFamily) {
    const UChar* monospaceFamily = findMonospaceFontForScript(script);
    if (monospaceFamily)
      return monospaceFamily;
  }
  if (scriptFontMap[script].candidateFamilyNames)
    findFirstExistingCandidateFont(scriptFontMap[script], fontManager);
  return scriptFontMap[script].familyName;
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
                               const LayoutLocale* contentLocale,
                               UScriptCode* scriptChecked,
                               FontFallbackPriority fallbackPriority,
                               SkFontMgr* fontManager) {
  ASSERT(character);
  ASSERT(fontManager);
  UBlockCode block = fallbackPriority == FontFallbackPriority::EmojiEmoji
                         ? UBLOCK_EMOTICONS
                         : ublock_getCode(character);
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
  if (script == USCRIPT_HAN) {
    if (const LayoutLocale* localeForHan =
            LayoutLocale::localeForHan(contentLocale))
      script = localeForHan->scriptForHan();
    // If still unknown, USCRIPT_HAN uses UI locale.
    // See initializeScriptFontMap().
  }

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
        // fonts do support a small subset of ExtB (that are included in JIS X
        // 0213), but its coverage is rather sparse.
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

}  // namespace blink
