/****************************************************************************
**
** Copyright (C) 2016 Centria research and development
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

#include "qnearfieldmanager_android_p.h"
#include "qnearfieldtarget_android_p.h"

#include "qndeffilter.h"
#include "qndefmessage.h"
#include "qndefrecord.h"
#include "qbytearray.h"
#include "qcoreapplication.h"
#include "qdebug.h"
#include "qlist.h"

#include <QtCore/QMetaType>
#include <QtCore/QMetaMethod>

QT_BEGIN_NAMESPACE

QNearFieldManagerPrivateImpl::QNearFieldManagerPrivateImpl() :
    m_detecting(false), m_handlerID(0)
{
    qRegisterMetaType<QAndroidJniObject>("QAndroidJniObject");
    qRegisterMetaType<QNdefMessage>("QNdefMessage");
    connect(this, SIGNAL(targetDetected(QNearFieldTarget*)), this, SLOT(handlerTargetDetected(QNearFieldTarget*)));
    connect(this, SIGNAL(targetLost(QNearFieldTarget*)), this, SLOT(handlerTargetLost(QNearFieldTarget*)));
}

QNearFieldManagerPrivateImpl::~QNearFieldManagerPrivateImpl()
{
}

void QNearFieldManagerPrivateImpl::handlerTargetDetected(QNearFieldTarget *target)
{
    if (ndefMessageHandlers.count() == 0 && ndefFilterHandlers.count() == 0) // if no handler is registered
        return;
    if (target->hasNdefMessage()) {
        connect(target, SIGNAL(ndefMessageRead(const QNdefMessage &, const QNearFieldTarget::RequestId &)),
                this, SLOT(handlerNdefMessageRead(const QNdefMessage &, const QNearFieldTarget::RequestId &)));
        connect(target, SIGNAL(requestCompleted(const QNearFieldTarget::RequestId &)),
                this, SLOT(handlerRequestCompleted(const QNearFieldTarget::RequestId &)));
        connect(target, SIGNAL(error(QNearFieldTarget::Error, const QNearFieldTarget::RequestId &)),
                this, SLOT(handlerError(QNearFieldTarget::Error, const QNearFieldTarget::RequestId &)));

        QNearFieldTarget::RequestId id = target->readNdefMessages();
        m_idToTarget.insert(id, target);
    }
}

void QNearFieldManagerPrivateImpl::handlerTargetLost(QNearFieldTarget *target)
{
    disconnect(target, SIGNAL(ndefMessageRead(const QNdefMessage &, const QNearFieldTarget::RequestId &)),
            this, SLOT(handlerNdefMessageRead(const QNdefMessage &, const QNearFieldTarget::RequestId &)));
    disconnect(target, SIGNAL(requestCompleted(const QNearFieldTarget::RequestId &)),
            this, SLOT(handlerRequestCompleted(const QNearFieldTarget::RequestId &)));
    disconnect(target, SIGNAL(error(QNearFieldTarget::Error, const QNearFieldTarget::RequestId &)),
            this, SLOT(handlerError(QNearFieldTarget::Error, const QNearFieldTarget::RequestId &)));
    m_idToTarget.remove(m_idToTarget.key(target));
}

struct VerifyRecord
{
    QNdefFilter::Record filterRecord;
    unsigned int count;
};

void QNearFieldManagerPrivateImpl::handlerNdefMessageRead(const QNdefMessage &message, const QNearFieldTarget::RequestId &id)
{
    QNearFieldTarget *target = m_idToTarget.value(id);
    //For message handlers without filters
    for (int i = 0; i < ndefMessageHandlers.count(); i++) {
        ndefMessageHandlers.at(i).second.invoke(ndefMessageHandlers.at(i).first.second, Q_ARG(QNdefMessage, message), Q_ARG(QNearFieldTarget*, target));
    }

    //For message handlers that specified a filter
    for (int i = 0; i < ndefFilterHandlers.count(); ++i) {
        bool matched = true;

        QNdefFilter filter = ndefFilterHandlers.at(i).second.first;

        QList<VerifyRecord> filterRecords;
        for (int j = 0; j < filter.recordCount(); ++j) {
            VerifyRecord vr;
            vr.count = 0;
            vr.filterRecord = filter.recordAt(j);

            filterRecords.append(vr);
        }

        foreach (const QNdefRecord &record, message) {
            for (int j = 0; matched && (j < filterRecords.count()); ++j) {
                VerifyRecord &vr = filterRecords[j];

                if (vr.filterRecord.typeNameFormat == record.typeNameFormat() &&
                    ( vr.filterRecord.type == record.type() ||
                      vr.filterRecord.type.isEmpty()) ) {
                    ++vr.count;
                    break;
                } else {
                    if (filter.orderMatch()) {
                        if (vr.filterRecord.minimum <= vr.count &&
                            vr.count <= vr.filterRecord.maximum) {
                            continue;
                        } else {
                            matched = false;
                        }
                    }
                }
            }
        }

        for (int j = 0; matched && (j < filterRecords.count()); ++j) {
            const VerifyRecord &vr = filterRecords.at(j);

            if (vr.filterRecord.minimum <= vr.count && vr.count <= vr.filterRecord.maximum)
                continue;
            else
                matched = false;
        }

        if (matched) {
            ndefFilterHandlers.at(i).second.second.invoke(ndefFilterHandlers.at(i).first.second, Q_ARG(QNdefMessage, message), Q_ARG(QNearFieldTarget*, target));
        }
    }
}

void QNearFieldManagerPrivateImpl::handlerRequestCompleted(const QNearFieldTarget::RequestId &id)
{
    m_idToTarget.remove(id);
}

void QNearFieldManagerPrivateImpl::handlerError(QNearFieldTarget::Error error, const QNearFieldTarget::RequestId &id)
{
    Q_UNUSED(error);
    m_idToTarget.remove(id);
}

bool QNearFieldManagerPrivateImpl::isAvailable() const
{
    return AndroidNfc::isAvailable();
}

bool QNearFieldManagerPrivateImpl::startTargetDetection()
{
    if (m_detecting)
        return false;   // Already detecting targets

    m_detecting = true;
    updateReceiveState();
    return true;
}

void QNearFieldManagerPrivateImpl::stopTargetDetection()
{
    m_detecting = false;
    updateReceiveState();
}

// FIXME This is supposed to be a platform registration. A message that
// matches the given NDEF filter should restart the current application.
// The implementation below only works as long as the current application
// is running. It is not a platform wide registration on Android.
int QNearFieldManagerPrivateImpl::registerNdefMessageHandler(QObject *object, const QMetaMethod &method)
{
    ndefMessageHandlers.append(QPair<QPair<int, QObject *>, QMetaMethod>(QPair<int, QObject *>(m_handlerID, object), method));
    updateReceiveState();
    //Returns the handler ID and increments it afterwards
    return m_handlerID++;
}

// FIXME see above
int QNearFieldManagerPrivateImpl::registerNdefMessageHandler(const QNdefFilter &filter,
                                                             QObject *object, const QMetaMethod &method)
{
    //If no record is set in the filter, we ignore the filter
    if (filter.recordCount()==0)
        return registerNdefMessageHandler(object, method);

    ndefFilterHandlers.append(QPair<QPair<int, QObject*>, QPair<QNdefFilter, QMetaMethod> >
                              (QPair<int, QObject*>(m_handlerID, object), QPair<QNdefFilter, QMetaMethod>(filter, method)));

    updateReceiveState();

    return m_handlerID++;
}

bool QNearFieldManagerPrivateImpl::unregisterNdefMessageHandler(int handlerId)
{
    for (int i=0; i<ndefMessageHandlers.count(); ++i) {
        if (ndefMessageHandlers.at(i).first.first == handlerId) {
            ndefMessageHandlers.removeAt(i);
            updateReceiveState();
            return true;
        }
    }
    for (int i=0; i<ndefFilterHandlers.count(); ++i) {
        if (ndefFilterHandlers.at(i).first.first == handlerId) {
            ndefFilterHandlers.removeAt(i);
            updateReceiveState();
            return true;
        }
    }
    return false;
}

void QNearFieldManagerPrivateImpl::requestAccess(QNearFieldManager::TargetAccessModes accessModes)
{
    Q_UNUSED(accessModes);
    //Do nothing, because we dont have access modes for the target
}

void QNearFieldManagerPrivateImpl::releaseAccess(QNearFieldManager::TargetAccessModes accessModes)
{
    Q_UNUSED(accessModes);
    //Do nothing, because we dont have access modes for the target
}

void QNearFieldManagerPrivateImpl::newIntent(QAndroidJniObject intent)
{
    // This function is called from different thread and is used to move intent to main thread.
    QMetaObject::invokeMethod(this, "onTargetDiscovered", Qt::QueuedConnection, Q_ARG(QAndroidJniObject, intent));
}

QByteArray QNearFieldManagerPrivateImpl::getUid(const QAndroidJniObject &intent)
{
    if (!intent.isValid())
        return QByteArray();

    QAndroidJniEnvironment env;
    QAndroidJniObject tag = AndroidNfc::getTag(intent);
    return getUidforTag(tag);
}

void QNearFieldManagerPrivateImpl::onTargetDiscovered(QAndroidJniObject intent)
{
    // Getting UID
    QByteArray uid = getUid(intent);

    // Accepting all targets but only sending signal of requested types.
    NearFieldTarget *&target = m_detectedTargets[uid];
    if (target) {
        target->setIntent(intent);  // Updating existing target
    } else {
        target = new NearFieldTarget(intent, uid, this);
        connect(target, SIGNAL(targetDestroyed(QByteArray)), this, SLOT(onTargetDestroyed(QByteArray)));
        connect(target, SIGNAL(targetLost(QNearFieldTarget*)), this, SIGNAL(targetLost(QNearFieldTarget*)));
    }
    emit targetDetected(target);
}

void QNearFieldManagerPrivateImpl::onTargetDestroyed(const QByteArray &uid)
{
    m_detectedTargets.remove(uid);
}

QByteArray QNearFieldManagerPrivateImpl::getUidforTag(const QAndroidJniObject &tag)
{
    if (!tag.isValid())
        return QByteArray();

    QAndroidJniEnvironment env;
    QAndroidJniObject tagId = tag.callObjectMethod("getId", "()[B");
    QByteArray uid;
    jsize len = env->GetArrayLength(tagId.object<jbyteArray>());
    uid.resize(len);
    env->GetByteArrayRegion(tagId.object<jbyteArray>(), 0, len, reinterpret_cast<jbyte*>(uid.data()));
    return uid;
}

void QNearFieldManagerPrivateImpl::updateReceiveState()
{
    if (m_detecting) {
        AndroidNfc::registerListener(this);
    } else {
        if (ndefMessageHandlers.count() || ndefFilterHandlers.count()) {
            AndroidNfc::registerListener(this);
        } else {
            AndroidNfc::unregisterListener(this);
        }
    }
}

QT_END_NAMESPACE
