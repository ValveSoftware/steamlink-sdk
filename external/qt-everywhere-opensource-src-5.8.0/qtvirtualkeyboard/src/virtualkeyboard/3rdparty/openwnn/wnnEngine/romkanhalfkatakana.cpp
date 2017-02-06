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

#include "romkanhalfkatakana.h"
#include "strsegment.h"
#include <QMap>

#include <QtCore/private/qobject_p.h>

class RomkanHalfKatakanaPrivate : public QObjectPrivate
{
public:
    /** HashMap for Romaji-to-Kana conversion (Japanese mode) */
    static QMap<QString, QString> initRomkanTable()
    {
        QMap<QString, QString> map;
        map.insert(QStringLiteral("la"), QStringLiteral("\uff67"));
        map.insert(QStringLiteral("xa"), QStringLiteral("\uff67"));
        map.insert(QStringLiteral("a"), QStringLiteral("\uff71"));
        map.insert(QStringLiteral("li"), QStringLiteral("\uff68"));
        map.insert(QStringLiteral("lyi"), QStringLiteral("\uff68"));
        map.insert(QStringLiteral("xi"), QStringLiteral("\uff68"));
        map.insert(QStringLiteral("xyi"), QStringLiteral("\uff68"));
        map.insert(QStringLiteral("i"), QStringLiteral("\uff72"));
        map.insert(QStringLiteral("yi"), QStringLiteral("\uff72"));
        map.insert(QStringLiteral("ye"), QStringLiteral("\uff72\uff6a"));
        map.insert(QStringLiteral("lu"), QStringLiteral("\uff69"));
        map.insert(QStringLiteral("xu"), QStringLiteral("\uff69"));
        map.insert(QStringLiteral("u"), QStringLiteral("\uff73"));
        map.insert(QStringLiteral("whu"), QStringLiteral("\uff73"));
        map.insert(QStringLiteral("wu"), QStringLiteral("\uff73"));
        map.insert(QStringLiteral("wha"), QStringLiteral("\uff73\uff67"));
        map.insert(QStringLiteral("whi"), QStringLiteral("\uff73\uff68"));
        map.insert(QStringLiteral("wi"), QStringLiteral("\uff73\uff68"));
        map.insert(QStringLiteral("we"), QStringLiteral("\uff73\uff6a"));
        map.insert(QStringLiteral("whe"), QStringLiteral("\uff73\uff6a"));
        map.insert(QStringLiteral("who"), QStringLiteral("\uff73\uff6b"));
        map.insert(QStringLiteral("le"), QStringLiteral("\uff6a"));
        map.insert(QStringLiteral("lye"), QStringLiteral("\uff6a"));
        map.insert(QStringLiteral("xe"), QStringLiteral("\uff6a"));
        map.insert(QStringLiteral("xye"), QStringLiteral("\uff6a"));
        map.insert(QStringLiteral("e"), QStringLiteral("\uff74"));
        map.insert(QStringLiteral("lo"), QStringLiteral("\uff6b"));
        map.insert(QStringLiteral("xo"), QStringLiteral("\uff6b"));
        map.insert(QStringLiteral("o"), QStringLiteral("\uff75"));
        map.insert(QStringLiteral("ca"), QStringLiteral("\uff76"));
        map.insert(QStringLiteral("ka"), QStringLiteral("\uff76"));
        map.insert(QStringLiteral("ga"), QStringLiteral("\uff76\uff9e"));
        map.insert(QStringLiteral("ki"), QStringLiteral("\uff77"));
        map.insert(QStringLiteral("kyi"), QStringLiteral("\uff77\uff68"));
        map.insert(QStringLiteral("kye"), QStringLiteral("\uff77\uff6a"));
        map.insert(QStringLiteral("kya"), QStringLiteral("\uff77\uff6c"));
        map.insert(QStringLiteral("kyu"), QStringLiteral("\uff77\uff6d"));
        map.insert(QStringLiteral("kyo"), QStringLiteral("\uff77\uff6e"));
        map.insert(QStringLiteral("gi"), QStringLiteral("\uff77\uff9e"));
        map.insert(QStringLiteral("gyi"), QStringLiteral("\uff77\uff9e\uff68"));
        map.insert(QStringLiteral("gye"), QStringLiteral("\uff77\uff9e\uff6a"));
        map.insert(QStringLiteral("gya"), QStringLiteral("\uff77\uff9e\uff6c"));
        map.insert(QStringLiteral("gyu"), QStringLiteral("\uff77\uff9e\uff6d"));
        map.insert(QStringLiteral("gyo"), QStringLiteral("\uff77\uff9e\uff6e"));
        map.insert(QStringLiteral("cu"), QStringLiteral("\uff78"));
        map.insert(QStringLiteral("ku"), QStringLiteral("\uff78"));
        map.insert(QStringLiteral("qu"), QStringLiteral("\uff78"));
        map.insert(QStringLiteral("kwa"), QStringLiteral("\uff78\uff67"));
        map.insert(QStringLiteral("qa"), QStringLiteral("\uff78\uff67"));
        map.insert(QStringLiteral("qwa"), QStringLiteral("\uff78\uff67"));
        map.insert(QStringLiteral("qi"), QStringLiteral("\uff78\uff68"));
        map.insert(QStringLiteral("qwi"), QStringLiteral("\uff78\uff68"));
        map.insert(QStringLiteral("qyi"), QStringLiteral("\uff78\uff68"));
        map.insert(QStringLiteral("qwu"), QStringLiteral("\uff78\uff69"));
        map.insert(QStringLiteral("qe"), QStringLiteral("\uff78\uff6a"));
        map.insert(QStringLiteral("qwe"), QStringLiteral("\uff78\uff6a"));
        map.insert(QStringLiteral("qye"), QStringLiteral("\uff78\uff6a"));
        map.insert(QStringLiteral("qo"), QStringLiteral("\uff78\uff6b"));
        map.insert(QStringLiteral("qwo"), QStringLiteral("\uff78\uff6b"));
        map.insert(QStringLiteral("qya"), QStringLiteral("\uff78\uff6c"));
        map.insert(QStringLiteral("qyu"), QStringLiteral("\uff78\uff6d"));
        map.insert(QStringLiteral("qyo"), QStringLiteral("\uff78\uff6e"));
        map.insert(QStringLiteral("gu"), QStringLiteral("\uff78\uff9e"));
        map.insert(QStringLiteral("gwa"), QStringLiteral("\uff78\uff9e\uff67"));
        map.insert(QStringLiteral("gwi"), QStringLiteral("\uff78\uff9e\uff68"));
        map.insert(QStringLiteral("gwu"), QStringLiteral("\uff78\uff9e\uff69"));
        map.insert(QStringLiteral("gwe"), QStringLiteral("\uff78\uff9e\uff6a"));
        map.insert(QStringLiteral("gwo"), QStringLiteral("\uff78\uff9e\uff6b"));
        map.insert(QStringLiteral("ke"), QStringLiteral("\uff79"));
        map.insert(QStringLiteral("ge"), QStringLiteral("\uff79\uff9e"));
        map.insert(QStringLiteral("co"), QStringLiteral("\uff7a"));
        map.insert(QStringLiteral("ko"), QStringLiteral("\uff7a"));
        map.insert(QStringLiteral("go"), QStringLiteral("\uff7a\uff9e"));
        map.insert(QStringLiteral("sa"), QStringLiteral("\uff7b"));
        map.insert(QStringLiteral("za"), QStringLiteral("\uff7b\uff9e"));
        map.insert(QStringLiteral("ci"), QStringLiteral("\uff7c"));
        map.insert(QStringLiteral("shi"), QStringLiteral("\uff7c"));
        map.insert(QStringLiteral("si"), QStringLiteral("\uff7c"));
        map.insert(QStringLiteral("syi"), QStringLiteral("\uff7c\uff68"));
        map.insert(QStringLiteral("she"), QStringLiteral("\uff7c\uff6a"));
        map.insert(QStringLiteral("sye"), QStringLiteral("\uff7c\uff6a"));
        map.insert(QStringLiteral("sha"), QStringLiteral("\uff7c\uff6c"));
        map.insert(QStringLiteral("sya"), QStringLiteral("\uff7c\uff6c"));
        map.insert(QStringLiteral("shu"), QStringLiteral("\uff7c\uff6d"));
        map.insert(QStringLiteral("syu"), QStringLiteral("\uff7c\uff6d"));
        map.insert(QStringLiteral("sho"), QStringLiteral("\uff7c\uff6e"));
        map.insert(QStringLiteral("syo"), QStringLiteral("\uff7c\uff6e"));
        map.insert(QStringLiteral("ji"), QStringLiteral("\uff7c\uff9e"));
        map.insert(QStringLiteral("zi"), QStringLiteral("\uff7c\uff9e"));
        map.insert(QStringLiteral("jyi"), QStringLiteral("\uff7c\uff9e\uff68"));
        map.insert(QStringLiteral("zyi"), QStringLiteral("\uff7c\uff9e\uff68"));
        map.insert(QStringLiteral("je"), QStringLiteral("\uff7c\uff9e\uff6a"));
        map.insert(QStringLiteral("jye"), QStringLiteral("\uff7c\uff9e\uff6a"));
        map.insert(QStringLiteral("zye"), QStringLiteral("\uff7c\uff9e\uff6a"));
        map.insert(QStringLiteral("ja"), QStringLiteral("\uff7c\uff9e\uff6c"));
        map.insert(QStringLiteral("jya"), QStringLiteral("\uff7c\uff9e\uff6c"));
        map.insert(QStringLiteral("zya"), QStringLiteral("\uff7c\uff9e\uff6c"));
        map.insert(QStringLiteral("ju"), QStringLiteral("\uff7c\uff9e\uff6d"));
        map.insert(QStringLiteral("jyu"), QStringLiteral("\uff7c\uff9e\uff6d"));
        map.insert(QStringLiteral("zyu"), QStringLiteral("\uff7c\uff9e\uff6d"));
        map.insert(QStringLiteral("jo"), QStringLiteral("\uff7c\uff9e\uff6e"));
        map.insert(QStringLiteral("jyo"), QStringLiteral("\uff7c\uff9e\uff6e"));
        map.insert(QStringLiteral("zyo"), QStringLiteral("\uff7c\uff9e\uff6e"));
        map.insert(QStringLiteral("su"), QStringLiteral("\uff7d"));
        map.insert(QStringLiteral("swa"), QStringLiteral("\uff7d\uff67"));
        map.insert(QStringLiteral("swi"), QStringLiteral("\uff7d\uff68"));
        map.insert(QStringLiteral("swu"), QStringLiteral("\uff7d\uff69"));
        map.insert(QStringLiteral("swe"), QStringLiteral("\uff7d\uff6a"));
        map.insert(QStringLiteral("swo"), QStringLiteral("\uff7d\uff6b"));
        map.insert(QStringLiteral("zu"), QStringLiteral("\uff7d\uff9e"));
        map.insert(QStringLiteral("ce"), QStringLiteral("\uff7e"));
        map.insert(QStringLiteral("se"), QStringLiteral("\uff7e"));
        map.insert(QStringLiteral("ze"), QStringLiteral("\uff7e\uff9e"));
        map.insert(QStringLiteral("so"), QStringLiteral("\uff7f"));
        map.insert(QStringLiteral("zo"), QStringLiteral("\uff7f\uff9e"));
        map.insert(QStringLiteral("ta"), QStringLiteral("\uff80"));
        map.insert(QStringLiteral("da"), QStringLiteral("\uff80\uff9e"));
        map.insert(QStringLiteral("chi"), QStringLiteral("\uff81"));
        map.insert(QStringLiteral("ti"), QStringLiteral("\uff81"));
        map.insert(QStringLiteral("cyi"), QStringLiteral("\uff81\uff68"));
        map.insert(QStringLiteral("tyi"), QStringLiteral("\uff81\uff68"));
        map.insert(QStringLiteral("che"), QStringLiteral("\uff81\uff6a"));
        map.insert(QStringLiteral("cye"), QStringLiteral("\uff81\uff6a"));
        map.insert(QStringLiteral("tye"), QStringLiteral("\uff81\uff6a"));
        map.insert(QStringLiteral("cha"), QStringLiteral("\uff81\uff6c"));
        map.insert(QStringLiteral("cya"), QStringLiteral("\uff81\uff6c"));
        map.insert(QStringLiteral("tya"), QStringLiteral("\uff81\uff6c"));
        map.insert(QStringLiteral("chu"), QStringLiteral("\uff81\uff6d"));
        map.insert(QStringLiteral("cyu"), QStringLiteral("\uff81\uff6d"));
        map.insert(QStringLiteral("tyu"), QStringLiteral("\uff81\uff6d"));
        map.insert(QStringLiteral("cho"), QStringLiteral("\uff81\uff6e"));
        map.insert(QStringLiteral("cyo"), QStringLiteral("\uff81\uff6e"));
        map.insert(QStringLiteral("tyo"), QStringLiteral("\uff81\uff6e"));
        map.insert(QStringLiteral("di"), QStringLiteral("\uff81\uff9e"));
        map.insert(QStringLiteral("dyi"), QStringLiteral("\uff81\uff9e\uff68"));
        map.insert(QStringLiteral("dye"), QStringLiteral("\uff81\uff9e\uff6a"));
        map.insert(QStringLiteral("dya"), QStringLiteral("\uff81\uff9e\uff6c"));
        map.insert(QStringLiteral("dyu"), QStringLiteral("\uff81\uff9e\uff6d"));
        map.insert(QStringLiteral("dyo"), QStringLiteral("\uff81\uff9e\uff6e"));
        map.insert(QStringLiteral("ltsu"), QStringLiteral("\uff6f"));
        map.insert(QStringLiteral("ltu"), QStringLiteral("\uff6f"));
        map.insert(QStringLiteral("xtu"), QStringLiteral("\uff6f"));
        map.insert(QStringLiteral(""), QStringLiteral("\uff6f"));
        map.insert(QStringLiteral("tsu"), QStringLiteral("\uff82"));
        map.insert(QStringLiteral("tu"), QStringLiteral("\uff82"));
        map.insert(QStringLiteral("tsa"), QStringLiteral("\uff82\uff67"));
        map.insert(QStringLiteral("tsi"), QStringLiteral("\uff82\uff68"));
        map.insert(QStringLiteral("tse"), QStringLiteral("\uff82\uff6a"));
        map.insert(QStringLiteral("tso"), QStringLiteral("\uff82\uff6b"));
        map.insert(QStringLiteral("du"), QStringLiteral("\uff82\uff9e"));
        map.insert(QStringLiteral("te"), QStringLiteral("\uff83"));
        map.insert(QStringLiteral("thi"), QStringLiteral("\uff83\uff68"));
        map.insert(QStringLiteral("the"), QStringLiteral("\uff83\uff6a"));
        map.insert(QStringLiteral("tha"), QStringLiteral("\uff83\uff6c"));
        map.insert(QStringLiteral("thu"), QStringLiteral("\uff83\uff6d"));
        map.insert(QStringLiteral("tho"), QStringLiteral("\uff83\uff6e"));
        map.insert(QStringLiteral("de"), QStringLiteral("\uff83\uff9e"));
        map.insert(QStringLiteral("dhi"), QStringLiteral("\uff83\uff9e\uff68"));
        map.insert(QStringLiteral("dhe"), QStringLiteral("\uff83\uff9e\uff6a"));
        map.insert(QStringLiteral("dha"), QStringLiteral("\uff83\uff9e\uff6c"));
        map.insert(QStringLiteral("dhu"), QStringLiteral("\uff83\uff9e\uff6d"));
        map.insert(QStringLiteral("dho"), QStringLiteral("\uff83\uff9e\uff6e"));
        map.insert(QStringLiteral("to"), QStringLiteral("\uff84"));
        map.insert(QStringLiteral("twa"), QStringLiteral("\uff84\uff67"));
        map.insert(QStringLiteral("twi"), QStringLiteral("\uff84\uff68"));
        map.insert(QStringLiteral("twu"), QStringLiteral("\uff84\uff69"));
        map.insert(QStringLiteral("twe"), QStringLiteral("\uff84\uff6a"));
        map.insert(QStringLiteral("two"), QStringLiteral("\uff84\uff6b"));
        map.insert(QStringLiteral("do"), QStringLiteral("\uff84\uff9e"));
        map.insert(QStringLiteral("dwa"), QStringLiteral("\uff84\uff9e\uff67"));
        map.insert(QStringLiteral("dwi"), QStringLiteral("\uff84\uff9e\uff68"));
        map.insert(QStringLiteral("dwu"), QStringLiteral("\uff84\uff9e\uff69"));
        map.insert(QStringLiteral("dwe"), QStringLiteral("\uff84\uff9e\uff6a"));
        map.insert(QStringLiteral("dwo"), QStringLiteral("\uff84\uff9e\uff6b"));
        map.insert(QStringLiteral("na"), QStringLiteral("\uff85"));
        map.insert(QStringLiteral("ni"), QStringLiteral("\uff86"));
        map.insert(QStringLiteral("nyi"), QStringLiteral("\uff86\uff68"));
        map.insert(QStringLiteral("nye"), QStringLiteral("\uff86\uff6a"));
        map.insert(QStringLiteral("nya"), QStringLiteral("\uff86\uff6c"));
        map.insert(QStringLiteral("nyu"), QStringLiteral("\uff86\uff6d"));
        map.insert(QStringLiteral("nyo"), QStringLiteral("\uff86\uff6e"));
        map.insert(QStringLiteral("nu"), QStringLiteral("\uff87"));
        map.insert(QStringLiteral("ne"), QStringLiteral("\uff88"));
        map.insert(QStringLiteral("no"), QStringLiteral("\uff89"));
        map.insert(QStringLiteral("ha"), QStringLiteral("\uff8a"));
        map.insert(QStringLiteral("ba"), QStringLiteral("\uff8a\uff9e"));
        map.insert(QStringLiteral("pa"), QStringLiteral("\uff8a\uff9f"));
        map.insert(QStringLiteral("hi"), QStringLiteral("\uff8b"));
        map.insert(QStringLiteral("hyi"), QStringLiteral("\uff8b\uff68"));
        map.insert(QStringLiteral("hye"), QStringLiteral("\uff8b\uff6a"));
        map.insert(QStringLiteral("hya"), QStringLiteral("\uff8b\uff6c"));
        map.insert(QStringLiteral("hyu"), QStringLiteral("\uff8b\uff6d"));
        map.insert(QStringLiteral("hyo"), QStringLiteral("\uff8b\uff6e"));
        map.insert(QStringLiteral("bi"), QStringLiteral("\uff8b\uff9e"));
        map.insert(QStringLiteral("byi"), QStringLiteral("\uff8b\uff9e\uff68"));
        map.insert(QStringLiteral("bye"), QStringLiteral("\uff8b\uff9e\uff6a"));
        map.insert(QStringLiteral("bya"), QStringLiteral("\uff8b\uff9e\uff6c"));
        map.insert(QStringLiteral("byu"), QStringLiteral("\uff8b\uff9e\uff6d"));
        map.insert(QStringLiteral("byo"), QStringLiteral("\uff8b\uff9e\uff6e"));
        map.insert(QStringLiteral("pi"), QStringLiteral("\uff8b\uff9f"));
        map.insert(QStringLiteral("pyi"), QStringLiteral("\uff8b\uff9f\uff68"));
        map.insert(QStringLiteral("pye"), QStringLiteral("\uff8b\uff9f\uff6a"));
        map.insert(QStringLiteral("pya"), QStringLiteral("\uff8b\uff9f\uff6c"));
        map.insert(QStringLiteral("pyu"), QStringLiteral("\uff8b\uff9f\uff6d"));
        map.insert(QStringLiteral("pyo"), QStringLiteral("\uff8b\uff9f\uff6e"));
        map.insert(QStringLiteral("fu"), QStringLiteral("\uff8c"));
        map.insert(QStringLiteral("hu"), QStringLiteral("\uff8c"));
        map.insert(QStringLiteral("fa"), QStringLiteral("\uff8c\uff67"));
        map.insert(QStringLiteral("fwa"), QStringLiteral("\uff8c\uff67"));
        map.insert(QStringLiteral("fi"), QStringLiteral("\uff8c\uff68"));
        map.insert(QStringLiteral("fwi"), QStringLiteral("\uff8c\uff68"));
        map.insert(QStringLiteral("fyi"), QStringLiteral("\uff8c\uff68"));
        map.insert(QStringLiteral("fwu"), QStringLiteral("\uff8c\uff69"));
        map.insert(QStringLiteral("fe"), QStringLiteral("\uff8c\uff6a"));
        map.insert(QStringLiteral("fwe"), QStringLiteral("\uff8c\uff6a"));
        map.insert(QStringLiteral("fye"), QStringLiteral("\uff8c\uff6a"));
        map.insert(QStringLiteral("fo"), QStringLiteral("\uff8c\uff6b"));
        map.insert(QStringLiteral("fwo"), QStringLiteral("\uff8c\uff6b"));
        map.insert(QStringLiteral("fya"), QStringLiteral("\uff8c\uff6c"));
        map.insert(QStringLiteral("fyu"), QStringLiteral("\uff8c\uff6d"));
        map.insert(QStringLiteral("fyo"), QStringLiteral("\uff8c\uff6e"));
        map.insert(QStringLiteral("bu"), QStringLiteral("\uff8c\uff9e"));
        map.insert(QStringLiteral("pu"), QStringLiteral("\uff8c\uff9f"));
        map.insert(QStringLiteral("he"), QStringLiteral("\uff8d"));
        map.insert(QStringLiteral("be"), QStringLiteral("\uff8d\uff9e"));
        map.insert(QStringLiteral("pe"), QStringLiteral("\uff8d\uff9f"));
        map.insert(QStringLiteral("ho"), QStringLiteral("\uff8e"));
        map.insert(QStringLiteral("bo"), QStringLiteral("\uff8e\uff9e"));
        map.insert(QStringLiteral("po"), QStringLiteral("\uff8e\uff9f"));
        map.insert(QStringLiteral("ma"), QStringLiteral("\uff8f"));
        map.insert(QStringLiteral("mi"), QStringLiteral("\uff90"));
        map.insert(QStringLiteral("myi"), QStringLiteral("\uff90\uff68"));
        map.insert(QStringLiteral("mye"), QStringLiteral("\uff90\uff6a"));
        map.insert(QStringLiteral("mya"), QStringLiteral("\uff90\uff6c"));
        map.insert(QStringLiteral("myu"), QStringLiteral("\uff90\uff6d"));
        map.insert(QStringLiteral("myo"), QStringLiteral("\uff90\uff6e"));
        map.insert(QStringLiteral("mu"), QStringLiteral("\uff91"));
        map.insert(QStringLiteral("me"), QStringLiteral("\uff92"));
        map.insert(QStringLiteral("mo"), QStringLiteral("\uff93"));
        map.insert(QStringLiteral("lya"), QStringLiteral("\uff6c"));
        map.insert(QStringLiteral("xya"), QStringLiteral("\uff6c"));
        map.insert(QStringLiteral("ya"), QStringLiteral("\uff94"));
        map.insert(QStringLiteral("lyu"), QStringLiteral("\uff6d"));
        map.insert(QStringLiteral("xyu"), QStringLiteral("\uff6d"));
        map.insert(QStringLiteral("yu"), QStringLiteral("\uff95"));
        map.insert(QStringLiteral("lyo"), QStringLiteral("\uff6e"));
        map.insert(QStringLiteral("xyo"), QStringLiteral("\uff6e"));
        map.insert(QStringLiteral("yo"), QStringLiteral("\uff96"));
        map.insert(QStringLiteral("ra"), QStringLiteral("\uff97"));
        map.insert(QStringLiteral("ri"), QStringLiteral("\uff98"));
        map.insert(QStringLiteral("ryi"), QStringLiteral("\uff98\uff68"));
        map.insert(QStringLiteral("rye"), QStringLiteral("\uff98\uff6a"));
        map.insert(QStringLiteral("rya"), QStringLiteral("\uff98\uff6c"));
        map.insert(QStringLiteral("ryu"), QStringLiteral("\uff98\uff6d"));
        map.insert(QStringLiteral("ryo"), QStringLiteral("\uff98\uff6e"));
        map.insert(QStringLiteral("ru"), QStringLiteral("\uff99"));
        map.insert(QStringLiteral("re"), QStringLiteral("\uff9a"));
        map.insert(QStringLiteral("ro"), QStringLiteral("\uff9b"));
        map.insert(QStringLiteral("lwa"), QStringLiteral("\uff9c"));
        map.insert(QStringLiteral("xwa"), QStringLiteral("\uff9c"));
        map.insert(QStringLiteral("wa"), QStringLiteral("\uff9c"));
        map.insert(QStringLiteral("wo"), QStringLiteral("\uff66"));
        map.insert(QStringLiteral("nn"), QStringLiteral("\uff9d"));
        map.insert(QStringLiteral("xn"), QStringLiteral("\uff9d"));
        map.insert(QStringLiteral("vu"), QStringLiteral("\uff73\uff9e"));
        map.insert(QStringLiteral("va"), QStringLiteral("\uff73\uff9e\uff67"));
        map.insert(QStringLiteral("vi"), QStringLiteral("\uff73\uff9e\uff68"));
        map.insert(QStringLiteral("vyi"), QStringLiteral("\uff73\uff9e\uff68"));
        map.insert(QStringLiteral("ve"), QStringLiteral("\uff73\uff9e\uff6a"));
        map.insert(QStringLiteral("vye"), QStringLiteral("\uff73\uff9e\uff6a"));
        map.insert(QStringLiteral("vo"), QStringLiteral("\uff73\uff9e\uff6b"));
        map.insert(QStringLiteral("vya"), QStringLiteral("\uff73\uff9e\uff6c"));
        map.insert(QStringLiteral("vyu"), QStringLiteral("\uff73\uff9e\uff6d"));
        map.insert(QStringLiteral("vyo"), QStringLiteral("\uff73\uff9e\uff6e"));

        map.insert(QStringLiteral("bb"), QStringLiteral("\uff6fb"));
        map.insert(QStringLiteral("cc"), QStringLiteral("\uff6fc"));
        map.insert(QStringLiteral("dd"), QStringLiteral("\uff6fd"));
        map.insert(QStringLiteral("ff"), QStringLiteral("\uff6ff"));
        map.insert(QStringLiteral("gg"), QStringLiteral("\uff6fg"));
        map.insert(QStringLiteral("hh"), QStringLiteral("\uff6fh"));
        map.insert(QStringLiteral("jj"), QStringLiteral("\uff6fj"));
        map.insert(QStringLiteral("kk"), QStringLiteral("\uff6fk"));
        map.insert(QStringLiteral("ll"), QStringLiteral("\uff6fl"));
        map.insert(QStringLiteral("mm"), QStringLiteral("\uff6fm"));
        map.insert(QStringLiteral("pp"), QStringLiteral("\uff6fp"));
        map.insert(QStringLiteral("qq"), QStringLiteral("\uff6fq"));
        map.insert(QStringLiteral("rr"), QStringLiteral("\uff6fr"));
        map.insert(QStringLiteral("ss"), QStringLiteral("\uff6fs"));
        map.insert(QStringLiteral("tt"), QStringLiteral("\uff6ft"));
        map.insert(QStringLiteral("vv"), QStringLiteral("\uff6fv"));
        map.insert(QStringLiteral("ww"), QStringLiteral("\uff6fw"));
        map.insert(QStringLiteral("xx"), QStringLiteral("\uff6fx"));
        map.insert(QStringLiteral("yy"), QStringLiteral("\uff6fy"));
        map.insert(QStringLiteral("zz"), QStringLiteral("\uff6fz"));
        map.insert(QStringLiteral("nb"), QStringLiteral("\uff9db"));
        map.insert(QStringLiteral("nc"), QStringLiteral("\uff9dc"));
        map.insert(QStringLiteral("nd"), QStringLiteral("\uff9dd"));
        map.insert(QStringLiteral("nf"), QStringLiteral("\uff9df"));
        map.insert(QStringLiteral("ng"), QStringLiteral("\uff9dg"));
        map.insert(QStringLiteral("nh"), QStringLiteral("\uff9dh"));
        map.insert(QStringLiteral("nj"), QStringLiteral("\uff9dj"));
        map.insert(QStringLiteral("nk"), QStringLiteral("\uff9dk"));
        map.insert(QStringLiteral("nm"), QStringLiteral("\uff9dm"));
        map.insert(QStringLiteral("np"), QStringLiteral("\uff9dp"));
        map.insert(QStringLiteral("nq"), QStringLiteral("\uff9dq"));
        map.insert(QStringLiteral("nr"), QStringLiteral("\uff9dr"));
        map.insert(QStringLiteral("ns"), QStringLiteral("\uff9ds"));
        map.insert(QStringLiteral("nt"), QStringLiteral("\uff9dt"));
        map.insert(QStringLiteral("nv"), QStringLiteral("\uff9dv"));
        map.insert(QStringLiteral("nw"), QStringLiteral("\uff9dw"));
        map.insert(QStringLiteral("nx"), QStringLiteral("\uff9dx"));
        map.insert(QStringLiteral("nz"), QStringLiteral("\uff9dz"));
        map.insert(QStringLiteral("nl"), QStringLiteral("\uff9dl"));

        map.insert(QStringLiteral("-"), QStringLiteral("\uff70"));
        map.insert(QStringLiteral("."), QStringLiteral("\uff61"));
        map.insert(QStringLiteral(","), QStringLiteral("\uff64"));
        map.insert(QStringLiteral("/"), QStringLiteral("\uff65"));
        return map;
    }

    static const QMap<QString, QString> romkanTable;
};

const QMap<QString, QString> RomkanHalfKatakanaPrivate::romkanTable = RomkanHalfKatakanaPrivate::initRomkanTable();

RomkanHalfKatakana::RomkanHalfKatakana(QObject *parent) :
    Romkan(*new RomkanHalfKatakanaPrivate(), parent)
{
}

RomkanHalfKatakana::~RomkanHalfKatakana()
{
}

bool RomkanHalfKatakana::convert(ComposingText &text) const
{
    return convertImpl(text, RomkanHalfKatakanaPrivate::romkanTable);
}
