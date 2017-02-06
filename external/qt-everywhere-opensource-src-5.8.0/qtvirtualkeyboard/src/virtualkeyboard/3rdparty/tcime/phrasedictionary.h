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

#ifndef PHRASEDICTIONARY_H
#define PHRASEDICTIONARY_H

#include "worddictionary.h"

namespace tcime {

/**
 * Reads a phrase dictionary and provides following-word suggestions as a list
 * of characters for the given character.
 */
class PhraseDictionary : public WordDictionary
{
public:
    PhraseDictionary();

    QStringList getWords(const QString &input) const;
};

}

#endif // PHRASEDICTIONARY_H
