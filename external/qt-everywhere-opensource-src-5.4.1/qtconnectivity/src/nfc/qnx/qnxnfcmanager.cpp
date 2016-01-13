/***************************************************************************
**
** Copyright (C) 2012 Research In Motion
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

#include "qnxnfcmanager_p.h"
#include <QMetaMethod>
#include <QMetaObject>
#include "../qllcpsocket_qnx_p.h"
#include <QCoreApplication>
#include <QStringList>

QT_BEGIN_NAMESPACE

QNXNFCManager *QNXNFCManager::m_instance = 0;

QNXNFCManager *QNXNFCManager::instance()
{
    if (!m_instance) {
        qQNXNFCDebug() << "creating manager instance";
        m_instance = new QNXNFCManager;
    }

    return m_instance;
}

void QNXNFCManager::registerForNewInstance()
{
    m_instanceCount++;
}

void QNXNFCManager::unregisterForInstance()
{
    if (m_instanceCount>=1) {
        m_instanceCount--;
        if (m_instanceCount==0) {
            delete m_instance;
            m_instance = 0;
        }
    } else {
        qWarning() << Q_FUNC_INFO << "instance count below 0";
    }
}

void QNXNFCManager::unregisterTargetDetection(QObject *obj)
{
    Q_UNUSED(obj)
    //TODO another instance of the nearfieldmanager might still
    //want to detect targets so we have to do ref counting
    nfc_unregister_tag_readerwriter();
}

nfc_target_t *QNXNFCManager::getLastTarget()
{
    return m_lastTarget;
}

bool QNXNFCManager::isAvailable()
{
    return m_available;
}

void QNXNFCManager::registerLLCPConnection(nfc_llcp_connection_listener_t listener, QObject *obj)
{
    llcpConnections.append(QPair<nfc_llcp_connection_listener_t, QObject*> (listener, obj));
}

void QNXNFCManager::unregisterLLCPConnection(nfc_llcp_connection_listener_t listener)
{
    for (int i=0; i<llcpConnections.size(); i++) {
        if (llcpConnections.at(i).first == listener) {
            llcpConnections.removeAt(i);
        }
    }
}

void QNXNFCManager::requestTargetLost(QObject *object, int targetId)
{
    nfcTargets.append(QPair<unsigned int, QObject*> (targetId, object));
}

void QNXNFCManager::unregisterTargetLost(QObject *object)
{
    for (int i=0; i<nfcTargets.size(); i++) {
        if (nfcTargets.at(i).second == object) {
            nfcTargets.removeAt(i);
            break;
        }
    }
}

QNXNFCManager::QNXNFCManager()
    : QObject(), nfcNotifier(0)
{
    nfc_set_verbosity(2);
    qQNXNFCDebug() << "Initializing BB NFC";

    if (nfc_connect() != NFC_RESULT_SUCCESS) {
        qWarning() << Q_FUNC_INFO << "Could not connect to NFC system";
        return;
    }

    bool nfcStatus;
    nfc_get_setting(NFC_SETTING_ENABLED, &nfcStatus);
    qQNXNFCDebug() << "NFC status" << nfcStatus;
    if (!nfcStatus) {
        qWarning() << "NFC not enabled...enabling";
        nfc_set_setting(NFC_SETTING_ENABLED, true);
    }
    m_available = true;

    if (nfc_get_fd(NFC_CHANNEL_TYPE_PUBLIC, &nfcFD) != NFC_RESULT_SUCCESS) {
        qWarning() << Q_FUNC_INFO << "Could not get NFC FD";
        return;
    }

    nfcNotifier = new QSocketNotifier(nfcFD, QSocketNotifier::Read);
    qQNXNFCDebug() << "Connecting SocketNotifier" << connect(nfcNotifier, SIGNAL(activated(int)), this, SLOT(newNfcEvent(int)));

    ndefEventFilter = new QNXNFCEventFilter();
    ndefEventFilter->installOnEventDispatcher(QAbstractEventDispatcher::instance());
    connect(ndefEventFilter, SIGNAL(ndefEvent(QNdefMessage)), this, SLOT(invokeNdefMessage(QNdefMessage)));
}

QNXNFCManager::~QNXNFCManager()
{
    nfc_disconnect();

    if (nfcNotifier)
        delete nfcNotifier;
    ndefEventFilter->uninstallEventFilter();
}

QList<QNdefMessage> QNXNFCManager::decodeTargetMessage(nfc_target_t *target)
{
    unsigned int messageCount;
    QList<QNdefMessage> ndefMessages;

    if (nfc_get_ndef_message_count(target, &messageCount) != NFC_RESULT_SUCCESS)
        qWarning() << Q_FUNC_INFO << "Could not get ndef message count";

    for (unsigned int i=0; i<messageCount; i++) {
        nfc_ndef_message_t *nextMessage;
        if (nfc_get_ndef_message(target, i, &nextMessage) != NFC_RESULT_SUCCESS) {
            qWarning() << Q_FUNC_INFO << "Could not get ndef message";
        } else {
            QNdefMessage newNdefMessage = decodeMessage(nextMessage);
            ndefMessages.append(newNdefMessage);
        }
    }
    return ndefMessages;
}

void QNXNFCManager::newNfcEvent(int fd)
{
    nfc_event_t *nfcEvent;
    nfc_event_type_t nfcEventType;

    if (nfc_read_event(fd, &nfcEvent) != NFC_RESULT_SUCCESS) {
        qWarning() << Q_FUNC_INFO << "Could not read NFC event";
        return;
    }

    if (nfc_get_event_type(nfcEvent, &nfcEventType) != NFC_RESULT_SUCCESS) {
        qWarning() << Q_FUNC_INFO << "Could not get NFC event type";
        return;
    }

    switch (nfcEventType) { //TODO handle all the events
    case NFC_TAG_READWRITE_EVENT: qQNXNFCDebug() << "NFC read write event"; nfcReadWriteEvent(nfcEvent); break;
    case NFC_OFF_EVENT: qQNXNFCDebug() << "NFC is off"; break;
    case NFC_ON_EVENT: qQNXNFCDebug() << "NFC is on"; break;
    case NFC_HANDOVER_COMPLETE_EVENT: qQNXNFCDebug() << "NFC handover event"; break;
    case NFC_HANDOVER_DETECTED_EVENT: qQNXNFCDebug() << "NFC Handover detected"; break;
    case NFC_SNEP_CONNECTION_EVENT: qQNXNFCDebug() << "NFC SNEP detected"; break;
    case NFC_LLCP_READ_COMPLETE_EVENT: llcpReadComplete(nfcEvent); break;
    case NFC_LLCP_WRITE_COMPLETE_EVENT: llcpWriteComplete(nfcEvent); break;
    case NFC_LLCP_CONNECTION_EVENT: llcpConnectionEvent(nfcEvent); break;
    case NFC_TARGET_LOST_EVENT: targetLostEvent(nfcEvent); break;
    default: qQNXNFCDebug() << "Got NFC event" << nfcEventType; break;
    }

    nfc_free_event (nfcEvent);
}

void QNXNFCManager::invokeNdefMessage(const QNdefMessage &msg)
{
    emit ndefMessage(msg, 0);
}

void QNXNFCManager::llcpReadComplete(nfc_event_t *nfcEvent)
{
    nfc_target_t *target;
    if (nfc_get_target(nfcEvent, &target) != NFC_RESULT_SUCCESS) {
        qWarning() << Q_FUNC_INFO << "Could not retrieve LLCP NFC target";
        return;
    }
    nfc_result_t result;
    unsigned int bufferLength = -1;
    result = nfc_llcp_get_local_miu(target, &bufferLength);
    if (result != NFC_RESULT_SUCCESS) {
        qWarning() << Q_FUNC_INFO << "could not get local miu";
    }
    uchar_t buffer[bufferLength];

    size_t bytesRead;

    m_lastTarget = target;
    unsigned int targetId;
    nfc_get_target_connection_id(target, &targetId);

    QByteArray data;
    result = nfc_llcp_get_read_result(getLastTarget(), buffer, bufferLength, &bytesRead);
    if (result == NFC_RESULT_SUCCESS) {
        data = QByteArray(reinterpret_cast<char *> (buffer), bytesRead);
        qQNXNFCDebug() << "Read LLCP data" << bytesRead << data;
    } else if (result == NFC_RESULT_READ_FAILED) { //This most likely means, that the target has been disconnected
        qWarning() << Q_FUNC_INFO << "LLCP read failed";
        nfc_llcp_close(target);
        targetLost(targetId);
        return;
    } else {
        qWarning() << Q_FUNC_INFO << "LLCP read unknown error";
        //return;
    }

    for (int i=0; i<nfcTargets.size(); i++) {
        if (nfcTargets.at(i).first == targetId) {
            qobject_cast<QLlcpSocketPrivate*>(nfcTargets.at(i).second)->dataRead(data);
        }
    }
}

void QNXNFCManager::llcpWriteComplete(nfc_event_t *nfcEvent)
{
    nfc_target_t *target;
    if (nfc_get_target(nfcEvent, &target) != NFC_RESULT_SUCCESS) {
        qWarning() << Q_FUNC_INFO << "Could not retrieve LLCP NFC target";
        return;
    }

    if (nfc_llcp_get_write_status(target) != NFC_RESULT_SUCCESS) {
        qWarning() << Q_FUNC_INFO << "LLCP write failed";
    } else {
        qQNXNFCDebug() << "write completed succesfull";
    }

    unsigned int targetId;
    nfc_get_target_connection_id(target, &targetId);

    for (int i=0; i<nfcTargets.size(); i++) {
        if (nfcTargets.at(i).first == targetId) {
            qobject_cast<QLlcpSocketPrivate*>(nfcTargets.at(i).second)->dataWritten();
        }
    }
}

void QNXNFCManager::nfcReadWriteEvent(nfc_event_t *nfcEvent)
{
    nfc_target_t *target;

    if (nfc_get_target(nfcEvent, &target) != NFC_RESULT_SUCCESS) {
        qWarning() << Q_FUNC_INFO << "Could not retrieve NFC target";
        return;
    }
    tag_variant_type_t variant;
    nfc_get_tag_variant(target, &variant);
    qQNXNFCDebug() << "Variant:" << variant;

    QList<QNdefMessage> targetMessages = decodeTargetMessage(target);
    NearFieldTarget<QNearFieldTarget> *bbNFTarget = new NearFieldTarget<QNearFieldTarget>(this, target, targetMessages);
    emit targetDetected(bbNFTarget, targetMessages);
    for (int i=0; i< targetMessages.count(); i++) {
        emit ndefMessage(targetMessages.at(i), reinterpret_cast<QNearFieldTarget *> (bbNFTarget));
    }
}

void QNXNFCManager::llcpConnectionEvent(nfc_event_t *nfcEvent)
{
    nfc_target_t *target;

    if (nfc_get_target(nfcEvent, &target) != NFC_RESULT_SUCCESS) {
        qWarning() << Q_FUNC_INFO << "Could not retrieve NFC target";
        return;
    }
    nfc_llcp_connection_listener_t conListener;
    nfc_llcp_get_connection_status(target, &conListener);
    unsigned int lmiu;
    nfc_llcp_get_local_miu(target, &lmiu);
    m_lastTarget = target;

    qQNXNFCDebug() << "LLCP connection event; local MIU" << lmiu;
    for (int i=0; i<llcpConnections.size(); i++) {
        if (llcpConnections.at(i).first == conListener) {
            //Do we also have to destroy the conn listener afterwards?
            QMetaObject::invokeMethod(llcpConnections.at(i).second, "connected",
                                      Q_ARG(nfc_target_t *, target));
            break;
        }
    }
}
void QNXNFCManager::setupInvokeTarget() {
    qQNXNFCDebug() << "Setting up invoke target";
    QByteArray uriFilter;
    bool registerAll = false;

    if (!absNdefFilters.isEmpty()) {
        uriFilter = "uris=";
    }
    for (int i=0; i<absNdefFilters.size(); i++) {
        if (absNdefFilters.at(i) == "*") {
            registerAll = true;
            break;
        }
        uriFilter.append(absNdefFilters.at(i));
        if (i==absNdefFilters.size()-1)
            uriFilter += ';';
        else
            uriFilter += ',';
    }
    if (registerAll) {
        uriFilter = "uris=ndef://;";
    }

    const char *filters[1];
    QByteArray filter = "actions=bb.action.OPEN;types=application/vnd.rim.nfc.ndef;" + uriFilter;
    filters[0] = filter.constData();

    //Get the correct target-id
    QString targetId = QCoreApplication::instance()->arguments().first();
    targetId = targetId.left(targetId.lastIndexOf(QLatin1Char('.')));

    if (BPS_SUCCESS != navigator_invoke_set_filters("20", targetId.toLatin1().constData(), filters, 1)) {
        qWarning() << "NFC Error setting share target filter";
    } else {
        qQNXNFCDebug() << "NFC share target filter set" << filters[0] << " Target:" << targetId;
    }
}

void QNXNFCManager::targetLostEvent(nfc_event_t *nfcEvent)
{
    unsigned int targetId;
    nfc_get_notification_value(nfcEvent, &targetId);
    qQNXNFCDebug() << "Target lost with target ID:" << targetId;
    targetLost(targetId);
}

void QNXNFCManager::targetLost(unsigned int targetId)
{
    for (int i=0; i<nfcTargets.size(); i++) {
        if (nfcTargets.at(i).first == targetId) {
            QMetaObject::invokeMethod(nfcTargets.at(i).second, "targetLost");
            nfcTargets.removeAt(i);
            break;
        }
    }
}

bool QNXNFCManager::startTargetDetection()
{
    qQNXNFCDebug() << "Start target detection for all types";
    //TODO handle the target types
    if (nfc_register_tag_readerwriter(TAG_TYPE_ALL) == NFC_RESULT_SUCCESS) {
        return true;
    } else {
        qWarning() << Q_FUNC_INFO << "Could not start Target detection";
        return false;
    }
}

void QNXNFCManager::updateNdefFilters(QList<QByteArray> filters, QObject *obj)
{
    qQNXNFCDebug() << Q_FUNC_INFO << "NDEF Filter update";
    //Updating the filters for an object
    if (!filters.isEmpty()) {
        if (ndefFilters.contains(obj)) {
            ndefFilters[obj] = filters;
            qQNXNFCDebug() << "Updateing filter list for"<< obj;
        } else {
            qQNXNFCDebug() << "Appending new filter for"<< obj;
            ndefFilters[obj] = filters;
        }
    } else {
        ndefFilters.remove(obj);
    }

    //Iterate over all registered object filters and construct a filter list for the application
    QList<QByteArray> newFilters;
    if (ndefFilters.size() > 0) {
        QHash<QObject*, QList<QByteArray> >::const_iterator it=ndefFilters.constBegin();
        do {
            foreach (const QByteArray filter, it.value()) {
                if (!newFilters.contains(filter)) {
                    newFilters.append(filter);
                    qQNXNFCDebug() << "Appending Filter" << filter;
                }
            }
            it++;
        } while (it != ndefFilters.constEnd());
    }

    if (newFilters != absNdefFilters) {
        absNdefFilters = newFilters;
        setupInvokeTarget();
    }
}

QNdefMessage QNXNFCManager::decodeMessage(nfc_ndef_message_t *nextMessage)
{
    QNdefMessage newNdefMessage;
    unsigned int recordCount;
    nfc_get_ndef_record_count(nextMessage, &recordCount);
    for (unsigned int j=0; j<recordCount; j++) {
        nfc_ndef_record_t *newRecord;
        char *recordType;
        uchar_t *payLoad;
        char *recordId;
        size_t payLoadSize;
        tnf_type_t typeNameFormat;

        nfc_get_ndef_record(nextMessage, j, &newRecord);

        nfc_get_ndef_record_type(newRecord, &recordType);
        QNdefRecord newNdefRecord;
        newNdefRecord.setType(QByteArray(recordType));

        nfc_get_ndef_record_payload(newRecord, &payLoad, &payLoadSize);
        newNdefRecord.setPayload(QByteArray(reinterpret_cast<const char*>(payLoad), payLoadSize));

        nfc_get_ndef_record_id(newRecord, &recordId);
        newNdefRecord.setId(QByteArray(recordId));

        nfc_get_ndef_record_tnf(newRecord, &typeNameFormat);
        QNdefRecord::TypeNameFormat recordTnf = QNdefRecord::Unknown;
        switch (typeNameFormat) {
        case NDEF_TNF_WELL_KNOWN: recordTnf = QNdefRecord::NfcRtd; break;
        case NDEF_TNF_EMPTY: recordTnf = QNdefRecord::Empty; break;
        case NDEF_TNF_MEDIA: recordTnf = QNdefRecord::Mime; break;
        case NDEF_TNF_ABSOLUTE_URI: recordTnf = QNdefRecord::Uri; break;
        case NDEF_TNF_EXTERNAL: recordTnf = QNdefRecord::ExternalRtd; break;
        case NDEF_TNF_UNKNOWN: recordTnf = QNdefRecord::Unknown; break;
            //TODO add the rest
        case NDEF_TNF_UNCHANGED: recordTnf = QNdefRecord::Unknown; break;
        }

        newNdefRecord.setTypeNameFormat(recordTnf);
        qQNXNFCDebug() << "Adding NFC record";
        newNdefMessage << newNdefRecord;
        delete recordType;
        delete payLoad;
        delete recordId;
    }
    return newNdefMessage;
}

QT_END_NAMESPACE
