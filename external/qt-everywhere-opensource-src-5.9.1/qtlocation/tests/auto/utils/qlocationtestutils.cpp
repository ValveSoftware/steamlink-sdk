/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qlocationtestutils_p.h"

bool QLocationTestUtils::hasDefaultSource()
{
    return false;
}

bool QLocationTestUtils::hasDefaultMonitor()
{
    return false;
}

QString QLocationTestUtils::addNmeaChecksumAndBreaks(const QString &sentence)
{
    Q_ASSERT(sentence[0] == '$' && sentence[sentence.length()-1] == '*');

    // XOR byte value of all characters between '$' and '*'
    int result = 0;
    for (int i=1; i<sentence.length()-1; i++)
        result ^= sentence[i].toLatin1();
    QString sum;
    sum.sprintf("%02x", result);
    return sentence + sum + "\r\n";
}

QString QLocationTestUtils::createRmcSentence(const QDateTime &dt)
{
    QString time = dt.toString("hhmmss.zzz");
    QString date = dt.toString("ddMMyy");
    QString nmea = QString("$GPRMC,%1,A,2730.83609,S,15301.87844,E,0.7,9.0,%2,11.2,W,A*")
        .arg(time).arg(date);
    return addNmeaChecksumAndBreaks(nmea);
}

QString QLocationTestUtils::createGgaSentence(const QTime &time)
{
    QString nmea = QString("$GPGGA,%1,2734.76859,S,15305.99361,E,1,04,3.5,49.4,M,39.2,M,,*")
            .arg(time.toString("hhmmss.zzz"));
    return addNmeaChecksumAndBreaks(nmea);
}

QString QLocationTestUtils::createGgaSentence(int lat, int lng, const QTime &time) {
    QString nmea = QString("$GPGGA,%1,%200.00000,S,%300.,E,1,04,3.5,49.4,M,39.2,M,,*")
            .arg(time.toString("hhmmss.zzz")).arg(lat).arg(lng);
    return addNmeaChecksumAndBreaks(nmea);
}

QString QLocationTestUtils::createZdaSentence(const QDateTime &dt)
{
    QString time = dt.toString("hhmmss.zzz");
    QString nmea = QString("$GPZDA,%1,%2,%3,%4,,*")
        .arg(time).arg(dt.toString("dd")).arg(dt.toString("MM")).arg(dt.toString("yyyy"));
    return addNmeaChecksumAndBreaks(nmea);
}

QString QLocationTestUtils::createGsaSentence()
{
    return addNmeaChecksumAndBreaks(QStringLiteral("$GPGSA,A,3,,,,,,,,,,,,,3.0,3.5,4.0*"));
}
