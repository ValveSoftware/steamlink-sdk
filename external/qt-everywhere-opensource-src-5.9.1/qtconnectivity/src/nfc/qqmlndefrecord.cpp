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

#include "qqmlndefrecord.h"

#include <QtCore/QMap>
#include <QtCore/QRegExp>

#include <QtCore/qglobalstatic.h>

QT_BEGIN_NAMESPACE

/*!
    \class QQmlNdefRecord
    \brief The QQmlNdefRecord class implements the NdefRecord type in QML.

    \ingroup connectivity-nfc
    \inmodule QtNfc
    \since 5.2

    \sa NdefRecord

    The QQmlNdefRecord class is the base class for all NdefRecord types in QML.  To
    support a new NDEF record type in QML subclass this class and expose new properties, member
    functions and signals appropriate for the new record type.  The following must be done to
    create a new NDEF record type in QML:

    \list
        \li The subclass must have a Q_OBJECT macro in its declaration.
        \li The subclass must have an \l {Q_INVOKABLE}{invokable} constructor that takes a
           QNdefRecord and a QObject pointer.
        \li The subclass must be declared as an NDEF record by expanding the Q_DECLARE_NDEFRECORD()
           macro in the implementation file of the subclass.
        \li The subclass must be registered with QML.
    \endlist

    For example the declaration of such a class may look like the following.

    \snippet foorecord.h Foo declaration

    Within the implementation file the Q_DECLARE_NDEFRECORD() macro is expanded:

    \snippet foorecord.cpp Declare foo record

    Finially the application or plugin code calls qmlRegisterType():

    \code
        qmlRegisterType<QQmlNdefFooRecord>(uri, 1, 0, "NdefFooRecord");
    \endcode
*/

/*!
    \qmltype NdefRecord
    \instantiates QQmlNdefRecord
    \brief The NdefRecord type represents a record in an NDEF message.

    \ingroup nfc-qml
    \inqmlmodule QtNfc

    \sa NdefFilter
    \sa NearField

    \sa QNdefRecord

    The NdefRecord type is the base type for all NDEF record types in QML.  It contains
    a single property holding the type of record.

    This class is not intended to be used directly, but extended from C++.

    \sa QQmlNdefRecord
*/

/*!
    \qmlproperty string NdefRecord::type

    This property holds the type of the NDEF record.
*/

/*!
    \qmlproperty enumeration NdefRecord::typeNameFormat

    This property holds the TNF of the NDEF record.

    \table
    \header \li Property \li Description
    \row \li \c NdefRecord.Empty
         \li An empty NDEF record, the record does not contain a payload.
    \row \li \c NdefRecord.NfcRtd
         \li The NDEF record type is defined by an NFC RTD Specification.
    \row \li \c NdefRecord.Mime
         \li The NDEF record type follows the construct described in RFC 2046.
    \row \li \c NdefRecord.Uri
         \li The NDEF record type follows the construct described in RFC 3986.
    \row \li \c NdefRecord.ExternalRtd
         \li The NDEF record type follows the construct for external type names
             described the NFC RTD Specification.
    \endtable

    \sa QNdefRecord::typeNameFormat()
*/

/*!
    \qmlproperty string NdefRecord::record

    This property holds the NDEF record.
*/

/*!
    \fn void QQmlNdefRecord::typeChanged()

    This signal is emitted when the record type changes.
*/

/*!
    \property QQmlNdefRecord::record

    This property hold the NDEF record that this class represents.
*/

/*!
    \property QQmlNdefRecord::type

    This property hold the type of the NDEF record.
*/

/*!
    \property QQmlNdefRecord::typeNameFormat

    This property hold the TNF of the NDEF record.
*/

/*!
    \macro Q_DECLARE_NDEFRECORD(className, typeNameFormat, type)
    \relates QQmlNdefRecord

    This macro ensures that \a className is declared as the class implementing the NDEF record
    identified by \a typeNameFormat and \a type.

    This macro should be expanded in the implementation file for \a className.
*/

typedef QMap<QString, const QMetaObject *> NDefRecordTypesMap;
Q_GLOBAL_STATIC(NDefRecordTypesMap, registeredNdefRecordTypes)

class QQmlNdefRecordPrivate
{
public:
    QNdefRecord record;
};

static QString urnForRecordType(QNdefRecord::TypeNameFormat typeNameFormat, const QByteArray &type)
{
    switch (typeNameFormat) {
    case QNdefRecord::NfcRtd:
        return QStringLiteral("urn:nfc:wkt:") + QString::fromLatin1(type);
    case QNdefRecord::ExternalRtd:
        return QStringLiteral("urn:nfc:ext:") + QString::fromLatin1(type);
    case QNdefRecord::Mime:
        return QStringLiteral("urn:nfc:mime:") + QString::fromLatin1(type);
    default:
        return QString();
    }
}

/*!
    \internal
*/
void qRegisterNdefRecordTypeHelper(const QMetaObject *metaObject,
                                   QNdefRecord::TypeNameFormat typeNameFormat,
                                   const QByteArray &type)
{
    registeredNdefRecordTypes()->insert(urnForRecordType(typeNameFormat, type), metaObject);
}

/*!
    \internal
*/
QQmlNdefRecord *qNewDeclarativeNdefRecordForNdefRecord(const QNdefRecord &record)
{
    const QString urn = urnForRecordType(record.typeNameFormat(), record.type());

    QMapIterator<QString, const QMetaObject *> i(*registeredNdefRecordTypes());
    while (i.hasNext()) {
        i.next();

        QRegExp ex(i.key());
        if (!ex.exactMatch(urn))
            continue;

        const QMetaObject *metaObject = i.value();
        if (!metaObject)
            continue;

        return static_cast<QQmlNdefRecord *>(metaObject->newInstance(
            Q_ARG(QNdefRecord, record), Q_ARG(QObject *, 0)));
    }

    return new QQmlNdefRecord(record);
}

/*!
    Constructs a new empty QQmlNdefRecord with \a parent.
*/
QQmlNdefRecord::QQmlNdefRecord(QObject *parent)
:   QObject(parent), d_ptr(new QQmlNdefRecordPrivate)
{
}

/*!
   Constructs a new QQmlNdefRecord representing \a record.  The parent of the newly
   constructed object will be set to \a parent.
*/
QQmlNdefRecord::QQmlNdefRecord(const QNdefRecord &record, QObject *parent)
:   QObject(parent), d_ptr(new QQmlNdefRecordPrivate)
{
    d_ptr->record = record;
}

/*!
    Destroys the QQmlNdefRecord instance.
*/
QQmlNdefRecord::~QQmlNdefRecord()
{
    delete d_ptr;
}

/*!
    \enum QQmlNdefRecord::TypeNameFormat

    This enum describes the type name format of an NDEF record. The values of this enum are according to
    \l QNdefRecord::TypeNameFormat

    \value Empty        An empty NDEF record, the record does not contain a payload.
    \value NfcRtd       The NDEF record type is defined by an NFC RTD Specification.
    \value Mime         The NDEF record type follows the construct described in RFC 2046.
    \value Uri          The NDEF record type follows the construct described in RFC 3986.
    \value ExternalRtd  The NDEF record type follows the construct for external type names
                        described the NFC RTD Specification.
    \value Unknown      The NDEF record type is unknown.
*/

/*!
    Returns the type of the record.

    \sa QNdefRecord::setType(), QNdefRecord::type()
*/
QString QQmlNdefRecord::type() const
{
    Q_D(const QQmlNdefRecord);

    return QLatin1String(d->record.type());
}

/*!
    Sets the record type to \a newtype if it is not currently equal to \l type(); otherwise does
    nothing.  If the record type is set the typeChanged() signal will be emitted.

    \sa QNdefRecord::setType(), QNdefRecord::type()
*/
void QQmlNdefRecord::setType(const QString &newtype)
{
    if (newtype == type())
        return;

    Q_D(QQmlNdefRecord);
    d->record.setType(newtype.toUtf8());

    emit typeChanged();
}

/*!
    Sets the type name format of the NDEF record to \a newTypeNameFormat.
*/
void QQmlNdefRecord::setTypeNameFormat(QQmlNdefRecord::TypeNameFormat newTypeNameFormat)
{
    if (newTypeNameFormat == typeNameFormat())
        return;

    Q_D(QQmlNdefRecord);
    d->record.setTypeNameFormat(static_cast<QNdefRecord::TypeNameFormat>(newTypeNameFormat));

    emit typeNameFormatChanged();
}

/*!
    \fn QQmlNdefRecord::TypeNameFormat QQmlNdefRecord::typeNameFormat() const

    Returns the type name format of the NDEF record.
*/
QQmlNdefRecord::TypeNameFormat QQmlNdefRecord::typeNameFormat() const
{
    Q_D(const QQmlNdefRecord);
    return static_cast<QQmlNdefRecord::TypeNameFormat>(d->record.typeNameFormat());
}

/*!
    Returns a copy of the record.
*/
QNdefRecord QQmlNdefRecord::record() const
{
    Q_D(const QQmlNdefRecord);

    return d->record;
}

/*!
    Sets the record to \a record. If the record is set the recordChanged() signal will
    be emitted.
*/
void QQmlNdefRecord::setRecord(const QNdefRecord &record)
{
    Q_D(QQmlNdefRecord);

    if (d->record == record)
        return;

    d->record = record;
    emit recordChanged();
}

QT_END_NAMESPACE
