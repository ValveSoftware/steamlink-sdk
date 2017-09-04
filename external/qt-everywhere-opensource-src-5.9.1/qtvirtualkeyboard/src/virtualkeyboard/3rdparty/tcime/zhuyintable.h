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

#ifndef ZHUYINTABLE_H
#define ZHUYINTABLE_H

#include <QMap>
#include <QChar>
#include <QString>

namespace tcime {

class ZhuyinTable
{
    Q_DISABLE_COPY(ZhuyinTable)
    ZhuyinTable() {}

    // All Chinese characters are mapped into a zhuyin table as described in
    // http://en.wikipedia.org/wiki/Zhuyin_table.
    static const int INITIALS_SIZE;

    // Finals that can be appended after 'ㄧ' (yi), 'ㄨ' (wu), or 'ㄩ' (yu).
    static const QList<QChar> yiEndingFinals;
    static const QList<QChar> wuEndingFinals;
    static const QList<QChar> yuEndingFinals;

    // 'ㄧ' (yi) finals start from position 14 and are followed by 'ㄨ' (wu)
    // finals, and 'ㄩ' (yu) finals follow after 'ㄨ' (wu) finals.
    static const int YI_FINALS_INDEX;
    static const int WU_FINALS_INDEX;
    static const int YU_FINALS_INDEX;

    // 'ㄧ' (yi), 'ㄨ' (wu) , and 'ㄩ' (yu) finals.
    static const QChar YI_FINALS;
    static const QChar WU_FINALS;
    static const QChar YU_FINALS;

    // Default tone and four tone symbols: '˙', 'ˊ', 'ˇ', and 'ˋ'.
    static const QList<QChar> tones;

public:
    static const QChar DEFAULT_TONE;

    static int getInitials(const QChar &initials);
    static int getFinals(const QString &finals);
    static int getSyllablesIndex(const QString &syllables);
    static int getTones(const QChar &c);
    static int getTonesCount();
    static bool isTone(const QChar &c);
    static bool isYiWuYuFinals(const QChar &c);
    static QStringList stripTones(const QString &input);
};

}

#endif // ZHUYINTABLE_H
