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

#include "qnearfieldtagtype3_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QNearFieldTagType3
    \brief The QNearFieldTagType3 class provides an interface for communicating with an NFC Tag
           Type 3 tag.

    \ingroup connectivity-nfc
    \inmodule QtNfc
    \internal
*/

/*!
    \fn Type QNearFieldTagType3::type() const
    \reimp
*/

/*!
    Constructs a new tag type 3 near field target with \a parent.
*/
QNearFieldTagType3::QNearFieldTagType3(QObject *parent) :
    QNearFieldTarget(parent)
{
}

/*!
    Returns the system code of the target.
*/
quint16 QNearFieldTagType3::systemCode()
{
    return 0;
}

/*!
    Returns a list of available services.
*/
QList<quint16> QNearFieldTagType3::services()
{
    return QList<quint16>();
}

/*!
    Returns the memory size of the service specified by \a serviceCode.
*/
int QNearFieldTagType3::serviceMemorySize(quint16 serviceCode)
{
    Q_UNUSED(serviceCode);

    return 0;
}

/*!
    Requests the data contents of the service specified by \a serviceCode. Returns a request id
    which can be used to track the completion status of the request.

    Once the request completes successfully the service data can be retrieved from the
    requestResponse() function. The response of this request will be a QByteArray.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType3::serviceData(quint16 serviceCode)
{
    Q_UNUSED(serviceCode);

    return RequestId();
}

/*!
    Writes \a data to the service specified by \a serviceCode. Returns a request id which can
    be used to track the completion status of the request.

    Once the request completes the response can be retrieved from the requestResponse() function.
    The response of this request will be a boolean value, true for success; otherwise false.

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType3::writeServiceData(quint16 serviceCode,
                                                                 const QByteArray &data)
{
    Q_UNUSED(serviceCode);
    Q_UNUSED(data);

    return RequestId();
}

/*!
    Sends the \e check request to the target. Requests the service data blocks specified by
    \a serviceBlockList. Returns a request id which can be used to track the completion status of
    the request.

    The \a serviceBlockList parameter is a map with the key being the service code and the value
    being a list of block indexes to retrieve.

    Once the request completes the response can be retrieved from the requestResponse() function.
    The response of this request will be a QMap<quint16, QByteArray>, with the key being the
    service code and the value being the concatenated blocks retrieved for that service.

    This is a low level function, to retrieve the entire data contents of a service use
    serviceData().

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType3::check(const QMap<quint16, QList<quint16> > &serviceBlockList)
{
    Q_UNUSED(serviceBlockList);

    return RequestId();
}

/*!
    Sends the \e update request to the target. Writes \a data to the services and block indexes
    sepecified by \a serviceBlockList. Returns a request id which can be used to track the
    completion status of the request.

    The \a serviceBlockList parameter is a map with the key being the service code and the value
    being a list of block indexes to write to.

    Once the request completes the response can be retried from the requestResponse() function. The
    response of this request will be a boolean value, true for success; otherwise false.

    This is a low level function, to write the entire data contents of a service use
    writeServiceData().

    \sa requestCompleted(), waitForRequestCompleted()
*/
QNearFieldTarget::RequestId QNearFieldTagType3::update(const QMap<quint16, QList<quint16> > &serviceBlockList,
                                                       const QByteArray &data)
{
    Q_UNUSED(serviceBlockList);
    Q_UNUSED(data);

    return RequestId();
}

/*!
    \reimp
*/
bool QNearFieldTagType3::handleResponse(const QNearFieldTarget::RequestId &id,
                                        const QByteArray &response)
{
    return QNearFieldTarget::handleResponse(id, response);
}

QT_END_NAMESPACE
