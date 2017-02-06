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

#include <qndefnfctextrecord.h>

#include <QtCore/QTextCodec>
#include <QtCore/QLocale>

QT_BEGIN_NAMESPACE

/*!
    \class QNdefNfcTextRecord
    \brief The QNdefNfcTextRecord class provides an NFC RTD-Text

    \ingroup connectivity-nfc
    \inmodule QtNfc
    \since 5.2

    RTD-Text encapsulates a user displayable text record.
*/

/*!
    \enum QNdefNfcTextRecord::Encoding

    This enum describes the text encoding standard used.

    \value Utf8     The text is encoded with UTF-8.
    \value Utf16    The text is encoding with UTF-16.
*/

/*!
    \fn QNdefNfcTextRecord::QNdefNfcTextRecord()

    Constructs an empty NFC text record of type \l QNdefRecord::NfcRtd.
*/

/*!
    \fn QNdefNfcTextRecord::QNdefNfcTextRecord(const QNdefRecord& other)

    Constructs a new NFC text record that is a copy of \a other.
*/

/*!
    Returns the locale of the text record.
*/
QString QNdefNfcTextRecord::locale() const
{
    const QByteArray p = payload();

    if (p.isEmpty())
        return QString();

    quint8 status = p.at(0);

    quint8 codeLength = status & 0x3f;

    return QString::fromLatin1(p.constData() + 1, codeLength);
}

/*!
    Sets the locale of the text record to \a locale.
*/
void QNdefNfcTextRecord::setLocale(const QString &locale)
{
    QByteArray p = payload();

    quint8 status = p.isEmpty() ? 0 : p.at(0);

    quint8 codeLength = status & 0x3f;

    quint8 newStatus = (status & 0xd0) | locale.length();

    p[0] = newStatus;
    p.replace(1, codeLength, locale.toLatin1());

    setPayload(p);
}

/*!
    Returns the contents of the text record as a string.
*/
QString QNdefNfcTextRecord::text() const
{
    const QByteArray p = payload();

    if (p.isEmpty())
        return QString();

    quint8 status = p.at(0);

    bool utf16 = status & 0x80;
    quint8 codeLength = status & 0x3f;

    QTextCodec *codec = QTextCodec::codecForName(utf16 ? "UTF-16BE" : "UTF-8");

    return codec ? codec->toUnicode(p.constData() + 1 + codeLength, p.length() - 1 - codeLength) : QString();
}

/*!
    Sets the contents of the text record to \a text.
*/
void QNdefNfcTextRecord::setText(const QString text)
{
    if (payload().isEmpty())
        setLocale(QLocale().name());

    QByteArray p = payload();

    quint8 status = p.at(0);

    bool utf16 = status & 0x80;
    quint8 codeLength = status & 0x3f;

    p.truncate(1 + codeLength);

    QTextCodec *codec = QTextCodec::codecForName(utf16 ? "UTF-16BE" : "UTF-8");

    p += codec->fromUnicode(text);

    setPayload(p);
}

/*!
    Returns the encoding of the contents.
*/
QNdefNfcTextRecord::Encoding QNdefNfcTextRecord::encoding() const
{
    if (payload().isEmpty())
        return Utf8;

    QByteArray p = payload();

    quint8 status = p.at(0);

    bool utf16 = status & 0x80;

    if (utf16)
        return Utf16;
    else
        return Utf8;
}

/*!
    Sets the enconding of the contents to \a encoding.
*/
void QNdefNfcTextRecord::setEncoding(Encoding encoding)
{
    QByteArray p = payload();

    quint8 status = p.isEmpty() ? 0 : p.at(0);

    QString string = text();

    if (encoding == Utf8)
        status &= ~0x80;
    else
        status |= 0x80;

    p[0] = status;

    setPayload(p);

    setText(string);
}

QT_END_NAMESPACE
