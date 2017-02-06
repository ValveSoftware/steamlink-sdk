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

#ifndef KANACONVERTER_H
#define KANACONVERTER_H

#include <QObject>
#include "openwnndictionary.h"
#include "wnnword.h"

class KanaConverterPrivate;

class KanaConverter : QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(KanaConverter)
    Q_DECLARE_PRIVATE(KanaConverter)
public:
    explicit KanaConverter(QObject *parent = 0);
    ~KanaConverter();

    void setDictionary(OpenWnnDictionary *dict);
    QList<WnnWord> createPseudoCandidateList(const QString &inputHiragana, const QString &inputRomaji);
};

#endif // KANACONVERTER_H
