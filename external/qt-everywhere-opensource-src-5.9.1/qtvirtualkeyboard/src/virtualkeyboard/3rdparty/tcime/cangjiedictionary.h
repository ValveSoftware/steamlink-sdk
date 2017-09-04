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

#ifndef CANGJIEDICTIONARY_H
#define CANGJIEDICTIONARY_H

#include "worddictionary.h"
#include <QCollator>

namespace tcime {

/**
 * Extends WordDictionary to provide cangjie word-suggestions.
 */
class CangjieDictionary : public WordDictionary
{
public:
    CangjieDictionary();

    bool simplified() const;
    void setSimplified(bool simplified);

    QStringList getWords(const QString &input) const;

private:
    QStringList sortWords(const DictionaryEntry &data) const;
    QStringList searchWords(int secondaryIndex, const DictionaryEntry &data) const;

private:
    QCollator _collator;
    static bool _simplified;
};

}

#endif // CANGJIEDICTIONARY_H
