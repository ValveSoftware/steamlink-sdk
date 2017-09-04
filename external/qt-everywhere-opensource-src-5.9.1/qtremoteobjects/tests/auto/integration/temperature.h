/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <QSharedPointer>
#include <QString>

class Temperature
{
public:
    typedef QSharedPointer<Temperature> Ptr;

    Temperature() : _value(0) {}
    Temperature(double value, const QString &unit) : _value(value), _unit(unit) {}

    void setValue(double arg) { _value = arg; }
    double value() const { return _value; }

    void setUnit(const QString &arg) { _unit = arg; }
    QString unit() const { return _unit; }

private:
    double _value;
    QString _unit;
};

inline bool operator==(const Temperature &lhs, const Temperature &rhs)
{
    return lhs.unit() == rhs.unit() &&
            lhs.value() == rhs.value();
}

inline bool operator!=(const Temperature &lhs, const Temperature &rhs)
{
    return !(lhs == rhs);
}

inline QDataStream &operator<<(QDataStream &out, const Temperature &temperature)
{
    out << temperature.value();
    out << temperature.unit();
    return out;
}

inline QDataStream &operator>>(QDataStream &in, Temperature &temperature)
{
    double value;
    in >> value;
    temperature.setValue(value);

    QString unit;
    in >> unit;
    temperature.setUnit(unit);
    return in;
}

inline QDataStream &operator>>(QDataStream &out, const Temperature::Ptr& temperaturePtr)
{
    out << *temperaturePtr;
    return out;
}

inline QDataStream &operator>>(QDataStream &in, Temperature::Ptr& temperaturePtr)
{
    Temperature *temperature = new Temperature;
    in >> *temperature;
    temperaturePtr.reset(temperature);
    return in;
}

Q_DECLARE_METATYPE(Temperature);
Q_DECLARE_METATYPE(Temperature::Ptr);

#endif
