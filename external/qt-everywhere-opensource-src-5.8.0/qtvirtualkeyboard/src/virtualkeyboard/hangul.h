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

#ifndef HANGUL_H
#define HANGUL_H

#include <QString>
#include <QList>
#include <QMap>

namespace QtVirtualKeyboard {

class Hangul
{
    Q_DISABLE_COPY(Hangul)

    enum HangulMedialIndex {          // VIndex    Letter Jungseong   Double Jamo
        // ----------------------------------------------------------------------
        HANGUL_MEDIAL_A,              //      0      314F      1161
        HANGUL_MEDIAL_AE,             //      1      3150      1162
        HANGUL_MEDIAL_YA,             //      2      3151      1163
        HANGUL_MEDIAL_YAE,            //      3      3152      1164
        HANGUL_MEDIAL_EO,             //      4      3153      1165
        HANGUL_MEDIAL_E,              //      5      3154      1166
        HANGUL_MEDIAL_YEO,            //      6      3155      1167
        HANGUL_MEDIAL_YE,             //      7      3156      1168
        HANGUL_MEDIAL_O,              //      8      3157      1169
        HANGUL_MEDIAL_WA,             //      9      3158      116A     3157+314F
        HANGUL_MEDIAL_WAE,            //     10      3159      116B     3157+3150
        HANGUL_MEDIAL_OE,             //     11      315A      116C     3157+3163
        HANGUL_MEDIAL_YO,             //     12      315B      116D
        HANGUL_MEDIAL_U,              //     13      315C      116E
        HANGUL_MEDIAL_WEO,            //     14      315D      116F     315C+3153
        HANGUL_MEDIAL_WE,             //     15      315E      1170     315C+3154
        HANGUL_MEDIAL_WI,             //     16      315F      1171     315C+3163
        HANGUL_MEDIAL_YU,             //     17      3160      1172
        HANGUL_MEDIAL_EU,             //     18      3161      1173
        HANGUL_MEDIAL_YI,             //     19      3162      1174     3161+3163
        HANGUL_MEDIAL_I               //     20      3163      1175
    };

    enum HangulFinalIndex {           // TIndex    Letter Jongseong   Double Jamo
        // ----------------------------------------------------------------------
        HANGUL_FINAL_NONE,            //      0       n/a       n/a
        HANGUL_FINAL_KIYEOK,          //      1      3131      11A8
        HANGUL_FINAL_SSANGKIYEOK,     //      2      3132      11A9
        HANGUL_FINAL_KIYEOK_SIOS,     //      3      3133      11AA     3131+3145
        HANGUL_FINAL_NIEUN,           //      4      3134      11AB
        HANGUL_FINAL_NIEUN_CIEUC,     //      5      3135      11AC     3134+3148
        HANGUL_FINAL_NIEUN_HIEUH,     //      6      3136      11AD     3134+314E
        HANGUL_FINAL_TIKEUT,          //      7      3137      11AE
        HANGUL_FINAL_RIEUL,           //      8      3139      11AF
        HANGUL_FINAL_RIEUL_KIYEOK,    //      9      313A      11B0     3139+3131
        HANGUL_FINAL_RIEUL_MIEUM,     //     10      313B      11B1     3139+3141
        HANGUL_FINAL_RIEUL_PIEUP,     //     11      313C      11B2     3139+3142
        HANGUL_FINAL_RIEUL_SIOS,      //     12      313D      11B3     3139+3145
        HANGUL_FINAL_RIEUL_THIEUTH,   //     13      313E      11B4     3139+314C
        HANGUL_FINAL_RIEUL_PHIEUPH,   //     14      313F      11B5     3139+314D
        HANGUL_FINAL_RIEUL_HIEUH,     //     15      3140      11B6     3139+314E
        HANGUL_FINAL_MIEUM,           //     16      3141      11B7
        HANGUL_FINAL_PIEUP,           //     17      3142      11B8
        HANGUL_FINAL_PIEUP_SIOS,      //     18      3144      11B9     3142+3145
        HANGUL_FINAL_SIOS,            //     19      3145      11BA
        HANGUL_FINAL_SSANGSIOS,       //     20      3146      11BB     3145+3145
        HANGUL_FINAL_IEUNG,           //     21      3147      11BC
        HANGUL_FINAL_CIEUC,           //     22      3148      11BD
        HANGUL_FINAL_CHIEUCH,         //     23      314A      11BE
        HANGUL_FINAL_KHIEUKH,         //     24      314B      11BF
        HANGUL_FINAL_THIEUTH,         //     25      314C      11C0
        HANGUL_FINAL_PHIEUPH,         //     26      314D      11C1
        HANGUL_FINAL_HIEUH            //     27      314E      11C2
    };

    Hangul();

public:
    static QString decompose(const QString &source);
    static QString compose(const QString &source);
    static bool isJamo(const ushort &unicode);

private:
    static bool isMedial(HangulMedialIndex vowel);
    static bool isFinal(HangulFinalIndex consonant);
    static ushort findDoubleMedial(HangulMedialIndex vowel);
    static ushort findDoubleFinal(HangulFinalIndex consonant);
    static ushort packDoubleMedial(HangulMedialIndex a, HangulMedialIndex b);
    static ushort packDoubleFinal(HangulFinalIndex a, HangulFinalIndex b);
    static void unpackDoubleMedial(ushort key, HangulMedialIndex &a, HangulMedialIndex &b);
    static void unpackDoubleFinal(ushort key, HangulFinalIndex &a, HangulFinalIndex &b);
    static QMap<ushort, HangulMedialIndex> initDoubleMedialMap();
    static QMap<ushort, HangulFinalIndex> initDoubleFinalMap();

    static const QList<ushort> initials;
    static const QList<ushort> finals;
    static const QMap<ushort, HangulMedialIndex> doubleMedialMap;
    static const QMap<ushort, HangulFinalIndex> doubleFinalMap;
    static const int SBase;
    static const int LBase;
    static const int VBase;
    static const int TBase;
    static const int LCount;
    static const int VCount;
    static const int TCount;
    static const int NCount;
    static const int SCount;
};

} // namespace QtVirtualKeyboard

#endif
