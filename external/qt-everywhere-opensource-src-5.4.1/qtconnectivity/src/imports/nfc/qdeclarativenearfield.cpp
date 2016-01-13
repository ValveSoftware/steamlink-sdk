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

#include "qdeclarativenearfield_p.h"
#include "qdeclarativendeffilter_p.h"
#include "qdeclarativendeftextrecord_p.h"
#include "qdeclarativendefurirecord_p.h"
#include "qdeclarativendefmimerecord_p.h"

#include <qndefmessage.h>
#include <qndefnfctextrecord.h>
#include <qndefnfcurirecord.h>

/*!
    \qmltype NearField
    \instantiates QDeclarativeNearField
    \since 5.2
    \brief Provides access to NDEF messages stored on NFC Forum tags.

    \ingroup nfc-qml
    \inqmlmodule QtNfc

    \sa NdefFilter
    \sa NdefRecord

    \sa QNearFieldManager
    \sa QNdefMessage
    \sa QNdefRecord

    The NearField type can be used to read NDEF messages from NFC Forum tags.  Set the \l filter
    and \l orderMatch properties to match the required NDEF messages.  Once an NDEF message is
    successfully read from a tag the \l messageRecords property is updated.

    \snippet doc_src_qtnfc.qml QML register for messages
*/

/*!
    \qmlproperty list<NdefRecord> NearField::messageRecords

    This property contains the list of NDEF records in the last NDEF message read.
*/

/*!
    \qmlproperty list<NdefFilter> NearField::filter

    This property holds the NDEF filter constraints.  The \l messageRecords property will only be
    set to NDEF messages which match the filter. If no filter is set, a message handler for
    all NDEF messages will be registered.

    \l QNearFieldManager::registerNdefMessageHandler()
*/

/*!
    \qmlproperty bool NearField::orderMatch

    This property indicates whether the order of records should be taken into account when matching
    messages.
*/

QDeclarativeNearField::QDeclarativeNearField(QObject *parent)
:   QObject(parent), m_orderMatch(false), m_componentCompleted(false), m_messageUpdating(false),
    m_manager(0), m_messageHandlerId(-1)
{
}

QQmlListProperty<QQmlNdefRecord> QDeclarativeNearField::messageRecords()
{
    return QQmlListProperty<QQmlNdefRecord>(this, 0,
                                                    &QDeclarativeNearField::append_messageRecord,
                                                    &QDeclarativeNearField::count_messageRecords,
                                                    &QDeclarativeNearField::at_messageRecord,
                                                    &QDeclarativeNearField::clear_messageRecords);

}

QQmlListProperty<QDeclarativeNdefFilter> QDeclarativeNearField::filter()
{
    return QQmlListProperty<QDeclarativeNdefFilter>(this, 0,
                                                    &QDeclarativeNearField::append_filter,
                                                    &QDeclarativeNearField::count_filters,
                                                    &QDeclarativeNearField::at_filter,
                                                    &QDeclarativeNearField::clear_filter);
}

bool QDeclarativeNearField::orderMatch() const
{
    return m_orderMatch;
}

void QDeclarativeNearField::setOrderMatch(bool on)
{
    if (m_orderMatch == on)
        return;

    m_orderMatch = on;
    emit orderMatchChanged();
}

void QDeclarativeNearField::componentComplete()
{
    m_componentCompleted = true;

    registerMessageHandler();
}

void QDeclarativeNearField::registerMessageHandler()
{
    if (!m_manager)
        m_manager = new QNearFieldManager(this);

    if (m_messageHandlerId != -1)
        m_manager->unregisterNdefMessageHandler(m_messageHandlerId);

    QNdefFilter ndefFilter;
    ndefFilter.setOrderMatch(m_orderMatch);
    foreach (const QDeclarativeNdefFilter *filter, m_filterList) {
        const QString type = filter->type();
        uint min = filter->minimum() < 0 ? UINT_MAX : filter->minimum();
        uint max = filter->maximum() < 0 ? UINT_MAX : filter->maximum();

        ndefFilter.appendRecord(static_cast<QNdefRecord::TypeNameFormat>(filter->typeNameFormat()), type.toUtf8(), min, max);
    }

    m_messageHandlerId = m_manager->registerNdefMessageHandler(ndefFilter, this, SLOT(_q_handleNdefMessage(QNdefMessage)));
}

void QDeclarativeNearField::_q_handleNdefMessage(const QNdefMessage &message)
{
    m_messageUpdating = true;

    QQmlListReference listRef(this, "messageRecords");

    listRef.clear();

    foreach (const QNdefRecord &record, message)
        listRef.append(qNewDeclarativeNdefRecordForNdefRecord(record));

    m_messageUpdating = false;

    emit messageRecordsChanged();
}

void QDeclarativeNearField::append_messageRecord(QQmlListProperty<QQmlNdefRecord> *list,
                                                 QQmlNdefRecord *record)
{
    QDeclarativeNearField *nearField = qobject_cast<QDeclarativeNearField *>(list->object);
    if (!nearField)
        return;

    record->setParent(nearField);
    nearField->m_message.append(record);
    if (!nearField->m_messageUpdating)
        emit nearField->messageRecordsChanged();
}

int QDeclarativeNearField::count_messageRecords(QQmlListProperty<QQmlNdefRecord> *list)
{
    QDeclarativeNearField *nearField = qobject_cast<QDeclarativeNearField *>(list->object);
    if (!nearField)
        return 0;

    return nearField->m_message.count();
}

QQmlNdefRecord *QDeclarativeNearField::at_messageRecord(QQmlListProperty<QQmlNdefRecord> *list,
                                                                int index)
{
    QDeclarativeNearField *nearField = qobject_cast<QDeclarativeNearField *>(list->object);
    if (!nearField)
        return 0;

    return nearField->m_message.at(index);
}

void QDeclarativeNearField::clear_messageRecords(QQmlListProperty<QQmlNdefRecord> *list)
{
    QDeclarativeNearField *nearField = qobject_cast<QDeclarativeNearField *>(list->object);
    if (nearField) {
        qDeleteAll(nearField->m_message);
        nearField->m_message.clear();
        if (!nearField->m_messageUpdating)
            emit nearField->messageRecordsChanged();
    }
}

void QDeclarativeNearField::append_filter(QQmlListProperty<QDeclarativeNdefFilter> *list,
                                          QDeclarativeNdefFilter *filter)
{
    QDeclarativeNearField *nearField = qobject_cast<QDeclarativeNearField *>(list->object);
    if (!nearField)
        return;

    filter->setParent(nearField);
    nearField->m_filterList.append(filter);
    emit nearField->filterChanged();

    if (nearField->m_componentCompleted)
        nearField->registerMessageHandler();
}

int QDeclarativeNearField::count_filters(QQmlListProperty<QDeclarativeNdefFilter> *list)
{
    QDeclarativeNearField *nearField = qobject_cast<QDeclarativeNearField *>(list->object);
    if (!nearField)
        return 0;

    return nearField->m_filterList.count();
}

QDeclarativeNdefFilter *QDeclarativeNearField::at_filter(QQmlListProperty<QDeclarativeNdefFilter> *list,
                                                         int index)
{
    QDeclarativeNearField *nearField = qobject_cast<QDeclarativeNearField *>(list->object);
    if (!nearField)
        return 0;

    return nearField->m_filterList.at(index);
}

void QDeclarativeNearField::clear_filter(QQmlListProperty<QDeclarativeNdefFilter> *list)
{
    QDeclarativeNearField *nearField = qobject_cast<QDeclarativeNearField *>(list->object);
    if (!nearField)
        return;

    qDeleteAll(nearField->m_filterList);
    nearField->m_filterList.clear();
    emit nearField->filterChanged();
    if (nearField->m_componentCompleted)
        nearField->registerMessageHandler();
}
