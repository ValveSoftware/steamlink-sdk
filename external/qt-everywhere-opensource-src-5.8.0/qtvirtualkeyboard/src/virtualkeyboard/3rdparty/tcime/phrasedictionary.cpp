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

#include "phrasedictionary.h"

using namespace tcime;

PhraseDictionary::PhraseDictionary() :
    WordDictionary()
{
}

QStringList PhraseDictionary::getWords(const QString &input) const
{
    if (input.length() != 1)
        return QStringList();

    // Phrases are stored in an array consisting of three character arrays.
    // char[0][] contains a char[] of words to look for phrases.
    // char[2][] contains a char[] of following words for char[0][].
    // char[1][] contains offsets of char[0][] words to map its following words.
    // For example, there are 5 phrases: Aa, Aa', Bb, Bb', Cc.
    // char[0][] { A, B, C }
    // char[1][] { 0, 2, 4 }
    // char[2][] { a, a', b, b', c}
    const Dictionary &dict = dictionary();
    if (dict.length() != 3)
        return QStringList();

    const DictionaryEntry &words = dict[0];

    DictionaryEntry::ConstIterator word = qBinaryFind(words, input.at(0));
    if (word == words.constEnd())
        return QStringList();

    int index = word - words.constBegin();
    const DictionaryEntry &offsets = dict[1];
    const DictionaryEntry &phrases = dict[2];
    int offset = (int)offsets[index].unicode();
    int count = (index < offsets.length() - 1) ?
        ((int)offsets[index + 1].unicode() - offset) : (phrases.length() - offset);

    QStringList result;
    for (int i = 0; i < count; ++i)
        result.append(phrases[offset + i]);

    return result;
}
