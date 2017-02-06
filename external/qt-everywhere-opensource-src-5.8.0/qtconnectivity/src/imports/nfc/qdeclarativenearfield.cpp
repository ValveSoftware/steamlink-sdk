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

#include "qdeclarativenearfield_p.h"
#include "qdeclarativendeffilter_p.h"
#include "qdeclarativendeftextrecord_p.h"
#include "qdeclarativendefurirecord_p.h"
#include "qdeclarativendefmimerecord_p.h"

#include <qndefmessage.h>
#include <qndefnfctextrecord.h>
#include <qndefnfcurirecord.h>

#include <QtNfc/QNearFieldTarget>

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

    \note For platforms using neard, filtering is currently not implemented. For more information
    on neard see \l QNearFieldManager.

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

    \note Filtering is not supported when using neard.

    \l QNearFieldManager::registerNdefMessageHandler()
*/

/*!
    \qmlproperty bool NearField::orderMatch

    This property indicates whether the order of records should be taken into account when matching
    messages. This is not supported when using neard.

    The default of orderMatch is false.
*/

/*!
    \qmlproperty bool NearField::polling
    \since 5.5

    This property indicates if the underlying adapter is currently in polling state. If set to \c true
    the adapter will start polling and stop polling if set to \c false.

    \note On platforms using neard, the adapter will stop polling as soon as a tag has been detected.
    For more information see \l QNearFieldManager.
*/

/*!
    \qmlsignal NearField::tagFound()
    \since 5.5

    This signal will be emitted when a tag has been detected.
*/

/*!
    \qmlsignal NearField::tagRemoved()
    \since 5.5

    This signal will be emitted when a tag has been removed.
*/

QDeclarativeNearField::QDeclarativeNearField(QObject *parent)
    : QObject(parent),
      m_orderMatch(false),
      m_componentCompleted(false),
      m_messageUpdating(false),
      m_manager(new QNearFieldManager(this)),
      m_messageHandlerId(-1),
      m_polling(false)
{
    connect(m_manager, SIGNAL(targetDetected(QNearFieldTarget*)),
            this, SLOT(_q_handleTargetDetected(QNearFieldTarget*)));
    connect(m_manager, SIGNAL(targetLost(QNearFieldTarget*)),
            this, SLOT(_q_handleTargetLost(QNearFieldTarget*)));
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

bool QDeclarativeNearField::polling() const
{
    return m_polling;
}

void QDeclarativeNearField::setPolling(bool on)
{
    if (m_polling == on)
        return;

    if (on) {
        if (m_manager->startTargetDetection()) {
            m_polling = true;
            emit pollingChanged();
        }
    } else {
        m_manager->stopTargetDetection();
        m_polling = false;
        emit pollingChanged();
    }
}

void QDeclarativeNearField::registerMessageHandler()
{
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

    // FIXME: if a message handler has been registered we just assume that constant polling is done
    if (m_messageHandlerId >= 0) {
        m_polling = true;
        emit pollingChanged();
    }
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

void QDeclarativeNearField::_q_handleTargetLost(QNearFieldTarget *target)
{
    Q_UNUSED(target);
    // FIXME: only notify that polling stopped when there is no registered message handler
    if (m_messageHandlerId == -1) {
        m_polling = false;
        emit pollingChanged();
    }

    emit tagRemoved();
}

void QDeclarativeNearField::_q_handleTargetDetected(QNearFieldTarget *target)
{
    Q_UNUSED(target);

    if (m_messageHandlerId == -1) {
        connect(target, SIGNAL(ndefMessageRead(QNdefMessage)),
                this, SLOT(_q_handleNdefMessage(QNdefMessage)));
        target->readNdefMessages();
    }

    emit tagFound();
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
