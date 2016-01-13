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

#include "bbcamerainfocontrol.h"

#include "bbcamerasession.h"

QT_BEGIN_NAMESPACE

BbCameraInfoControl::BbCameraInfoControl(QObject *parent)
    : QCameraInfoControl(parent)
{
}

QCamera::Position BbCameraInfoControl::position(const QString &deviceName)
{
    if (deviceName == QString::fromUtf8(BbCameraSession::cameraIdentifierFront()))
        return QCamera::FrontFace;
    else if (deviceName == QString::fromUtf8(BbCameraSession::cameraIdentifierRear()))
        return QCamera::BackFace;
    else
        return QCamera::UnspecifiedPosition;
}

int BbCameraInfoControl::orientation(const QString &deviceName)
{
    // The camera sensor orientation could be retrieved with camera_get_native_orientation()
    // but since the sensor angular offset is compensated with camera_set_videovf_property() and
    // camera_set_photovf_property() we should always return 0 here.
    Q_UNUSED(deviceName);
    return 0;
}

QCamera::Position BbCameraInfoControl::cameraPosition(const QString &deviceName) const
{
    return position(deviceName);
}

int BbCameraInfoControl::cameraOrientation(const QString &deviceName) const
{
    return orientation(deviceName);
}

QT_END_NAMESPACE

