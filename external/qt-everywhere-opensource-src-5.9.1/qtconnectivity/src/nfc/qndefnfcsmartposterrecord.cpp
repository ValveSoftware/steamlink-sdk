/***************************************************************************
 **
 ** Copyright (C) 2016 - 2012 Research In Motion
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

#include <qndefnfcsmartposterrecord.h>
#include "qndefnfcsmartposterrecord_p.h"
#include <qndefmessage.h>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

QT_BEGIN_NAMESPACE

/*!
    \class QNdefNfcSmartPosterRecord
    \brief The QNdefNfcSmartPosterRecord class provides an NFC RTD-SmartPoster.

    \ingroup connectivity-nfc
    \inmodule QtNfc
    \since Qt 5.2

    RTD-SmartPoster encapsulates a Smart Poster.
 */

/*!
    \enum QNdefNfcSmartPosterRecord::Action

    This enum describes the course of action that a device should take with the content.

    \value UnspecifiedAction    The action is not defined.
    \value DoAction             Do the action (send the SMS, launch the browser, make the telephone call).
    \value SaveAction           Save for later (store the SMS in INBOX, put the URI in a bookmark, save the telephone number in contacts).
    \value EditAction           Open for editing (open an SMS in the SMS editor, open the URI in a URI editor, open the telephone number for editing).
 */

/*!
    Constructs a new empty smart poster.
*/
QNdefNfcSmartPosterRecord::QNdefNfcSmartPosterRecord()
    : QNdefRecord(QNdefRecord::NfcRtd, "Sp"),
      d(new QNdefNfcSmartPosterRecordPrivate)
{
}

/*!
    Constructs a new smart poster that is a copy of \a other.
*/
QNdefNfcSmartPosterRecord::QNdefNfcSmartPosterRecord(const QNdefRecord &other)
    : QNdefRecord(other, QNdefRecord::NfcRtd, "Sp"),
      d(new QNdefNfcSmartPosterRecordPrivate)
{
    // Need to set payload again to create internal structure
    setPayload(other.payload());
}

/*!
    Constructs a new smart poster that is a copy of \a other.
*/
QNdefNfcSmartPosterRecord::QNdefNfcSmartPosterRecord(const QNdefNfcSmartPosterRecord &other)
    : QNdefRecord(other, QNdefRecord::NfcRtd, "Sp"), d(other.d)
{
}

/*!
    Assigns the \a other smart poster record to this record and returns a reference to
    this record.
*/
QNdefNfcSmartPosterRecord &QNdefNfcSmartPosterRecord::operator=(const QNdefNfcSmartPosterRecord &other)
{
    if (this != &other)
        d = other.d;

    return *this;
}

/*!
    Destroys the smart poster.
*/
QNdefNfcSmartPosterRecord::~QNdefNfcSmartPosterRecord()
{
}

void QNdefNfcSmartPosterRecord::cleanup()
{
    if (d) {
        // Clean-up existing internal structure
        d->m_titleList.clear();
        if (d->m_uri) delete d->m_uri;
        if (d->m_action) delete d->m_action;
        d->m_iconList.clear();
        if (d->m_size) delete d->m_size;
        if (d->m_type) delete d->m_type;
    }
}

/*!
    \internal
    Sets the payload of the NDEF record to \a payload
*/
void QNdefNfcSmartPosterRecord::setPayload(const QByteArray &payload)
{
    QNdefRecord::setPayload(payload);

    cleanup();

    if (!payload.isEmpty()) {
        // Create new structure
        QNdefMessage message = QNdefMessage::fromByteArray(payload);

        // Iterate through all the records contained in the payload's message.
        for (QList<QNdefRecord>::const_iterator iter = message.begin(); iter != message.end(); iter++) {
            QNdefRecord record = *iter;

            // Title
            if (record.isRecordType<QNdefNfcTextRecord>()) {
                addTitleInternal(record);
            }

            // URI
            else if (record.isRecordType<QNdefNfcUriRecord>()) {
                d->m_uri = new QNdefNfcUriRecord(record);
            }

            // Action
            else if (record.isRecordType<QNdefNfcActRecord>()) {
                d->m_action = new QNdefNfcActRecord(record);
            }

            // Icon
            else if (record.isRecordType<QNdefNfcIconRecord>()) {
                addIconInternal(record);
            }

            // Size
            else if (record.isRecordType<QNdefNfcSizeRecord>()) {
                d->m_size = new QNdefNfcSizeRecord(record);
            }

            // Type
            else if (record.isRecordType<QNdefNfcTypeRecord>()) {
                d->m_type = new QNdefNfcTypeRecord(record);
            }
        }
    }
}

void QNdefNfcSmartPosterRecord::convertToPayload()
{
    QNdefMessage message;

    // Title
    for (int t=0; t<titleCount(); t++)
        message.append(titleRecord(t));

    // URI
    if (d->m_uri)
        message.append(*(d->m_uri));

    // Action
    if (d->m_action)
        message.append(*(d->m_action));

    // Icon
    for (int i=0; i<iconCount(); i++)
        message.append(iconRecord(i));

    // Size
    if (d->m_size)
        message.append(*(d->m_size));

    // Type
    if (d->m_type)
        message.append(*(d->m_type));

    QNdefRecord::setPayload(message.toByteArray());
}

/*!
    Returns true if the smart poster contains a title record using locale \a locale. If \a locale
    is empty then true is returned if the smart poster contains at least one title record. In all
    cases false is returned.
 */
bool QNdefNfcSmartPosterRecord::hasTitle(const QString &locale) const
{
    for (int i = 0; i < d->m_titleList.length(); ++i) {
        const QNdefNfcTextRecord &text = d->m_titleList[i];

        if (locale.isEmpty() || text.locale() == locale)
            return true;
    }

    return false;
}

/*!
    Returns true if the smart poster contains an action record, otherwise false.
 */
bool QNdefNfcSmartPosterRecord::hasAction() const
{
    return d->m_action != 0;
}

/*!
    Returns true if the smart poster contains an icon record using type \a mimetype.
    If \a mimetype is empty then true is returned if the smart poster contains at least one icon record.
    In all other cases false is returned.
 */
bool QNdefNfcSmartPosterRecord::hasIcon(const QByteArray &mimetype) const
{
    for (int i = 0; i < d->m_iconList.length(); ++i) {
        const QNdefNfcIconRecord &icon = d->m_iconList[i];

        if (mimetype.isEmpty() || icon.type() == mimetype)
            return true;
    }

    return false;
}

/*!
    Returns true if the smart poster contains a size record, otherwise false.
 */
bool QNdefNfcSmartPosterRecord::hasSize() const
{
    return d->m_size != 0;
}

/*!
    Returns true if the smart poster contains a type record, otherwise false.
 */
bool QNdefNfcSmartPosterRecord::hasTypeInfo() const
{
    return d->m_type != 0;
}

/*!
    Returns the number of title records contained inside the smart poster.
 */
int QNdefNfcSmartPosterRecord::titleCount() const
{
    return d->m_titleList.length();
}

/*!
    Returns the title record corresponding to the index \a index inside the smart poster, where \a index is a value between 0 and titleCount() - 1. Values outside of this range return an empty record.
 */
QNdefNfcTextRecord QNdefNfcSmartPosterRecord::titleRecord(const int index) const
{
    if (index >= 0 && index < d->m_titleList.length())
        return d->m_titleList[index];

    return QNdefNfcTextRecord();
}

/*!
    Returns the title record text associated with locale \a locale if available. If \a locale
    is empty then the title text of the first available record is returned. In all other
    cases an empty string is returned.
 */
QString QNdefNfcSmartPosterRecord::title(const QString &locale) const
{
    for (int i = 0; i < d->m_titleList.length(); ++i) {
        const QNdefNfcTextRecord &text = d->m_titleList[i];

        if (locale.isEmpty() || text.locale() == locale)
            return text.text();
    }

    return QString();
}

/*!
    Returns a copy of all title records inside the smart poster.
 */
QList<QNdefNfcTextRecord> QNdefNfcSmartPosterRecord::titleRecords() const
{
    return d->m_titleList;
}

/*!
    Attempts to add a title record \a text to the smart poster. If the smart poster does not already
    contain a title record with the same locale as title record \a text, then the title record is added
    and the function returns true. Otherwise false is returned.
 */
bool QNdefNfcSmartPosterRecord::addTitle(const QNdefNfcTextRecord &text)
{
    bool status = addTitleInternal(text);

    // Convert to payload
    convertToPayload();

    return status;
}

bool QNdefNfcSmartPosterRecord::addTitleInternal(const QNdefNfcTextRecord &text)
{
    for (int i = 0; i < d->m_titleList.length(); ++i) {
        const QNdefNfcTextRecord &rec = d->m_titleList[i];

        if (rec.locale() == text.locale())
            return false;
    }

    d->m_titleList.append(text);
    return true;
}

/*!
    Attempts to add a new title record with title \a text, locale \a locale and encoding \a encoding.
    If the smart poster does not already contain a title record with locale \a locale, then the title record
    is added and the function returns true. Otherwise false is returned.
 */
bool QNdefNfcSmartPosterRecord::addTitle(const QString &text, const QString &locale, QNdefNfcTextRecord::Encoding encoding)
{
    QNdefNfcTextRecord rec;
    rec.setText(text);
    rec.setLocale(locale);
    rec.setEncoding(encoding);

    return addTitle(rec);
}

/*!
    Attempts to remove the title record \a text from the smart poster. Removes the record and returns true
    if the smart poster contains a matching record, otherwise false.
 */
bool QNdefNfcSmartPosterRecord::removeTitle(const QNdefNfcTextRecord &text)
{
    bool status = false;

    for (int i = 0; i < d->m_titleList.length(); ++i) {
        const QNdefNfcTextRecord &rec = d->m_titleList[i];

        if (rec.text() == text.text() && rec.locale() == text.locale() && rec.encoding() == text.encoding()) {
            d->m_titleList.removeAt(i);
            status = true;
            break;
        }
    }

    // Convert to payload
    convertToPayload();

    return status;
}

/*!
    Attempts to remove a title record with locale \a locale from the smart poster. Removes the record and returns true
    if the smart poster contains a matching record, otherwise false.
 */
bool QNdefNfcSmartPosterRecord::removeTitle(const QString &locale)
{
    bool status = false;

    for (int i = 0; i < d->m_titleList.length(); ++i) {
        const QNdefNfcTextRecord &rec = d->m_titleList[i];

        if (rec.locale() == locale) {
            d->m_titleList.removeAt(i);
            status = true;
            break;
        }
    }

    // Convert to payload
    convertToPayload();

    return status;
}

/*!
    Adds the title record list \a titles to the smart poster. Any existing records are overwritten.
 */
void QNdefNfcSmartPosterRecord::setTitles(const QList<QNdefNfcTextRecord> &titles)
{
    d->m_titleList.clear();

    for (int i = 0; i < titles.length(); ++i) {
        d->m_titleList.append(titles[i]);
    }

    // Convert to payload
    convertToPayload();
}

/*!
    Returns the URI from the smart poster's URI record if set. Otherwise an empty URI is returned.
 */
QUrl QNdefNfcSmartPosterRecord::uri() const
{
    if (d->m_uri)
        return d->m_uri->uri();

    return QUrl();
}

/*!
    Returns the smart poster's URI record if set. Otherwise an empty URI is returned.
 */
QNdefNfcUriRecord QNdefNfcSmartPosterRecord::uriRecord() const
{
    if (d->m_uri)
        return *(d->m_uri);

    return QNdefNfcUriRecord();
}

/*!
    Sets the URI record to \a url
 */
void QNdefNfcSmartPosterRecord::setUri(const QNdefNfcUriRecord &url)
{
    if (d->m_uri)
        delete d->m_uri;

    d->m_uri = new QNdefNfcUriRecord(url);

    // Convert to payload
    convertToPayload();
}

/*!
    Constructs a URI record and sets its content inside the smart poster to \a url
 */
void QNdefNfcSmartPosterRecord::setUri(const QUrl &url)
{
    QNdefNfcUriRecord rec;
    rec.setUri(url);

    setUri(rec);
}

/*!
    Returns the action from the action record if available. Otherwise \l UnspecifiedAction is returned.
 */
QNdefNfcSmartPosterRecord::Action QNdefNfcSmartPosterRecord::action() const
{
    if (d->m_action)
        return d->m_action->action();

    return UnspecifiedAction;
}

/*!
    Sets the action record to \a act
 */
void QNdefNfcSmartPosterRecord::setAction(Action act)
{
    if (!d->m_action)
        d->m_action = new QNdefNfcActRecord();

    d->m_action->setAction(act);

    // Convert to payload
    convertToPayload();
}

/*!
    Returns the number of icon records contained inside the smart poster.
 */
int QNdefNfcSmartPosterRecord::iconCount() const
{
    return d->m_iconList.length();
}

/*!
    Returns the icon record corresponding to the index \a index inside the smart poster, where \a index is a value between 0 and \l iconCount() - 1. Values outside of this range return an empty record.
 */
QNdefNfcIconRecord QNdefNfcSmartPosterRecord::iconRecord(const int index) const
{
    if (index >= 0 && index < d->m_iconList.length())
        return d->m_iconList[index];

    return QNdefNfcIconRecord();
}

/*!
    Returns the associated icon record data if the smart poster contains an icon record with MIME type \a mimetype.
    If \a mimetype is omitted or empty then the first icon's record data is returned. In all other cases, an empty array is returned.
 */
QByteArray QNdefNfcSmartPosterRecord::icon(const QByteArray& mimetype) const
{
    for (int i = 0; i < d->m_iconList.length(); ++i) {
        const QNdefNfcIconRecord &icon = d->m_iconList[i];

        if (mimetype.isEmpty() || icon.type() == mimetype)
            return icon.data();
    }

    return QByteArray();
}

/*!
    Returns a copy of all icon records inside the smart poster.
 */
QList<QNdefNfcIconRecord> QNdefNfcSmartPosterRecord::iconRecords() const
{
    return d->m_iconList;
}

/*!
    Adds an icon record \a icon to the smart poster. If the smart poster already contains an icon
    record with the same type then the existing icon record is replaced.
 */
void QNdefNfcSmartPosterRecord::addIcon(const QNdefNfcIconRecord &icon)
{
    addIconInternal(icon);

    // Convert to payload
    convertToPayload();
}

void QNdefNfcSmartPosterRecord::addIconInternal(const QNdefNfcIconRecord &icon)
{
    for (int i = 0; i < d->m_iconList.length(); ++i) {
        const QNdefNfcIconRecord &rec = d->m_iconList[i];

        if (rec.type() == icon.type())
            d->m_iconList.removeAt(i);
    }

    d->m_iconList.append(icon);
}

/*!
    Adds an icon record with type \a type and data \a data to the smart poster. If the smart poster
    already contains an icon record with the same type then the existing icon record is replaced.
 */
void QNdefNfcSmartPosterRecord::addIcon(const QByteArray &type, const QByteArray &data)
{
    QNdefNfcIconRecord rec;
    rec.setType(type);
    rec.setData(data);

    return addIcon(rec);
}

/*!
    Attempts to remove the icon record \a icon from the smart poster. Removes the record and returns true
    if the smart poster contains a matching record, otherwise false.
 */
bool QNdefNfcSmartPosterRecord::removeIcon(const QNdefNfcIconRecord &icon)
{
    bool status = false;

    for (int i = 0; i < d->m_iconList.length(); ++i) {
        const QNdefNfcIconRecord &rec = d->m_iconList[i];

        if (rec.type() == icon.type() && rec.data() == icon.data()) {
            d->m_iconList.removeAt(i);
            status = true;
            break;
        }
    }

    // Convert to payload
    convertToPayload();

    return status;
}

/*!
    Attempts to remove the icon record with type \a type from the smart poster. Removes the record
    and returns true if the smart poster contains a matching record, otherwise false.
 */
bool QNdefNfcSmartPosterRecord::removeIcon(const QByteArray &type)
{
    bool status = false;

    for (int i = 0; i < d->m_iconList.length(); ++i) {
        const QNdefNfcIconRecord &rec = d->m_iconList[i];

        if (rec.type() == type) {
            d->m_iconList.removeAt(i);
            status = true;
            break;
        }
    }

    // Convert to payload
    convertToPayload();

    return status;
}

/*!
    Adds the icon record list \a icons to the smart poster.
    Any existing records are overwritten.

    \sa hasIcon(), icon()
 */
void QNdefNfcSmartPosterRecord::setIcons(const QList<QNdefNfcIconRecord> &icons)
{
    d->m_iconList.clear();

    for (int i = 0; i < icons.length(); ++i) {
        d->m_iconList.append(icons[i]);
    }

    // Convert to payload
    convertToPayload();
}

/*!
    Returns the size from the size record if available; otherwise returns 0.

    The value is optional and contains the size in bytes of the object
    that the URI refers to. It may be used by the device to determine
    whether it can accommodate the object.

    \sa setSize()
 */
quint32 QNdefNfcSmartPosterRecord::size() const
{
    if (d->m_size)
        return d->m_size->size();

    return 0;
}

/*!
    Sets the record \a size. The value contains the size in bytes of
    the object that the URI refers to.

    \sa size(), hasSize()
 */
void QNdefNfcSmartPosterRecord::setSize(quint32 size)
{
    if (!d->m_size)
        d->m_size = new QNdefNfcSizeRecord();

    d->m_size->setSize(size);

    // Convert to payload
    convertToPayload();
}

/*!
    Returns the UTF-8 encoded MIME type that describes the type of the objects
    that can be reached via uri().

    If the type is not known the return QByteArray is empty.

    \sa setTypeInfo(), hasTypeInfo()
 */
QByteArray QNdefNfcSmartPosterRecord::typeInfo() const
{
    if (d->m_type)
        return d->m_type->typeInfo();

    return QByteArray();
}

/*!
    Sets the type record to \a type. \a type must be UTF-8 encoded
    and describes the type of the object referenced by uri()

    \sa typeInfo()
 */
void QNdefNfcSmartPosterRecord::setTypeInfo(const QByteArray &type)
{
    if (d->m_type)
        delete d->m_type;

    d->m_type = new QNdefNfcTypeRecord();
    d->m_type->setTypeInfo(type);

    // Convert to payload
    convertToPayload();
}

void QNdefNfcActRecord::setAction(QNdefNfcSmartPosterRecord::Action action)
{
    QByteArray data;
    data[0] = action;

    setPayload(data);
}

QNdefNfcSmartPosterRecord::Action QNdefNfcActRecord::action() const
{
    const QByteArray p = payload();
    QNdefNfcSmartPosterRecord::Action value =
            QNdefNfcSmartPosterRecord::UnspecifiedAction;

    if (!p.isEmpty())
        value = QNdefNfcSmartPosterRecord::Action(p[0]);

    return value;
}

void QNdefNfcIconRecord::setData(const QByteArray &data)
{
    setPayload(data);
}

QByteArray QNdefNfcIconRecord::data() const
{
    return payload();
}

void QNdefNfcSizeRecord::setSize(quint32 size)
{
    QByteArray data;

    data[0] = (int) ((size & 0xFF000000) >> 24);
    data[1] = (int) ((size & 0x00FF0000) >> 16);
    data[2] = (int) ((size & 0x0000FF00) >> 8);
    data[3] = (int) ((size & 0x000000FF));

    setPayload(data);
}

quint32 QNdefNfcSizeRecord::size() const
{
    const QByteArray p = payload();

    if (p.isEmpty())
        return 0;

    return ((p[0] << 24) & 0xFF000000) + ((p[1] << 16) & 0x00FF0000)
            + ((p[2] << 8) & 0x0000FF00) + (p[3] & 0x000000FF);
}

void QNdefNfcTypeRecord::setTypeInfo(const QByteArray &type)
{
    setPayload(type);
}

QByteArray QNdefNfcTypeRecord::typeInfo() const
{
    return payload();
}

QT_END_NAMESPACE
