/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#ifndef QREMOTEOBJECTREPLICA_P_H
#define QREMOTEOBJECTREPLICA_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qremoteobjectreplica.h"

#include "qremoteobjectpendingcall.h"

#include "qremoteobjectpacket_p.h"

#include <QPointer>
#include <QVector>
#include <QDataStream>
#include <qcompilerdetection.h>

QT_BEGIN_NAMESPACE

class QRemoteObjectReplica;
class QRemoteObjectSource;
class ClientIoDevice;

namespace QRemoteObjectPackets {
class QInitDynamicPacket;
class QInvokePacket;
class QInvokeReplyPacket;
class QRemoteObjectPacket;
}

class QReplicaPrivateInterface
{
public:
    virtual ~QReplicaPrivateInterface() {}
    virtual const QVariant getProperty(int i) const = 0;
    virtual void setProperties(const QVariantList &) = 0;
    virtual void setProperty(int i, const QVariant &) = 0;
    virtual bool isInitialized() const = 0;
    virtual QRemoteObjectReplica::State state() const = 0;
    virtual bool waitForSource(int) = 0;
    virtual QRemoteObjectNode *node() const = 0;

    virtual void _q_send(QMetaObject::Call call, int index, const QVariantList &args) = 0;
    virtual QRemoteObjectPendingCall _q_sendWithReply(QMetaObject::Call call, int index, const QVariantList &args) = 0;
};

class QStubReplicaPrivate : public QReplicaPrivateInterface
{
public:
    explicit QStubReplicaPrivate();
    virtual ~QStubReplicaPrivate();

    const QVariant getProperty(int i) const override;
    void setProperties(const QVariantList &) override;
    void setProperty(int i, const QVariant &) override;
    bool isInitialized() const override { return false; }
    QRemoteObjectReplica::State state() const override { return QRemoteObjectReplica::State::Uninitialized;}
    bool waitForSource(int) override { return false; }
    QRemoteObjectNode *node() const override { return nullptr; }

    void _q_send(QMetaObject::Call call, int index, const QVariantList &args) override;
    QRemoteObjectPendingCall _q_sendWithReply(QMetaObject::Call call, int index, const QVariantList &args) override;
    QVariantList m_propertyStorage;
};

class QRemoteObjectReplicaPrivate : public QObject, public QReplicaPrivateInterface
{
public:
    explicit QRemoteObjectReplicaPrivate(const QString &name, const QMetaObject *, QRemoteObjectNode *);
    virtual ~QRemoteObjectReplicaPrivate();

    bool needsDynamicInitialization() const;

    virtual const QVariant getProperty(int i) const override = 0;
    virtual void setProperties(const QVariantList &) override = 0;
    virtual void setProperty(int i, const QVariant &) override = 0;
    virtual bool isShortCircuit() const = 0;
    virtual bool isInitialized() const override { return true; }
    QRemoteObjectReplica::State state() const override { return QRemoteObjectReplica::State(m_state.load()); }
    void setState(QRemoteObjectReplica::State state);
    virtual bool waitForSource(int) override { return true; }
    virtual bool waitForFinished(const QRemoteObjectPendingCall &, int) { return true; }
    virtual void notifyAboutReply(int, const QVariant &) {}
    virtual void configurePrivate(QRemoteObjectReplica *);
    void emitInitialized();
    QRemoteObjectNode *node() const override { return m_node; }

    virtual void _q_send(QMetaObject::Call call, int index, const QVariantList &args) override = 0;
    virtual QRemoteObjectPendingCall _q_sendWithReply(QMetaObject::Call call, int index, const QVariantList &args) override = 0;

    //Dynamic replica functions
    virtual void initializeMetaObject(const QMetaObjectBuilder &builder, const QVariantList &values);

    QString m_objectName;
    const QMetaObject *m_metaObject;

    //Dynamic Replica data
    int m_numSignals;//TODO maybe here too
    int m_methodOffset;
    int m_signalOffset;
    int m_propertyOffset;
    QRemoteObjectNode *m_node;
    QByteArray m_objectSignature;
    QAtomicInt m_state;
};

class QConnectedReplicaPrivate : public QRemoteObjectReplicaPrivate
{
public:
    explicit QConnectedReplicaPrivate(const QString &name, const QMetaObject *, QRemoteObjectNode *);
    virtual ~QConnectedReplicaPrivate();
    const QVariant getProperty(int i) const override;
    void setProperties(const QVariantList &) override;
    void setProperty(int i, const QVariant &) override;
    bool isShortCircuit() const override { return false; }
    bool isInitialized() const override;
    bool waitForSource(int timeout) override;
    void initialize(const QVariantList &values);
    void configurePrivate(QRemoteObjectReplica *) override;
    void requestRemoteObjectSource();
    bool sendCommand();
    QRemoteObjectPendingCall sendCommandWithReply(int serialId);
    bool waitForFinished(const QRemoteObjectPendingCall &call, int timeout) override;
    void notifyAboutReply(int ackedSerialId, const QVariant &value) override;
    void setConnection(ClientIoDevice *conn);
    void setDisconnected();

    void _q_send(QMetaObject::Call call, int index, const QVariantList &args) override;
    QRemoteObjectPendingCall _q_sendWithReply(QMetaObject::Call call, int index, const QVariantList& args) override;

    void initializeMetaObject(const QMetaObjectBuilder&, const QVariantList&) override;
    QVector<QRemoteObjectReplica *> m_parentsNeedingConnect;
    QVariantList m_propertyStorage;
    QPointer<ClientIoDevice> connectionToSource;

    // pending call data
    int m_curSerialId;
    QHash<int, QRemoteObjectPendingCall> m_pendingCalls;
    QRemoteObjectPackets::DataStreamPacket m_packet;
};

class QInProcessReplicaPrivate : public QRemoteObjectReplicaPrivate
{
public:
    explicit QInProcessReplicaPrivate(const QString &name, const QMetaObject *, QRemoteObjectNode *);
    virtual ~QInProcessReplicaPrivate();

    const QVariant getProperty(int i) const override;
    void setProperties(const QVariantList &) override;
    void setProperty(int i, const QVariant &) override;
    bool isShortCircuit() const override { return true; }

    void _q_send(QMetaObject::Call call, int index, const QVariantList &args) override;
    QRemoteObjectPendingCall _q_sendWithReply(QMetaObject::Call call, int index, const QVariantList& args) override;

    QPointer<QRemoteObjectSource> connectionToSource;
};

QT_END_NAMESPACE

#endif
