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

#include "qnearfieldmanager_qnx_p.h"
#include <QDebug>
#include "qnearfieldtarget_qnx_p.h"
#include <QMetaMethod>
#include "qndeffilter.h"
#include "qndefrecord.h"

QT_BEGIN_NAMESPACE

QNearFieldManagerPrivateImpl::QNearFieldManagerPrivateImpl() :
    m_handlerID(0)
{
    QNXNFCManager::instance()->registerForNewInstance();
    connect(QNXNFCManager::instance(), SIGNAL(ndefMessage(QNdefMessage,QNearFieldTarget*)), this, SLOT(handleMessage(QNdefMessage,QNearFieldTarget*)));

    m_requestedModes = QNearFieldManager::NdefWriteTargetAccess;
    qQNXNFCDebug() << "Nearfieldmanager created";
}

QNearFieldManagerPrivateImpl::~QNearFieldManagerPrivateImpl()
{
    //First, remove all ndef message handlers
    QNXNFCManager::instance()->updateNdefFilters(QList<QByteArray>(), this);
    QNXNFCManager::instance()->unregisterForInstance();
}

bool QNearFieldManagerPrivateImpl::isAvailable() const
{
    return QNXNFCManager::instance()->isAvailable();
}

bool QNearFieldManagerPrivateImpl::startTargetDetection()
{
    qQNXNFCDebug() << "Starting targetdetection in nearfieldmanager";
    if (QNXNFCManager::instance()->startTargetDetection()) {
        connect(QNXNFCManager::instance(), SIGNAL(targetDetected(QNearFieldTarget*,QList<QNdefMessage>)),
                this, SLOT(newTarget(QNearFieldTarget*,QList<QNdefMessage>)));
        return true;
    } else {
        qWarning()<<Q_FUNC_INFO<<"Could not start Target detection";
        return false;
    }
}

void QNearFieldManagerPrivateImpl::stopTargetDetection()
{
    disconnect(QNXNFCManager::instance(), SIGNAL(targetDetected(NearFieldTarget*,QList<QNdefMessage>)),
               this, SLOT(newTarget(NearFieldTarget*,QList<QNdefMessage>)));
    QNXNFCManager::instance()->unregisterTargetDetection(this);
}

int QNearFieldManagerPrivateImpl::registerNdefMessageHandler(QObject *object, const QMetaMethod &method)
{
    QList<QByteArray> filterList;
    filterList += "*";
    QNXNFCManager::instance()->updateNdefFilters(filterList, this);

    ndefMessageHandlers.append(QPair<QPair<int, QObject *>, QMetaMethod>(QPair<int, QObject *>(m_handlerID, object), method));

    //Returns the handler ID and increments it afterwards
    return m_handlerID++;
}

int QNearFieldManagerPrivateImpl::registerNdefMessageHandler(const QNdefFilter &filter,
                                                             QObject *object, const QMetaMethod &method)
{
    //If no record is set in the filter, we ignore the filter
    if (filter.recordCount()==0)
        return registerNdefMessageHandler(object, method);

    ndefFilterHandlers.append(QPair<QPair<int, QObject*>, QPair<QNdefFilter, QMetaMethod> >
                              (QPair<int, QObject*>(m_handlerID, object), QPair<QNdefFilter, QMetaMethod>(filter, method)));

    updateNdefFilter();

    return m_handlerID++;
}

bool QNearFieldManagerPrivateImpl::unregisterNdefMessageHandler(int handlerId)
{
    for (int i=0; i<ndefMessageHandlers.count(); i++) {
        if (ndefMessageHandlers.at(i).first.first == handlerId) {
            ndefMessageHandlers.removeAt(i);
            updateNdefFilter();
            return true;
        }
    }
    for (int i=0; i<ndefFilterHandlers.count(); i++) {
        if (ndefFilterHandlers.at(i).first.first == handlerId) {
            ndefFilterHandlers.removeAt(i);
            updateNdefFilter();
            return true;
        }
    }
    return false;
}

void QNearFieldManagerPrivateImpl::requestAccess(QNearFieldManager::TargetAccessModes accessModes)
{
    Q_UNUSED(accessModes);
    //Do nothing, because we don't have access modes for the target
}

void QNearFieldManagerPrivateImpl::releaseAccess(QNearFieldManager::TargetAccessModes accessModes)
{
    Q_UNUSED(accessModes);
    //Do nothing, because we don't have access modes for the target
}

void QNearFieldManagerPrivateImpl::handleMessage(const QNdefMessage &message, QNearFieldTarget *target)
{
    qQNXNFCDebug() << "Handling message in near field manager. Filtercount:"
                   << ndefFilterHandlers.count() << message.count();
    //For message handlers without filters
    for (int i = 0; i < ndefMessageHandlers.count(); i++) {
        ndefMessageHandlers.at(i).second.invoke(ndefMessageHandlers.at(i).first.second, Q_ARG(QNdefMessage, message), Q_ARG(QNearFieldTarget*, target));
    }

    //For message handlers that specified a filter
    for (int i = 0; i < ndefFilterHandlers.count(); i++) {
        QNdefFilter filter = ndefFilterHandlers.at(i).second.first;
        if (filter.recordCount() > message.count())
            continue;

        int j=0;
        for (j = 0; j < filter.recordCount();) {
            if (message.at(j).typeNameFormat() != filter.recordAt(j).typeNameFormat
                    || message.at(j).type() != filter.recordAt(j).type ) {
                break;
            }
            j++;
        }
        if (j == filter.recordCount())
            ndefFilterHandlers.at(i).second.second.invoke(ndefFilterHandlers.at(i).first.second, Q_ARG(QNdefMessage, message), Q_ARG(QNearFieldTarget*, target));
    }
}

void QNearFieldManagerPrivateImpl::newTarget(QNearFieldTarget *target, const QList<QNdefMessage> &messages)
{
    Q_UNUSED(messages)
    qQNXNFCDebug() << "New Target";
    emit targetDetected(target);
}

void QNearFieldManagerPrivateImpl::updateNdefFilter()
{
    qQNXNFCDebug() << "Updating NDEF filter";
    QList<QByteArray> filterList;
    if (ndefMessageHandlers.size() > 0) { ///SUbscribe for all ndef messages
        filterList += "*";
        QNXNFCManager::instance()->updateNdefFilters(filterList, this);
    } else if (ndefFilterHandlers.size() > 0){
        for (int i = 0; i < ndefFilterHandlers.count(); i++) {
            QByteArray filter = "ndef://" + QByteArray::number(ndefFilterHandlers.at(i).second.first.recordAt(0).typeNameFormat)
                    + '/' + ndefFilterHandlers.at(i).second.first.recordAt(0).type;
            if (!filterList.contains(filter))
                filterList.append(filter);
        }
        QNXNFCManager::instance()->updateNdefFilters(filterList, this);
    } else { //Remove all ndef message handlers for this object
        QNXNFCManager::instance()->updateNdefFilters(filterList, this);
    }
}


QT_END_NAMESPACE
