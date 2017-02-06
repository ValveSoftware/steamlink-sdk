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

#ifndef OPENWNNDICTIONARY_H
#define OPENWNNDICTIONARY_H

#include <QObject>
#include <QBitArray>
#include "wnnword.h"

class OpenWnnDictionaryPrivate;

class OpenWnnDictionary : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(OpenWnnDictionary)
    Q_DECLARE_PRIVATE(OpenWnnDictionary)
public:
    explicit OpenWnnDictionary(QObject *parent = 0);
    ~OpenWnnDictionary();

    enum ApproxPattern {
        APPROX_PATTERN_EN_TOUPPER,
        APPROX_PATTERN_EN_TOLOWER,
        APPROX_PATTERN_EN_QWERTY_NEAR,
        APPROX_PATTERN_EN_QWERTY_NEAR_UPPER,
        APPROX_PATTERN_JAJP_12KEY_NORMAL
    };

    enum SearchOperation {
        SEARCH_EXACT,
        SEARCH_PREFIX,
        SEARCH_LINK
    };

    enum SearchOrder {
        ORDER_BY_FREQUENCY,
        ORDER_BY_KEY
    };

    enum PartOfSpeechType {
        POS_TYPE_V1,
        POS_TYPE_V2,
        POS_TYPE_V3,
        POS_TYPE_BUNTOU,
        POS_TYPE_TANKANJI,
        POS_TYPE_SUUJI,
        POS_TYPE_MEISI,
        POS_TYPE_JINMEI,
        POS_TYPE_CHIMEI,
        POS_TYPE_KIGOU
    };

    enum {
        INDEX_USER_DICTIONARY = -1,
        INDEX_LEARN_DICTIONARY = -2,
    };

    void setInUseState(bool flag);

    void clearDictionary();
    int setDictionary(int index, int base, int high);

    void clearApproxPattern();
    int setApproxPattern(const QString &src, const QString &dst);
    int setApproxPattern(ApproxPattern approxPattern);

    int searchWord(SearchOperation operation, SearchOrder order, const QString &keyString);
    int searchWord(SearchOperation operation, SearchOrder order, const QString &keyString, const WnnWord &wnnWord);
    QSharedPointer<WnnWord> getNextWord(int length = 0);

    QList<QBitArray> getConnectMatrix();
    WnnPOS getPOS(PartOfSpeechType type);

    int learnWord(const WnnWord &word, const WnnWord *previousWord = 0);
};

#endif // OPENWNNDICTIONARY_H
