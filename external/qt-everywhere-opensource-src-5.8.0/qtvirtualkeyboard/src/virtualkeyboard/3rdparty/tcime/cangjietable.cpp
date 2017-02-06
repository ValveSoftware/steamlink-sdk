/*
 * Qt implementation of TCIME library
 * This file is part of the Qt Virtual Keyboard module.
 * Contact: http://www.qt.io/licensing/
 *
 * Copyright (C) 2015  The Qt Company
 * Copyright 2010 Google Inc.
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

#include "cangjietable.h"

using namespace tcime;

const int CangjieTable::BASE_NUMBER = 26;
const int CangjieTable::MAX_CODE_LENGTH = 5;
const int CangjieTable::MAX_SIMPLIFIED_CODE_LENGTH = 2;

const QMap<QChar, int> &CangjieTable::letters()
{
    static QMap<QChar, int> letters;
    if (letters.isEmpty()) {
        int i = 1;
        letters.insert(0x65e5, i++);
        letters.insert(0x6708, i++);
        letters.insert(0x91d1, i++);
        letters.insert(0x6728, i++);
        letters.insert(0x6c34, i++);
        letters.insert(0x706b, i++);
        letters.insert(0x571f, i++);
        letters.insert(0x7af9, i++);
        letters.insert(0x6208, i++);
        letters.insert(0x5341, i++);
        letters.insert(0x5927, i++);
        letters.insert(0x4e2d, i++);
        letters.insert(0x4e00, i++);
        letters.insert(0x5f13, i++);
        letters.insert(0x4eba, i++);
        letters.insert(0x5fc3, i++);
        letters.insert(0x624b, i++);
        letters.insert(0x53e3, i++);
        letters.insert(0x5c38, i++);
        letters.insert(0x5eff, i++);
        letters.insert(0x5c71, i++);
        letters.insert(0x5973, i++);
        letters.insert(0x7530, i++);
        letters.insert(0x96e3, i++);
        letters.insert(0x535c, i++);
    }
    return letters;
}

bool CangjieTable::isLetter(const QChar &c)
{
    static const QMap<QChar, int> &letters = CangjieTable::letters();
    return letters.contains(c);
}

int CangjieTable::getPrimaryIndex(const QString &code)
{
    static const QMap<QChar, int> &letters = CangjieTable::letters();
    int length = code.length();
    if ((length < 1) || (length > MAX_CODE_LENGTH))
        return -1;

    QChar c = code.at(0);
    if (!isLetter(c))
        return -1;

    // The first letter cannot be absent in the code; therefore, the numerical
    // index of the first letter starts from 0 instead.
    int index = (letters[c] - 1) * BASE_NUMBER;
    if (length < 2)
        return index;

    c = code.at(length - 1);
    if (!isLetter(c))
        return -1;

    return index + letters[c];
}

int CangjieTable::getSecondaryIndex(const QString &code)
{
    static const QMap<QChar, int> &letters = CangjieTable::letters();
    int index = 0;
    int last = code.length() - 1;
    for (int i = 1; i < last; i++) {
        QChar c = code.at(i);
        if (!isLetter(c))
            return -1;
        index = index * BASE_NUMBER + letters[c];
    }

    int maxEnd = MAX_CODE_LENGTH - 1;
    for (int i = last; i < maxEnd; i++)
        index = index * BASE_NUMBER;

    return index;
}
