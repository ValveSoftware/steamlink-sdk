/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "hangul.h"

namespace QtVirtualKeyboard {

const QList<ushort> Hangul::initials = QList<ushort>()
    << 0x3131 << 0x3132 << 0x3134 << 0x3137 << 0x3138 << 0x3139 << 0x3141
    << 0x3142 << 0x3143 << 0x3145 << 0x3146 << 0x3147 << 0x3148 << 0x3149
    << 0x314A << 0x314B << 0x314C << 0x314D << 0x314E;
const QList<ushort> Hangul::finals = QList<ushort>()
    << 0x0000 << 0x3131 << 0x3132 << 0x3133 << 0x3134 << 0x3135 << 0x3136
    << 0x3137 << 0x3139 << 0x313A << 0x313B << 0x313C << 0x313D << 0x313E
    << 0x313F << 0x3140 << 0x3141 << 0x3142 << 0x3144 << 0x3145 << 0x3146
    << 0x3147 << 0x3148 << 0x314A << 0x314B << 0x314C << 0x314D << 0x314E;
const QMap<ushort, Hangul::HangulMedialIndex> Hangul::doubleMedialMap =
    Hangul::initDoubleMedialMap();
const QMap<ushort, Hangul::HangulFinalIndex> Hangul::doubleFinalMap =
    Hangul::initDoubleFinalMap();
const int Hangul::SBase = 0xAC00;
const int Hangul::LBase = 0x1100;
const int Hangul::VBase = 0x314F;
const int Hangul::TBase = 0x11A7;
const int Hangul::LCount = 19;
const int Hangul::VCount = 21;
const int Hangul::TCount = 28;
const int Hangul::NCount = Hangul::VCount * Hangul::TCount; // 588
const int Hangul::SCount = Hangul::LCount * Hangul::NCount; // 11172

/*!
    \class QtVirtualKeyboard::Hangul
    \internal
*/

QString Hangul::decompose(const QString &source)
{
    QString result;
    const int len = source.length();
    for (int i = 0; i < len; i++) {
        QChar ch = source.at(i);
        int SIndex = (int)ch.unicode() - SBase;
        if (SIndex >= 0 && SIndex < SCount) {

            // Decompose initial consonant
            result.append(QChar((int)initials[SIndex / NCount]));

            // Decompose medial vowel and check if it consists of double Jamo
            int VIndex = (SIndex % NCount) / TCount;
            ushort key = findDoubleMedial((HangulMedialIndex)VIndex);
            if (key) {
                HangulMedialIndex VIndexA, VIndexB;
                unpackDoubleMedial(key, VIndexA, VIndexB);
                result.append(QChar(VBase + (int)VIndexA));
                result.append(QChar(VBase + (int)VIndexB));
            } else {
                result.append(QChar(VBase + VIndex));
            }

            // Decompose final consonant and check if it consists of double Jamo
            int TIndex = SIndex % TCount;
            if (TIndex != 0) {
                key = findDoubleFinal((HangulFinalIndex)TIndex);
                if (key) {
                    HangulFinalIndex TIndexA, TIndexB;
                    unpackDoubleFinal(key, TIndexA, TIndexB);
                    result.append(QChar(finals[(int)TIndexA]));
                    result.append(QChar(finals[(int)TIndexB]));
                } else {
                    result.append(QChar(finals[TIndex]));
                }
            }
        } else {
            result.append(ch);
        }
    }
    return result;
}

QString Hangul::compose(const QString &source)
{
    const int len = source.length();
    if (len == 0)
        return QString();

    // Always add the initial character into buffer.
    // The last character will serve as the current
    // Hangul Syllable.
    QChar last = source.at(0);
    QString result = QString(last);

    // Go through the input buffer starting at next character
    for (int i = 1; i < len; i++) {
        const QChar ch = source.at(i);

        // Check to see if the character is Hangul Compatibility Jamo
        const ushort unicode = ch.unicode();
        if (isJamo(unicode)) {

            // Check to see if the character is syllable
            const ushort lastUnicode = last.unicode();
            int SIndex = (int)lastUnicode - SBase;
            if (SIndex >= 0 && SIndex < SCount) {

                // Check to see if the syllable type is LV or LV+T
                int TIndex = SIndex % TCount;
                if (TIndex == 0) {

                    // If the current character is final consonant, then
                    // make syllable of form LV+T
                    TIndex = finals.indexOf(unicode);
                    if (TIndex != -1) {
                        last = QChar((int)lastUnicode + TIndex);
                        result.replace(result.length() - 1, 1, last);
                        continue;
                    }

                    // Check to see if the current character is vowel
                    HangulMedialIndex VIndexB = (HangulMedialIndex)((int)unicode - VBase);
                    if (isMedial(VIndexB)) {

                        // Some medial Jamos do not exist in the keyboard layout as is.
                        // Such Jamos can only be formed by combining the two specific Jamos,
                        // aka the double Jamos.

                        HangulMedialIndex VIndexA = (HangulMedialIndex)((SIndex % NCount) / TCount);
                        if (isMedial(VIndexA)) {

                            // Search the double medial map if such a combination exists
                            ushort key = packDoubleMedial(VIndexA, VIndexB);
                            const auto it = doubleMedialMap.constFind(key);
                            if (it != doubleMedialMap.cend()) {

                                // Update syllable by adding the difference between
                                // the vowels indices
                                HangulMedialIndex VIndexD = it.value();
                                int VDiff = (int)VIndexD - (int)VIndexA;
                                last = QChar((int)lastUnicode + VDiff * TCount);
                                result.replace(result.length() - 1, 1, last);
                                continue;
                            }
                        }
                    }

                } else {

                    // Check to see if current jamo is vowel
                    int VIndex = (int)unicode - VBase;
                    if (VIndex >= 0 && VIndex < VCount) {

                        // Since some initial and final consonants use the same
                        // Unicode values, we need to check whether the previous final
                        // Jamo is actually an initial Jamo of the next syllable.
                        //
                        // Consider the following scenario:
                        //      LVT+V == not possible
                        //      LV, L+V == possible
                        int LIndex = initials.indexOf(finals[TIndex]);
                        if (LIndex >= 0 && LIndex < LCount) {

                            // Remove the previous final jamo from the syllable,
                            // making the current syllable of form LV
                            last = QChar((int)lastUnicode - TIndex);
                            result.replace(result.length() - 1, 1, last);

                            // Make new syllable of form LV
                            last = QChar(SBase + (LIndex * VCount + VIndex) * TCount);
                            result.append(last);
                            continue;
                        }

                        // Check to see if the current final Jamo is double consonant.
                        // In this scenario, the double consonant is split into parts
                        // and the second part is removed from the current syllable.
                        // Then the second part is joined with the current vowel making
                        // the new syllable of form LV.
                        ushort key = findDoubleFinal((HangulFinalIndex)TIndex);
                        if (key) {

                            // Split the consonant into two jamos and remove the
                            // second jamo B from the current syllable
                            HangulFinalIndex TIndexA, TIndexB;
                            unpackDoubleFinal(key, TIndexA, TIndexB);
                            last = QChar((int)lastUnicode - TIndex + (int)TIndexA);
                            result.replace(result.length() - 1, 1, last);

                            // Add new syllable by combining the initial jamo
                            // and the current vowel
                            LIndex = initials.indexOf(finals[TIndexB]);
                            last = QChar(SBase + (LIndex * VCount + VIndex) * TCount);
                            result.append(last);
                            continue;
                        }
                    }

                    // Check whether the current consonant can connect to current
                    // consonant forming a double final consonant
                    HangulFinalIndex TIndexA = (HangulFinalIndex)TIndex;
                    if (isFinal(TIndexA)) {

                        HangulFinalIndex TIndexB = (HangulFinalIndex)finals.indexOf(unicode);
                        if (isFinal(TIndexB)) {

                            // Search the double final map if such a combination exists
                            ushort key = packDoubleFinal(TIndexA, TIndexB);
                            const auto it = doubleFinalMap.constFind(key);
                            if (it != doubleFinalMap.cend()) {

                                // Update syllable by adding the difference between
                                // the consonant indices
                                HangulFinalIndex TIndexD = it.value();
                                int TDiff = (int)TIndexD - (int)TIndexA;
                                last = QChar((int)lastUnicode + TDiff);
                                result.replace(result.length() - 1, 1, last);
                                continue;
                            }
                        }
                    }
                }

            } else {

                // The last character is not syllable.
                // Check to see if the last character is an initial consonant
                int LIndex = initials.indexOf(lastUnicode);
                if (LIndex != -1) {

                    // If the current character is medial vowel,
                    // make syllable of form LV
                    int VIndex = (int)unicode - VBase;
                    if (VIndex >= 0 && VIndex < VCount) {
                        last = QChar(SBase + (LIndex * VCount + VIndex) * TCount);
                        result.replace(result.length() - 1, 1, last);
                        continue;
                    }
                }

            }
        }

        // Otherwise, add the character into buffer
        last = ch;
        result = result.append(ch);
    }
    return result;
}

bool Hangul::isJamo(const ushort &unicode)
{
    return unicode >= 0x3131 && unicode <= 0x3163;
}

bool Hangul::isMedial(HangulMedialIndex vowel)
{
    return vowel >= HANGUL_MEDIAL_A && vowel <= HANGUL_MEDIAL_I;
}

bool Hangul::isFinal(HangulFinalIndex consonant)
{
    return consonant >= HANGUL_FINAL_KIYEOK && consonant <= HANGUL_FINAL_HIEUH;
}

ushort Hangul::findDoubleMedial(HangulMedialIndex vowel)
{
    return doubleMedialMap.key(vowel, 0);
}

ushort Hangul::findDoubleFinal(HangulFinalIndex consonant)
{
    return doubleFinalMap.key(consonant, 0);
}

// Packs two Hangul Jamo indices into 16-bit integer.
// The result can be used as a key to the double jamos lookup table.
// Note: The returned value is not a Unicode character!
ushort Hangul::packDoubleMedial(HangulMedialIndex a, HangulMedialIndex b)
{
    Q_ASSERT(isMedial(a));
    Q_ASSERT(isMedial(b));
    return (ushort)a | ((ushort)b << 8);
}

ushort Hangul::packDoubleFinal(HangulFinalIndex a, HangulFinalIndex b)
{
    Q_ASSERT(isFinal(a));
    Q_ASSERT(isFinal(b));
    return (ushort)a | ((ushort)b << 8);
}

void Hangul::unpackDoubleMedial(ushort key, HangulMedialIndex &a, HangulMedialIndex &b)
{
    a = (HangulMedialIndex)(key & 0xFF);
    b = (HangulMedialIndex)(key >> 8);
    Q_ASSERT(isMedial(a));
    Q_ASSERT(isMedial(b));
}

void Hangul::unpackDoubleFinal(ushort key, HangulFinalIndex &a, HangulFinalIndex &b)
{
    a = (HangulFinalIndex)(key & 0xFF);
    b = (HangulFinalIndex)(key >> 8);
    Q_ASSERT(isFinal(a));
    Q_ASSERT(isFinal(b));
}

QMap<ushort, Hangul::HangulMedialIndex> Hangul::initDoubleMedialMap()
{
    QMap<ushort, HangulMedialIndex> map;
    map.insert(packDoubleMedial(HANGUL_MEDIAL_O, HANGUL_MEDIAL_A), HANGUL_MEDIAL_WA);
    map.insert(packDoubleMedial(HANGUL_MEDIAL_O, HANGUL_MEDIAL_AE), HANGUL_MEDIAL_WAE);
    map.insert(packDoubleMedial(HANGUL_MEDIAL_O, HANGUL_MEDIAL_I), HANGUL_MEDIAL_OE);
    map.insert(packDoubleMedial(HANGUL_MEDIAL_U, HANGUL_MEDIAL_EO), HANGUL_MEDIAL_WEO);
    map.insert(packDoubleMedial(HANGUL_MEDIAL_U, HANGUL_MEDIAL_E), HANGUL_MEDIAL_WE);
    map.insert(packDoubleMedial(HANGUL_MEDIAL_U, HANGUL_MEDIAL_I), HANGUL_MEDIAL_WI);
    map.insert(packDoubleMedial(HANGUL_MEDIAL_EU, HANGUL_MEDIAL_I), HANGUL_MEDIAL_YI);
    return map;
}

QMap<ushort, Hangul::HangulFinalIndex> Hangul::initDoubleFinalMap()
{
    QMap<ushort, HangulFinalIndex> map;
    map.insert(packDoubleFinal(HANGUL_FINAL_KIYEOK, HANGUL_FINAL_SIOS), HANGUL_FINAL_KIYEOK_SIOS);
    map.insert(packDoubleFinal(HANGUL_FINAL_NIEUN, HANGUL_FINAL_CIEUC), HANGUL_FINAL_NIEUN_CIEUC);
    map.insert(packDoubleFinal(HANGUL_FINAL_NIEUN, HANGUL_FINAL_HIEUH), HANGUL_FINAL_NIEUN_HIEUH);
    map.insert(packDoubleFinal(HANGUL_FINAL_RIEUL, HANGUL_FINAL_KIYEOK), HANGUL_FINAL_RIEUL_KIYEOK);
    map.insert(packDoubleFinal(HANGUL_FINAL_RIEUL, HANGUL_FINAL_MIEUM), HANGUL_FINAL_RIEUL_MIEUM);
    map.insert(packDoubleFinal(HANGUL_FINAL_RIEUL, HANGUL_FINAL_PIEUP), HANGUL_FINAL_RIEUL_PIEUP);
    map.insert(packDoubleFinal(HANGUL_FINAL_RIEUL, HANGUL_FINAL_SIOS), HANGUL_FINAL_RIEUL_SIOS);
    map.insert(packDoubleFinal(HANGUL_FINAL_RIEUL, HANGUL_FINAL_THIEUTH), HANGUL_FINAL_RIEUL_THIEUTH);
    map.insert(packDoubleFinal(HANGUL_FINAL_RIEUL, HANGUL_FINAL_PHIEUPH), HANGUL_FINAL_RIEUL_PHIEUPH);
    map.insert(packDoubleFinal(HANGUL_FINAL_RIEUL, HANGUL_FINAL_HIEUH), HANGUL_FINAL_RIEUL_HIEUH);
    map.insert(packDoubleFinal(HANGUL_FINAL_PIEUP, HANGUL_FINAL_SIOS), HANGUL_FINAL_PIEUP_SIOS);
    map.insert(packDoubleFinal(HANGUL_FINAL_SIOS, HANGUL_FINAL_SIOS), HANGUL_FINAL_SSANGSIOS);
    return map;
}

} // namespace QtVirtualKeyboard
