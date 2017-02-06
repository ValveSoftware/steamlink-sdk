// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Character.h"

#include <unicode/uvernum.h>

#if defined(USING_SYSTEM_ICU) && (U_ICU_VERSION_MAJOR_NUM <= 56)
#include <unicode/uniset.h>
#else
#include <unicode/uchar.h>
#endif

using namespace WTF;
using namespace Unicode;

namespace blink {

// ICU 56 or earlier does not have API for Emoji properties,
// but Chrome's copy of ICU 56 does.
#if defined(USING_SYSTEM_ICU) && (U_ICU_VERSION_MAJOR_NUM <= 56)
// The following UnicodeSet patterns were compiled from
// http://www.unicode.org/Public/emoji/2.0//emoji-data.txt

static const char kEmojiTextPattern[] =
    R"([[#][*][0-9][\u00A9][\u00AE][\u203C][\u2049][\u2122][\u2139])"
    R"([\u2194-\u2199][\u21A9-\u21AA][\u231A-\u231B][\u2328][\u23CF])"
    R"([\u23E9-\u23F3][\u23F8-\u23FA][\u24C2][\u25AA-\u25AB][\u25B6][\u25C0])"
    R"([\u25FB-\u25FE][\u2600-\u2604][\u260E][\u2611][\u2614-\u2615][\u2618])"
    R"([\u261D][\u2620][\u2622-\u2623][\u2626][\u262A][\u262E-\u262F])"
    R"([\u2638-\u263A][\u2648-\u2653][\u2660][\u2663][\u2665-\u2666][\u2668])"
    R"([\u267B][\u267F][\u2692-\u2694][\u2696-\u2697][\u2699][\u269B-\u269C])"
    R"([\u26A0-\u26A1][\u26AA-\u26AB][\u26B0-\u26B1][\u26BD-\u26BE])"
    R"([\u26C4-\u26C5][\u26C8][\u26CE-\u26CF][\u26D1][\u26D3-\u26D4])"
    R"([\u26E9-\u26EA][\u26F0-\u26F5][\u26F7-\u26FA][\u26FD][\u2702][\u2705])"
    R"([\u2708-\u270D][\u270F][\u2712][\u2714][\u2716][\u271D][\u2721][\u2728])"
    R"([\u2733-\u2734][\u2744][\u2747][\u274C][\u274E][\u2753-\u2755][\u2757])"
    R"([\u2763-\u2764][\u2795-\u2797][\u27A1][\u27B0][\u27BF][\u2934-\u2935])"
    R"([\u2B05-\u2B07][\u2B1B-\u2B1C][\u2B50][\u2B55][\u3030][\u303D][\u3297])"
    R"([\u3299][\U0001F004][\U0001F0CF][\U0001F170-\U0001F171])"
    R"([\U0001F17E-\U0001F17F][\U0001F18E][\U0001F191-\U0001F19A])"
    R"([\U0001F1E6-\U0001F1FF][\U0001F201-\U0001F202][\U0001F21A][\U0001F22F])"
    R"([\U0001F232-\U0001F23A][\U0001F250-\U0001F251][\U0001F300-\U0001F321])"
    R"([\U0001F324-\U0001F393][\U0001F396-\U0001F397][\U0001F399-\U0001F39B])"
    R"([\U0001F39E-\U0001F3F0][\U0001F3F3-\U0001F3F5][\U0001F3F7-\U0001F4FD])"
    R"([\U0001F4FF-\U0001F53D][\U0001F549-\U0001F54E][\U0001F550-\U0001F567])"
    R"([\U0001F56F-\U0001F570][\U0001F573-\U0001F579][\U0001F587])"
    R"([\U0001F58A-\U0001F58D][\U0001F590][\U0001F595-\U0001F596][\U0001F5A5])"
    R"([\U0001F5A8][\U0001F5B1-\U0001F5B2][\U0001F5BC][\U0001F5C2-\U0001F5C4])"
    R"([\U0001F5D1-\U0001F5D3][\U0001F5DC-\U0001F5DE][\U0001F5E1][\U0001F5E3])"
    R"([\U0001F5E8][\U0001F5EF][\U0001F5F3][\U0001F5FA-\U0001F64F])"
    R"([\U0001F680-\U0001F6C5][\U0001F6CB-\U0001F6D0][\U0001F6E0-\U0001F6E5])"
    R"([\U0001F6E9][\U0001F6EB-\U0001F6EC][\U0001F6F0][\U0001F6F3])"
    R"([\U0001F910-\U0001F918][\U0001F980-\U0001F984][\U0001F9C0]])";

static const char kEmojiEmojiPattern[] =
    R"([[\u231A-\u231B][\u23E9-\u23EC][\u23F0][\u23F3][\u25FD-\u25FE])"
    R"([\u2614-\u2615][\u2648-\u2653][\u267F][\u2693][\u26A1][\u26AA-\u26AB])"
    R"([\u26BD-\u26BE][\u26C4-\u26C5][\u26CE][\u26D4][\u26EA][\u26F2-\u26F3])"
    R"([\u26F5][\u26FA][\u26FD][\u2705][\u270A-\u270B][\u2728][\u274C][\u274E])"
    R"([\u2753-\u2755][\u2757][\u2795-\u2797][\u27B0][\u27BF][\u2B1B-\u2B1C])"
    R"([\u2B50][\u2B55][\U0001F004][\U0001F0CF][\U0001F18E])"
    R"([\U0001F191-\U0001F19A][\U0001F1E6-\U0001F1FF][\U0001F201][\U0001F21A])"
    R"([\U0001F22F][\U0001F232-\U0001F236][\U0001F238-\U0001F23A])"
    R"([\U0001F250-\U0001F251][\U0001F300-\U0001F320][\U0001F32D-\U0001F335])"
    R"([\U0001F337-\U0001F37C][\U0001F37E-\U0001F393][\U0001F3A0-\U0001F3CA])"
    R"([\U0001F3CF-\U0001F3D3][\U0001F3E0-\U0001F3F0][\U0001F3F4])"
    R"([\U0001F3F8-\U0001F43E][\U0001F440][\U0001F442-\U0001F4FC])"
    R"([\U0001F4FF-\U0001F53D][\U0001F54B-\U0001F54E][\U0001F550-\U0001F567])"
    R"([\U0001F595-\U0001F596][\U0001F5FB-\U0001F64F][\U0001F680-\U0001F6C5])"
    R"([\U0001F6CC][\U0001F6D0][\U0001F6EB-\U0001F6EC][\U0001F910-\U0001F918])"
    R"([\U0001F980-\U0001F984][\U0001F9C0]])";

static const char kEmojiModifierBasePattern[] =
    R"([[\u261D][\u26F9][\u270A-\u270D][\U0001F385][\U0001F3C3-\U0001F3C4])"
    R"([\U0001F3CA-\U0001F3CB][\U0001F442-\U0001F443][\U0001F446-\U0001F450])"
    R"([\U0001F466-\U0001F469][\U0001F46E][\U0001F470-\U0001F478][\U0001F47C])"
    R"([\U0001F481-\U0001F483][\U0001F485-\U0001F487][\U0001F4AA][\U0001F575])"
    R"([\U0001F590][\U0001F595-\U0001F596][\U0001F645-\U0001F647])"
    R"([\U0001F64B-\U0001F64F][\U0001F6A3][\U0001F6B4-\U0001F6B6][\U0001F6C0])"
    R"([\U0001F918]])";

static void applyPatternAndFreeze(icu::UnicodeSet* unicodeSet, const char* pattern)
{
    UErrorCode err = U_ZERO_ERROR;
    // Use ICU's invariant-character initialization method.
    unicodeSet->applyPattern(icu::UnicodeString(pattern, -1, US_INV), err);
    unicodeSet->freeze();
    ASSERT(err == U_ZERO_ERROR);
}

bool Character::isEmoji(UChar32 ch)
{
    return Character::isEmojiTextDefault(ch) || Character::isEmojiEmojiDefault(ch);
}

bool Character::isEmojiTextDefault(UChar32 ch)
{
    DEFINE_STATIC_LOCAL(icu::UnicodeSet, emojiTextSet, ());
    if (emojiTextSet.isEmpty())
        applyPatternAndFreeze(&emojiTextSet, kEmojiTextPattern);
    return emojiTextSet.contains(ch) && !isEmojiEmojiDefault(ch);
}

bool Character::isEmojiEmojiDefault(UChar32 ch)
{
    DEFINE_STATIC_LOCAL(icu::UnicodeSet, emojiEmojiSet, ());
    if (emojiEmojiSet.isEmpty())
        applyPatternAndFreeze(&emojiEmojiSet, kEmojiEmojiPattern);
    return emojiEmojiSet.contains(ch);
}

bool Character::isEmojiModifierBase(UChar32 ch)
{
    DEFINE_STATIC_LOCAL(icu::UnicodeSet, emojieModifierBaseSet, ());
    if (emojieModifierBaseSet.isEmpty())
        applyPatternAndFreeze(&emojieModifierBaseSet, kEmojiModifierBasePattern);
    return emojieModifierBaseSet.contains(ch);
}
#else
bool Character::isEmoji(UChar32 ch)
{
    return u_hasBinaryProperty(ch, UCHAR_EMOJI);
}
bool Character::isEmojiTextDefault(UChar32 ch)
{
    return u_hasBinaryProperty(ch, UCHAR_EMOJI)
        && !u_hasBinaryProperty(ch, UCHAR_EMOJI_PRESENTATION);
}

bool Character::isEmojiEmojiDefault(UChar32 ch)
{
    return u_hasBinaryProperty(ch, UCHAR_EMOJI_PRESENTATION);
}

bool Character::isEmojiModifierBase(UChar32 ch)
{
    return u_hasBinaryProperty(ch, UCHAR_EMOJI_MODIFIER_BASE);
}
#endif // defined(USING_SYSTEM_ICU) && (U_ICU_VERSION_MAJOR_NUM <= 56)

bool Character::isEmojiKeycapBase(UChar32 ch)
{
    return (ch >= '0' && ch <= '9') || ch == '#';
}

bool Character::isRegionalIndicator(UChar32 ch)
{
    return (ch >= 0x1F1E6 && ch <= 0x1F1FF);
}


}; // namespace blink
