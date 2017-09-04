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

#include "zhuyintable.h"
#include <QStringList>

using namespace tcime;

const int ZhuyinTable::INITIALS_SIZE = 22;
const QList<QChar> ZhuyinTable::yiEndingFinals = QList<QChar>()
        << 0x311a << 0x311b << 0x311d << 0x311e << 0x3120 << 0x3121 << 0x3122
        << 0x3123 << 0x3124 << 0x3125;
const QList<QChar> ZhuyinTable::wuEndingFinals = QList<QChar>()
        << 0x311a << 0x311b << 0x311e << 0x311f << 0x3122 << 0x3123 << 0x3124
        << 0x3125;
const QList<QChar> ZhuyinTable::yuEndingFinals = QList<QChar>()
        << 0x311d << 0x3122 << 0x3123 << 0x3125;
const int ZhuyinTable::YI_FINALS_INDEX = 14;
const int ZhuyinTable::WU_FINALS_INDEX = 25;
const int ZhuyinTable::YU_FINALS_INDEX = 34;
const QChar ZhuyinTable::YI_FINALS = 0x3127;
const QChar ZhuyinTable::WU_FINALS = 0x3128;
const QChar ZhuyinTable::YU_FINALS = 0x3129;
const QList<QChar> ZhuyinTable::tones = QList<QChar>()
        << ZhuyinTable::DEFAULT_TONE << 0x02d9 << 0x02ca << 0x02c7 << 0x02cb;
const QChar ZhuyinTable::DEFAULT_TONE = QChar(' ');

int ZhuyinTable::getInitials(const QChar &initials)
{
    // Calculate the index by its distance to the first initials 'ㄅ' (b).
    int index = initials.unicode() - 0x3105 + 1;
    if (index >= ZhuyinTable::INITIALS_SIZE)
        // Syllables starting with finals can still be valid.
        return 0;

    return (index >= 0) ? index : -1;
}

int ZhuyinTable::getFinals(const QString &finals)
{
    if (finals.length() == 0)
        // Syllables ending with no finals can still be valid.
        return 0;

    if (finals.length() > 2)
        return -1;

    // Compute the index instead of direct lookup the whole array to save
    // traversing time. First calculate the distance to the first finals
    // 'ㄚ' (a).
    const QChar firstFinal = finals.at(0);
    int index = firstFinal.unicode() - 0x311a + 1;
    if (index < YI_FINALS_INDEX)
        return index;

    // Check 'ㄧ' (yi), 'ㄨ' (wu) , and 'ㄩ' (yu) group finals.
    QList<QChar> endingFinals;
    if (firstFinal == YI_FINALS) {
        index = YI_FINALS_INDEX;
        endingFinals = yiEndingFinals;
    } else if (firstFinal == WU_FINALS) {
        index = WU_FINALS_INDEX;
        endingFinals = wuEndingFinals;
    } else if (firstFinal == YU_FINALS) {
        index = YU_FINALS_INDEX;
        endingFinals = yuEndingFinals;
    } else {
        return -1;
    }

    if (finals.length() == 1)
        return index;

    for (int i = 0; i < endingFinals.size(); ++i) {
        if (finals.at(1) == endingFinals[i])
            return index + i + 1;
    }
    return -1;
}

int ZhuyinTable::getSyllablesIndex(const QString &syllables)
{
    if (syllables.isEmpty())
        return -1;

    int initials = getInitials(syllables.at(0));
    if (initials < 0)
        return -1;

    // Strip out initials before getting finals column-index.
    int finals = getFinals((initials != 0) ? syllables.mid(1) : syllables);
    if (finals < 0)
        return -1;

    return (finals * INITIALS_SIZE + initials);
}

int ZhuyinTable::getTones(const QChar &c)
{
    for (int i = 0; i < tones.size(); ++i) {
        if (tones[i] == c)
            return i;
    }
    // Treat all other characters as the default tone with the index 0.
    return 0;
}

int ZhuyinTable::getTonesCount()
{
    return tones.size();
}

bool ZhuyinTable::isTone(const QChar &c)
{
    for (int i = 0; i < tones.size(); ++i) {
        if (tones[i] == c)
            return true;
    }
    return false;
}

bool ZhuyinTable::isYiWuYuFinals(const QChar &c)
{
    ushort unicode = c.unicode();
    return unicode == YI_FINALS || unicode == WU_FINALS || unicode == YU_FINALS;
}

QStringList ZhuyinTable::stripTones(const QString &input)
{
    const int last = input.length() - 1;
    if (last < 0)
        return QStringList();

    QChar tone = input.at(last);
    if (isTone(tone)) {
        QString syllables = input.left(last);
        if (syllables.length() <= 0)
            return QStringList();
        return QStringList() << syllables << QString(tone);
    }
    // Treat the tone-less input as the default tone (tone-0).
    return QStringList() << input << QString(DEFAULT_TONE);
}
