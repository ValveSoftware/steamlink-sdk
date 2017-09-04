/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "sensoritem.h"
#include <QtCore/QDebug>
#include <QtSensors>

QT_BEGIN_NAMESPACE

/*
    \class QPropertyInfo
    \brief The QPropertyInfo type provides an easy access for reading and writing the property values.
*/

/*
    Construct a QPropertyInfo object with parent \a parent
*/
QPropertyInfo::QPropertyInfo(QObject* parent)
    : QObject(parent)
    , _index(0)
    , _isWriteable(false)
    , _name("")
    , _typeName("")
    , _value("")
{}

/*
    Construct a QPropertyInfo object with parent \a parent, property name \a name, property index \a index,
    property write access \a writeable, property type \a typeName and property value \a value
*/
QPropertyInfo::QPropertyInfo(const QString& name, int index, bool writeable, const QString& typeName, const QString& value, QObject* parent)
    : QObject(parent)
    , _index(index)
    , _isWriteable(writeable)
    , _name(name)
    , _typeName(typeName)
    , _value(value)
{}

/*
    \property QPropertyInfo::name
    Returns the name of the property
*/
QString QPropertyInfo::name()
{
    return _name;
}

/*
    \property QPropertyInfo::typeName
    Returns the type of the property
*/
QString QPropertyInfo::typeName()
{
    return _typeName;
}

/*
    \property QPropertyInfo::value
    Returns the current value of the property
*/
QString QPropertyInfo::value()
{
    return _value;
}

/*
    \fn void QPropertyInfo::valueChanged()
    Signal that notifies the client if the property value was changed.
*/

/*
    \fn QPropertyInfo::setValue(const QString& value)
    Sets the value \a value of the property
*/
void QPropertyInfo::setValue(const QString& value)
{
    if (value != _value){
        _value = value;
        emit valueChanged();
    }
}

/*
    \fn QPropertyInfo::index()
    Returns the meta-data index of the property
*/
int QPropertyInfo::index()
{
    return _index;
}

/*
    \property QPropertyInfo::isWriteable
    Returns true if the property is writeable false if property is read only
*/
bool QPropertyInfo::isWriteable()
{
    return _isWriteable;
}

QT_END_NAMESPACE
