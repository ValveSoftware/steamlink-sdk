/****************************************************************************
**
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
#include "qgeosatelliteinfo.h"

#include <QHash>
#include <QDebug>
#include <QDataStream>

QT_BEGIN_NAMESPACE

class QGeoSatelliteInfoPrivate
{
public:
    int signal;
    int satId;
    QGeoSatelliteInfo::SatelliteSystem system;
    QHash<int, qreal> doubleAttribs;
};


/*!
    \class QGeoSatelliteInfo
    \inmodule QtPositioning
    \ingroup QtPositioning-positioning
    \since 5.2

    \brief The QGeoSatelliteInfo class contains basic information about a satellite.

    \sa QGeoSatelliteInfoSource
*/

/*!
    \enum QGeoSatelliteInfo::Attribute
    Defines the attributes for the satellite information.
    \value Elevation The elevation of the satellite, in degrees.
    \value Azimuth The azimuth to true north, in degrees.
*/

/*!
    \enum QGeoSatelliteInfo::SatelliteSystem
    Defines the GNSS system of the satellite.
    \value Undefined Not defined.
    \value GPS Global Positioning System (USA).
    \value GLONASS Global Positioning System (Russia).

*/


/*!
    Creates a satellite information object.
*/
QGeoSatelliteInfo::QGeoSatelliteInfo()
        : d(new QGeoSatelliteInfoPrivate)
{
    d->signal = -1;
    d->satId = -1;
    d->system = QGeoSatelliteInfo::Undefined;
}

/*!
    Creates a satellite information object with the values of \a other.
*/

QGeoSatelliteInfo::QGeoSatelliteInfo(const QGeoSatelliteInfo &other)
        : d(new QGeoSatelliteInfoPrivate)
{
    operator=(other);
}

/*!
    Destroys a satellite information object.
*/
QGeoSatelliteInfo::~QGeoSatelliteInfo()
{
    delete d;
}

/*!
    Assigns the values from \a other to this object.
*/
QGeoSatelliteInfo &QGeoSatelliteInfo::operator=(const QGeoSatelliteInfo & other)
{
    if (this == &other)
        return *this;

    d->signal = other.d->signal;
    d->satId = other.d->satId;
    d->system = other.d->system;
    d->doubleAttribs = other.d->doubleAttribs;
    return *this;
}

/*!
    Returns true if all the information for this satellite
    are the same as those of \a other.
*/
bool QGeoSatelliteInfo::operator==(const QGeoSatelliteInfo &other) const
{
    return d->signal == other.d->signal
           && d->satId == other.d->satId
           && d->system == other.d->system
           && d->doubleAttribs == other.d->doubleAttribs;
}

/*!
    \fn bool QGeoSatelliteInfo::operator!=(const QGeoSatelliteInfo &other) const;

    Returns true if any of the information for this satellite
    are not the same as those of \a other.
*/


/*!
    Sets the Satellite System (GPS, GLONASS, ...) to \a system.
*/
void QGeoSatelliteInfo::setSatelliteSystem(SatelliteSystem system)
{
    d->system = system;
}

/*!
    Returns the Satellite System (GPS, GLONASS, ...)
*/
QGeoSatelliteInfo::SatelliteSystem QGeoSatelliteInfo::satelliteSystem() const
{
    return d->system;
}

/*!
    Sets the satellite identifier number to \a satId.

    The satellite identifier number can be used to identify a satellite inside the satellite system.
    For satellite system GPS the satellite identifier number represents the PRN (Pseudo-random noise) number.
    For satellite system GLONASS the satellite identifier number represents the slot number.
*/
void QGeoSatelliteInfo::setSatelliteIdentifier(int satId)
{
    d->satId = satId;
}

/*!
    Returns the satellite identifier number.

    The satellite identifier number can be used to identify a satellite inside the satellite system.
    For satellite system GPS the satellite identifier number represents the PRN (Pseudo-random noise) number.
    For satellite system GLONASS the satellite identifier number represents the slot number.
*/
int QGeoSatelliteInfo::satelliteIdentifier() const
{
    return d->satId;
}

/*!
    Sets the signal strength to \a signalStrength, in decibels.
*/
void QGeoSatelliteInfo::setSignalStrength(int signalStrength)
{
    d->signal = signalStrength;
}

/*!
    Returns the signal strength, or -1 if the value has not been set.
*/
int QGeoSatelliteInfo::signalStrength() const
{
    return d->signal;
}

/*!
    Sets the value for \a attribute to \a value.
*/
void QGeoSatelliteInfo::setAttribute(Attribute attribute, qreal value)
{
    d->doubleAttribs[int(attribute)] = value;
}

/*!
    Returns the value of the specified \a attribute as a qreal value.

    Returns -1 if the value has not been set.

    \sa hasAttribute(), setAttribute()
*/
qreal QGeoSatelliteInfo::attribute(Attribute attribute) const
{
    if (d->doubleAttribs.contains(int(attribute)))
        return d->doubleAttribs[int(attribute)];
    return -1;
}

/*!
    Removes the specified \a attribute and its value.
*/
void QGeoSatelliteInfo::removeAttribute(Attribute attribute)
{
    d->doubleAttribs.remove(int(attribute));
}

/*!
    Returns true if the specified \a attribute is present in this update.
*/
bool QGeoSatelliteInfo::hasAttribute(Attribute attribute) const
{
    return d->doubleAttribs.contains(int(attribute));
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QGeoSatelliteInfo &info)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QGeoSatelliteInfo(system=" << info.d->system;
    dbg << ", satId=" << info.d->satId;
    dbg << ", signal-strength=" << info.d->signal;


    QList<int> attribs = info.d->doubleAttribs.keys();
    for (int i = 0; i < attribs.count(); ++i) {
        dbg << ", ";
        switch (attribs[i]) {
            case QGeoSatelliteInfo::Elevation:
                dbg << "Elevation=";
                break;
            case QGeoSatelliteInfo::Azimuth:
                dbg << "Azimuth=";
                break;
        }
        dbg << info.d->doubleAttribs[attribs[i]];
    }
    dbg << ')';
    return dbg;
}
#endif

#ifndef QT_NO_DATASTREAM
/*!
    \fn QDataStream &operator<<(QDataStream &stream, const QGeoSatelliteInfo &info)
    \relates QGeoSatelliteInfo

    Writes the given \a info to the specified \a stream.

    \sa \link datastreamformat.html Format of the QDataStream operators \endlink

*/

QDataStream &operator<<(QDataStream &stream, const QGeoSatelliteInfo &info)
{
    stream << info.d->signal;
    stream << info.d->doubleAttribs;
    stream << info.d->satId;
    stream << int(info.d->system);
    return stream;
}
#endif

#ifndef QT_NO_DATASTREAM
/*!
    \fn QDataStream &operator>>(QDataStream &stream, QGeoSatelliteInfo &info)
    \relates QGeoSatelliteInfo

    Reads satellite information from the specified \a stream into the given
    \a info.

    \sa \link datastreamformat.html Format of the QDataStream operators \endlink
*/

QDataStream &operator>>(QDataStream &stream, QGeoSatelliteInfo &info)
{
    int system;
    stream >> info.d->signal;
    stream >> info.d->doubleAttribs;
    stream >> info.d->satId;
    stream >> system;
    info.d->system = (QGeoSatelliteInfo::SatelliteSystem)system;
    return stream;
}
#endif

QT_END_NAMESPACE
