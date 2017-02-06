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

#include "qdeclarativendeftextrecord_p.h"

#include <QtCore/QLocale>

/*!
    \qmltype NdefTextRecord
    \since 5.2
    \brief Represents an NFC RTD-Text NDEF record.

    \ingroup nfc-qml
    \inqmlmodule QtNfc

    \inherits NdefRecord

    \sa QNdefNfcTextRecord

    The NdefTextRecord type contains a localized piece of text that can be display to the user.
    An NDEF message may contain many text records for different locales, it is up to the
    application to select the most appropriate one to display to the user.  The localeMatch
    property can be used to determine if the text record has been matched.
*/

/*!
    \qmlproperty string NdefTextRecord::text

    This property holds the text which should be displayed when the current locale matches
    \l locale.
*/

/*!
    \qmlproperty string NdefTextRecord::locale

    This property holds the locale that this text record is for.
*/

/*!
    \qmlproperty enumeration NdefTextRecord::localeMatch

    This property holds an enum describing how closely the locale of the text record matches the
    applications current locale.  The application should display only the text record that most
    closely matches the applications current locale.

    \table
        \header
            \li Value
            \li Description
        \row
            \li LocaleMatchedNone
            \li The text record does not match at all.
        \row
            \li LocaleMatchedEnglish
            \li The language of the text record is English and the language of application's current
               locale is \b {not} English.  The English language text should be displayed if
               there is not a more appropriate match.
        \row
            \li LocaleMatchedLanguage
            \li The language of the text record and the language of the applications's current
               locale are the same.
        \row
            \li LocaleMatchedLanguageAndCountry
            \li The language and country of the text record matches that of the applicatin's current
               locale.
    \endtable
*/

Q_DECLARE_NDEFRECORD(QDeclarativeNdefTextRecord, QNdefRecord::NfcRtd, "T")

QDeclarativeNdefTextRecord::QDeclarativeNdefTextRecord(QObject *parent)
:   QQmlNdefRecord(QNdefNfcTextRecord(), parent)
{
}

QDeclarativeNdefTextRecord::QDeclarativeNdefTextRecord(const QNdefRecord &record, QObject *parent)
:   QQmlNdefRecord(QNdefNfcTextRecord(record), parent)
{
}

QDeclarativeNdefTextRecord::~QDeclarativeNdefTextRecord()
{
}

QString QDeclarativeNdefTextRecord::text() const
{
    QNdefNfcTextRecord textRecord(record());

    return textRecord.text();
}

void QDeclarativeNdefTextRecord::setText(const QString &text)
{
    QNdefNfcTextRecord textRecord(record());

    if (textRecord.text() == text)
        return;

    textRecord.setText(text);
    setRecord(textRecord);
    emit textChanged();
}

QString QDeclarativeNdefTextRecord::locale() const
{
    if (!record().isRecordType<QNdefNfcTextRecord>())
        return QString();

    QNdefNfcTextRecord textRecord(record());

    return textRecord.locale();
}

void QDeclarativeNdefTextRecord::setLocale(const QString &locale)
{
    QNdefNfcTextRecord textRecord(record());

    if (textRecord.locale() == locale)
        return;

    LocaleMatch previous = localeMatch();

    textRecord.setLocale(locale);
    setRecord(textRecord);
    emit localeChanged();

    if (previous != localeMatch())
        emit localeMatchChanged();
}

QDeclarativeNdefTextRecord::LocaleMatch QDeclarativeNdefTextRecord::localeMatch() const
{
    const QLocale recordLocale(locale());
    const QLocale defaultLocale;

    if (recordLocale == defaultLocale)
        return LocaleMatchedLanguageAndCountry;
    else if (recordLocale.language() == defaultLocale.language())
        return LocaleMatchedLanguage;
    else if (recordLocale.language() == QLocale::English)
        return LocaleMatchedEnglish;
    else
        return LocaleMatchedNone;
}
