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

#ifndef WORDDICTIONARY_H
#define WORDDICTIONARY_H

#include <QVector>
#include <QString>
#include <QStringList>

namespace tcime {

/**
 * Reads a word-dictionary and provides word-suggestions as a list of characters
 * for the specified input.
 */
class WordDictionary
{
    Q_DISABLE_COPY(WordDictionary)

protected:
    typedef QChar DictionaryWord;
    typedef QVector<DictionaryWord> DictionaryEntry;
    typedef QVector<DictionaryEntry> Dictionary;

    const Dictionary &dictionary() const { return _dictionary; }

public:
    WordDictionary() {}
    virtual ~WordDictionary() {}

    bool isEmpty() const { return _dictionary.isEmpty(); }

    virtual bool load(const QString &fileName, bool littleEndian = false);
    virtual QStringList getWords(const QString &input) const = 0;

private:
    Dictionary _dictionary;
};

}

#endif // WORDDICTIONARY_H
