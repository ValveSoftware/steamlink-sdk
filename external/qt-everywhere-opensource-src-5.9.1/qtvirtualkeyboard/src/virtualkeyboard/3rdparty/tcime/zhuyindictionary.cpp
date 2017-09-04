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

#include "zhuyindictionary.h"
#include "zhuyintable.h"

using namespace tcime;

ZhuyinDictionary::ZhuyinDictionary() :
    WordDictionary()
{
}

QStringList ZhuyinDictionary::getWords(const QString &input) const
{
    // Look up the syllables index; return empty string for invalid syllables.
    QStringList pair = ZhuyinTable::stripTones(input);
    int syllablesIndex = !pair.isEmpty() ? ZhuyinTable::getSyllablesIndex(pair[0]) : -1;
    if (syllablesIndex < 0 || syllablesIndex >= dictionary().size())
        return QStringList();

    // [22-initials * 39-finals] syllables array; each syllables entry points to
    // a char[] containing words for that syllables.
    const DictionaryEntry &data = dictionary()[syllablesIndex];
    if (data.isEmpty())
        return QStringList();

    // Counts of words for each tone are stored in the array beginning.
    int tone = ZhuyinTable::getTones(pair[1].at(0));
    int length = (int) data[tone].unicode();
    if (length == 0)
        return QStringList();

    int start = ZhuyinTable::getTonesCount();
    for (int i = 0;  i < tone; ++i)
        start += (int) data[i].unicode();

    QStringList words;
    for (int i = 0; i < length; ++i)
        words.append(data[start + i]);

    return words;
}
