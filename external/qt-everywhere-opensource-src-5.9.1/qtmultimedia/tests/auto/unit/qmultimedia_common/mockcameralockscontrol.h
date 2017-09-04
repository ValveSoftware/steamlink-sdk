/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MOCKCAMERALOCKCONTROL_H
#define MOCKCAMERALOCKCONTROL_H

#include <QTimer>
#include "qcameralockscontrol.h"

class MockCameraLocksControl : public QCameraLocksControl
{
    Q_OBJECT
public:
    MockCameraLocksControl(QObject *parent = 0):
            QCameraLocksControl(parent),
            m_focusLock(QCamera::Unlocked),
            m_exposureLock(QCamera::Unlocked)
    {
    }

    ~MockCameraLocksControl() {}

    QCamera::LockTypes supportedLocks() const
    {
        return QCamera::LockExposure | QCamera::LockFocus;
    }

    QCamera::LockStatus lockStatus(QCamera::LockType lock) const
    {
        switch (lock) {
        case QCamera::LockExposure:
            return m_exposureLock;
        case QCamera::LockFocus:
            return m_focusLock;
        default:
            return QCamera::Unlocked;
        }
    }

    void searchAndLock(QCamera::LockTypes locks)
    {
        if (locks & QCamera::LockExposure) {
            QCamera::LockStatus newStatus = locks & QCamera::LockFocus ? QCamera::Searching : QCamera::Locked;

            if (newStatus != m_exposureLock)
                emit lockStatusChanged(QCamera::LockExposure,
                                       m_exposureLock = newStatus,
                                       QCamera::UserRequest);
        }

        if (locks & QCamera::LockFocus) {
            emit lockStatusChanged(QCamera::LockFocus,
                                   m_focusLock = QCamera::Searching,
                                   QCamera::UserRequest);

            QTimer::singleShot(5, this, SLOT(focused()));
        }
    }

    void unlock(QCamera::LockTypes locks) {
        if (locks & QCamera::LockFocus && m_focusLock != QCamera::Unlocked) {
            emit lockStatusChanged(QCamera::LockFocus,
                                   m_focusLock = QCamera::Unlocked,
                                   QCamera::UserRequest);
        }

        if (locks & QCamera::LockExposure && m_exposureLock != QCamera::Unlocked) {
            emit lockStatusChanged(QCamera::LockExposure,
                                   m_exposureLock = QCamera::Unlocked,
                                   QCamera::UserRequest);
        }
    }

    /* helper method to emit the signal with LockChangeReason */
    void setLockChangeReason (QCamera::LockChangeReason lockChangeReason)
    {
        emit lockStatusChanged(QCamera::NoLock,
                               QCamera::Unlocked,
                               lockChangeReason);

    }

private slots:
    void focused()
    {
        if (m_focusLock == QCamera::Searching) {
            emit lockStatusChanged(QCamera::LockFocus,
                                   m_focusLock = QCamera::Locked,
                                   QCamera::UserRequest);
        }

        if (m_exposureLock == QCamera::Searching) {
            emit lockStatusChanged(QCamera::LockExposure,
                                   m_exposureLock = QCamera::Locked,
                                   QCamera::UserRequest);
        }
    }


private:
    QCamera::LockStatus m_focusLock;
    QCamera::LockStatus m_exposureLock;
};


#endif // MOCKCAMERALOCKCONTROL_H
