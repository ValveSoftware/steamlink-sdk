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

#include "qcamerainfocontrol.h"

QT_BEGIN_NAMESPACE

/*!
    \class QCameraInfoControl
    \since 5.3

    \brief The QCameraInfoControl class provides a camera info media control.
    \inmodule QtMultimedia

    \ingroup multimedia_control

    The QCameraInfoControl class provides information about the camera devices
    available on the system.

    The interface name of QCameraInfoControl is \c org.qt-project.qt.camerainfocontrol/5.3 as
    defined in QCameraInfoControl_iid.
*/

/*!
    \macro QCameraInfoControl_iid

    \c org.qt-project.qt.camerainfocontrol/5.3

    Defines the interface name of the QCameraInfoControl class.

    \relates QVideoDeviceSelectorControl
*/

/*!
    Constructs a camera info control with the given \a parent.
*/
QCameraInfoControl::QCameraInfoControl(QObject *parent)
    : QMediaControl(parent)
{
}

/*!
    Destroys a camera info control.
*/
QCameraInfoControl::~QCameraInfoControl()
{
}

/*!
    \fn QCameraInfoControl::cameraPosition(const QString &deviceName) const

    Returns the physical position of the camera named \a deviceName on the hardware system.
*/

/*!
    \fn QCameraInfoControl::cameraOrientation(const QString &deviceName) const

    Returns the physical orientation of the sensor for the camera named \a deviceName.

    The value is the orientation angle (clockwise, in steps of 90 degrees) of the camera sensor
    in relation to the display in its natural orientation.
*/

#include "moc_qcamerainfocontrol.cpp"

QT_END_NAMESPACE
