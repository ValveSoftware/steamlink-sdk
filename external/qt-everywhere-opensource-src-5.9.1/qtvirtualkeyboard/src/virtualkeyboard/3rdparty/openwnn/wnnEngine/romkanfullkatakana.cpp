/*
 * Qt implementation of OpenWnn library
 * This file is part of the Qt Virtual Keyboard module.
 * Contact: http://www.qt.io/licensing/
 *
 * Copyright (C) 2015  The Qt Company
 * Copyright (C) 2008-2012  OMRON SOFTWARE Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "romkanfullkatakana.h"
#include "strsegment.h"
#include <QMap>

#include <QtCore/private/qobject_p.h>

class RomkanFullKatakanaPrivate : public QObjectPrivate
{
public:
    /** HashMap for Romaji-to-Kana conversion (Japanese mode) */
    static QMap<QString, QString> initRomkanTable()
    {
        QMap<QString, QString> map;
        map.insert(QStringLiteral("la"), QStringLiteral("\u30a1"));
        map.insert(QStringLiteral("xa"), QStringLiteral("\u30a1"));
        map.insert(QStringLiteral("a"), QStringLiteral("\u30a2"));
        map.insert(QStringLiteral("li"), QStringLiteral("\u30a3"));
        map.insert(QStringLiteral("lyi"), QStringLiteral("\u30a3"));
        map.insert(QStringLiteral("xi"), QStringLiteral("\u30a3"));
        map.insert(QStringLiteral("xyi"), QStringLiteral("\u30a3"));
        map.insert(QStringLiteral("i"), QStringLiteral("\u30a4"));
        map.insert(QStringLiteral("yi"), QStringLiteral("\u30a4"));
        map.insert(QStringLiteral("ye"), QStringLiteral("\u30a4\u30a7"));
        map.insert(QStringLiteral("lu"), QStringLiteral("\u30a5"));
        map.insert(QStringLiteral("xu"), QStringLiteral("\u30a5"));
        map.insert(QStringLiteral("u"), QStringLiteral("\u30a6"));
        map.insert(QStringLiteral("whu"), QStringLiteral("\u30a6"));
        map.insert(QStringLiteral("wu"), QStringLiteral("\u30a6"));
        map.insert(QStringLiteral("wha"), QStringLiteral("\u30a6\u30a1"));
        map.insert(QStringLiteral("whi"), QStringLiteral("\u30a6\u30a3"));
        map.insert(QStringLiteral("wi"), QStringLiteral("\u30a6\u30a3"));
        map.insert(QStringLiteral("we"), QStringLiteral("\u30a6\u30a7"));
        map.insert(QStringLiteral("whe"), QStringLiteral("\u30a6\u30a7"));
        map.insert(QStringLiteral("who"), QStringLiteral("\u30a6\u30a9"));
        map.insert(QStringLiteral("le"), QStringLiteral("\u30a7"));
        map.insert(QStringLiteral("lye"), QStringLiteral("\u30a7"));
        map.insert(QStringLiteral("xe"), QStringLiteral("\u30a7"));
        map.insert(QStringLiteral("xye"), QStringLiteral("\u30a7"));
        map.insert(QStringLiteral("e"), QStringLiteral("\u30a8"));
        map.insert(QStringLiteral("lo"), QStringLiteral("\u30a9"));
        map.insert(QStringLiteral("xo"), QStringLiteral("\u30a9"));
        map.insert(QStringLiteral("o"), QStringLiteral("\u30aa"));
        map.insert(QStringLiteral("ca"), QStringLiteral("\u30ab"));
        map.insert(QStringLiteral("lka"), QStringLiteral("\u30f5"));
        map.insert(QStringLiteral("xka"), QStringLiteral("\u30f5"));
        map.insert(QStringLiteral("ka"), QStringLiteral("\u30ab"));
        map.insert(QStringLiteral("ga"), QStringLiteral("\u30ac"));
        map.insert(QStringLiteral("ki"), QStringLiteral("\u30ad"));
        map.insert(QStringLiteral("kyi"), QStringLiteral("\u30ad\u30a3"));
        map.insert(QStringLiteral("kye"), QStringLiteral("\u30ad\u30a7"));
        map.insert(QStringLiteral("kya"), QStringLiteral("\u30ad\u30e3"));
        map.insert(QStringLiteral("kyu"), QStringLiteral("\u30ad\u30e5"));
        map.insert(QStringLiteral("kyo"), QStringLiteral("\u30ad\u30e7"));
        map.insert(QStringLiteral("gi"), QStringLiteral("\u30ae"));
        map.insert(QStringLiteral("gyi"), QStringLiteral("\u30ae\u30a3"));
        map.insert(QStringLiteral("gye"), QStringLiteral("\u30ae\u30a7"));
        map.insert(QStringLiteral("gya"), QStringLiteral("\u30ae\u30e3"));
        map.insert(QStringLiteral("gyu"), QStringLiteral("\u30ae\u30e5"));
        map.insert(QStringLiteral("gyo"), QStringLiteral("\u30ae\u30e7"));
        map.insert(QStringLiteral("cu"), QStringLiteral("\u30af"));
        map.insert(QStringLiteral("ku"), QStringLiteral("\u30af"));
        map.insert(QStringLiteral("qu"), QStringLiteral("\u30af"));
        map.insert(QStringLiteral("kwa"), QStringLiteral("\u30af\u30a1"));
        map.insert(QStringLiteral("qa"), QStringLiteral("\u30af\u30a1"));
        map.insert(QStringLiteral("qwa"), QStringLiteral("\u30af\u30a1"));
        map.insert(QStringLiteral("qi"), QStringLiteral("\u30af\u30a3"));
        map.insert(QStringLiteral("qwi"), QStringLiteral("\u30af\u30a3"));
        map.insert(QStringLiteral("qyi"), QStringLiteral("\u30af\u30a3"));
        map.insert(QStringLiteral("qwu"), QStringLiteral("\u30af\u30a5"));
        map.insert(QStringLiteral("qe"), QStringLiteral("\u30af\u30a7"));
        map.insert(QStringLiteral("qwe"), QStringLiteral("\u30af\u30a7"));
        map.insert(QStringLiteral("qye"), QStringLiteral("\u30af\u30a7"));
        map.insert(QStringLiteral("qo"), QStringLiteral("\u30af\u30a9"));
        map.insert(QStringLiteral("qwo"), QStringLiteral("\u30af\u30a9"));
        map.insert(QStringLiteral("qya"), QStringLiteral("\u30af\u30e3"));
        map.insert(QStringLiteral("qyu"), QStringLiteral("\u30af\u30e5"));
        map.insert(QStringLiteral("qyo"), QStringLiteral("\u30af\u30e7"));
        map.insert(QStringLiteral("gu"), QStringLiteral("\u30b0"));
        map.insert(QStringLiteral("gwa"), QStringLiteral("\u30b0\u30a1"));
        map.insert(QStringLiteral("gwi"), QStringLiteral("\u30b0\u30a3"));
        map.insert(QStringLiteral("gwu"), QStringLiteral("\u30b0\u30a5"));
        map.insert(QStringLiteral("gwe"), QStringLiteral("\u30b0\u30a7"));
        map.insert(QStringLiteral("gwo"), QStringLiteral("\u30b0\u30a9"));
        map.insert(QStringLiteral("lke"), QStringLiteral("\u30f6"));
        map.insert(QStringLiteral("xke"), QStringLiteral("\u30f6"));
        map.insert(QStringLiteral("ke"), QStringLiteral("\u30b1"));
        map.insert(QStringLiteral("ge"), QStringLiteral("\u30b2"));
        map.insert(QStringLiteral("co"), QStringLiteral("\u30b3"));
        map.insert(QStringLiteral("ko"), QStringLiteral("\u30b3"));
        map.insert(QStringLiteral("go"), QStringLiteral("\u30b4"));
        map.insert(QStringLiteral("sa"), QStringLiteral("\u30b5"));
        map.insert(QStringLiteral("za"), QStringLiteral("\u30b6"));
        map.insert(QStringLiteral("ci"), QStringLiteral("\u30b7"));
        map.insert(QStringLiteral("shi"), QStringLiteral("\u30b7"));
        map.insert(QStringLiteral("si"), QStringLiteral("\u30b7"));
        map.insert(QStringLiteral("syi"), QStringLiteral("\u30b7\u30a3"));
        map.insert(QStringLiteral("she"), QStringLiteral("\u30b7\u30a7"));
        map.insert(QStringLiteral("sye"), QStringLiteral("\u30b7\u30a7"));
        map.insert(QStringLiteral("sha"), QStringLiteral("\u30b7\u30e3"));
        map.insert(QStringLiteral("sya"), QStringLiteral("\u30b7\u30e3"));
        map.insert(QStringLiteral("shu"), QStringLiteral("\u30b7\u30e5"));
        map.insert(QStringLiteral("syu"), QStringLiteral("\u30b7\u30e5"));
        map.insert(QStringLiteral("sho"), QStringLiteral("\u30b7\u30e7"));
        map.insert(QStringLiteral("syo"), QStringLiteral("\u30b7\u30e7"));
        map.insert(QStringLiteral("ji"), QStringLiteral("\u30b8"));
        map.insert(QStringLiteral("zi"), QStringLiteral("\u30b8"));
        map.insert(QStringLiteral("jyi"), QStringLiteral("\u30b8\u30a3"));
        map.insert(QStringLiteral("zyi"), QStringLiteral("\u30b8\u30a3"));
        map.insert(QStringLiteral("je"), QStringLiteral("\u30b8\u30a7"));
        map.insert(QStringLiteral("jye"), QStringLiteral("\u30b8\u30a7"));
        map.insert(QStringLiteral("zye"), QStringLiteral("\u30b8\u30a7"));
        map.insert(QStringLiteral("ja"), QStringLiteral("\u30b8\u30e3"));
        map.insert(QStringLiteral("jya"), QStringLiteral("\u30b8\u30e3"));
        map.insert(QStringLiteral("zya"), QStringLiteral("\u30b8\u30e3"));
        map.insert(QStringLiteral("ju"), QStringLiteral("\u30b8\u30e5"));
        map.insert(QStringLiteral("jyu"), QStringLiteral("\u30b8\u30e5"));
        map.insert(QStringLiteral("zyu"), QStringLiteral("\u30b8\u30e5"));
        map.insert(QStringLiteral("jo"), QStringLiteral("\u30b8\u30e7"));
        map.insert(QStringLiteral("jyo"), QStringLiteral("\u30b8\u30e7"));
        map.insert(QStringLiteral("zyo"), QStringLiteral("\u30b8\u30e7"));
        map.insert(QStringLiteral("su"), QStringLiteral("\u30b9"));
        map.insert(QStringLiteral("swa"), QStringLiteral("\u30b9\u30a1"));
        map.insert(QStringLiteral("swi"), QStringLiteral("\u30b9\u30a3"));
        map.insert(QStringLiteral("swu"), QStringLiteral("\u30b9\u30a5"));
        map.insert(QStringLiteral("swe"), QStringLiteral("\u30b9\u30a7"));
        map.insert(QStringLiteral("swo"), QStringLiteral("\u30b9\u30a9"));
        map.insert(QStringLiteral("zu"), QStringLiteral("\u30ba"));
        map.insert(QStringLiteral("ce"), QStringLiteral("\u30bb"));
        map.insert(QStringLiteral("se"), QStringLiteral("\u30bb"));
        map.insert(QStringLiteral("ze"), QStringLiteral("\u30bc"));
        map.insert(QStringLiteral("so"), QStringLiteral("\u30bd"));
        map.insert(QStringLiteral("zo"), QStringLiteral("\u30be"));
        map.insert(QStringLiteral("ta"), QStringLiteral("\u30bf"));
        map.insert(QStringLiteral("da"), QStringLiteral("\u30c0"));
        map.insert(QStringLiteral("chi"), QStringLiteral("\u30c1"));
        map.insert(QStringLiteral("ti"), QStringLiteral("\u30c1"));
        map.insert(QStringLiteral("cyi"), QStringLiteral("\u30c1\u30a3"));
        map.insert(QStringLiteral("tyi"), QStringLiteral("\u30c1\u30a3"));
        map.insert(QStringLiteral("che"), QStringLiteral("\u30c1\u30a7"));
        map.insert(QStringLiteral("cye"), QStringLiteral("\u30c1\u30a7"));
        map.insert(QStringLiteral("tye"), QStringLiteral("\u30c1\u30a7"));
        map.insert(QStringLiteral("cha"), QStringLiteral("\u30c1\u30e3"));
        map.insert(QStringLiteral("cya"), QStringLiteral("\u30c1\u30e3"));
        map.insert(QStringLiteral("tya"), QStringLiteral("\u30c1\u30e3"));
        map.insert(QStringLiteral("chu"), QStringLiteral("\u30c1\u30e5"));
        map.insert(QStringLiteral("cyu"), QStringLiteral("\u30c1\u30e5"));
        map.insert(QStringLiteral("tyu"), QStringLiteral("\u30c1\u30e5"));
        map.insert(QStringLiteral("cho"), QStringLiteral("\u30c1\u30e7"));
        map.insert(QStringLiteral("cyo"), QStringLiteral("\u30c1\u30e7"));
        map.insert(QStringLiteral("tyo"), QStringLiteral("\u30c1\u30e7"));
        map.insert(QStringLiteral("di"), QStringLiteral("\u30c2"));
        map.insert(QStringLiteral("dyi"), QStringLiteral("\u30c2\u30a3"));
        map.insert(QStringLiteral("dye"), QStringLiteral("\u30c2\u30a7"));
        map.insert(QStringLiteral("dya"), QStringLiteral("\u30c2\u30e3"));
        map.insert(QStringLiteral("dyu"), QStringLiteral("\u30c2\u30e5"));
        map.insert(QStringLiteral("dyo"), QStringLiteral("\u30c2\u30e7"));
        map.insert(QStringLiteral("ltsu"), QStringLiteral("\u30c3"));
        map.insert(QStringLiteral("ltu"), QStringLiteral("\u30c3"));
        map.insert(QStringLiteral("xtu"), QStringLiteral("\u30c3"));
        map.insert(QStringLiteral(""), QStringLiteral("\u30c3"));
        map.insert(QStringLiteral("tsu"), QStringLiteral("\u30c4"));
        map.insert(QStringLiteral("tu"), QStringLiteral("\u30c4"));
        map.insert(QStringLiteral("tsa"), QStringLiteral("\u30c4\u30a1"));
        map.insert(QStringLiteral("tsi"), QStringLiteral("\u30c4\u30a3"));
        map.insert(QStringLiteral("tse"), QStringLiteral("\u30c4\u30a7"));
        map.insert(QStringLiteral("tso"), QStringLiteral("\u30c4\u30a9"));
        map.insert(QStringLiteral("du"), QStringLiteral("\u30c5"));
        map.insert(QStringLiteral("te"), QStringLiteral("\u30c6"));
        map.insert(QStringLiteral("thi"), QStringLiteral("\u30c6\u30a3"));
        map.insert(QStringLiteral("the"), QStringLiteral("\u30c6\u30a7"));
        map.insert(QStringLiteral("tha"), QStringLiteral("\u30c6\u30e3"));
        map.insert(QStringLiteral("thu"), QStringLiteral("\u30c6\u30e5"));
        map.insert(QStringLiteral("tho"), QStringLiteral("\u30c6\u30e7"));
        map.insert(QStringLiteral("de"), QStringLiteral("\u30c7"));
        map.insert(QStringLiteral("dhi"), QStringLiteral("\u30c7\u30a3"));
        map.insert(QStringLiteral("dhe"), QStringLiteral("\u30c7\u30a7"));
        map.insert(QStringLiteral("dha"), QStringLiteral("\u30c7\u30e3"));
        map.insert(QStringLiteral("dhu"), QStringLiteral("\u30c7\u30e5"));
        map.insert(QStringLiteral("dho"), QStringLiteral("\u30c7\u30e7"));
        map.insert(QStringLiteral("to"), QStringLiteral("\u30c8"));
        map.insert(QStringLiteral("twa"), QStringLiteral("\u30c8\u30a1"));
        map.insert(QStringLiteral("twi"), QStringLiteral("\u30c8\u30a3"));
        map.insert(QStringLiteral("twu"), QStringLiteral("\u30c8\u30a5"));
        map.insert(QStringLiteral("twe"), QStringLiteral("\u30c8\u30a7"));
        map.insert(QStringLiteral("two"), QStringLiteral("\u30c8\u30a9"));
        map.insert(QStringLiteral("do"), QStringLiteral("\u30c9"));
        map.insert(QStringLiteral("dwa"), QStringLiteral("\u30c9\u30a1"));
        map.insert(QStringLiteral("dwi"), QStringLiteral("\u30c9\u30a3"));
        map.insert(QStringLiteral("dwu"), QStringLiteral("\u30c9\u30a5"));
        map.insert(QStringLiteral("dwe"), QStringLiteral("\u30c9\u30a7"));
        map.insert(QStringLiteral("dwo"), QStringLiteral("\u30c9\u30a9"));
        map.insert(QStringLiteral("na"), QStringLiteral("\u30ca"));
        map.insert(QStringLiteral("ni"), QStringLiteral("\u30cb"));
        map.insert(QStringLiteral("nyi"), QStringLiteral("\u30cb\u30a3"));
        map.insert(QStringLiteral("nye"), QStringLiteral("\u30cb\u30a7"));
        map.insert(QStringLiteral("nya"), QStringLiteral("\u30cb\u30e3"));
        map.insert(QStringLiteral("nyu"), QStringLiteral("\u30cb\u30e5"));
        map.insert(QStringLiteral("nyo"), QStringLiteral("\u30cb\u30e7"));
        map.insert(QStringLiteral("nu"), QStringLiteral("\u30cc"));
        map.insert(QStringLiteral("ne"), QStringLiteral("\u30cd"));
        map.insert(QStringLiteral("no"), QStringLiteral("\u30ce"));
        map.insert(QStringLiteral("ha"), QStringLiteral("\u30cf"));
        map.insert(QStringLiteral("ba"), QStringLiteral("\u30d0"));
        map.insert(QStringLiteral("pa"), QStringLiteral("\u30d1"));
        map.insert(QStringLiteral("hi"), QStringLiteral("\u30d2"));
        map.insert(QStringLiteral("hyi"), QStringLiteral("\u30d2\u30a3"));
        map.insert(QStringLiteral("hye"), QStringLiteral("\u30d2\u30a7"));
        map.insert(QStringLiteral("hya"), QStringLiteral("\u30d2\u30e3"));
        map.insert(QStringLiteral("hyu"), QStringLiteral("\u30d2\u30e5"));
        map.insert(QStringLiteral("hyo"), QStringLiteral("\u30d2\u30e7"));
        map.insert(QStringLiteral("bi"), QStringLiteral("\u30d3"));
        map.insert(QStringLiteral("byi"), QStringLiteral("\u30d3\u30a3"));
        map.insert(QStringLiteral("bye"), QStringLiteral("\u30d3\u30a7"));
        map.insert(QStringLiteral("bya"), QStringLiteral("\u30d3\u30e3"));
        map.insert(QStringLiteral("byu"), QStringLiteral("\u30d3\u30e5"));
        map.insert(QStringLiteral("byo"), QStringLiteral("\u30d3\u30e7"));
        map.insert(QStringLiteral("pi"), QStringLiteral("\u30d4"));
        map.insert(QStringLiteral("pyi"), QStringLiteral("\u30d4\u30a3"));
        map.insert(QStringLiteral("pye"), QStringLiteral("\u30d4\u30a7"));
        map.insert(QStringLiteral("pya"), QStringLiteral("\u30d4\u30e3"));
        map.insert(QStringLiteral("pyu"), QStringLiteral("\u30d4\u30e5"));
        map.insert(QStringLiteral("pyo"), QStringLiteral("\u30d4\u30e7"));
        map.insert(QStringLiteral("fu"), QStringLiteral("\u30d5"));
        map.insert(QStringLiteral("hu"), QStringLiteral("\u30d5"));
        map.insert(QStringLiteral("fa"), QStringLiteral("\u30d5\u30a1"));
        map.insert(QStringLiteral("fwa"), QStringLiteral("\u30d5\u30a1"));
        map.insert(QStringLiteral("fi"), QStringLiteral("\u30d5\u30a3"));
        map.insert(QStringLiteral("fwi"), QStringLiteral("\u30d5\u30a3"));
        map.insert(QStringLiteral("fyi"), QStringLiteral("\u30d5\u30a3"));
        map.insert(QStringLiteral("fwu"), QStringLiteral("\u30d5\u30a5"));
        map.insert(QStringLiteral("fe"), QStringLiteral("\u30d5\u30a7"));
        map.insert(QStringLiteral("fwe"), QStringLiteral("\u30d5\u30a7"));
        map.insert(QStringLiteral("fye"), QStringLiteral("\u30d5\u30a7"));
        map.insert(QStringLiteral("fo"), QStringLiteral("\u30d5\u30a9"));
        map.insert(QStringLiteral("fwo"), QStringLiteral("\u30d5\u30a9"));
        map.insert(QStringLiteral("fya"), QStringLiteral("\u30d5\u30e3"));
        map.insert(QStringLiteral("fyu"), QStringLiteral("\u30d5\u30e5"));
        map.insert(QStringLiteral("fyo"), QStringLiteral("\u30d5\u30e7"));
        map.insert(QStringLiteral("bu"), QStringLiteral("\u30d6"));
        map.insert(QStringLiteral("pu"), QStringLiteral("\u30d7"));
        map.insert(QStringLiteral("he"), QStringLiteral("\u30d8"));
        map.insert(QStringLiteral("be"), QStringLiteral("\u30d9"));
        map.insert(QStringLiteral("pe"), QStringLiteral("\u30da"));
        map.insert(QStringLiteral("ho"), QStringLiteral("\u30db"));
        map.insert(QStringLiteral("bo"), QStringLiteral("\u30dc"));
        map.insert(QStringLiteral("po"), QStringLiteral("\u30dd"));
        map.insert(QStringLiteral("ma"), QStringLiteral("\u30de"));
        map.insert(QStringLiteral("mi"), QStringLiteral("\u30df"));
        map.insert(QStringLiteral("myi"), QStringLiteral("\u30df\u30a3"));
        map.insert(QStringLiteral("mye"), QStringLiteral("\u30df\u30a7"));
        map.insert(QStringLiteral("mya"), QStringLiteral("\u30df\u30e3"));
        map.insert(QStringLiteral("myu"), QStringLiteral("\u30df\u30e5"));
        map.insert(QStringLiteral("myo"), QStringLiteral("\u30df\u30e7"));
        map.insert(QStringLiteral("mu"), QStringLiteral("\u30e0"));
        map.insert(QStringLiteral("me"), QStringLiteral("\u30e1"));
        map.insert(QStringLiteral("mo"), QStringLiteral("\u30e2"));
        map.insert(QStringLiteral("lya"), QStringLiteral("\u30e3"));
        map.insert(QStringLiteral("xya"), QStringLiteral("\u30e3"));
        map.insert(QStringLiteral("ya"), QStringLiteral("\u30e4"));
        map.insert(QStringLiteral("lyu"), QStringLiteral("\u30e5"));
        map.insert(QStringLiteral("xyu"), QStringLiteral("\u30e5"));
        map.insert(QStringLiteral("yu"), QStringLiteral("\u30e6"));
        map.insert(QStringLiteral("lyo"), QStringLiteral("\u30e7"));
        map.insert(QStringLiteral("xyo"), QStringLiteral("\u30e7"));
        map.insert(QStringLiteral("yo"), QStringLiteral("\u30e8"));
        map.insert(QStringLiteral("ra"), QStringLiteral("\u30e9"));
        map.insert(QStringLiteral("ri"), QStringLiteral("\u30ea"));
        map.insert(QStringLiteral("ryi"), QStringLiteral("\u30ea\u30a3"));
        map.insert(QStringLiteral("rye"), QStringLiteral("\u30ea\u30a7"));
        map.insert(QStringLiteral("rya"), QStringLiteral("\u30ea\u30e3"));
        map.insert(QStringLiteral("ryu"), QStringLiteral("\u30ea\u30e5"));
        map.insert(QStringLiteral("ryo"), QStringLiteral("\u30ea\u30e7"));
        map.insert(QStringLiteral("ru"), QStringLiteral("\u30eb"));
        map.insert(QStringLiteral("re"), QStringLiteral("\u30ec"));
        map.insert(QStringLiteral("ro"), QStringLiteral("\u30ed"));
        map.insert(QStringLiteral("lwa"), QStringLiteral("\u30ee"));
        map.insert(QStringLiteral("xwa"), QStringLiteral("\u30ee"));
        map.insert(QStringLiteral("wa"), QStringLiteral("\u30ef"));
        map.insert(QStringLiteral("wo"), QStringLiteral("\u30f2"));
        map.insert(QStringLiteral("nn"), QStringLiteral("\u30f3"));
        map.insert(QStringLiteral("xn"), QStringLiteral("\u30f3"));
        map.insert(QStringLiteral("vu"), QStringLiteral("\u30f4"));
        map.insert(QStringLiteral("va"), QStringLiteral("\u30f4\u30a1"));
        map.insert(QStringLiteral("vi"), QStringLiteral("\u30f4\u30a3"));
        map.insert(QStringLiteral("vyi"), QStringLiteral("\u30f4\u30a3"));
        map.insert(QStringLiteral("ve"), QStringLiteral("\u30f4\u30a7"));
        map.insert(QStringLiteral("vye"), QStringLiteral("\u30f4\u30a7"));
        map.insert(QStringLiteral("vo"), QStringLiteral("\u30f4\u30a9"));
        map.insert(QStringLiteral("vya"), QStringLiteral("\u30f4\u30e3"));
        map.insert(QStringLiteral("vyu"), QStringLiteral("\u30f4\u30e5"));
        map.insert(QStringLiteral("vyo"), QStringLiteral("\u30f4\u30e7"));

        map.insert(QStringLiteral("bb"), QStringLiteral("\u30c3b"));
        map.insert(QStringLiteral("cc"), QStringLiteral("\u30c3c"));
        map.insert(QStringLiteral("dd"), QStringLiteral("\u30c3d"));
        map.insert(QStringLiteral("ff"), QStringLiteral("\u30c3f"));
        map.insert(QStringLiteral("gg"), QStringLiteral("\u30c3g"));
        map.insert(QStringLiteral("hh"), QStringLiteral("\u30c3h"));
        map.insert(QStringLiteral("jj"), QStringLiteral("\u30c3j"));
        map.insert(QStringLiteral("kk"), QStringLiteral("\u30c3k"));
        map.insert(QStringLiteral("ll"), QStringLiteral("\u30c3l"));
        map.insert(QStringLiteral("mm"), QStringLiteral("\u30c3m"));
        map.insert(QStringLiteral("pp"), QStringLiteral("\u30c3p"));
        map.insert(QStringLiteral("qq"), QStringLiteral("\u30c3q"));
        map.insert(QStringLiteral("rr"), QStringLiteral("\u30c3r"));
        map.insert(QStringLiteral("ss"), QStringLiteral("\u30c3s"));
        map.insert(QStringLiteral("tt"), QStringLiteral("\u30c3t"));
        map.insert(QStringLiteral("vv"), QStringLiteral("\u30c3v"));
        map.insert(QStringLiteral("ww"), QStringLiteral("\u30c3w"));
        map.insert(QStringLiteral("xx"), QStringLiteral("\u30c3x"));
        map.insert(QStringLiteral("yy"), QStringLiteral("\u30c3y"));
        map.insert(QStringLiteral("zz"), QStringLiteral("\u30c3z"));
        map.insert(QStringLiteral("nb"), QStringLiteral("\u30f3b"));
        map.insert(QStringLiteral("nc"), QStringLiteral("\u30f3c"));
        map.insert(QStringLiteral("nd"), QStringLiteral("\u30f3d"));
        map.insert(QStringLiteral("nf"), QStringLiteral("\u30f3f"));
        map.insert(QStringLiteral("ng"), QStringLiteral("\u30f3g"));
        map.insert(QStringLiteral("nh"), QStringLiteral("\u30f3h"));
        map.insert(QStringLiteral("nj"), QStringLiteral("\u30f3j"));
        map.insert(QStringLiteral("nk"), QStringLiteral("\u30f3k"));
        map.insert(QStringLiteral("nm"), QStringLiteral("\u30f3m"));
        map.insert(QStringLiteral("np"), QStringLiteral("\u30f3p"));
        map.insert(QStringLiteral("nq"), QStringLiteral("\u30f3q"));
        map.insert(QStringLiteral("nr"), QStringLiteral("\u30f3r"));
        map.insert(QStringLiteral("ns"), QStringLiteral("\u30f3s"));
        map.insert(QStringLiteral("nt"), QStringLiteral("\u30f3t"));
        map.insert(QStringLiteral("nv"), QStringLiteral("\u30f3v"));
        map.insert(QStringLiteral("nw"), QStringLiteral("\u30f3w"));
        map.insert(QStringLiteral("nx"), QStringLiteral("\u30f3x"));
        map.insert(QStringLiteral("nz"), QStringLiteral("\u30f3z"));
        map.insert(QStringLiteral("nl"), QStringLiteral("\u30f3l"));

        map.insert(QStringLiteral("-"), QStringLiteral("\u30fc"));
        map.insert(QStringLiteral("."), QStringLiteral("\u3002"));
        map.insert(QStringLiteral(","), QStringLiteral("\u3001"));
        map.insert(QStringLiteral("?"), QStringLiteral("\uff1f"));
        map.insert(QStringLiteral("/"), QStringLiteral("\u30fb"));
        return map;
    }

    static const QMap<QString, QString> romkanTable;
};

const QMap<QString, QString> RomkanFullKatakanaPrivate::romkanTable = RomkanFullKatakanaPrivate::initRomkanTable();

RomkanFullKatakana::RomkanFullKatakana(QObject *parent) :
    Romkan(*new RomkanFullKatakanaPrivate(), parent)
{
}

RomkanFullKatakana::~RomkanFullKatakana()
{
}

bool RomkanFullKatakana::convert(ComposingText &text) const
{
    return convertImpl(text, RomkanFullKatakanaPrivate::romkanTable);
}
