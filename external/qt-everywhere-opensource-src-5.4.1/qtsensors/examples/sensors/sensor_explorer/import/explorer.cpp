/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "explorer.h"
#include <QtCore/QDebug>
#include <qsensor.h>
#include <QMetaObject>
#include <QMetaProperty>

QT_BEGIN_NAMESPACE

/*
    \class QSensorExplorer
    \brief The QSensorExplorer type provides an easy access for reading all avalaible sensors installed on the system.
*/

/*
    Construct a QSensorExplorer object with parent \a parent
*/
QSensorExplorer::QSensorExplorer(QObject* parent)
    : QObject(parent)
    , _selectedSensorItem(0)
{
    loadSensors();
}

/*
    Destructor of a QSensorExplorer
*/
QSensorExplorer::~QSensorExplorer()
{
}

/*
    Load all available sensors and store it in a list.
*/
void QSensorExplorer::loadSensors()
{
    //! [0]
    _availableSensors.clear();

    foreach (const QByteArray &type, QSensor::sensorTypes()) {
        qDebug() << "Found type" << type;
        foreach (const QByteArray &identifier, QSensor::sensorsForType(type)) {
            qDebug() << "Found identifier" << identifier;
            // Don't put in sensors we can't connect to
            QSensor* sensor = new QSensor(type, this);
            sensor->setIdentifier(identifier);
            if (!sensor->connectToBackend()) {
                qDebug() << "Couldn't connect to" << identifier;
                continue;
            }

            qDebug() << "Adding identifier" << identifier;
            _availableSensors.append(new QSensorItem(sensor, this));
        }
    }
    emit availableSensorsChanged();
    //! [0]
}

/*
    \fn QSensorExplorer::availableSensorsChanged()
    Notifies the client if the list of the available sensors was changed
*/

/*
    \property QSensorExplorer::availableSensors
    Returns a list of all available sensor.
*/
QQmlListProperty<QSensorItem> QSensorExplorer::availableSensors()
{
    return QQmlListProperty<QSensorItem>(this,_availableSensors);
}

/*
    \fn QSensorExplorer::selectedSensorItemChanged()
    Notifies the client if the selected sensor has been changed
*/

/*
    \property QSensorExplorer::selectedSensorItem
    Returns the current selected sensor item.
*/
QSensorItem* QSensorExplorer::selectedSensorItem()
{
    return _selectedSensorItem;
}

/*
    \fn QSensorExplorer::setSelectedSensorItem(QSensorItem* selitem)
    Sets the QSensorItem \a selitem as the current selected QSensorItem.
*/
void QSensorExplorer::setSelectedSensorItem(QSensorItem* selitem)
{
    if (selitem  && _selectedSensorItem != selitem) {
        if (_selectedSensorItem)
            _selectedSensorItem->unSelect();
        _selectedSensorItem = selitem;
        _selectedSensorItem->select();
        emit selectedSensorItemChanged();
    }
}

QT_END_NAMESPACE
