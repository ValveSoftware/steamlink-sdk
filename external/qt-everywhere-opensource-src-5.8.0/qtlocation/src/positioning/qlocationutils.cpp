/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd.
** Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "qlocationutils_p.h"
#include "qgeopositioninfo.h"

#include <QTime>
#include <QList>
#include <QByteArray>
#include <QDebug>

#include <math.h>

QT_BEGIN_NAMESPACE

// converts e.g. 15306.0235 from NMEA sentence to 153.100392
static double qlocationutils_nmeaDegreesToDecimal(double nmeaDegrees)
{
    double deg;
    double min = 100.0 * modf(nmeaDegrees / 100.0, &deg);
    return deg + (min / 60.0);
}

static void qlocationutils_readGga(const char *data, int size, QGeoPositionInfo *info, double uere,
                                   bool *hasFix)
{
    QByteArray sentence(data, size);
    QList<QByteArray> parts = sentence.split(',');
    QGeoCoordinate coord;

    if (hasFix && parts.count() > 6 && parts[6].count() > 0)
        *hasFix = parts[6].toInt() > 0;

    if (parts.count() > 1 && parts[1].count() > 0) {
        QTime time;
        if (QLocationUtils::getNmeaTime(parts[1], &time))
            info->setTimestamp(QDateTime(QDate(), time, Qt::UTC));
    }

    if (parts.count() > 5 && parts[3].count() == 1 && parts[5].count() == 1) {
        double lat;
        double lng;
        if (QLocationUtils::getNmeaLatLong(parts[2], parts[3][0], parts[4], parts[5][0], &lat, &lng)) {
            coord.setLatitude(lat);
            coord.setLongitude(lng);
        }
    }

    if (parts.count() > 8 && !parts[8].isEmpty()) {
        bool hasHdop = false;
        double hdop = parts[8].toDouble(&hasHdop);
        if (hasHdop)
            info->setAttribute(QGeoPositionInfo::HorizontalAccuracy, 2 * hdop * uere);
    }

    if (parts.count() > 9 && parts[9].count() > 0) {
        bool hasAlt = false;
        double alt = parts[9].toDouble(&hasAlt);
        if (hasAlt)
            coord.setAltitude(alt);
    }

    if (coord.type() != QGeoCoordinate::InvalidCoordinate)
        info->setCoordinate(coord);
}

static void qlocationutils_readGsa(const char *data, int size, QGeoPositionInfo *info, double uere,
                                   bool *hasFix)
{
    QList<QByteArray> parts = QByteArray::fromRawData(data, size).split(',');

    if (hasFix && parts.count() > 2 && !parts[2].isEmpty())
        *hasFix = parts[2].toInt() > 0;

    if (parts.count() > 16 && !parts[16].isEmpty()) {
        bool hasHdop = false;
        double hdop = parts[16].toDouble(&hasHdop);
        if (hasHdop)
            info->setAttribute(QGeoPositionInfo::HorizontalAccuracy, 2 * hdop * uere);
    }

    if (parts.count() > 17 && !parts[17].isEmpty()) {
        bool hasVdop = false;
        double vdop = parts[17].toDouble(&hasVdop);
        if (hasVdop)
            info->setAttribute(QGeoPositionInfo::VerticalAccuracy, 2 * vdop * uere);
    }
}

static void qlocationutils_readGll(const char *data, int size, QGeoPositionInfo *info, bool *hasFix)
{
    QByteArray sentence(data, size);
    QList<QByteArray> parts = sentence.split(',');
    QGeoCoordinate coord;

    if (hasFix && parts.count() > 6 && parts[6].count() > 0)
        *hasFix = (parts[6][0] == 'A');

    if (parts.count() > 5 && parts[5].count() > 0) {
        QTime time;
        if (QLocationUtils::getNmeaTime(parts[5], &time))
            info->setTimestamp(QDateTime(QDate(), time, Qt::UTC));
    }

    if (parts.count() > 4 && parts[2].count() == 1 && parts[4].count() == 1) {
        double lat;
        double lng;
        if (QLocationUtils::getNmeaLatLong(parts[1], parts[2][0], parts[3], parts[4][0], &lat, &lng)) {
            coord.setLatitude(lat);
            coord.setLongitude(lng);
        }
    }

    if (coord.type() != QGeoCoordinate::InvalidCoordinate)
        info->setCoordinate(coord);
}

static void qlocationutils_readRmc(const char *data, int size, QGeoPositionInfo *info, bool *hasFix)
{
    QByteArray sentence(data, size);
    QList<QByteArray> parts = sentence.split(',');
    QGeoCoordinate coord;
    QDate date;
    QTime time;

    if (hasFix && parts.count() > 2 && parts[2].count() > 0)
        *hasFix = (parts[2][0] == 'A');

    if (parts.count() > 9 && parts[9].count() == 6) {
        date = QDate::fromString(QString::fromLatin1(parts[9]), QStringLiteral("ddMMyy"));
        if (date.isValid())
            date = date.addYears(100);     // otherwise starts from 1900
        else
            date = QDate();
    }

    if (parts.count() > 1 && parts[1].count() > 0)
        QLocationUtils::getNmeaTime(parts[1], &time);

    if (parts.count() > 6 && parts[4].count() == 1 && parts[6].count() == 1) {
        double lat;
        double lng;
        if (QLocationUtils::getNmeaLatLong(parts[3], parts[4][0], parts[5], parts[6][0], &lat, &lng)) {
            coord.setLatitude(lat);
            coord.setLongitude(lng);
        }
    }

    bool parsed = false;
    double value = 0.0;
    if (parts.count() > 7 && parts[7].count() > 0) {
        value = parts[7].toDouble(&parsed);
        if (parsed)
            info->setAttribute(QGeoPositionInfo::GroundSpeed, qreal(value * 1.852 / 3.6));    // knots -> m/s
    }
    if (parts.count() > 8 && parts[8].count() > 0) {
        value = parts[8].toDouble(&parsed);
        if (parsed)
            info->setAttribute(QGeoPositionInfo::Direction, qreal(value));
    }
    if (parts.count() > 11 && parts[11].count() == 1
            && (parts[11][0] == 'E' || parts[11][0] == 'W')) {
        value = parts[10].toDouble(&parsed);
        if (parsed) {
            if (parts[11][0] == 'W')
                value *= -1;
            info->setAttribute(QGeoPositionInfo::MagneticVariation, qreal(value));
        }
    }

    if (coord.type() != QGeoCoordinate::InvalidCoordinate)
        info->setCoordinate(coord);

    info->setTimestamp(QDateTime(date, time, Qt::UTC));
}

static void qlocationutils_readVtg(const char *data, int size, QGeoPositionInfo *info, bool *hasFix)
{
    if (hasFix)
        *hasFix = false;

    QByteArray sentence(data, size);
    QList<QByteArray> parts = sentence.split(',');

    bool parsed = false;
    double value = 0.0;
    if (parts.count() > 1 && parts[1].count() > 0) {
        value = parts[1].toDouble(&parsed);
        if (parsed)
            info->setAttribute(QGeoPositionInfo::Direction, qreal(value));
    }
    if (parts.count() > 7 && parts[7].count() > 0) {
        value = parts[7].toDouble(&parsed);
        if (parsed)
            info->setAttribute(QGeoPositionInfo::GroundSpeed, qreal(value / 3.6));    // km/h -> m/s
    }
}

static void qlocationutils_readZda(const char *data, int size, QGeoPositionInfo *info, bool *hasFix)
{
    if (hasFix)
        *hasFix = false;

    QByteArray sentence(data, size);
    QList<QByteArray> parts = sentence.split(',');
    QDate date;
    QTime time;

    if (parts.count() > 1 && parts[1].count() > 0)
        QLocationUtils::getNmeaTime(parts[1], &time);

    if (parts.count() > 4 && parts[2].count() > 0 && parts[3].count() > 0
            && parts[4].count() == 4) {     // must be full 4-digit year
        int day = parts[2].toUInt();
        int month = parts[3].toUInt();
        int year = parts[4].toUInt();
        if (day > 0 && month > 0 && year > 0)
            date.setDate(year, month, day);
    }

    info->setTimestamp(QDateTime(date, time, Qt::UTC));
}

bool QLocationUtils::getPosInfoFromNmea(const char *data, int size, QGeoPositionInfo *info,
                                        double uere, bool *hasFix)
{
    if (!info)
        return false;

    if (hasFix)
        *hasFix = false;
    if (size < 6 || data[0] != '$' || !hasValidNmeaChecksum(data, size))
        return false;

    // Adjust size so that * and following characters are not parsed by the following functions.
    for (int i = 0; i < size; ++i) {
        if (data[i] == '*') {
            size = i;
            break;
        }
    }

    if (data[3] == 'G' && data[4] == 'G' && data[5] == 'A') {
        // "$--GGA" sentence.
        qlocationutils_readGga(data, size, info, uere, hasFix);
        return true;
    }

    if (data[3] == 'G' && data[4] == 'S' && data[5] == 'A') {
        // "$--GSA" sentence.
        qlocationutils_readGsa(data, size, info, uere, hasFix);
        return true;
    }

    if (data[3] == 'G' && data[4] == 'L' && data[5] == 'L') {
        // "$--GLL" sentence.
        qlocationutils_readGll(data, size, info, hasFix);
        return true;
    }

    if (data[3] == 'R' && data[4] == 'M' && data[5] == 'C') {
        // "$--RMC" sentence.
        qlocationutils_readRmc(data, size, info, hasFix);
        return true;
    }

    if (data[3] == 'V' && data[4] == 'T' && data[5] == 'G') {
        // "$--VTG" sentence.
        qlocationutils_readVtg(data, size, info, hasFix);
        return true;
    }

    if (data[3] == 'Z' && data[4] == 'D' && data[5] == 'A') {
        // "$--ZDA" sentence.
        qlocationutils_readZda(data, size, info, hasFix);
        return true;
    }

    return false;
}

bool QLocationUtils::hasValidNmeaChecksum(const char *data, int size)
{
    int asteriskIndex = -1;
    for (int i = 0; i < size; ++i) {
        if (data[i] == '*') {
            asteriskIndex = i;
            break;
        }
    }

    const int CSUM_LEN = 2;
    if (asteriskIndex < 0 || asteriskIndex + CSUM_LEN >= size)
        return false;

    // XOR byte value of all characters between '$' and '*'
    int result = 0;
    for (int i = 1; i < asteriskIndex; ++i)
        result ^= data[i];
    /*
        char calc[CSUM_LEN + 1];
        ::snprintf(calc, CSUM_LEN + 1, "%02x", result);
        return ::strncmp(calc, &data[asteriskIndex+1], 2) == 0;
        */

    QByteArray checkSumBytes(&data[asteriskIndex + 1], 2);
    bool ok = false;
    int checksum = checkSumBytes.toInt(&ok,16);
    return ok && checksum == result;
}

bool QLocationUtils::getNmeaTime(const QByteArray &bytes, QTime *time)
{
    int dotIndex = bytes.indexOf('.');
    QTime tempTime;

    if (dotIndex < 0) {
        tempTime = QTime::fromString(QString::fromLatin1(bytes.constData()),
                                     QStringLiteral("hhmmss"));
    } else {
        tempTime = QTime::fromString(QString::fromLatin1(bytes.mid(0, dotIndex)),
                                     QStringLiteral("hhmmss"));
        bool hasMsecs = false;
        int midLen = qMin(3, bytes.size() - dotIndex - 1);
        int msecs = bytes.mid(dotIndex + 1, midLen).toUInt(&hasMsecs);
        if (hasMsecs)
            tempTime = tempTime.addMSecs(msecs);
    }

    if (tempTime.isValid()) {
        *time = tempTime;
        return true;
    }
    return false;
}

bool QLocationUtils::getNmeaLatLong(const QByteArray &latString, char latDirection, const QByteArray &lngString, char lngDirection, double *lat, double *lng)
{
    if ((latDirection != 'N' && latDirection != 'S')
            || (lngDirection != 'E' && lngDirection != 'W')) {
        return false;
    }

    bool hasLat = false;
    bool hasLong = false;
    double tempLat = latString.toDouble(&hasLat);
    double tempLng = lngString.toDouble(&hasLong);
    if (hasLat && hasLong) {
        tempLat = qlocationutils_nmeaDegreesToDecimal(tempLat);
        if (latDirection == 'S')
            tempLat *= -1;
        tempLng = qlocationutils_nmeaDegreesToDecimal(tempLng);
        if (lngDirection == 'W')
            tempLng *= -1;

        if (isValidLat(tempLat) && isValidLong(tempLng)) {
            *lat = tempLat;
            *lng = tempLng;
            return true;
        }
    }
    return false;
}

QT_END_NAMESPACE

