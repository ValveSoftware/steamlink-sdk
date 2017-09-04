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
#include "bbcameralockscontrol.h"

#include "bbcamerasession.h"

QT_BEGIN_NAMESPACE

BbCameraLocksControl::BbCameraLocksControl(BbCameraSession *session, QObject *parent)
    : QCameraLocksControl(parent)
    , m_session(session)
    , m_locksApplyMode(IndependentMode)
    , m_focusLockStatus(QCamera::Unlocked)
    , m_exposureLockStatus(QCamera::Unlocked)
    , m_whiteBalanceLockStatus(QCamera::Unlocked)
    , m_currentLockTypes(QCamera::NoLock)
    , m_supportedLockTypes(QCamera::NoLock)
{
    connect(m_session, SIGNAL(cameraOpened()), SLOT(cameraOpened()));
    connect(m_session, SIGNAL(focusStatusChanged(int)), SLOT(focusStatusChanged(int)));
}

QCamera::LockTypes BbCameraLocksControl::supportedLocks() const
{
    return (QCamera::LockFocus | QCamera::LockExposure | QCamera::LockWhiteBalance);
}

QCamera::LockStatus BbCameraLocksControl::lockStatus(QCamera::LockType lock) const
{
    if (!m_supportedLockTypes.testFlag(lock) || (m_session->handle() == CAMERA_HANDLE_INVALID))
        return QCamera::Locked;

    switch (lock) {
    case QCamera::LockExposure:
        return m_exposureLockStatus;
    case QCamera::LockWhiteBalance:
        return m_whiteBalanceLockStatus;
    case QCamera::LockFocus:
        return m_focusLockStatus;
    default:
        return QCamera::Locked;
    }
}

void BbCameraLocksControl::searchAndLock(QCamera::LockTypes locks)
{
    if (m_session->handle() == CAMERA_HANDLE_INVALID)
        return;

    // filter out unsupported locks
    locks &= m_supportedLockTypes;

    m_currentLockTypes |= locks;

    uint32_t lockModes = CAMERA_3A_NONE;

    switch (m_locksApplyMode) {
    case IndependentMode:
        if (m_currentLockTypes & QCamera::LockExposure)
            lockModes |= CAMERA_3A_AUTOEXPOSURE;
        if (m_currentLockTypes & QCamera::LockWhiteBalance)
            lockModes |= CAMERA_3A_AUTOWHITEBALANCE;
        if (m_currentLockTypes & QCamera::LockFocus)
            lockModes |= CAMERA_3A_AUTOFOCUS;
        break;
    case FocusExposureBoundMode:
        if ((m_currentLockTypes & QCamera::LockExposure) || (m_currentLockTypes & QCamera::LockFocus))
            lockModes = (CAMERA_3A_AUTOEXPOSURE | CAMERA_3A_AUTOFOCUS);
        break;
    case AllBoundMode:
        lockModes = (CAMERA_3A_AUTOEXPOSURE | CAMERA_3A_AUTOFOCUS | CAMERA_3A_AUTOWHITEBALANCE);
        break;
    case FocusOnlyMode:
        lockModes = CAMERA_3A_AUTOFOCUS;
        break;
    }

    const camera_error_t result = camera_set_3a_lock(m_session->handle(), lockModes);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to set lock modes:" << result;
    } else {
        if (lockModes & CAMERA_3A_AUTOFOCUS) {
            // handled by focusStatusChanged()
        }

        if (lockModes & CAMERA_3A_AUTOEXPOSURE) {
            m_exposureLockStatus = QCamera::Locked;
            emit lockStatusChanged(QCamera::LockExposure, QCamera::Locked, QCamera::LockAcquired);
        }

        if (lockModes & CAMERA_3A_AUTOWHITEBALANCE) {
            m_whiteBalanceLockStatus = QCamera::Locked;
            emit lockStatusChanged(QCamera::LockWhiteBalance, QCamera::Locked, QCamera::LockAcquired);
        }
    }
}

void BbCameraLocksControl::unlock(QCamera::LockTypes locks)
{
    // filter out unsupported locks
    locks &= m_supportedLockTypes;

    m_currentLockTypes &= ~locks;

    uint32_t lockModes = CAMERA_3A_NONE;

    switch (m_locksApplyMode) {
    case IndependentMode:
        if (m_currentLockTypes & QCamera::LockExposure)
            lockModes |= CAMERA_3A_AUTOEXPOSURE;
        if (m_currentLockTypes & QCamera::LockWhiteBalance)
            lockModes |= CAMERA_3A_AUTOWHITEBALANCE;
        if (m_currentLockTypes & QCamera::LockFocus)
            lockModes |= CAMERA_3A_AUTOFOCUS;
        break;
    case FocusExposureBoundMode:
        if ((m_currentLockTypes & QCamera::LockExposure) || (m_currentLockTypes & QCamera::LockFocus))
            lockModes = (CAMERA_3A_AUTOEXPOSURE | CAMERA_3A_AUTOFOCUS);
        break;
    case AllBoundMode:
            lockModes = (CAMERA_3A_AUTOEXPOSURE | CAMERA_3A_AUTOFOCUS | CAMERA_3A_AUTOWHITEBALANCE);
        break;
    case FocusOnlyMode:
            lockModes = CAMERA_3A_AUTOFOCUS;
        break;
    }

    const camera_error_t result = camera_set_3a_lock(m_session->handle(), lockModes);

    if (result != CAMERA_EOK) {
        qWarning() << "Unable to set lock modes:" << result;
    } else {
        if (locks.testFlag(QCamera::LockFocus)) {
            // handled by focusStatusChanged()
        }

        if (locks.testFlag(QCamera::LockExposure)) {
            m_exposureLockStatus = QCamera::Unlocked;
            emit lockStatusChanged(QCamera::LockExposure, QCamera::Unlocked, QCamera::UserRequest);
        }

        if (locks.testFlag(QCamera::LockWhiteBalance)) {
            m_whiteBalanceLockStatus = QCamera::Unlocked;
            emit lockStatusChanged(QCamera::LockWhiteBalance, QCamera::Unlocked, QCamera::UserRequest);
        }
    }
}

void BbCameraLocksControl::cameraOpened()
{
    // retrieve information about lock apply modes
    int supported = 0;
    uint32_t modes[20];

    const camera_error_t result = camera_get_3a_lock_modes(m_session->handle(), 20, &supported, modes);

    if (result == CAMERA_EOK) {
        // see API documentation of camera_get_3a_lock_modes for explanation of case discrimination below
        if (supported == 4) {
            m_locksApplyMode = IndependentMode;
        } else if (supported == 3) {
            m_locksApplyMode = FocusExposureBoundMode;
        } else if (supported == 2) {
            if (modes[0] == (CAMERA_3A_AUTOFOCUS | CAMERA_3A_AUTOEXPOSURE | CAMERA_3A_AUTOWHITEBALANCE))
                m_locksApplyMode = AllBoundMode;
            else
                m_locksApplyMode = FocusOnlyMode;
        }
    }

    // retrieve information about supported lock types
    m_supportedLockTypes = QCamera::NoLock;

    if (camera_has_feature(m_session->handle(), CAMERA_FEATURE_AUTOFOCUS))
        m_supportedLockTypes |= QCamera::LockFocus;

    if (camera_has_feature(m_session->handle(), CAMERA_FEATURE_AUTOEXPOSURE))
        m_supportedLockTypes |= QCamera::LockExposure;

    if (camera_has_feature(m_session->handle(), CAMERA_FEATURE_AUTOWHITEBALANCE))
        m_supportedLockTypes |= QCamera::LockWhiteBalance;

    m_focusLockStatus = QCamera::Unlocked;
    m_exposureLockStatus = QCamera::Unlocked;
    m_whiteBalanceLockStatus = QCamera::Unlocked;
}

void BbCameraLocksControl::focusStatusChanged(int value)
{
    const camera_focusstate_t focusState = static_cast<camera_focusstate_t>(value);

    switch (focusState) {
    case CAMERA_FOCUSSTATE_NONE:
        m_focusLockStatus = QCamera::Unlocked;
        emit lockStatusChanged(QCamera::LockFocus, QCamera::Unlocked, QCamera::UserRequest);
        break;
    case CAMERA_FOCUSSTATE_WAITING:
    case CAMERA_FOCUSSTATE_SEARCHING:
        m_focusLockStatus = QCamera::Searching;
        emit lockStatusChanged(QCamera::LockFocus, QCamera::Searching, QCamera::UserRequest);
        break;
    case CAMERA_FOCUSSTATE_FAILED:
        m_focusLockStatus = QCamera::Unlocked;
        emit lockStatusChanged(QCamera::LockFocus, QCamera::Unlocked, QCamera::LockFailed);
        break;
    case CAMERA_FOCUSSTATE_LOCKED:
        m_focusLockStatus = QCamera::Locked;
        emit lockStatusChanged(QCamera::LockFocus, QCamera::Locked, QCamera::LockAcquired);
        break;
    case CAMERA_FOCUSSTATE_SCENECHANGE:
        m_focusLockStatus = QCamera::Unlocked;
        emit lockStatusChanged(QCamera::LockFocus, QCamera::Unlocked, QCamera::LockTemporaryLost);
        break;
    default:
        break;
    }
}

QT_END_NAMESPACE
