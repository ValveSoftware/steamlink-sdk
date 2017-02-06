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

#include "qdeclarativendefmimerecord_p.h"

/*!
    \qmltype NdefMimeRecord
    \since 5.2
    \brief Represents an NFC MIME record.

    \ingroup connectivity-nfc
    \inqmlmodule QtNfc

    \inherits NdefRecord

    The NdefMimeRecord type can contain data with an associated MIME type.  The data is
    accessible from the uri in the \l {NdefMimeRecord::uri}{uri} property.
*/

/*!
    \qmlproperty string NdefMimeRecord::uri

    This property hold the URI from which the MIME data can be fetched from.  Currently this
    property returns a data url.
*/

Q_DECLARE_NDEFRECORD(QDeclarativeNdefMimeRecord, QNdefRecord::Mime, ".*")

static inline QNdefRecord createMimeRecord()
{
    QNdefRecord mimeRecord;
    mimeRecord.setTypeNameFormat(QNdefRecord::Mime);
    return mimeRecord;
}

static inline QNdefRecord castToMimeRecord(const QNdefRecord &record)
{
    if (record.typeNameFormat() != QNdefRecord::Mime)
        return createMimeRecord();
    return record;
}

QDeclarativeNdefMimeRecord::QDeclarativeNdefMimeRecord(QObject *parent)
:   QQmlNdefRecord(createMimeRecord(), parent)
{
}

QDeclarativeNdefMimeRecord::QDeclarativeNdefMimeRecord(const QNdefRecord &record, QObject *parent)
:   QQmlNdefRecord(castToMimeRecord(record), parent)
{
}

QDeclarativeNdefMimeRecord::~QDeclarativeNdefMimeRecord()
{
}

QString QDeclarativeNdefMimeRecord::uri() const
{
    QByteArray dataUri = "data:" + record().type() + ";base64," + record().payload().toBase64();
    return QString::fromLatin1(dataUri.constData(), dataUri.length());
}
