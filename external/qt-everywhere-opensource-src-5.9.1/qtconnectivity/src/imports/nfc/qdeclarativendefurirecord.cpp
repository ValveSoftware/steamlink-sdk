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

#include "qdeclarativendefurirecord_p.h"

#include <QtCore/QUrl>

/*!
    \qmltype NdefUriRecord
    \since 5.2
    \brief Represents an NFC RTD-URI NDEF record.

    \ingroup nfc-qml
    \inqmlmodule QtNfc

    \inherits NdefRecord

    \sa QNdefNfcUriRecord

    The NdefUriRecord type can contain a uniform resource identifier.
*/

/*!
    \qmlproperty string NdefUriRecord::uri

    This property hold the URI stored in this URI record.
*/

Q_DECLARE_NDEFRECORD(QDeclarativeNdefUriRecord, QNdefRecord::NfcRtd, "U")

QDeclarativeNdefUriRecord::QDeclarativeNdefUriRecord(QObject *parent)
:   QQmlNdefRecord(QNdefNfcUriRecord(), parent)
{
}

QDeclarativeNdefUriRecord::QDeclarativeNdefUriRecord(const QNdefRecord &record, QObject *parent)
:   QQmlNdefRecord(QNdefNfcUriRecord(record), parent)
{
}

QDeclarativeNdefUriRecord::~QDeclarativeNdefUriRecord()
{
}

QString QDeclarativeNdefUriRecord::uri() const
{
    QNdefNfcUriRecord uriRecord(record());

    return uriRecord.uri().toString();
}

void QDeclarativeNdefUriRecord::setUri(const QString &uri)
{
    QNdefNfcUriRecord uriRecord(record());

    if (uriRecord.uri() == uri)
        return;

    uriRecord.setUri(uri);
    setRecord(uriRecord);
    emit uriChanged();
}
