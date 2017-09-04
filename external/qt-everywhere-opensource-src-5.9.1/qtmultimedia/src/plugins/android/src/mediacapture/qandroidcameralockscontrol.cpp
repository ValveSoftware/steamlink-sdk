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

#include "qandroidcameralockscontrol.h"

#include "qandroidcamerasession.h"
#include "androidcamera.h"
#include <qtimer.h>

QT_BEGIN_NAMESPACE

QAndroidCameraLocksControl::QAndroidCameraLocksControl(QAndroidCameraSession *session)
    : QCameraLocksControl()
    , m_session(session)
    , m_supportedLocks(QCamera::NoLock)
    , m_focusLockStatus(QCamera::Unlocked)
    , m_exposureLockStatus(QCamera::Unlocked)
    , m_whiteBalanceLockStatus(QCamera::Unlocked)
{
    connect(m_session, SIGNAL(opened()),
            this, SLOT(onCameraOpened()));

    m_recalculateTimer = new QTimer(this);
    m_recalculateTimer->setInterval(1000);
    m_recalculateTimer->setSingleShot(true);
    connect(m_recalculateTimer, SIGNAL(timeout()), this, SLOT(onRecalculateTimeOut()));
}

QCamera::LockTypes QAndroidCameraLocksControl::supportedLocks() const
{
    return m_supportedLocks;
}

QCamera::LockStatus QAndroidCameraLocksControl::lockStatus(QCamera::LockType lock) const
{
    if (!m_supportedLocks.testFlag(lock) || !m_session->camera())
        return QCamera::Unlocked;

    if (lock == QCamera::LockFocus)
        return m_focusLockStatus;

    if (lock == QCamera::LockExposure)
        return m_exposureLockStatus;

    if (lock == QCamera::LockWhiteBalance)
        return m_whiteBalanceLockStatus;

    return QCamera::Unlocked;
}

void QAndroidCameraLocksControl::searchAndLock(QCamera::LockTypes locks)
{
    if (!m_session->camera())
        return;

    // filter out unsupported locks
    locks &= m_supportedLocks;

    if (locks.testFlag(QCamera::LockFocus)) {
        QString focusMode = m_session->camera()->getFocusMode();
        if (focusMode == QLatin1String("auto")
                || focusMode == QLatin1String("macro")
                || focusMode == QLatin1String("continuous-picture")
                || focusMode == QLatin1String("continuous-video")) {

            if (m_focusLockStatus == QCamera::Searching)
                m_session->camera()->cancelAutoFocus();
            else
                setFocusLockStatus(QCamera::Searching, QCamera::UserRequest);

            m_session->camera()->autoFocus();

        } else {
            setFocusLockStatus(QCamera::Locked, QCamera::LockAcquired);
        }
    }

    if (locks.testFlag(QCamera::LockExposure) && m_exposureLockStatus != QCamera::Searching) {
        if (m_session->camera()->getAutoExposureLock()) {
            // if already locked, unlock and give some time to recalculate exposure
            m_session->camera()->setAutoExposureLock(false);
            setExposureLockStatus(QCamera::Searching, QCamera::UserRequest);
        } else {
            m_session->camera()->setAutoExposureLock(true);
            setExposureLockStatus(QCamera::Locked, QCamera::LockAcquired);
        }
    }

    if (locks.testFlag(QCamera::LockWhiteBalance) && m_whiteBalanceLockStatus != QCamera::Searching) {
        if (m_session->camera()->getAutoWhiteBalanceLock()) {
            // if already locked, unlock and give some time to recalculate white balance
            m_session->camera()->setAutoWhiteBalanceLock(false);
            setWhiteBalanceLockStatus(QCamera::Searching, QCamera::UserRequest);
        } else {
            m_session->camera()->setAutoWhiteBalanceLock(true);
            setWhiteBalanceLockStatus(QCamera::Locked, QCamera::LockAcquired);
        }
    }

    if (m_exposureLockStatus == QCamera::Searching || m_whiteBalanceLockStatus == QCamera::Searching)
        m_recalculateTimer->start();
}

void QAndroidCameraLocksControl::unlock(QCamera::LockTypes locks)
{
    if (!m_session->camera())
        return;

    if (m_recalculateTimer->isActive())
        m_recalculateTimer->stop();

    // filter out unsupported locks
    locks &= m_supportedLocks;

    if (locks.testFlag(QCamera::LockFocus)) {
        m_session->camera()->cancelAutoFocus();
        setFocusLockStatus(QCamera::Unlocked, QCamera::UserRequest);
    }

    if (locks.testFlag(QCamera::LockExposure)) {
        m_session->camera()->setAutoExposureLock(false);
        setExposureLockStatus(QCamera::Unlocked, QCamera::UserRequest);
    }

    if (locks.testFlag(QCamera::LockWhiteBalance)) {
        m_session->camera()->setAutoWhiteBalanceLock(false);
        setWhiteBalanceLockStatus(QCamera::Unlocked, QCamera::UserRequest);
    }
}

void QAndroidCameraLocksControl::onCameraOpened()
{
    m_supportedLocks = QCamera::NoLock;
    m_focusLockStatus = QCamera::Unlocked;
    m_exposureLockStatus = QCamera::Unlocked;
    m_whiteBalanceLockStatus = QCamera::Unlocked;

    // check if focus lock is supported
    QStringList focusModes = m_session->camera()->getSupportedFocusModes();
    for (int i = 0; i < focusModes.size(); ++i) {
        const QString &focusMode = focusModes.at(i);
        if (focusMode == QLatin1String("auto")
                || focusMode == QLatin1String("continuous-picture")
                || focusMode == QLatin1String("continuous-video")
                || focusMode == QLatin1String("macro")) {

            m_supportedLocks |= QCamera::LockFocus;
            setFocusLockStatus(QCamera::Unlocked, QCamera::UserRequest);

            connect(m_session->camera(), SIGNAL(autoFocusComplete(bool)),
                    this, SLOT(onCameraAutoFocusComplete(bool)));

            break;
        }
    }

    if (m_session->camera()->isAutoExposureLockSupported()) {
        m_supportedLocks |= QCamera::LockExposure;
        setExposureLockStatus(QCamera::Unlocked, QCamera::UserRequest);
    }

    if (m_session->camera()->isAutoWhiteBalanceLockSupported()) {
        m_supportedLocks |= QCamera::LockWhiteBalance;
        setWhiteBalanceLockStatus(QCamera::Unlocked, QCamera::UserRequest);

        connect(m_session->camera(), SIGNAL(whiteBalanceChanged()),
                this, SLOT(onWhiteBalanceChanged()));
    }
}

void QAndroidCameraLocksControl::onCameraAutoFocusComplete(bool success)
{
    m_focusLockStatus = success ? QCamera::Locked : QCamera::Unlocked;
    QCamera::LockChangeReason reason = success ? QCamera::LockAcquired : QCamera::LockFailed;
    emit lockStatusChanged(QCamera::LockFocus, m_focusLockStatus, reason);
}

void QAndroidCameraLocksControl::onRecalculateTimeOut()
{
    if (m_exposureLockStatus == QCamera::Searching) {
        m_session->camera()->setAutoExposureLock(true);
        setExposureLockStatus(QCamera::Locked, QCamera::LockAcquired);
    }

    if (m_whiteBalanceLockStatus == QCamera::Searching) {
        m_session->camera()->setAutoWhiteBalanceLock(true);
        setWhiteBalanceLockStatus(QCamera::Locked, QCamera::LockAcquired);
    }
}

void QAndroidCameraLocksControl::onWhiteBalanceChanged()
{
    // changing the white balance mode releases the white balance lock
    if (m_whiteBalanceLockStatus != QCamera::Unlocked)
        setWhiteBalanceLockStatus(QCamera::Unlocked, QCamera::LockLost);
}

void QAndroidCameraLocksControl::setFocusLockStatus(QCamera::LockStatus status, QCamera::LockChangeReason reason)
{
    m_focusLockStatus = status;
    emit lockStatusChanged(QCamera::LockFocus, m_focusLockStatus, reason);
}

void QAndroidCameraLocksControl::setWhiteBalanceLockStatus(QCamera::LockStatus status, QCamera::LockChangeReason reason)
{
    m_whiteBalanceLockStatus = status;
    emit lockStatusChanged(QCamera::LockWhiteBalance, m_whiteBalanceLockStatus, reason);
}

void QAndroidCameraLocksControl::setExposureLockStatus(QCamera::LockStatus status, QCamera::LockChangeReason reason)
{
    m_exposureLockStatus = status;
    emit lockStatusChanged(QCamera::LockExposure, m_exposureLockStatus, reason);
}

QT_END_NAMESPACE
