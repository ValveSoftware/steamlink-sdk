/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
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
