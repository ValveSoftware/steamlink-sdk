/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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
#include <QCameraFocusControl>

#include "qwinrtcameralockscontrol.h"
#include "qwinrtcameracontrol.h"

QT_BEGIN_NAMESPACE

QWinRTCameraLocksControl::QWinRTCameraLocksControl(QObject *parent)
    : QCameraLocksControl(parent)
    , m_supportedLocks(QCamera::NoLock)
    , m_focusLockStatus(QCamera::Unlocked)
{
}

QCamera::LockTypes QWinRTCameraLocksControl::supportedLocks() const
{
    return m_supportedLocks;
}

QCamera::LockStatus QWinRTCameraLocksControl::lockStatus(QCamera::LockType lock) const
{
    switch (lock) {
    case QCamera::LockFocus:
        return m_focusLockStatus;
    case QCamera::LockExposure:
    case QCamera::LockWhiteBalance:
    default:
        return QCamera::Unlocked;
    }
}

void QWinRTCameraLocksControl::searchAndLock(QCamera::LockTypes locks)
{
    QWinRTCameraControl *cameraControl = qobject_cast<QWinRTCameraControl *>(parent());
    Q_ASSERT(cameraControl);
    if (cameraControl->state() != QCamera::ActiveState)
        return;
    else if (locks.testFlag(QCamera::LockFocus))
        QMetaObject::invokeMethod(this, "searchAndLockFocus", Qt::QueuedConnection);
    else
        cameraControl->emitError(QCamera::InvalidRequestError, QStringLiteral("Unsupported camera lock type."));
}

void QWinRTCameraLocksControl::unlock(QCamera::LockTypes locks)
{
    if (locks.testFlag(QCamera::LockFocus))
        QMetaObject::invokeMethod(this, "unlockFocus", Qt::QueuedConnection);
}

void QWinRTCameraLocksControl::initialize()
{
    QWinRTCameraControl *cameraControl = qobject_cast<QWinRTCameraControl *>(parent());
    Q_ASSERT(cameraControl);
    QCameraFocusControl *focusControl = cameraControl->cameraFocusControl();
    Q_ASSERT(focusControl);
    if (focusControl->isFocusModeSupported(QCameraFocus::AutoFocus))
        m_supportedLocks |= QCamera::LockFocus;
}

void QWinRTCameraLocksControl::searchAndLockFocus()
{
    if (QCamera::Locked == m_focusLockStatus)
        unlockFocus();
    QWinRTCameraControl *cameraControl = qobject_cast<QWinRTCameraControl *>(parent());
    Q_ASSERT(cameraControl);
    QCameraFocusControl *focusControl = cameraControl->cameraFocusControl();
    Q_ASSERT(focusControl);
    if (focusControl->focusMode().testFlag(QCameraFocus::ContinuousFocus)) {
        cameraControl->emitError(QCamera::NotSupportedFeatureError, QStringLiteral("Camera can't lock focus in continuous focus mode."));
    } else {
        m_focusLockStatus = QCamera::Searching;
        emit lockStatusChanged(QCamera::LockFocus, m_focusLockStatus, QCamera::LockAcquired);
        if (cameraControl->focus()) {
            m_focusLockStatus = cameraControl->lockFocus() ? QCamera::Locked : QCamera::Unlocked;
            emit lockStatusChanged(QCamera::LockFocus, m_focusLockStatus, QCamera::LockAcquired);
        }
    }
}

void QWinRTCameraLocksControl::unlockFocus()
{
    if (QCamera::Unlocked == m_focusLockStatus)
        return;
    QWinRTCameraControl *cameraControl = qobject_cast<QWinRTCameraControl *>(parent());
    Q_ASSERT(cameraControl);
    m_focusLockStatus = cameraControl->unlockFocus() ? QCamera::Unlocked : QCamera::Locked;
    emit lockStatusChanged(QCamera::LockFocus, m_focusLockStatus, QCamera::UserRequest);
}

QT_END_NAMESPACE
