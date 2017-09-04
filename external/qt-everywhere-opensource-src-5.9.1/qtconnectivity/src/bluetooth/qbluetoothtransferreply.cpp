/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "qbluetoothtransferreply.h"
#include "qbluetoothtransferreply_p.h"
#include "qbluetoothaddress.h"

QT_BEGIN_NAMESPACE

/*!
    \class QBluetoothTransferReply
    \inmodule QtBluetooth
    \brief The QBluetoothTransferReply class stores the response for a data
    transfer request.

    \since 5.2

    In additional to a copy of the QBluetoothTransferRequest object used to create the request,
    QBluetoothTransferReply contains the contents of the reply itself.

    After the file transfer has started, QBluetoothTransferReply emits the transferProgress() signal,
    which indicates the progress of the file transfer.
*/

/*!
    \enum QBluetoothTransferReply::TransferError

    This enum describes the type of error that occurred

    \value NoError           No error.
    \value UnknownError      Unknown error, no better enum available.
    \value FileNotFoundError Unable to open the file specified.
    \value HostNotFoundError Unable to connect to the target host.
    \value UserCanceledTransferError User terminated the transfer.
    \value IODeviceNotReadableError File was not open before initiating the sending command.
    \value ResourceBusyError Unable to access the resource..
    \value SessionError      An error occurred during the handling of the session. This enum was
                             introduced by Qt 5.4.
*/



/*!
    \fn QBluetoothTransferReply::abort()

    Aborts this reply.
*/
void QBluetoothTransferReply::abort()
{
}

/*!
    \fn void QBluetoothTransferReply::finished(QBluetoothTransferReply *reply)

    This signal is emitted when the transfer is complete for \a reply.

    To avoid the loss of signal emissions it is recommend to immidiately connect
    to this signal once a \c QBluetoothTransferReply instance has been created.
*/

/*!
    \fn void QBluetoothTransferReply::transferProgress(qint64 bytesTransferred, qint64 bytesTotal)

    This signal is emitted whenever data is transferred. The \a bytesTransferred parameter contains the total
    number of bytes transferred so far out of \a bytesTotal.


    To avoid the loss of signal emissions it is recommend to immidiately connect
    to this signal once a QBluetoothTransferReply instance has been created.
*/

/*!
    \fn void QBluetoothTransferReply::error(QBluetoothTransferReply::TransferError errorType)
    \since 5.4

    This signal is emitted whenever an error has occurred. The \a errorType
    parameter indicates the type of error.

    To avoid the loss of signal emissions it is recommend to immidiately connect
    to this signal once a QBluetoothTransferReply instance has been created.

    \sa error(), errorString()
*/

/*!
    Constructs a new QBluetoothTransferReply with \a parent.
*/
QBluetoothTransferReply::QBluetoothTransferReply(QObject *parent)
    : QObject(parent), d_ptr(new QBluetoothTransferReplyPrivate())
{
}

/*!
    Destroys the QBluetoothTransferReply object.
*/
QBluetoothTransferReply::~QBluetoothTransferReply()
{
    delete d_ptr;
}

/*!
   \fn bool QBluetoothTransferReply::isFinished() const

    Returns true if this reply has finished, otherwise false.
*/

/*!
   \fn bool QBluetoothTransferReply::isRunning() const

    Returns true if this reply is running, otherwise false.
*/

/*!
    Returns the QBluetoothTransferManager that was used to create this QBluetoothTransferReply
    object. Initially, it is also the parent object.
*/
QBluetoothTransferManager *QBluetoothTransferReply::manager() const
{
    Q_D(const QBluetoothTransferReply);
    return d->m_manager;
}

/*!
    Returns the QBluetoothTransferRequest that was used to create this QBluetoothTransferReply
    object.
*/
QBluetoothTransferRequest QBluetoothTransferReply::request() const
{
    Q_D(const QBluetoothTransferReply);
    return d->m_request;
}

/*!
  \fn QBluetoothTransferReply::setManager(QBluetoothTransferManager *manager)

  Set the reply's manager to the \a manager.
*/

void QBluetoothTransferReply::setManager(QBluetoothTransferManager *manager)
{
    Q_D(QBluetoothTransferReply);
    d->m_manager = manager;
}

/*!
  \fn QBluetoothTransferReply::setRequest(const QBluetoothTransferRequest &request)

  Set the reply's request to \a request.
*/
void QBluetoothTransferReply::setRequest(const QBluetoothTransferRequest &request)
{
    Q_D(QBluetoothTransferReply);
    d->m_request = request;
}

/*!
  \fn TransferError QBluetoothTransferReply::error() const

  The error code of the error that occurred.

  \sa errorString()
*/

/*!
  \fn QString QBluetoothTransferReply::errorString() const

  String describing the error. Can be displayed to the user.

  \sa error()
*/

QBluetoothTransferReplyPrivate::QBluetoothTransferReplyPrivate()
    : m_manager(0)
{
}

#include "moc_qbluetoothtransferreply.cpp"

QT_END_NAMESPACE
