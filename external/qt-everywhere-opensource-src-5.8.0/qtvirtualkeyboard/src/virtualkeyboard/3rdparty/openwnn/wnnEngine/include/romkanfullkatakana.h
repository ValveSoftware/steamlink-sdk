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

#ifndef ROMKANFULLKATAKANA_H
#define ROMKANFULLKATAKANA_H

#include "romkan.h"

class RomkanFullKatakanaPrivate;

class RomkanFullKatakana : public Romkan
{
    Q_OBJECT
    Q_DISABLE_COPY(RomkanFullKatakana)
    Q_DECLARE_PRIVATE(RomkanFullKatakana)
public:
    explicit RomkanFullKatakana(QObject *parent = 0);
    ~RomkanFullKatakana();

    bool convert(ComposingText &text) const;
};

#endif // ROMKANFULLKATAKANA_H
