/****************************************************************************
**
** Copyright (C) 2016 Aaron McCarthy <mccarthy.aaron@gmail.com>
** Contact: http://www.qt-project.org/legal
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

#ifndef GEOCLUETYPES_H
#define GEOCLUETYPES_H

#include <QtDBus/QDBusArgument>
#include <QtPositioning/QGeoSatelliteInfo>

class Accuracy
{
public:
    enum Level {
        None = 0,
        Country,
        Region,
        Locality,
        PostalCode,
        Street,
        Detailed
    };

    Accuracy()
    :   m_level(None), m_horizontal(0), m_vertical(0)
    {
    }

    inline Level level() const { return m_level; }
    inline double horizontal() const { return m_horizontal; }
    inline double vertical() const { return m_vertical; }

private:
    Level m_level;
    double m_horizontal;
    double m_vertical;

    friend const QDBusArgument &dbus_argument_helper(const QDBusArgument &arg, Accuracy &accuracy);
};

Q_DECLARE_METATYPE(Accuracy)
Q_DECLARE_METATYPE(QList<QGeoSatelliteInfo>)


QT_BEGIN_NAMESPACE

Q_DECLARE_TYPEINFO(Accuracy, Q_MOVABLE_TYPE);

QDBusArgument &operator<<(QDBusArgument &arg, const Accuracy &accuracy);
const QDBusArgument &operator>>(const QDBusArgument &arg, Accuracy &accuracy);

const QDBusArgument &operator>>(const QDBusArgument &arg, QGeoSatelliteInfo &si);
const QDBusArgument &operator>>(const QDBusArgument &arg, QList<QGeoSatelliteInfo> &sis);

QT_END_NAMESPACE

#endif // GEOCLUETYPES_H

