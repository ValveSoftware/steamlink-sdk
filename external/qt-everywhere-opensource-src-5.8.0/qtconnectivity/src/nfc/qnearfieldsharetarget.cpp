/***************************************************************************
 **
 ** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qnearfieldsharetarget.h"
#include "qnearfieldsharetarget_p.h"

#include "qnearfieldsharetargetimpl_p.h"

#include "qnearfieldsharemanager.h"

QT_BEGIN_NAMESPACE

/*!
    \class QNearFieldShareTarget
    \brief The QNearFieldShareTarget class transfers data to remote device over NFC.

    \ingroup connectivity-nfc
    \inmodule QtNfc
    \since 5.3

    The QNearFieldShareTarget class can be used for sharing NDEF message or files to a remote
    NFC enabled device supporting the same protocol.

    \sa QNearFieldShareManager
*/

/*!
    \fn void QNearFieldShareTarget::error(QNearFieldShareManager::ShareError error)

    This signal is emitted whenever an \a error occurs during transfer.
*/

/*!
    \fn void QNearFieldShareTarget::shareFinished()

    This signal is emitted whenever a data or file transfer has completed successfully.
*/

/*!
    Constructs a new near field share target with \a parent.
*/
QNearFieldShareTarget::QNearFieldShareTarget(QNearFieldShareManager::ShareModes modes, QObject *parent)
:   QObject(parent), d_ptr(new QNearFieldShareTargetPrivateImpl(modes, this))
{
}

/*!
    Destroys the near field share target.
*/
QNearFieldShareTarget::~QNearFieldShareTarget()
{
}

/*!
    Returns the share mode supported by the share target.
*/
QNearFieldShareManager::ShareModes QNearFieldShareTarget::shareModes() const
{
    Q_D(const QNearFieldShareTarget);
    return d->shareModes();
}

/*!
    Share the NDEF \a message via the share target. This method starts sharing asynchronously and returns immediately.
    The method returns true if the request is accepted, otherwise returns false. Sharing is completed when the shareFinished()
    signal is emitted.
*/
bool QNearFieldShareTarget::share(const QNdefMessage &message)
{
    Q_D(QNearFieldShareTarget);
    return d->share(message);
}

/*!
    Share the \a files via the share target. This method starts sharing asynchronously and returns immediately.
    The method returns true if the request is accepted, otherwise returns false. Sharing is completed when the shareFinished()
    signal is emitted.
*/
bool QNearFieldShareTarget::share(const QList<QFileInfo> &files)
{
    Q_D(QNearFieldShareTarget);
    return d->share(files);
}

/*!
    Cancel the data or file sharing in progress.
*/
void QNearFieldShareTarget::cancel()
{
    Q_D(QNearFieldShareTarget);
    d->cancel();
}

/*!
    Returns true if data or file sharing is in progress, otherwise returns false.
*/
bool QNearFieldShareTarget::isShareInProgress() const
{
    Q_D(const QNearFieldShareTarget);
    return d->isShareInProgress();
}

/*!
     Returns the error code of the error that occurred.
 */
QNearFieldShareManager::ShareError QNearFieldShareTarget::shareError() const
{
    Q_D(const QNearFieldShareTarget);
    return d->shareError();
}

QT_END_NAMESPACE
