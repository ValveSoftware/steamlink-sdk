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

#include "qnearfieldsharemanager.h"
#include "qnearfieldsharemanager_p.h"

#include "qnearfieldsharemanagerimpl_p.h"

#include "qnearfieldsharetarget.h"

QT_BEGIN_NAMESPACE

/*!
    \class QNearFieldShareManager
    \brief The QNearFieldShareManager class manages all interactions related to sharing files and data over NFC.

    \ingroup connectivity-nfc
    \inmodule QtNfc
    \since 5.3

    Applications can share NDEF data or file content using NFC technology by tapping two NFC-enabled devices
    together. The QNearFieldShareManager provides a high level entry point to access this functionality.

    The class allows both NDEF data and/or files to be shared between two devices by calling the setShareModes()
    method. This method specifies either an NDEF Data and/or a File transfer. The targetDetected() signal is emitted
    each time a share target is detected. A QNearFieldShareTarget pointer is passed with the signal, which can
    be used to share either an NDEF message or one or more files.

    The process of sharing files via NFC involves other underlying communication transports such as Bluetooth or Wi-Fi Direct.
    It is implementation specific how and what type of transports are used to perform file transfer. The overall time taken to
    transfer content depends on the maximum speed of the transport used. Note that the process of sharing NDEF message/data
    does not require the use of other transports outside NFC.

    If an error occurs, shareError() returns the error type.

    Platforms that do not support both NDEF data and file content sharing modes can return the supported subset in the
    supportedShareModes() method. Applications that call setShareModes() with an unsupported mode will receive an error
    signal with a UnsupportedShareModeError.

    Since sharing data over NFC is effectively a data pipe between two processes (one on the sender and one of
    the receiver), the application developer should only create a single instance of QNearFieldShareManager per
    application. This avoids the possibility that different parts of the same application attempt to all consume
    data transferred over NFC.
*/

/*!
    \enum QNearFieldShareManager::ShareError

    This enum specifies the share error type.

    \value NoError                         No error.
    \value UnknownError                    Unknown or internal error occurred.
    \value InvalidShareContentError        Invalid content was provided for sharing.
    \value ShareCanceledError              Data or file sharing is canceled on the local or remote device.
    \value ShareInterruptedError           Data or file sharing is interrupted due to an I/O error.
    \value ShareRejectedError              Data or file sharing is rejected by the remote device.
    \value UnsupportedShareModeError       Data or file sharing is not supported by the share target.
    \value ShareAlreadyInProgressError     Data or file sharing is already in progress.
    \value SharePermissionDeniedError      File sharing is denied due to insufficient permission.
*/

/*!
    \enum QNearFieldShareManager::ShareMode

    This enum specifies the content type to be shared.

    \value NoShare                         No content is currently set to be shared.
    \value NdefShare                       Share NDEF message with target.
    \value FileShare                       Share file with target.
*/

/*!
    \fn void QNearFieldShareManager::targetDetected(QNearFieldShareTarget* shareTarget)

    This signal is emitted whenever a \a shareTarget is detected. The \a shareTarget
    instance is owned by QNearFieldShareManager and must not be deleted by the application.
*/

/*!
    \fn void QNearFieldShareManager::shareModesChanged(ShareModes modes)

    This signal is emitted whenever the share \a modes are changed.
*/

/*!
    \fn void QNearFieldShareManager::error(ShareError error)

    This signal is emitted whenever an \a error occurs related to a share request.
*/

/*!
    Constructs a new near field share manager with \a parent.
*/
QNearFieldShareManager::QNearFieldShareManager(QObject *parent)
:   QObject(parent), d_ptr(new QNearFieldShareManagerPrivateImpl(this))
{
}

/*!
    Destroys the near field share manager.
*/
QNearFieldShareManager::~QNearFieldShareManager()
{
}

/*!
    Initializes the NFC share \a mode to detect a QNearFieldShareTarget for data and/or file sharing.
    Calls to this method will overwrite previous share modes.

    A shareModesChanged() signal will be emitted when share modes are different from previous modes.
    A targetDetected() signal will be emitted if a share target is detected.
*/
void QNearFieldShareManager::setShareModes(ShareModes mode)
{
    Q_D(QNearFieldShareManager);
    return d->setShareModes(mode);
}

/*!
    Returns the shared modes supported by NFC.
*/
QNearFieldShareManager::ShareModes QNearFieldShareManager::supportedShareModes()
{
    return QNearFieldShareManagerPrivateImpl::supportedShareModes();
}

/*!
    Returns which shared modes are set.
*/
QNearFieldShareManager::ShareModes QNearFieldShareManager::shareModes() const
{
    Q_D(const QNearFieldShareManager);
    return d->shareModes();
}

/*!
     Returns the error code of the error that occurred.
 */
QNearFieldShareManager::ShareError QNearFieldShareManager::shareError() const
{
    Q_D(const QNearFieldShareManager);
    return d->shareError();
}

QT_END_NAMESPACE
