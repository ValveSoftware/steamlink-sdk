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

#include <qcameralockscontrol.h>
#include  "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QCameraLocksControl



    \brief The QCameraLocksControl class is an abstract base class for
    classes that control still cameras or video cameras.

    \inmodule QtMultimedia


    \ingroup multimedia_control

    This service is provided by a QMediaService object via
    QMediaService::control().  It is used by QCamera.

    The interface name of QCameraLocksControl is \c org.qt-project.qt.cameralockscontrol/5.0 as
    defined in QCameraLocksControl_iid.


    \sa QMediaService::requestControl(), QCamera
*/

/*!
    \macro QCameraLocksControl_iid

    \c org.qt-project.qt.cameralockscontrol/5.0

    Defines the interface name of the QCameraLocksControl class.

    \relates QCameraLocksControl
*/

/*!
    Constructs a camera locks control object with \a parent.
*/

QCameraLocksControl::QCameraLocksControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destruct the camera locks control object.
*/

QCameraLocksControl::~QCameraLocksControl()
{
}

/*!
    \fn QCameraLocksControl::supportedLocks() const

    Returns the lock types, the camera supports.
*/

/*!
    \fn QCameraLocksControl::lockStatus(QCamera::LockType lock) const

    Returns the camera \a lock status.
*/

/*!
    \fn QCameraLocksControl::searchAndLock(QCamera::LockTypes locks)

    Request camera \a locks.
*/

/*!
    \fn QCameraLocksControl::unlock(QCamera::LockTypes locks)

    Unlock camera \a locks.
*/

/*!
    \fn QCameraLocksControl::lockStatusChanged(QCamera::LockType lock, QCamera::LockStatus status, QCamera::LockChangeReason reason)

    Signals the \a lock \a status was changed with a specified \a reason.
*/



#include "moc_qcameralockscontrol.cpp"
QT_END_NAMESPACE
