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

#include "qnearfieldmanager_simulator_p.h"
#include "qnearfieldmanager.h"
#include "qnearfieldtarget_p.h"
#include "qnearfieldtagtype1_p.h"
#include "qndefmessage.h"

#include <mobilityconnection_p.h>
#include <QtGui/private/qsimulatordata_p.h>

#include <QtCore/QCoreApplication>

QT_BEGIN_NAMESPACE

using namespace QtSimulatorPrivate;

namespace Simulator {

class TagType1 : public QNearFieldTagType1
{
public:
    TagType1(const QByteArray &uid, QObject *parent);
    ~TagType1();

    QByteArray uid() const;

    AccessMethods accessMethods() const;

    RequestId sendCommand(const QByteArray &command);
    bool waitForRequestCompleted(const RequestId &id, int msecs = 5000);

private:
    QByteArray m_uid;
};

TagType1::TagType1(const QByteArray &uid, QObject *parent)
:   QNearFieldTagType1(parent), m_uid(uid)
{
}

TagType1::~TagType1()
{
}

QByteArray TagType1::uid() const
{
    return m_uid;
}

QNearFieldTarget::AccessMethods TagType1::accessMethods() const
{
    return NdefAccess | TagTypeSpecificAccess;
}

QNearFieldTarget::RequestId TagType1::sendCommand(const QByteArray &command)
{
    quint16 crc = qNfcChecksum(command.constData(), command.length());

    RequestId id(new RequestIdPrivate);

    MobilityConnection *connection = MobilityConnection::instance();
    QByteArray response =
        RemoteMetacall<QByteArray>::call(connection->sendSocket(), WaitSync, "nfcSendCommand",
                                         command + char(crc & 0xff) + char(crc >> 8));

    if (response.isEmpty()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, NoResponseError),
                                  Q_ARG(QNearFieldTarget::RequestId, id));
        return id;
    }

    // check crc
    if (qNfcChecksum(response.constData(), response.length()) != 0) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, ChecksumMismatchError),
                                  Q_ARG(QNearFieldTarget::RequestId, id));
        return id;
    }

    response.chop(2);

    QMetaObject::invokeMethod(this, "handleResponse", Qt::QueuedConnection,
                              Q_ARG(QNearFieldTarget::RequestId, id), Q_ARG(QByteArray, response));

    return id;
}

bool TagType1::waitForRequestCompleted(const RequestId &id, int msecs)
{
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);

    return QNearFieldTagType1::waitForRequestCompleted(id, msecs);
}

class NfcConnection : public QObject
{
    Q_OBJECT

public:
    NfcConnection();
    virtual ~NfcConnection();

signals:
    void targetEnteringProximity(const QByteArray &uid);
    void targetLeavingProximity(const QByteArray &uid);
};

NfcConnection::NfcConnection()
:   QObject(MobilityConnection::instance())
{
    MobilityConnection *connection = MobilityConnection::instance();
    connection->addMessageHandler(this);

    RemoteMetacall<void>::call(connection->sendSocket(), NoSync, "setRequestsNfc");
}

NfcConnection::~NfcConnection()
{
}

}

QNearFieldManagerPrivateImpl::QNearFieldManagerPrivateImpl()
:   nfcConnection(new Simulator::NfcConnection)
{
    connect(nfcConnection, SIGNAL(targetEnteringProximity(QByteArray)),
            this, SLOT(targetEnteringProximity(QByteArray)));
    connect(nfcConnection, SIGNAL(targetLeavingProximity(QByteArray)),
            this, SLOT(targetLeavingProximity(QByteArray)));
}

QNearFieldManagerPrivateImpl::~QNearFieldManagerPrivateImpl()
{
    delete nfcConnection;
}

bool QNearFieldManagerPrivateImpl::isAvailable() const
{
    return true;
}

void QNearFieldManagerPrivateImpl::targetEnteringProximity(const QByteArray &uid)
{
    QNearFieldTarget *target = m_targets.value(uid).data();
    if (!target) {
        target = new Simulator::TagType1(uid, this);
        m_targets.insert(uid, target);
    }

    targetActivated(target);
}

void QNearFieldManagerPrivateImpl::targetLeavingProximity(const QByteArray &uid)
{
    QNearFieldTarget *target = m_targets.value(uid).data();
    if (!target) {
        m_targets.remove(uid);
        return;
    }

    targetDeactivated(target);
}

QT_END_NAMESPACE

#include "qnearfieldmanager_simulator.moc"
