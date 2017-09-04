/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#include "bbcameraorientationhandler.h"

#include <QAbstractEventDispatcher>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>

#include <bps/orientation.h>

QT_BEGIN_NAMESPACE

BbCameraOrientationHandler::BbCameraOrientationHandler(QObject *parent)
    : QObject(parent)
    , m_orientation(0)
{
    QCoreApplication::eventDispatcher()->installNativeEventFilter(this);
    int result = orientation_request_events(0);
    if (result == BPS_FAILURE)
        qWarning() << "Unable to register for orientation change events";

    orientation_direction_t direction = ORIENTATION_FACE_UP;
    int angle = 0;

    result = orientation_get(&direction, &angle);
    if (result == BPS_FAILURE) {
        qWarning() << "Unable to retrieve initial orientation";
    } else {
        m_orientation = angle;
    }
}

BbCameraOrientationHandler::~BbCameraOrientationHandler()
{
    const int result = orientation_stop_events(0);
    if (result == BPS_FAILURE)
        qWarning() << "Unable to unregister for orientation change events";

    QCoreApplication::eventDispatcher()->removeNativeEventFilter(this);
}

bool BbCameraOrientationHandler::nativeEventFilter(const QByteArray&, void *message, long*)
{
    bps_event_t* const event = static_cast<bps_event_t*>(message);
    if (!event || bps_event_get_domain(event) != orientation_get_domain())
        return false;

    const int angle = orientation_event_get_angle(event);
    if (angle != m_orientation) {
        if (angle == 180) // The screen does not rotate at 180 degrees
            return false;

        m_orientation = angle;
        emit orientationChanged(m_orientation);
    }

    return false; // do not drop the event
}

int BbCameraOrientationHandler::viewfinderOrientation() const
{
    // On a keyboard device we do not rotate the screen at all
    if (qGuiApp->primaryScreen()->nativeOrientation()
            != qGuiApp->primaryScreen()->primaryOrientation()) {
        return m_orientation;
    }

    return 0;
}

int BbCameraOrientationHandler::orientation() const
{
    return m_orientation;
}

QT_END_NAMESPACE
