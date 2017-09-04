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

#include "cangjiedictionary.h"
#include "cangjietable.h"

using namespace tcime;

bool CangjieDictionary::_simplified = false;

CangjieDictionary::CangjieDictionary() :
    WordDictionary(),
    _collator(QLocale("zh_TW"))
{
}

bool CangjieDictionary::simplified() const
{
    return _simplified;
}

void CangjieDictionary::setSimplified(bool simplified)
{
    _simplified = simplified;
}

QStringList CangjieDictionary::getWords(const QString &input) const
{
    // Look up the index in the dictionary for the specified input.
    int primaryIndex = CangjieTable::getPrimaryIndex(input);
    if (primaryIndex < 0 || primaryIndex >= dictionary().size())
        return QStringList();

    // [25 * 26] char[] array; each primary entry points to a char[]
    // containing words with the same primary index; then words can be looked up
    // by their secondary index stored at the beginning of each char[].
    const DictionaryEntry &data = dictionary()[primaryIndex];
    if (data.isEmpty())
        return QStringList();

    if (_simplified)
        // Sort words of this primary index for simplified-cangjie.
        return sortWords(data);

    int secondaryIndex = CangjieTable::getSecondaryIndex(input);
    if (secondaryIndex < 0)
        return QStringList();

    // Find words match this secondary index for cangjie.
    return searchWords(secondaryIndex, data);
}

class DictionaryComparator
{
public:
    explicit DictionaryComparator(const std::vector<QCollatorSortKey> &sortKeys) :
        sortKeys(sortKeys)
    {}

    bool operator()(int a, int b)
    {
        return sortKeys[a] < sortKeys[b];
    }

private:
    const std::vector<QCollatorSortKey> &sortKeys;
};

QStringList CangjieDictionary::sortWords(const DictionaryEntry &data) const
{
    int length = data.size() / 2;
    std::vector<QCollatorSortKey> sortKeys;
    QVector<int> keys;
    sortKeys.reserve(length);
    keys.reserve(length);
    for (int i = 0; i < length; ++i) {
        sortKeys.push_back(_collator.sortKey(data[length + i]));
        keys.append(i);
    }
    DictionaryComparator dictionaryComparator(sortKeys);
    std::sort(keys.begin(), keys.end(), dictionaryComparator);

    QStringList words;
    for (int i = 0; i < length; ++i)
        words.append(data[length + keys[i]]);

    return words;
}

QStringList CangjieDictionary::searchWords(int secondaryIndex, const DictionaryEntry &data) const
{
    int length = data.size() / 2;

    DictionaryEntry::ConstIterator start = data.constBegin();
    DictionaryEntry::ConstIterator end = start + length;
    DictionaryEntry::ConstIterator rangeStart = qBinaryFind(start, end, (DictionaryWord)secondaryIndex);
    if (rangeStart == end)
        return QStringList();

    // There may be more than one words with the same index; look up words with
    // the same secondary index.
    while (rangeStart != start) {
        if (*(rangeStart - 1) != (DictionaryWord)secondaryIndex)
            break;
        rangeStart--;
    }

    DictionaryEntry::ConstIterator rangeEnd = rangeStart + 1;
    while (rangeEnd != end) {
        if (*rangeEnd != (DictionaryWord)secondaryIndex)
            break;
        rangeEnd++;
    }

    QStringList words;
    words.reserve(rangeEnd - rangeStart);
    for (DictionaryEntry::ConstIterator rangeIndex = rangeStart; rangeIndex < rangeEnd; ++rangeIndex) {
        DictionaryEntry::ConstIterator item(rangeIndex + length);
        words.append(*item);
    }

    return words;
}
