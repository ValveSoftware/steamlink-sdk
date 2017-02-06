/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qnearfieldtagtype4_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QNearFieldTagType4
    \brief The QNearFieldTagType4 class provides an interface for communicating with an NFC Tag
           Type 4 tag.

    \ingroup connectivity-nfc
    \inmodule QtNfc
    \internal
*/

/*!
    \fn Type QNearFieldTagType4::type() const
    \reimp
*/

/*!
    Constructs a new tag type 4 near field target with \a parent.
*/
QNearFieldTagType4::QNearFieldTagType4(QObject *parent)
:   QNearFieldTarget(parent)
{
}

/*!
    Destroys the tag type 4 near field target.
*/
QNearFieldTagType4::~QNearFieldTagType4()
{
}

/*!
    Returns the NFC Tag Type 4 specification version number that the tag supports.
*/
quint8 QNearFieldTagType4::version()
{
    return 0;
}

/*!
    Requests that the file specified by \a name be selected. Upon success calls to read() and
    write() will act on the selected file. Returns a request id which can be used to track the
    completion status of the request.

    Once the request completes the response can be retrieved from the requestResponse() function.
    The response of this request will be a boolean value, true for success; otherwise false.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType4::select(const QByteArray &name)
{
    Q_UNUSED(name);

    return RequestId();
}

/*!
    Requests that the file specified by \a fileIdentifier be selected. Upon success calls to read()
    and write() will act on the selected file. Returns a request id which can be used to track the
    completion status of the request.

    Once the request completes the response can be retrieved from the requestResponse() function.
    The response of this request will be a boolean value, true for success; otherwise false.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType4::select(quint16 fileIdentifier)
{
    Q_UNUSED(fileIdentifier);

    return RequestId();
}

/*!
    Requests that \a length bytes be read from the currently selected file starting from
    \a startOffset. If \a length is 0 all data or the maximum read size bytes will be read,
    whichever is smaller. Returns a request id which can be used to track the completion status of
    the request.

    Once the request completes successfully the response can be retrieved from the
    requestResponse() function. The response of this request will be a QByteArray.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType4::read(quint16 length, quint16 startOffset)
{
    Q_UNUSED(length);
    Q_UNUSED(startOffset);

    return RequestId();
}

/*!
    Writes \a data to the currently selected file starting at \a startOffset. Returns a request id
    which can be used to track the completion status of the request.

    Once the request completes the response can be retrieved from the requestResponse() function.
    The response of this request will be a boolean value, true for success; otherwise false.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType4::write(const QByteArray &data, quint16 startOffset)
{
    Q_UNUSED(data);
    Q_UNUSED(startOffset);

    return RequestId();
}

/*!
    \reimp
*/
bool QNearFieldTagType4::handleResponse(const QNearFieldTarget::RequestId &id,
                                        const QByteArray &response)
{
    return QNearFieldTarget::handleResponse(id, response);
}

QT_END_NAMESPACE
