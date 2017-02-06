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

#include "romkan.h"
#include "strsegment.h"
#include <QMap>

#include <QtCore/private/qobject_p.h>

class RomkanPrivate : public QObjectPrivate
{
public:
    /** HashMap for Romaji-to-Kana conversion (Japanese mode) */
    static QMap<QString, QString> initRomkanTable()
    {
        QMap<QString, QString> map;
        map.insert(QStringLiteral("la"), QStringLiteral("\u3041"));
        map.insert(QStringLiteral("xa"), QStringLiteral("\u3041"));
        map.insert(QStringLiteral("a"), QStringLiteral("\u3042"));
        map.insert(QStringLiteral("li"), QStringLiteral("\u3043"));
        map.insert(QStringLiteral("lyi"), QStringLiteral("\u3043"));
        map.insert(QStringLiteral("xi"), QStringLiteral("\u3043"));
        map.insert(QStringLiteral("xyi"), QStringLiteral("\u3043"));
        map.insert(QStringLiteral("i"), QStringLiteral("\u3044"));
        map.insert(QStringLiteral("yi"), QStringLiteral("\u3044"));
        map.insert(QStringLiteral("ye"), QStringLiteral("\u3044\u3047"));
        map.insert(QStringLiteral("lu"), QStringLiteral("\u3045"));
        map.insert(QStringLiteral("xu"), QStringLiteral("\u3045"));
        map.insert(QStringLiteral("u"), QStringLiteral("\u3046"));
        map.insert(QStringLiteral("whu"), QStringLiteral("\u3046"));
        map.insert(QStringLiteral("wu"), QStringLiteral("\u3046"));
        map.insert(QStringLiteral("wha"), QStringLiteral("\u3046\u3041"));
        map.insert(QStringLiteral("whi"), QStringLiteral("\u3046\u3043"));
        map.insert(QStringLiteral("wi"), QStringLiteral("\u3046\u3043"));
        map.insert(QStringLiteral("we"), QStringLiteral("\u3046\u3047"));
        map.insert(QStringLiteral("whe"), QStringLiteral("\u3046\u3047"));
        map.insert(QStringLiteral("who"), QStringLiteral("\u3046\u3049"));
        map.insert(QStringLiteral("le"), QStringLiteral("\u3047"));
        map.insert(QStringLiteral("lye"), QStringLiteral("\u3047"));
        map.insert(QStringLiteral("xe"), QStringLiteral("\u3047"));
        map.insert(QStringLiteral("xye"), QStringLiteral("\u3047"));
        map.insert(QStringLiteral("e"), QStringLiteral("\u3048"));
        map.insert(QStringLiteral("lo"), QStringLiteral("\u3049"));
        map.insert(QStringLiteral("xo"), QStringLiteral("\u3049"));
        map.insert(QStringLiteral("o"), QStringLiteral("\u304a"));
        map.insert(QStringLiteral("ca"), QStringLiteral("\u304b"));
        map.insert(QStringLiteral("ka"), QStringLiteral("\u304b"));
        map.insert(QStringLiteral("ga"), QStringLiteral("\u304c"));
        map.insert(QStringLiteral("ki"), QStringLiteral("\u304d"));
        map.insert(QStringLiteral("kyi"), QStringLiteral("\u304d\u3043"));
        map.insert(QStringLiteral("kye"), QStringLiteral("\u304d\u3047"));
        map.insert(QStringLiteral("kya"), QStringLiteral("\u304d\u3083"));
        map.insert(QStringLiteral("kyu"), QStringLiteral("\u304d\u3085"));
        map.insert(QStringLiteral("kyo"), QStringLiteral("\u304d\u3087"));
        map.insert(QStringLiteral("gi"), QStringLiteral("\u304e"));
        map.insert(QStringLiteral("gyi"), QStringLiteral("\u304e\u3043"));
        map.insert(QStringLiteral("gye"), QStringLiteral("\u304e\u3047"));
        map.insert(QStringLiteral("gya"), QStringLiteral("\u304e\u3083"));
        map.insert(QStringLiteral("gyu"), QStringLiteral("\u304e\u3085"));
        map.insert(QStringLiteral("gyo"), QStringLiteral("\u304e\u3087"));
        map.insert(QStringLiteral("cu"), QStringLiteral("\u304f"));
        map.insert(QStringLiteral("ku"), QStringLiteral("\u304f"));
        map.insert(QStringLiteral("qu"), QStringLiteral("\u304f"));
        map.insert(QStringLiteral("kwa"), QStringLiteral("\u304f\u3041"));
        map.insert(QStringLiteral("qa"), QStringLiteral("\u304f\u3041"));
        map.insert(QStringLiteral("qwa"), QStringLiteral("\u304f\u3041"));
        map.insert(QStringLiteral("qi"), QStringLiteral("\u304f\u3043"));
        map.insert(QStringLiteral("qwi"), QStringLiteral("\u304f\u3043"));
        map.insert(QStringLiteral("qyi"), QStringLiteral("\u304f\u3043"));
        map.insert(QStringLiteral("qwu"), QStringLiteral("\u304f\u3045"));
        map.insert(QStringLiteral("qe"), QStringLiteral("\u304f\u3047"));
        map.insert(QStringLiteral("qwe"), QStringLiteral("\u304f\u3047"));
        map.insert(QStringLiteral("qye"), QStringLiteral("\u304f\u3047"));
        map.insert(QStringLiteral("qo"), QStringLiteral("\u304f\u3049"));
        map.insert(QStringLiteral("qwo"), QStringLiteral("\u304f\u3049"));
        map.insert(QStringLiteral("qya"), QStringLiteral("\u304f\u3083"));
        map.insert(QStringLiteral("qyu"), QStringLiteral("\u304f\u3085"));
        map.insert(QStringLiteral("qyo"), QStringLiteral("\u304f\u3087"));
        map.insert(QStringLiteral("gu"), QStringLiteral("\u3050"));
        map.insert(QStringLiteral("gwa"), QStringLiteral("\u3050\u3041"));
        map.insert(QStringLiteral("gwi"), QStringLiteral("\u3050\u3043"));
        map.insert(QStringLiteral("gwu"), QStringLiteral("\u3050\u3045"));
        map.insert(QStringLiteral("gwe"), QStringLiteral("\u3050\u3047"));
        map.insert(QStringLiteral("gwo"), QStringLiteral("\u3050\u3049"));
        map.insert(QStringLiteral("ke"), QStringLiteral("\u3051"));
        map.insert(QStringLiteral("ge"), QStringLiteral("\u3052"));
        map.insert(QStringLiteral("co"), QStringLiteral("\u3053"));
        map.insert(QStringLiteral("ko"), QStringLiteral("\u3053"));
        map.insert(QStringLiteral("go"), QStringLiteral("\u3054"));
        map.insert(QStringLiteral("sa"), QStringLiteral("\u3055"));
        map.insert(QStringLiteral("za"), QStringLiteral("\u3056"));
        map.insert(QStringLiteral("ci"), QStringLiteral("\u3057"));
        map.insert(QStringLiteral("shi"), QStringLiteral("\u3057"));
        map.insert(QStringLiteral("si"), QStringLiteral("\u3057"));
        map.insert(QStringLiteral("syi"), QStringLiteral("\u3057\u3043"));
        map.insert(QStringLiteral("she"), QStringLiteral("\u3057\u3047"));
        map.insert(QStringLiteral("sye"), QStringLiteral("\u3057\u3047"));
        map.insert(QStringLiteral("sha"), QStringLiteral("\u3057\u3083"));
        map.insert(QStringLiteral("sya"), QStringLiteral("\u3057\u3083"));
        map.insert(QStringLiteral("shu"), QStringLiteral("\u3057\u3085"));
        map.insert(QStringLiteral("syu"), QStringLiteral("\u3057\u3085"));
        map.insert(QStringLiteral("sho"), QStringLiteral("\u3057\u3087"));
        map.insert(QStringLiteral("syo"), QStringLiteral("\u3057\u3087"));
        map.insert(QStringLiteral("ji"), QStringLiteral("\u3058"));
        map.insert(QStringLiteral("zi"), QStringLiteral("\u3058"));
        map.insert(QStringLiteral("jyi"), QStringLiteral("\u3058\u3043"));
        map.insert(QStringLiteral("zyi"), QStringLiteral("\u3058\u3043"));
        map.insert(QStringLiteral("je"), QStringLiteral("\u3058\u3047"));
        map.insert(QStringLiteral("jye"), QStringLiteral("\u3058\u3047"));
        map.insert(QStringLiteral("zye"), QStringLiteral("\u3058\u3047"));
        map.insert(QStringLiteral("ja"), QStringLiteral("\u3058\u3083"));
        map.insert(QStringLiteral("jya"), QStringLiteral("\u3058\u3083"));
        map.insert(QStringLiteral("zya"), QStringLiteral("\u3058\u3083"));
        map.insert(QStringLiteral("ju"), QStringLiteral("\u3058\u3085"));
        map.insert(QStringLiteral("jyu"), QStringLiteral("\u3058\u3085"));
        map.insert(QStringLiteral("zyu"), QStringLiteral("\u3058\u3085"));
        map.insert(QStringLiteral("jo"), QStringLiteral("\u3058\u3087"));
        map.insert(QStringLiteral("jyo"), QStringLiteral("\u3058\u3087"));
        map.insert(QStringLiteral("zyo"), QStringLiteral("\u3058\u3087"));
        map.insert(QStringLiteral("su"), QStringLiteral("\u3059"));
        map.insert(QStringLiteral("swa"), QStringLiteral("\u3059\u3041"));
        map.insert(QStringLiteral("swi"), QStringLiteral("\u3059\u3043"));
        map.insert(QStringLiteral("swu"), QStringLiteral("\u3059\u3045"));
        map.insert(QStringLiteral("swe"), QStringLiteral("\u3059\u3047"));
        map.insert(QStringLiteral("swo"), QStringLiteral("\u3059\u3049"));
        map.insert(QStringLiteral("zu"), QStringLiteral("\u305a"));
        map.insert(QStringLiteral("ce"), QStringLiteral("\u305b"));
        map.insert(QStringLiteral("se"), QStringLiteral("\u305b"));
        map.insert(QStringLiteral("ze"), QStringLiteral("\u305c"));
        map.insert(QStringLiteral("so"), QStringLiteral("\u305d"));
        map.insert(QStringLiteral("zo"), QStringLiteral("\u305e"));
        map.insert(QStringLiteral("ta"), QStringLiteral("\u305f"));
        map.insert(QStringLiteral("da"), QStringLiteral("\u3060"));
        map.insert(QStringLiteral("chi"), QStringLiteral("\u3061"));
        map.insert(QStringLiteral("ti"), QStringLiteral("\u3061"));
        map.insert(QStringLiteral("cyi"), QStringLiteral("\u3061\u3043"));
        map.insert(QStringLiteral("tyi"), QStringLiteral("\u3061\u3043"));
        map.insert(QStringLiteral("che"), QStringLiteral("\u3061\u3047"));
        map.insert(QStringLiteral("cye"), QStringLiteral("\u3061\u3047"));
        map.insert(QStringLiteral("tye"), QStringLiteral("\u3061\u3047"));
        map.insert(QStringLiteral("cha"), QStringLiteral("\u3061\u3083"));
        map.insert(QStringLiteral("cya"), QStringLiteral("\u3061\u3083"));
        map.insert(QStringLiteral("tya"), QStringLiteral("\u3061\u3083"));
        map.insert(QStringLiteral("chu"), QStringLiteral("\u3061\u3085"));
        map.insert(QStringLiteral("cyu"), QStringLiteral("\u3061\u3085"));
        map.insert(QStringLiteral("tyu"), QStringLiteral("\u3061\u3085"));
        map.insert(QStringLiteral("cho"), QStringLiteral("\u3061\u3087"));
        map.insert(QStringLiteral("cyo"), QStringLiteral("\u3061\u3087"));
        map.insert(QStringLiteral("tyo"), QStringLiteral("\u3061\u3087"));
        map.insert(QStringLiteral("di"), QStringLiteral("\u3062"));
        map.insert(QStringLiteral("dyi"), QStringLiteral("\u3062\u3043"));
        map.insert(QStringLiteral("dye"), QStringLiteral("\u3062\u3047"));
        map.insert(QStringLiteral("dya"), QStringLiteral("\u3062\u3083"));
        map.insert(QStringLiteral("dyu"), QStringLiteral("\u3062\u3085"));
        map.insert(QStringLiteral("dyo"), QStringLiteral("\u3062\u3087"));
        map.insert(QStringLiteral("ltsu"), QStringLiteral("\u3063"));
        map.insert(QStringLiteral("ltu"), QStringLiteral("\u3063"));
        map.insert(QStringLiteral("xtu"), QStringLiteral("\u3063"));
        map.insert(QStringLiteral(""), QStringLiteral("\u3063"));
        map.insert(QStringLiteral("tsu"), QStringLiteral("\u3064"));
        map.insert(QStringLiteral("tu"), QStringLiteral("\u3064"));
        map.insert(QStringLiteral("tsa"), QStringLiteral("\u3064\u3041"));
        map.insert(QStringLiteral("tsi"), QStringLiteral("\u3064\u3043"));
        map.insert(QStringLiteral("tse"), QStringLiteral("\u3064\u3047"));
        map.insert(QStringLiteral("tso"), QStringLiteral("\u3064\u3049"));
        map.insert(QStringLiteral("du"), QStringLiteral("\u3065"));
        map.insert(QStringLiteral("te"), QStringLiteral("\u3066"));
        map.insert(QStringLiteral("thi"), QStringLiteral("\u3066\u3043"));
        map.insert(QStringLiteral("the"), QStringLiteral("\u3066\u3047"));
        map.insert(QStringLiteral("tha"), QStringLiteral("\u3066\u3083"));
        map.insert(QStringLiteral("thu"), QStringLiteral("\u3066\u3085"));
        map.insert(QStringLiteral("tho"), QStringLiteral("\u3066\u3087"));
        map.insert(QStringLiteral("de"), QStringLiteral("\u3067"));
        map.insert(QStringLiteral("dhi"), QStringLiteral("\u3067\u3043"));
        map.insert(QStringLiteral("dhe"), QStringLiteral("\u3067\u3047"));
        map.insert(QStringLiteral("dha"), QStringLiteral("\u3067\u3083"));
        map.insert(QStringLiteral("dhu"), QStringLiteral("\u3067\u3085"));
        map.insert(QStringLiteral("dho"), QStringLiteral("\u3067\u3087"));
        map.insert(QStringLiteral("to"), QStringLiteral("\u3068"));
        map.insert(QStringLiteral("twa"), QStringLiteral("\u3068\u3041"));
        map.insert(QStringLiteral("twi"), QStringLiteral("\u3068\u3043"));
        map.insert(QStringLiteral("twu"), QStringLiteral("\u3068\u3045"));
        map.insert(QStringLiteral("twe"), QStringLiteral("\u3068\u3047"));
        map.insert(QStringLiteral("two"), QStringLiteral("\u3068\u3049"));
        map.insert(QStringLiteral("do"), QStringLiteral("\u3069"));
        map.insert(QStringLiteral("dwa"), QStringLiteral("\u3069\u3041"));
        map.insert(QStringLiteral("dwi"), QStringLiteral("\u3069\u3043"));
        map.insert(QStringLiteral("dwu"), QStringLiteral("\u3069\u3045"));
        map.insert(QStringLiteral("dwe"), QStringLiteral("\u3069\u3047"));
        map.insert(QStringLiteral("dwo"), QStringLiteral("\u3069\u3049"));
        map.insert(QStringLiteral("na"), QStringLiteral("\u306a"));
        map.insert(QStringLiteral("ni"), QStringLiteral("\u306b"));
        map.insert(QStringLiteral("nyi"), QStringLiteral("\u306b\u3043"));
        map.insert(QStringLiteral("nye"), QStringLiteral("\u306b\u3047"));
        map.insert(QStringLiteral("nya"), QStringLiteral("\u306b\u3083"));
        map.insert(QStringLiteral("nyu"), QStringLiteral("\u306b\u3085"));
        map.insert(QStringLiteral("nyo"), QStringLiteral("\u306b\u3087"));
        map.insert(QStringLiteral("nu"), QStringLiteral("\u306c"));
        map.insert(QStringLiteral("ne"), QStringLiteral("\u306d"));
        map.insert(QStringLiteral("no"), QStringLiteral("\u306e"));
        map.insert(QStringLiteral("ha"), QStringLiteral("\u306f"));
        map.insert(QStringLiteral("ba"), QStringLiteral("\u3070"));
        map.insert(QStringLiteral("pa"), QStringLiteral("\u3071"));
        map.insert(QStringLiteral("hi"), QStringLiteral("\u3072"));
        map.insert(QStringLiteral("hyi"), QStringLiteral("\u3072\u3043"));
        map.insert(QStringLiteral("hye"), QStringLiteral("\u3072\u3047"));
        map.insert(QStringLiteral("hya"), QStringLiteral("\u3072\u3083"));
        map.insert(QStringLiteral("hyu"), QStringLiteral("\u3072\u3085"));
        map.insert(QStringLiteral("hyo"), QStringLiteral("\u3072\u3087"));
        map.insert(QStringLiteral("bi"), QStringLiteral("\u3073"));
        map.insert(QStringLiteral("byi"), QStringLiteral("\u3073\u3043"));
        map.insert(QStringLiteral("bye"), QStringLiteral("\u3073\u3047"));
        map.insert(QStringLiteral("bya"), QStringLiteral("\u3073\u3083"));
        map.insert(QStringLiteral("byu"), QStringLiteral("\u3073\u3085"));
        map.insert(QStringLiteral("byo"), QStringLiteral("\u3073\u3087"));
        map.insert(QStringLiteral("pi"), QStringLiteral("\u3074"));
        map.insert(QStringLiteral("pyi"), QStringLiteral("\u3074\u3043"));
        map.insert(QStringLiteral("pye"), QStringLiteral("\u3074\u3047"));
        map.insert(QStringLiteral("pya"), QStringLiteral("\u3074\u3083"));
        map.insert(QStringLiteral("pyu"), QStringLiteral("\u3074\u3085"));
        map.insert(QStringLiteral("pyo"), QStringLiteral("\u3074\u3087"));
        map.insert(QStringLiteral("fu"), QStringLiteral("\u3075"));
        map.insert(QStringLiteral("hu"), QStringLiteral("\u3075"));
        map.insert(QStringLiteral("fa"), QStringLiteral("\u3075\u3041"));
        map.insert(QStringLiteral("fwa"), QStringLiteral("\u3075\u3041"));
        map.insert(QStringLiteral("fi"), QStringLiteral("\u3075\u3043"));
        map.insert(QStringLiteral("fwi"), QStringLiteral("\u3075\u3043"));
        map.insert(QStringLiteral("fyi"), QStringLiteral("\u3075\u3043"));
        map.insert(QStringLiteral("fwu"), QStringLiteral("\u3075\u3045"));
        map.insert(QStringLiteral("fe"), QStringLiteral("\u3075\u3047"));
        map.insert(QStringLiteral("fwe"), QStringLiteral("\u3075\u3047"));
        map.insert(QStringLiteral("fye"), QStringLiteral("\u3075\u3047"));
        map.insert(QStringLiteral("fo"), QStringLiteral("\u3075\u3049"));
        map.insert(QStringLiteral("fwo"), QStringLiteral("\u3075\u3049"));
        map.insert(QStringLiteral("fya"), QStringLiteral("\u3075\u3083"));
        map.insert(QStringLiteral("fyu"), QStringLiteral("\u3075\u3085"));
        map.insert(QStringLiteral("fyo"), QStringLiteral("\u3075\u3087"));
        map.insert(QStringLiteral("bu"), QStringLiteral("\u3076"));
        map.insert(QStringLiteral("pu"), QStringLiteral("\u3077"));
        map.insert(QStringLiteral("he"), QStringLiteral("\u3078"));
        map.insert(QStringLiteral("be"), QStringLiteral("\u3079"));
        map.insert(QStringLiteral("pe"), QStringLiteral("\u307a"));
        map.insert(QStringLiteral("ho"), QStringLiteral("\u307b"));
        map.insert(QStringLiteral("bo"), QStringLiteral("\u307c"));
        map.insert(QStringLiteral("po"), QStringLiteral("\u307d"));
        map.insert(QStringLiteral("ma"), QStringLiteral("\u307e"));
        map.insert(QStringLiteral("mi"), QStringLiteral("\u307f"));
        map.insert(QStringLiteral("myi"), QStringLiteral("\u307f\u3043"));
        map.insert(QStringLiteral("mye"), QStringLiteral("\u307f\u3047"));
        map.insert(QStringLiteral("mya"), QStringLiteral("\u307f\u3083"));
        map.insert(QStringLiteral("myu"), QStringLiteral("\u307f\u3085"));
        map.insert(QStringLiteral("myo"), QStringLiteral("\u307f\u3087"));
        map.insert(QStringLiteral("mu"), QStringLiteral("\u3080"));
        map.insert(QStringLiteral("me"), QStringLiteral("\u3081"));
        map.insert(QStringLiteral("mo"), QStringLiteral("\u3082"));
        map.insert(QStringLiteral("lya"), QStringLiteral("\u3083"));
        map.insert(QStringLiteral("xya"), QStringLiteral("\u3083"));
        map.insert(QStringLiteral("ya"), QStringLiteral("\u3084"));
        map.insert(QStringLiteral("lyu"), QStringLiteral("\u3085"));
        map.insert(QStringLiteral("xyu"), QStringLiteral("\u3085"));
        map.insert(QStringLiteral("yu"), QStringLiteral("\u3086"));
        map.insert(QStringLiteral("lyo"), QStringLiteral("\u3087"));
        map.insert(QStringLiteral("xyo"), QStringLiteral("\u3087"));
        map.insert(QStringLiteral("yo"), QStringLiteral("\u3088"));
        map.insert(QStringLiteral("ra"), QStringLiteral("\u3089"));
        map.insert(QStringLiteral("ri"), QStringLiteral("\u308a"));
        map.insert(QStringLiteral("ryi"), QStringLiteral("\u308a\u3043"));
        map.insert(QStringLiteral("rye"), QStringLiteral("\u308a\u3047"));
        map.insert(QStringLiteral("rya"), QStringLiteral("\u308a\u3083"));
        map.insert(QStringLiteral("ryu"), QStringLiteral("\u308a\u3085"));
        map.insert(QStringLiteral("ryo"), QStringLiteral("\u308a\u3087"));
        map.insert(QStringLiteral("ru"), QStringLiteral("\u308b"));
        map.insert(QStringLiteral("re"), QStringLiteral("\u308c"));
        map.insert(QStringLiteral("ro"), QStringLiteral("\u308d"));
        map.insert(QStringLiteral("lwa"), QStringLiteral("\u308e"));
        map.insert(QStringLiteral("xwa"), QStringLiteral("\u308e"));
        map.insert(QStringLiteral("wa"), QStringLiteral("\u308f"));
        map.insert(QStringLiteral("wo"), QStringLiteral("\u3092"));
        map.insert(QStringLiteral("nn"), QStringLiteral("\u3093"));
        map.insert(QStringLiteral("xn"), QStringLiteral("\u3093"));
        map.insert(QStringLiteral("vu"), QStringLiteral("\u30f4"));
        map.insert(QStringLiteral("va"), QStringLiteral("\u30f4\u3041"));
        map.insert(QStringLiteral("vi"), QStringLiteral("\u30f4\u3043"));
        map.insert(QStringLiteral("vyi"), QStringLiteral("\u30f4\u3043"));
        map.insert(QStringLiteral("ve"), QStringLiteral("\u30f4\u3047"));
        map.insert(QStringLiteral("vye"), QStringLiteral("\u30f4\u3047"));
        map.insert(QStringLiteral("vo"), QStringLiteral("\u30f4\u3049"));
        map.insert(QStringLiteral("vya"), QStringLiteral("\u30f4\u3083"));
        map.insert(QStringLiteral("vyu"), QStringLiteral("\u30f4\u3085"));
        map.insert(QStringLiteral("vyo"), QStringLiteral("\u30f4\u3087"));
        map.insert(QStringLiteral("bb"), QStringLiteral("\u3063b"));
        map.insert(QStringLiteral("cc"), QStringLiteral("\u3063c"));
        map.insert(QStringLiteral("dd"), QStringLiteral("\u3063d"));
        map.insert(QStringLiteral("ff"), QStringLiteral("\u3063f"));
        map.insert(QStringLiteral("gg"), QStringLiteral("\u3063g"));
        map.insert(QStringLiteral("hh"), QStringLiteral("\u3063h"));
        map.insert(QStringLiteral("jj"), QStringLiteral("\u3063j"));
        map.insert(QStringLiteral("kk"), QStringLiteral("\u3063k"));
        map.insert(QStringLiteral("ll"), QStringLiteral("\u3063l"));
        map.insert(QStringLiteral("mm"), QStringLiteral("\u3063m"));
        map.insert(QStringLiteral("pp"), QStringLiteral("\u3063p"));
        map.insert(QStringLiteral("qq"), QStringLiteral("\u3063q"));
        map.insert(QStringLiteral("rr"), QStringLiteral("\u3063r"));
        map.insert(QStringLiteral("ss"), QStringLiteral("\u3063s"));
        map.insert(QStringLiteral("tt"), QStringLiteral("\u3063t"));
        map.insert(QStringLiteral("vv"), QStringLiteral("\u3063v"));
        map.insert(QStringLiteral("ww"), QStringLiteral("\u3063w"));
        map.insert(QStringLiteral("xx"), QStringLiteral("\u3063x"));
        map.insert(QStringLiteral("yy"), QStringLiteral("\u3063y"));
        map.insert(QStringLiteral("zz"), QStringLiteral("\u3063z"));
        map.insert(QStringLiteral("nb"), QStringLiteral("\u3093b"));
        map.insert(QStringLiteral("nc"), QStringLiteral("\u3093c"));
        map.insert(QStringLiteral("nd"), QStringLiteral("\u3093d"));
        map.insert(QStringLiteral("nf"), QStringLiteral("\u3093f"));
        map.insert(QStringLiteral("ng"), QStringLiteral("\u3093g"));
        map.insert(QStringLiteral("nh"), QStringLiteral("\u3093h"));
        map.insert(QStringLiteral("nj"), QStringLiteral("\u3093j"));
        map.insert(QStringLiteral("nk"), QStringLiteral("\u3093k"));
        map.insert(QStringLiteral("nm"), QStringLiteral("\u3093m"));
        map.insert(QStringLiteral("np"), QStringLiteral("\u3093p"));
        map.insert(QStringLiteral("nq"), QStringLiteral("\u3093q"));
        map.insert(QStringLiteral("nr"), QStringLiteral("\u3093r"));
        map.insert(QStringLiteral("ns"), QStringLiteral("\u3093s"));
        map.insert(QStringLiteral("nt"), QStringLiteral("\u3093t"));
        map.insert(QStringLiteral("nv"), QStringLiteral("\u3093v"));
        map.insert(QStringLiteral("nw"), QStringLiteral("\u3093w"));
        map.insert(QStringLiteral("nx"), QStringLiteral("\u3093x"));
        map.insert(QStringLiteral("nz"), QStringLiteral("\u3093z"));
        map.insert(QStringLiteral("nl"), QStringLiteral("\u3093l"));
        map.insert(QStringLiteral("-"), QStringLiteral("\u30fc"));
        map.insert(QStringLiteral("."), QStringLiteral("\u3002"));
        map.insert(QStringLiteral(","), QStringLiteral("\u3001"));
        map.insert(QStringLiteral("?"), QStringLiteral("\uff1f"));
        map.insert(QStringLiteral("/"), QStringLiteral("\u30fb"));
        map.insert(QStringLiteral("@"), QStringLiteral("\uff20"));
        map.insert(QStringLiteral("#"), QStringLiteral("\uff03"));
        map.insert(QStringLiteral("%"), QStringLiteral("\uff05"));
        map.insert(QStringLiteral("&"), QStringLiteral("\uff06"));
        map.insert(QStringLiteral("*"), QStringLiteral("\uff0a"));
        map.insert(QStringLiteral("+"), QStringLiteral("\uff0b"));
        map.insert(QStringLiteral("="), QStringLiteral("\uff1d"));
        map.insert(QStringLiteral("("), QStringLiteral("\uff08"));
        map.insert(QStringLiteral(")"), QStringLiteral("\uff09"));
        map.insert(QStringLiteral("~"), QStringLiteral("\uff5e"));
        map.insert(QStringLiteral("\""), QStringLiteral("\uff02"));
        map.insert(QStringLiteral("'"), QStringLiteral("\uff07"));
        map.insert(QStringLiteral(":"), QStringLiteral("\uff1a"));
        map.insert(QStringLiteral(";"), QStringLiteral("\uff1b"));
        map.insert(QStringLiteral("!"), QStringLiteral("\uff01"));
        map.insert(QStringLiteral("^"), QStringLiteral("\uff3e"));
        map.insert(QStringLiteral("\u00a5"), QStringLiteral("\uffe5"));
        map.insert(QStringLiteral("$"), QStringLiteral("\uff04"));
        map.insert(QStringLiteral("["), QStringLiteral("\u300c"));
        map.insert(QStringLiteral("]"), QStringLiteral("\u300d"));
        map.insert(QStringLiteral("_"), QStringLiteral("\uff3f"));
        map.insert(QStringLiteral("{"), QStringLiteral("\uff5b"));
        map.insert(QStringLiteral("}"), QStringLiteral("\uff5d"));
        map.insert(QStringLiteral("`"), QStringLiteral("\uff40"));
        map.insert(QStringLiteral("<"), QStringLiteral("\uff1c"));
        map.insert(QStringLiteral(">"), QStringLiteral("\uff1e"));
        map.insert(QStringLiteral("\\"), QStringLiteral("\uff3c"));
        map.insert(QStringLiteral("|"), QStringLiteral("\uff5c"));
        map.insert(QStringLiteral("1"), QStringLiteral("\uff11"));
        map.insert(QStringLiteral("2"), QStringLiteral("\uff12"));
        map.insert(QStringLiteral("3"), QStringLiteral("\uff13"));
        map.insert(QStringLiteral("4"), QStringLiteral("\uff14"));
        map.insert(QStringLiteral("5"), QStringLiteral("\uff15"));
        map.insert(QStringLiteral("6"), QStringLiteral("\uff16"));
        map.insert(QStringLiteral("7"), QStringLiteral("\uff17"));
        map.insert(QStringLiteral("8"), QStringLiteral("\uff18"));
        map.insert(QStringLiteral("9"), QStringLiteral("\uff19"));
        map.insert(QStringLiteral("0"), QStringLiteral("\uff10"));
        return map;
    }

    static const QMap<QString, QString> romkanTable;

    /** Max length of the target text */
    static const int MAX_LENGTH;
};

const int RomkanPrivate::MAX_LENGTH = 4;

const QMap<QString, QString> RomkanPrivate::romkanTable = RomkanPrivate::initRomkanTable();

Romkan::Romkan(QObjectPrivate &dd, QObject *parent) :
    LetterConverter(dd, parent)
{
}

Romkan::Romkan(QObject *parent) :
    LetterConverter(*new RomkanPrivate(), parent)
{
}

Romkan::~Romkan()
{
}

bool Romkan::convert(ComposingText &text) const
{
    return convertImpl(text, RomkanPrivate::romkanTable);
}

bool Romkan::convertImpl(ComposingText &text, const QMap<QString, QString> &table) const
{
    int cursor = text.getCursor(ComposingText::LAYER1);

    if (cursor <= 0) {
        return false;
    }

    StrSegment str[RomkanPrivate::MAX_LENGTH];
    int start = RomkanPrivate::MAX_LENGTH;
    int checkLength = qMin(cursor, RomkanPrivate::MAX_LENGTH);
    for (int i = 1; i <= checkLength; i++) {
        str[RomkanPrivate::MAX_LENGTH - i] = text.getStrSegment(ComposingText::LAYER1, cursor - i);
        start--;
    }

    while (start < RomkanPrivate::MAX_LENGTH) {
        QString key;
        for (int i = start; i < RomkanPrivate::MAX_LENGTH; i++) {
            key.append(str[i].string);
        }
        bool upper = key.at(key.length() - 1).isUpper();
        QString match = table[key.toLower()];
        if (!match.isEmpty()) {
            if (upper) {
                match = match.toUpper();
            }
            QList<StrSegment> out;
            if (match.length() == 1) {
                out.append(StrSegment(match, str[start].from, str[RomkanPrivate::MAX_LENGTH - 1].to));
                text.replaceStrSegment(ComposingText::LAYER1, out, RomkanPrivate::MAX_LENGTH - start);
            } else {
                out.append(StrSegment(match.left(match.length() - 1),
                                      str[start].from, str[RomkanPrivate::MAX_LENGTH - 1].to - 1));
                out.append(StrSegment(match.mid(match.length() - 1),
                                      str[RomkanPrivate::MAX_LENGTH - 1].to, str[RomkanPrivate::MAX_LENGTH - 1].to));
                text.replaceStrSegment(ComposingText::LAYER1, out, RomkanPrivate::MAX_LENGTH - start);
            }
            return true;
        }
        start++;
    }

    return false;
}
