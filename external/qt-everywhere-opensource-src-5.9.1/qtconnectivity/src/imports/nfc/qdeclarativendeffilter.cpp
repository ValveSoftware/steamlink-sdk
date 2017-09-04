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

#include "qdeclarativendeffilter_p.h"

/*!
    \qmltype NdefFilter
    \instantiates QDeclarativeNdefFilter
    \since 5.2
    \brief Represents a filtering constraint for NDEF message records.

    \ingroup nfc-qml
    \inqmlmodule QtNfc

    \sa NearField
    \sa QNdefFilter

    The NdefFilter type is used with the NearField type to read NDEF messages from NFC Forum
    tags that match a given structure.

    \code
        NearField {
            filter: [
                NdefFilter {
                    type: "urn:nfc:wkt:U"
                    minimum: 1
                    maximum: 1
                }
            ]
        }
    \endcode
*/

/*!
    \qmlproperty string NdefFilter::type

    This property holds the NDEF record type that the filter matches.  This property must be set to
    the fully qualified record type, i.e. including the NIS and NSS prefixes.  For example set to
    \c {urn:nfc:wkt:U} to match NFC RTD-URI records.
*/

/*!
    \qmlproperty QQmlNdefRecord::TypeNameFormat NdefFilter::typeNameFormat

    This property holds the NDEF record name format type \enum QQmlNdefRecord::TypeNameFormat.
*/

/*!
    \qmlproperty int NdefFilter::minimum

    This property holds the minimum number of records of the given type that must be in the NDEF
    message for it match.

    The default minimum is 1.

    \sa maximum
*/

/*!
    \qmlproperty int NdefFilter::maximum

    This property holds the maximum number of records of the given type that must be in the NDEF
    message for it match.

    The default maximum is UINT_MAX.

    \sa minimum
*/

QDeclarativeNdefFilter::QDeclarativeNdefFilter(QObject *parent)
:   QObject(parent), m_minimum(1), m_maximum(UINT_MAX)
{
}

QString QDeclarativeNdefFilter::type() const
{
    return m_type;
}

void QDeclarativeNdefFilter::setType(const QString &t)
{
    if (m_type == t)
        return;

    m_type = t;
    emit typeChanged();
}

QQmlNdefRecord::TypeNameFormat QDeclarativeNdefFilter::typeNameFormat() const
{
    return m_typeNameFormat;
}

void QDeclarativeNdefFilter::setTypeNameFormat(QQmlNdefRecord::TypeNameFormat format)
{
    if (m_typeNameFormat == format)
        return;

    m_typeNameFormat = format;
    emit typeNameFormatChanged();
}

int QDeclarativeNdefFilter::minimum() const
{
    return m_minimum;
}

void QDeclarativeNdefFilter::setMinimum(int value)
{
    if (m_minimum == value)
        return;

    m_minimum = value;
    emit minimumChanged();
}

int QDeclarativeNdefFilter::maximum() const
{
    return m_maximum;
}

void QDeclarativeNdefFilter::setMaximum(int value)
{
    if (m_maximum == value)
        return;

    m_maximum = value;
    emit maximumChanged();
}
