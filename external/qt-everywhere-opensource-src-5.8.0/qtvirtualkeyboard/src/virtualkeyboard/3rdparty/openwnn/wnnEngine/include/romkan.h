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

#ifndef ROMKAN_H
#define ROMKAN_H

#include "letterconverter.h"
#include <QMap>

class RomkanPrivate;

class Romkan : public LetterConverter
{
    Q_OBJECT
    Q_DISABLE_COPY(Romkan)
    Q_DECLARE_PRIVATE(Romkan)
protected:
    Romkan(QObjectPrivate &dd, QObject *parent = 0);

public:
    explicit Romkan(QObject *parent = 0);
    ~Romkan();

    bool convert(ComposingText &text) const;

protected:
    bool convertImpl(ComposingText &text, const QMap<QString, QString> &table) const;
};

#endif // ROMKAN_H
