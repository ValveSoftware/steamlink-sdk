/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qndefnfcurirecord.h"

#include <QtCore/QString>
#include <QtCore/QUrl>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

/*!
    \class QNdefNfcUriRecord
    \brief The QNdefNfcUriRecord class provides an NFC RTD-URI

    \ingroup connectivity-nfc
    \inmodule QtNfc
    \since 5.2

    RTD-URI encapsulates a URI.
*/

/*!
    \fn QNdefNfcUriRecord::QNdefNfcUriRecord()

    Constructs an empty NFC uri record.
*/

/*!
    \fn QNdefNfcUriRecord::QNdefNfcUriRecord(const QNdefRecord& other)

    Constructs a new NFC uri record that is a copy of \a other.
*/
static const char * const abbreviations[] = {
    0,
    "http://www.",
    "https://www.",
    "http://",
    "https://",
    "tel:",
    "mailto:",
    "ftp://anonymous:anonymous@",
    "ftp://ftp.",
    "ftps://",
    "sftp://",
    "smb://",
    "nfs://",
    "ftp://",
    "dav://",
    "news:",
    "telnet://",
    "imap:",
    "rtsp://",
    "urn:",
    "pop:",
    "sip:",
    "sips:",
    "tftp:",
    "btspp://",
    "btl2cap://",
    "btgoep://",
    "tcpobex://",
    "irdaobex://",
    "file://",
    "urn:epc:id:",
    "urn:epc:tag:",
    "urn:epc:pat:",
    "urn:epc:raw:",
    "urn:epc:",
    "urn:nfc:",
};

/*!
    Returns the URI of this URI record.
*/
QUrl QNdefNfcUriRecord::uri() const
{
    QByteArray p = payload();

    if (p.isEmpty())
        return QUrl();

    quint8 code = p.at(0);
    if (code >= sizeof(abbreviations) / sizeof(*abbreviations))
        code = 0;
    p.remove(0, 1);
    if (const char *abbreviation = abbreviations[code])
        p.insert(0, abbreviation);
    return QUrl(QString::fromUtf8(p));
}

/*!
    Sets the URI of this URI record to \a uri.
*/
void QNdefNfcUriRecord::setUri(const QUrl &uri)
{
    int abbrevs = sizeof(abbreviations) / sizeof(*abbreviations);

    for (int i = 1; i < abbrevs; ++i) {
        if (uri.toString().startsWith(QLatin1String(abbreviations[i]))) {
            QByteArray p;

            p[0] = i;
            p += uri.toString().mid(qstrlen(abbreviations[i])).toUtf8();

            setPayload(p);

            return;
        }
    }

    QByteArray p;
    p[0] = 0;
    p += uri.toString().toUtf8();

    setPayload(p);
}

QT_END_NAMESPACE
