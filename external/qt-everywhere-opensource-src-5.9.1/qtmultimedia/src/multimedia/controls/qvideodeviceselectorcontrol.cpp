/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

