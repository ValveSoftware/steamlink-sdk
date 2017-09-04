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

#include "kanaconverter.h"

#include <QtCore/private/qobject_p.h>

class KanaConverterPrivate : public QObjectPrivate
{
public:
    static QMap<QString, QString> initHalfKatakanaMap()
    {
        QMap<QString, QString> map;
        map.insert(QStringLiteral("\u3042"), QStringLiteral("\uff71"));
        map.insert(QStringLiteral("\u3044"), QStringLiteral("\uff72"));
        map.insert(QStringLiteral("\u3046"), QStringLiteral("\uff73"));
        map.insert(QStringLiteral("\u3048"), QStringLiteral("\uff74"));
        map.insert(QStringLiteral("\u304a"), QStringLiteral("\uff75"));
        map.insert(QStringLiteral("\u3041"), QStringLiteral("\uff67"));
        map.insert(QStringLiteral("\u3043"), QStringLiteral("\uff68"));
        map.insert(QStringLiteral("\u3045"), QStringLiteral("\uff69"));
        map.insert(QStringLiteral("\u3047"), QStringLiteral("\uff6a"));
        map.insert(QStringLiteral("\u3049"), QStringLiteral("\uff6b"));
        //map.insert(QStringLiteral("\u30f4\u3041"), QStringLiteral("\uff73\uff9e\uff67"));
        //map.insert(QStringLiteral("\u30f4\u3043"), QStringLiteral("\uff73\uff9e\uff68"));
        map.insert(QStringLiteral("\u30f4"), QStringLiteral("\uff73\uff9e"));
        //map.insert(QStringLiteral("\u30f4\u3047"), QStringLiteral("\uff73\uff9e\uff6a"));
        //map.insert(QStringLiteral("\u30f4\u3049"), QStringLiteral("\uff73\uff9e\uff6b"));
        map.insert(QStringLiteral("\u304b"), QStringLiteral("\uff76"));
        map.insert(QStringLiteral("\u304d"), QStringLiteral("\uff77"));
        map.insert(QStringLiteral("\u304f"), QStringLiteral("\uff78"));
        map.insert(QStringLiteral("\u3051"), QStringLiteral("\uff79"));
        map.insert(QStringLiteral("\u3053"), QStringLiteral("\uff7a"));
        map.insert(QStringLiteral("\u304c"), QStringLiteral("\uff76\uff9e"));
        map.insert(QStringLiteral("\u304e"), QStringLiteral("\uff77\uff9e"));
        map.insert(QStringLiteral("\u3050"), QStringLiteral("\uff78\uff9e"));
        map.insert(QStringLiteral("\u3052"), QStringLiteral("\uff79\uff9e"));
        map.insert(QStringLiteral("\u3054"), QStringLiteral("\uff7a\uff9e"));
        map.insert(QStringLiteral("\u3055"), QStringLiteral("\uff7b"));
        map.insert(QStringLiteral("\u3057"), QStringLiteral("\uff7c"));
        map.insert(QStringLiteral("\u3059"), QStringLiteral("\uff7d"));
        map.insert(QStringLiteral("\u305b"), QStringLiteral("\uff7e"));
        map.insert(QStringLiteral("\u305d"), QStringLiteral("\uff7f"));
        map.insert(QStringLiteral("\u3056"), QStringLiteral("\uff7b\uff9e"));
        map.insert(QStringLiteral("\u3058"), QStringLiteral("\uff7c\uff9e"));
        map.insert(QStringLiteral("\u305a"), QStringLiteral("\uff7d\uff9e"));
        map.insert(QStringLiteral("\u305c"), QStringLiteral("\uff7e\uff9e"));
        map.insert(QStringLiteral("\u305e"), QStringLiteral("\uff7f\uff9e"));
        map.insert(QStringLiteral("\u305f"), QStringLiteral("\uff80"));
        map.insert(QStringLiteral("\u3061"), QStringLiteral("\uff81"));
        map.insert(QStringLiteral("\u3064"), QStringLiteral("\uff82"));
        map.insert(QStringLiteral("\u3066"), QStringLiteral("\uff83"));
        map.insert(QStringLiteral("\u3068"), QStringLiteral("\uff84"));
        map.insert(QStringLiteral("\u3063"), QStringLiteral("\uff6f"));
        map.insert(QStringLiteral("\u3060"), QStringLiteral("\uff80\uff9e"));
        map.insert(QStringLiteral("\u3062"), QStringLiteral("\uff81\uff9e"));
        map.insert(QStringLiteral("\u3065"), QStringLiteral("\uff82\uff9e"));
        map.insert(QStringLiteral("\u3067"), QStringLiteral("\uff83\uff9e"));
        map.insert(QStringLiteral("\u3069"), QStringLiteral("\uff84\uff9e"));
        map.insert(QStringLiteral("\u306a"), QStringLiteral("\uff85"));
        map.insert(QStringLiteral("\u306b"), QStringLiteral("\uff86"));
        map.insert(QStringLiteral("\u306c"), QStringLiteral("\uff87"));
        map.insert(QStringLiteral("\u306d"), QStringLiteral("\uff88"));
        map.insert(QStringLiteral("\u306e"), QStringLiteral("\uff89"));
        map.insert(QStringLiteral("\u306f"), QStringLiteral("\uff8a"));
        map.insert(QStringLiteral("\u3072"), QStringLiteral("\uff8b"));
        map.insert(QStringLiteral("\u3075"), QStringLiteral("\uff8c"));
        map.insert(QStringLiteral("\u3078"), QStringLiteral("\uff8d"));
        map.insert(QStringLiteral("\u307b"), QStringLiteral("\uff8e"));
        map.insert(QStringLiteral("\u3070"), QStringLiteral("\uff8a\uff9e"));
        map.insert(QStringLiteral("\u3073"), QStringLiteral("\uff8b\uff9e"));
        map.insert(QStringLiteral("\u3076"), QStringLiteral("\uff8c\uff9e"));
        map.insert(QStringLiteral("\u3079"), QStringLiteral("\uff8d\uff9e"));
        map.insert(QStringLiteral("\u307c"), QStringLiteral("\uff8e\uff9e"));
        map.insert(QStringLiteral("\u3071"), QStringLiteral("\uff8a\uff9f"));
        map.insert(QStringLiteral("\u3074"), QStringLiteral("\uff8b\uff9f"));
        map.insert(QStringLiteral("\u3077"), QStringLiteral("\uff8c\uff9f"));
        map.insert(QStringLiteral("\u307a"), QStringLiteral("\uff8d\uff9f"));
        map.insert(QStringLiteral("\u307d"), QStringLiteral("\uff8e\uff9f"));
        map.insert(QStringLiteral("\u307e"), QStringLiteral("\uff8f"));
        map.insert(QStringLiteral("\u307f"), QStringLiteral("\uff90"));
        map.insert(QStringLiteral("\u3080"), QStringLiteral("\uff91"));
        map.insert(QStringLiteral("\u3081"), QStringLiteral("\uff92"));
        map.insert(QStringLiteral("\u3082"), QStringLiteral("\uff93"));
        map.insert(QStringLiteral("\u3084"), QStringLiteral("\uff94"));
        map.insert(QStringLiteral("\u3086"), QStringLiteral("\uff95"));
        map.insert(QStringLiteral("\u3088"), QStringLiteral("\uff96"));
        map.insert(QStringLiteral("\u3083"), QStringLiteral("\uff6c"));
        map.insert(QStringLiteral("\u3085"), QStringLiteral("\uff6d"));
        map.insert(QStringLiteral("\u3087"), QStringLiteral("\uff6e"));
        map.insert(QStringLiteral("\u3089"), QStringLiteral("\uff97"));
        map.insert(QStringLiteral("\u308a"), QStringLiteral("\uff98"));
        map.insert(QStringLiteral("\u308b"), QStringLiteral("\uff99"));
        map.insert(QStringLiteral("\u308c"), QStringLiteral("\uff9a"));
        map.insert(QStringLiteral("\u308d"), QStringLiteral("\uff9b"));
        map.insert(QStringLiteral("\u308f"), QStringLiteral("\uff9c"));
        map.insert(QStringLiteral("\u3092"), QStringLiteral("\uff66"));
        map.insert(QStringLiteral("\u3093"), QStringLiteral("\uff9d"));
        map.insert(QStringLiteral("\u308e"), QStringLiteral("\uff9c"));
        map.insert(QStringLiteral("\u30fc"), QStringLiteral("\uff70"));
        return map;
    }

    static QMap<QString, QString> initFullKatakanaMap()
    {
        QMap<QString, QString> map;
        map.insert(QStringLiteral("\u3042"), QStringLiteral("\u30a2"));
        map.insert(QStringLiteral("\u3044"), QStringLiteral("\u30a4"));
        map.insert(QStringLiteral("\u3046"), QStringLiteral("\u30a6"));
        map.insert(QStringLiteral("\u3048"), QStringLiteral("\u30a8"));
        map.insert(QStringLiteral("\u304a"), QStringLiteral("\u30aa"));
        map.insert(QStringLiteral("\u3041"), QStringLiteral("\u30a1"));
        map.insert(QStringLiteral("\u3043"), QStringLiteral("\u30a3"));
        map.insert(QStringLiteral("\u3045"), QStringLiteral("\u30a5"));
        map.insert(QStringLiteral("\u3047"), QStringLiteral("\u30a7"));
        map.insert(QStringLiteral("\u3049"), QStringLiteral("\u30a9"));
        //map.insert(QStringLiteral("\u30f4\u3041"), QStringLiteral("\u30f4\u30a1"));
        //map.insert(QStringLiteral("\u30f4\u3043"), QStringLiteral("\u30f4\u30a3"));
        map.insert(QStringLiteral("\u30f4"), QStringLiteral("\u30f4"));
        //map.insert(QStringLiteral("\u30f4\u3047"), QStringLiteral("\u30f4\u30a7"));
        //map.insert(QStringLiteral("\u30f4\u3049"), QStringLiteral("\u30f4\u30a9"));
        map.insert(QStringLiteral("\u304b"), QStringLiteral("\u30ab"));
        map.insert(QStringLiteral("\u304d"), QStringLiteral("\u30ad"));
        map.insert(QStringLiteral("\u304f"), QStringLiteral("\u30af"));
        map.insert(QStringLiteral("\u3051"), QStringLiteral("\u30b1"));
        map.insert(QStringLiteral("\u3053"), QStringLiteral("\u30b3"));
        map.insert(QStringLiteral("\u304c"), QStringLiteral("\u30ac"));
        map.insert(QStringLiteral("\u304e"), QStringLiteral("\u30ae"));
        map.insert(QStringLiteral("\u3050"), QStringLiteral("\u30b0"));
        map.insert(QStringLiteral("\u3052"), QStringLiteral("\u30b2"));
        map.insert(QStringLiteral("\u3054"), QStringLiteral("\u30b4"));
        map.insert(QStringLiteral("\u3055"), QStringLiteral("\u30b5"));
        map.insert(QStringLiteral("\u3057"), QStringLiteral("\u30b7"));
        map.insert(QStringLiteral("\u3059"), QStringLiteral("\u30b9"));
        map.insert(QStringLiteral("\u305b"), QStringLiteral("\u30bb"));
        map.insert(QStringLiteral("\u305d"), QStringLiteral("\u30bd"));
        map.insert(QStringLiteral("\u3056"), QStringLiteral("\u30b6"));
        map.insert(QStringLiteral("\u3058"), QStringLiteral("\u30b8"));
        map.insert(QStringLiteral("\u305a"), QStringLiteral("\u30ba"));
        map.insert(QStringLiteral("\u305c"), QStringLiteral("\u30bc"));
        map.insert(QStringLiteral("\u305e"), QStringLiteral("\u30be"));
        map.insert(QStringLiteral("\u305f"), QStringLiteral("\u30bf"));
        map.insert(QStringLiteral("\u3061"), QStringLiteral("\u30c1"));
        map.insert(QStringLiteral("\u3064"), QStringLiteral("\u30c4"));
        map.insert(QStringLiteral("\u3066"), QStringLiteral("\u30c6"));
        map.insert(QStringLiteral("\u3068"), QStringLiteral("\u30c8"));
        map.insert(QStringLiteral("\u3063"), QStringLiteral("\u30c3"));
        map.insert(QStringLiteral("\u3060"), QStringLiteral("\u30c0"));
        map.insert(QStringLiteral("\u3062"), QStringLiteral("\u30c2"));
        map.insert(QStringLiteral("\u3065"), QStringLiteral("\u30c5"));
        map.insert(QStringLiteral("\u3067"), QStringLiteral("\u30c7"));
        map.insert(QStringLiteral("\u3069"), QStringLiteral("\u30c9"));
        map.insert(QStringLiteral("\u306a"), QStringLiteral("\u30ca"));
        map.insert(QStringLiteral("\u306b"), QStringLiteral("\u30cb"));
        map.insert(QStringLiteral("\u306c"), QStringLiteral("\u30cc"));
        map.insert(QStringLiteral("\u306d"), QStringLiteral("\u30cd"));
        map.insert(QStringLiteral("\u306e"), QStringLiteral("\u30ce"));
        map.insert(QStringLiteral("\u306f"), QStringLiteral("\u30cf"));
        map.insert(QStringLiteral("\u3072"), QStringLiteral("\u30d2"));
        map.insert(QStringLiteral("\u3075"), QStringLiteral("\u30d5"));
        map.insert(QStringLiteral("\u3078"), QStringLiteral("\u30d8"));
        map.insert(QStringLiteral("\u307b"), QStringLiteral("\u30db"));
        map.insert(QStringLiteral("\u3070"), QStringLiteral("\u30d0"));
        map.insert(QStringLiteral("\u3073"), QStringLiteral("\u30d3"));
        map.insert(QStringLiteral("\u3076"), QStringLiteral("\u30d6"));
        map.insert(QStringLiteral("\u3079"), QStringLiteral("\u30d9"));
        map.insert(QStringLiteral("\u307c"), QStringLiteral("\u30dc"));
        map.insert(QStringLiteral("\u3071"), QStringLiteral("\u30d1"));
        map.insert(QStringLiteral("\u3074"), QStringLiteral("\u30d4"));
        map.insert(QStringLiteral("\u3077"), QStringLiteral("\u30d7"));
        map.insert(QStringLiteral("\u307a"), QStringLiteral("\u30da"));
        map.insert(QStringLiteral("\u307d"), QStringLiteral("\u30dd"));
        map.insert(QStringLiteral("\u307e"), QStringLiteral("\u30de"));
        map.insert(QStringLiteral("\u307f"), QStringLiteral("\u30df"));
        map.insert(QStringLiteral("\u3080"), QStringLiteral("\u30e0"));
        map.insert(QStringLiteral("\u3081"), QStringLiteral("\u30e1"));
        map.insert(QStringLiteral("\u3082"), QStringLiteral("\u30e2"));
        map.insert(QStringLiteral("\u3084"), QStringLiteral("\u30e4"));
        map.insert(QStringLiteral("\u3086"), QStringLiteral("\u30e6"));
        map.insert(QStringLiteral("\u3088"), QStringLiteral("\u30e8"));
        map.insert(QStringLiteral("\u3083"), QStringLiteral("\u30e3"));
        map.insert(QStringLiteral("\u3085"), QStringLiteral("\u30e5"));
        map.insert(QStringLiteral("\u3087"), QStringLiteral("\u30e7"));
        map.insert(QStringLiteral("\u3089"), QStringLiteral("\u30e9"));
        map.insert(QStringLiteral("\u308a"), QStringLiteral("\u30ea"));
        map.insert(QStringLiteral("\u308b"), QStringLiteral("\u30eb"));
        map.insert(QStringLiteral("\u308c"), QStringLiteral("\u30ec"));
        map.insert(QStringLiteral("\u308d"), QStringLiteral("\u30ed"));
        map.insert(QStringLiteral("\u308f"), QStringLiteral("\u30ef"));
        map.insert(QStringLiteral("\u3092"), QStringLiteral("\u30f2"));
        map.insert(QStringLiteral("\u3093"), QStringLiteral("\u30f3"));
        map.insert(QStringLiteral("\u308e"), QStringLiteral("\u30ee"));
        map.insert(QStringLiteral("\u30fc"), QStringLiteral("\u30fc"));
        return map;
    }

    static QMap<QString, QString> initFullAlphabetMapQwety()
    {
        QMap<QString, QString> map;
        map.insert(QStringLiteral("a"), QStringLiteral("\uff41"));
        map.insert(QStringLiteral("b"), QStringLiteral("\uff42"));
        map.insert(QStringLiteral("c"), QStringLiteral("\uff43"));
        map.insert(QStringLiteral("d"), QStringLiteral("\uff44"));
        map.insert(QStringLiteral("e"), QStringLiteral("\uff45"));
        map.insert(QStringLiteral("f"), QStringLiteral("\uff46"));
        map.insert(QStringLiteral("g"), QStringLiteral("\uff47"));
        map.insert(QStringLiteral("h"), QStringLiteral("\uff48"));
        map.insert(QStringLiteral("i"), QStringLiteral("\uff49"));
        map.insert(QStringLiteral("j"), QStringLiteral("\uff4a"));
        map.insert(QStringLiteral("k"), QStringLiteral("\uff4b"));
        map.insert(QStringLiteral("l"), QStringLiteral("\uff4c"));
        map.insert(QStringLiteral("m"), QStringLiteral("\uff4d"));
        map.insert(QStringLiteral("n"), QStringLiteral("\uff4e"));
        map.insert(QStringLiteral("o"), QStringLiteral("\uff4f"));
        map.insert(QStringLiteral("p"), QStringLiteral("\uff50"));
        map.insert(QStringLiteral("q"), QStringLiteral("\uff51"));
        map.insert(QStringLiteral("r"), QStringLiteral("\uff52"));
        map.insert(QStringLiteral("s"), QStringLiteral("\uff53"));
        map.insert(QStringLiteral("t"), QStringLiteral("\uff54"));
        map.insert(QStringLiteral("u"), QStringLiteral("\uff55"));
        map.insert(QStringLiteral("v"), QStringLiteral("\uff56"));
        map.insert(QStringLiteral("w"), QStringLiteral("\uff57"));
        map.insert(QStringLiteral("x"), QStringLiteral("\uff58"));
        map.insert(QStringLiteral("y"), QStringLiteral("\uff59"));
        map.insert(QStringLiteral("z"), QStringLiteral("\uff5a"));
        map.insert(QStringLiteral("A"), QStringLiteral("\uff21"));
        map.insert(QStringLiteral("B"), QStringLiteral("\uff22"));
        map.insert(QStringLiteral("C"), QStringLiteral("\uff23"));
        map.insert(QStringLiteral("D"), QStringLiteral("\uff24"));
        map.insert(QStringLiteral("E"), QStringLiteral("\uff25"));
        map.insert(QStringLiteral("F"), QStringLiteral("\uff26"));
        map.insert(QStringLiteral("G"), QStringLiteral("\uff27"));
        map.insert(QStringLiteral("H"), QStringLiteral("\uff28"));
        map.insert(QStringLiteral("I"), QStringLiteral("\uff29"));
        map.insert(QStringLiteral("J"), QStringLiteral("\uff2a"));
        map.insert(QStringLiteral("K"), QStringLiteral("\uff2b"));
        map.insert(QStringLiteral("L"), QStringLiteral("\uff2c"));
        map.insert(QStringLiteral("M"), QStringLiteral("\uff2d"));
        map.insert(QStringLiteral("N"), QStringLiteral("\uff2e"));
        map.insert(QStringLiteral("O"), QStringLiteral("\uff2f"));
        map.insert(QStringLiteral("P"), QStringLiteral("\uff30"));
        map.insert(QStringLiteral("Q"), QStringLiteral("\uff31"));
        map.insert(QStringLiteral("R"), QStringLiteral("\uff32"));
        map.insert(QStringLiteral("S"), QStringLiteral("\uff33"));
        map.insert(QStringLiteral("T"), QStringLiteral("\uff34"));
        map.insert(QStringLiteral("U"), QStringLiteral("\uff35"));
        map.insert(QStringLiteral("V"), QStringLiteral("\uff36"));
        map.insert(QStringLiteral("W"), QStringLiteral("\uff37"));
        map.insert(QStringLiteral("X"), QStringLiteral("\uff38"));
        map.insert(QStringLiteral("Y"), QStringLiteral("\uff39"));
        map.insert(QStringLiteral("Z"), QStringLiteral("\uff3a"));
        return map;
    }

    void createPseudoCandidateListForQwerty(QList<WnnWord> &list, const QString &inputHiragana, const QString &inputRomaji)
    {
        /* Create pseudo candidates for half width alphabet */
        QString convHanEijiLower = inputRomaji.toLower();
        list.append(WnnWord(inputRomaji, inputHiragana, mPosDefault));
        list.append(WnnWord(convHanEijiLower, inputHiragana, mPosSymbol));
        list.append(WnnWord(convertCaps(convHanEijiLower), inputHiragana, mPosSymbol));
        list.append(WnnWord(inputRomaji.toUpper(), inputHiragana, mPosSymbol));

        /* Create pseudo candidates for the full width alphabet */
        QString convZenEiji;
        if (createCandidateString(inputRomaji, mFullAlphabetMapQwety, convZenEiji)) {
            QString convZenEijiLower = convZenEiji.toLower();
            list.append(WnnWord(convZenEiji, inputHiragana, mPosSymbol));
            list.append(WnnWord(convZenEijiLower, inputHiragana, mPosSymbol));
            list.append(WnnWord(convertCaps(convZenEijiLower), inputHiragana, mPosSymbol));
            list.append(WnnWord(convZenEiji.toUpper(), inputHiragana, mPosSymbol));
        }
    }

    static bool createCandidateString(const QString &input, const QMap<QString, QString> &map, QString &outBuf) {
        outBuf.clear();
        const QString empty;
        for (int index = 0, length = input.length(); index < length; index++) {
            QString out = map.value(input.mid(index, 1), empty);
            if (out.isEmpty())
                return false;
            outBuf.append(out);
        }
        return true;
    }

    QString convertCaps(const QString &moji) {
        QString tmp;
        if (!moji.isEmpty()) {
            tmp.append(moji.left(1).toUpper());
            tmp.append(moji.mid(1).toLower());
        }
        return tmp;
    }

    static const QMap<QString, QString> mHalfKatakanaMap;
    static const QMap<QString, QString> mFullKatakanaMap;
    static const QMap<QString, QString> mFullAlphabetMapQwety;

    WnnPOS mPosDefault;
    WnnPOS mPosNumber;
    WnnPOS mPosSymbol;
};

const QMap<QString, QString> KanaConverterPrivate::mHalfKatakanaMap = KanaConverterPrivate::initHalfKatakanaMap();
const QMap<QString, QString> KanaConverterPrivate::mFullKatakanaMap = KanaConverterPrivate::initFullKatakanaMap();
const QMap<QString, QString> KanaConverterPrivate::mFullAlphabetMapQwety = KanaConverterPrivate::initFullAlphabetMapQwety();

KanaConverter::KanaConverter(QObject *parent) :
    QObject(*new KanaConverterPrivate(), parent)
{
}

KanaConverter::~KanaConverter()
{
}

void KanaConverter::setDictionary(OpenWnnDictionary *dict)
{
    Q_D(KanaConverter);
    /* get part of speech tags */
    d->mPosDefault  = dict->getPOS(OpenWnnDictionary::POS_TYPE_MEISI);
    d->mPosNumber   = dict->getPOS(OpenWnnDictionary::POS_TYPE_SUUJI);
    d->mPosSymbol   = dict->getPOS(OpenWnnDictionary::POS_TYPE_KIGOU);
}

QList<WnnWord> KanaConverter::createPseudoCandidateList(const QString &inputHiragana, const QString &inputRomaji)
{
    Q_D(KanaConverter);
    QList<WnnWord> list;

    if (inputHiragana.length() == 0) {
        return list;
    }

    /* Create pseudo candidates for all keyboard type */
    /* Hiragana(reading) / Full width katakana / Half width katakana */
    list.append(WnnWord(inputHiragana, inputHiragana));
    QString stringBuff;
    if (d->createCandidateString(inputHiragana, KanaConverterPrivate::mFullKatakanaMap, stringBuff)) {
        list.append(WnnWord(stringBuff, inputHiragana, d->mPosDefault));
    }
    if (d->createCandidateString(inputHiragana, KanaConverterPrivate::mHalfKatakanaMap, stringBuff)) {
        list.append(WnnWord(stringBuff, inputHiragana, d->mPosDefault));
    }

    /* Create pseudo candidates for Qwerty keyboard */
    d->createPseudoCandidateListForQwerty(list, inputHiragana, inputRomaji);
    return list;
}
