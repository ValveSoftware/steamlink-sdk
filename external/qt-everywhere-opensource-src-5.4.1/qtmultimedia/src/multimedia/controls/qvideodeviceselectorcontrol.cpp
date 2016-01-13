/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#include "qvideodeviceselectorcontrol.h"

QT_BEGIN_NAMESPACE

/*!
    \class QVideoDeviceSelectorControl

    \brief The QVideoDeviceSelectorControl class provides an video device selector media control.
    \inmodule QtMultimedia


    \ingroup multimedia_control

    The QVideoDeviceSelectorControl class provides descriptions of the video devices
    available on a system and allows one to be selected as the  endpoint of a
    media service.

    The interface name of QVideoDeviceSelectorControl is \c org.qt-project.qt.videodeviceselectorcontrol/5.0 as
    defined in QVideoDeviceSelectorControl_iid.
*/

/*!
    \macro QVideoDeviceSelectorControl_iid

    \c org.qt-project.qt.videodeviceselectorcontrol/5.0

    Defines the interface name of the QVideoDeviceSelectorControl class.

    \relates QVideoDeviceSelectorControl
*/

/*!
    Constructs a video device selector control with the given \a parent.
*/
QVideoDeviceSelectorControl::QVideoDeviceSelectorControl(QObject *parent)
    :QMediaControl(parent)
{
}

/*!
    Destroys a video device selector control.
*/
QVideoDeviceSelectorControl::~QVideoDeviceSelectorControl()
{
}

/*!
    \fn QVideoDeviceSelectorControl::deviceCount() const

    Returns the number of available video devices;
*/

/*!
    \fn QVideoDeviceSelectorControl::deviceName(int index) const

    Returns the name of the video device at \a index.
*/

/*!
    \fn QVideoDeviceSelectorControl::deviceDescription(int index) const

    Returns a description of the video device at \a index.
*/

/*!
    \fn QVideoDeviceSelectorControl::defaultDevice() const

    Returns the index of the default video device.
*/

/*!
    \fn QVideoDeviceSelectorControl::selectedDevice() const

    Returns the index of the selected video device.
*/

/*!
    \fn QVideoDeviceSelectorControl::setSelectedDevice(int index)

    Sets the selected video device \a index.
*/

/*!
    \fn QVideoDeviceSelectorControl::devicesChanged()

    Signals that the list of available video devices has changed.
*/

/*!
    \fn QVideoDeviceSelectorControl::selectedDeviceChanged(int index)

    Signals that the selected video device \a index has changed.
*/

/*!
    \fn QVideoDeviceSelectorControl::selectedDeviceChanged(const QString &name)

    Signals that the selected video device \a name has changed.
*/

#include "moc_qvideodeviceselectorcontrol.cpp"
QT_END_NAMESPACE

