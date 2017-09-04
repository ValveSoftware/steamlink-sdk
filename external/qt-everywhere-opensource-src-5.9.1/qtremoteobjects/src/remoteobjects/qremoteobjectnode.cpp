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

#include "private/qmetaobjectbuilder_p.h"

#include "qremoteobjectnode.h"
#include "qremoteobjectnode_p.h"

#include "qremoteobjectregistry.h"
#include "qremoteobjectdynamicreplica.h"
#include "qremoteobjectpacket_p.h"
#include "qremoteobjectregistrysource_p.h"
#include "qremoteobjectreplica_p.h"
#include "qremoteobjectsource_p.h"
#include "qremoteobjectabstractitemmodelreplica_p.h"
#include "qremoteobjectabstractitemmodeladapter_p.h"
#include <QAbstractItemModel>

QT_BEGIN_NAMESPACE

using namespace QtRemoteObjects;

static QString name(const QMetaObject * const mobj)
{
    const int ind = mobj->indexOfClassInfo(QCLASSINFO_REMOTEOBJECT_TYPE);
    return ind >= 0 ? QString::fromLatin1(mobj->classInfo(ind).value()) : QString();
}

template <typename K, typename V, typename Query>
bool map_contains(const QMap<K,V> &map, const Query &key, typename QMap<K,V>::const_iterator &result)
{
    const typename QMap<K,V>::const_iterator it = map.find(key);
    if (it == map.end())
        return false;
    result = it;
    return true;
}

QRemoteObjectNodePrivate::QRemoteObjectNodePrivate()
    : QObjectPrivate()
    , registry(nullptr)
    , retryInterval(250)
    , lastError(QRemoteObjectNode::NoError)
    , persistedStore(nullptr)
    , persistedStoreOwnership(QRemoteObjectNode::DoNotPassOwnership)
{ }

QRemoteObjectNodePrivate::~QRemoteObjectNodePrivate()
{
    if (persistedStore && persistedStoreOwnership == QRemoteObjectNode::PassOwnershipToNode)
        delete persistedStore;
}

QRemoteObjectSourceLocations QRemoteObjectNodePrivate::remoteObjectAddresses() const
{
    if (registry)
        return registry->sourceLocations();
    return QRemoteObjectSourceLocations();
}

QRemoteObjectSourceLocations QRemoteObjectRegistryHostPrivate::remoteObjectAddresses() const
{
    if (registrySource)
        return registrySource->sourceLocations();
    return QRemoteObjectSourceLocations();
}

/*!
    \reimp
*/
void QRemoteObjectNode::timerEvent(QTimerEvent*)
{
    Q_D(QRemoteObjectNode);
    Q_FOREACH (ClientIoDevice *conn, d->pendingReconnect) {
        if (conn->isOpen())
            d->pendingReconnect.remove(conn);
        else
            conn->connectToServer();
    }

    if (d->pendingReconnect.isEmpty())
        d->reconnectTimer.stop();

    qRODebug(this) << "timerEvent" << d->pendingReconnect.size();
}

/*!
    \internal The replica needs to have a default constructor to be able
    to create a replica from QML.  In order for it to be properly
    constructed, there needs to be a way to associate the replica with a
    node and start the replica initialization.  Thus we need a public
    method on node to facilitate that.  That's initializeReplica.
*/
void QRemoteObjectNode::initializeReplica(QRemoteObjectReplica *instance, const QString &name)
{
    Q_D(QRemoteObjectNode);
    if (instance->inherits("QRemoteObjectDynamicReplica")) {
        d->setReplicaPrivate(nullptr, instance, name);
    } else {
        const QMetaObject *meta = instance->metaObject();
        d->setReplicaPrivate(meta, instance, name.isEmpty() ? ::name(meta) : name);
    }
}

void QRemoteObjectNodePrivate::setLastError(QRemoteObjectNode::ErrorCode errorCode)
{
    Q_Q(QRemoteObjectNode);
    lastError = errorCode;
    emit q->error(lastError);
}

void QRemoteObjectNodePrivate::setReplicaPrivate(const QMetaObject *meta, QRemoteObjectReplica *instance, const QString &name)
{
    qROPrivDebug() << "Starting setReplicaPrivate for" << name;
    isInitialized.storeRelease(1);
    openConnectionIfNeeded(name);
    QMutexLocker locker(&mutex);
    if (hasInstance(name)) {
        qCDebug(QT_REMOTEOBJECT)<<"setReplicaPrivate - using existing instance";
        QSharedPointer<QRemoteObjectReplicaPrivate> rep = qSharedPointerCast<QRemoteObjectReplicaPrivate>(replicas.value(name).toStrongRef());
        Q_ASSERT(rep);
        instance->d_ptr = rep;
        rep->configurePrivate(instance);
    } else {
        instance->d_ptr.reset(handleNewAcquire(meta, instance, name));
        instance->initialize();
        replicas.insert(name, instance->d_ptr.toWeakRef());
        qROPrivDebug() << "setReplicaPrivate - Created new instance" << name<<remoteObjectAddresses();
    }
}

/*!
    Returns a pointer to the Node's \l {QRemoteObjectRegistry}, if the Node
    is using the Registry feature; otherwise it returns 0.
*/
const QRemoteObjectRegistry *QRemoteObjectNode::registry() const
{
    Q_D(const QRemoteObjectNode);
    return d->registry;
}

/*!
    \class QRemoteObjectPersistedStore
    \inmodule QtRemoteObjects
    \brief The QRemoteObjectPersistedStore virtual class provides the methods
    for setting PROP values of a replica to value they had the last time the
    replica was used.

    This can be used to provide a "reasonable" value to be displayed until the
    connection to the source is established and current values are available.

    This class must be overridden to provide an implementation for saving (\l
    QRemoteObjectPersistedStore::saveProperties) and restoring (\l
    QRemoteObjectPersistedStore::restoreProperties) PROP values. The derived
    type can then be set for a node, and any replica acquired from that node
    will then automatically store PERSISTED properties when the replica
    destructor is called, and retrieve the values when the replica is
    instantiated.
*/

/*!
    \fn virtual void QRemoteObjectPersistedStore::saveProperties(const QString &repName, const QByteArray &repSig, const QVariantList &values)

    This method will be provided the replica class's \a repName, \a repSig and the list of
    \a values that PERSISTED properties have when the replica destructor was
    called. It is the responsibility of the inheriting class to store the
    information in a manner consistent for \l
    QRemoteObjectPersistedStore::restoreProperties to retrieve.

    \sa QRemoteObjectPersistedStore::restoreProperties
*/

/*!
    \fn virtual QVariantList QRemoteObjectPersistedStore::restoreProperties(const QString &repName, const QByteArray &repSig)

    This method will be provided the replica class's \a repName and \a repSig when the
    replica is being initialized. It is the responsibility of the inheriting
    class to get the last values persisted by \l
    QRemoteObjectPersistedStore::saveProperties and return them. An empty
    QVariantList should be returned if no values are available.

    \sa QRemoteObjectPersistedStore::saveProperties
*/

/*!
    \enum QRemoteObjectNode::StorageOwnership

    Used to tell a node whether it should take ownership of a passed pointer or not:

    \value DoNotPassOwnership The ownership of the object is not passed.
    \value PassOwnershipToNode The ownership of the object is passed, and the node destructor will call delete.
*/

/*!
    Provides a \l QRemoteObjectPersistedStore \a store for the node, allowing
    replica \l PROP members with the PERSISTED trait of \l PROP to save their
    current value when the replica is deleted and restore a stored value the
    next time the replica is started. Requires a \l QRemoteObjectPersistedStore
    class implementation to control where and how persistence is handled. Use
    the \l QRemoteObjectNode::StorageOwnership enum passed by \a ownership to
    determine whether the Node will delete the provided pointer or not.

    \sa QRemoteObjectPersistedStore, QRemoteObjectNode::StorageOwnership
*/
void QRemoteObjectNode::setPersistedStore(QRemoteObjectPersistedStore *store, StorageOwnership ownership)
{
    Q_D(QRemoteObjectNode);
    d->persistedStore = store;
    d->persistedStoreOwnership = ownership;
}

void QRemoteObjectNodePrivate::connectReplica(QObject *object, QRemoteObjectReplica *instance)
{
    int nConnections = 0;
    const QMetaObject *us = instance->metaObject();
    const QMetaObject *them = object->metaObject();

    static const int memberOffset = QRemoteObjectReplica::staticMetaObject.methodCount();
    for (int idx = memberOffset; idx < us->methodCount(); ++idx) {
        const QMetaMethod mm = us->method(idx);

        qROPrivDebug() << idx << mm.name();
        if (mm.methodType() != QMetaMethod::Signal)
            continue;

        // try to connect to a signal on the parent that has the same method signature
        QByteArray sig = QMetaObject::normalizedSignature(mm.methodSignature().constData());
        qROPrivDebug() << sig;
        if (them->indexOfSignal(sig.constData()) == -1)
            continue;

        sig.prepend(QSIGNAL_CODE + '0');
        const char * const csig = sig.constData();
        const bool res = QObject::connect(object, csig, instance, csig);
        Q_UNUSED(res);
        ++nConnections;

        qROPrivDebug() << sig << res;
    }

    qROPrivDebug() << "# connections =" << nConnections;
}

void QRemoteObjectNodePrivate::openConnectionIfNeeded(const QString &name)
{
    qROPrivDebug() << Q_FUNC_INFO << name << this;
    if (!remoteObjectAddresses().contains(name)) {
        qROPrivDebug() << name << "not available - available addresses:" << remoteObjectAddresses();
        return;
    }

    if (!initConnection(remoteObjectAddresses()[name].hostUrl))
        qROPrivWarning() << "failed to open connection to" << name;
}

bool QRemoteObjectNodePrivate::initConnection(const QUrl &address)
{
    Q_Q(QRemoteObjectNode);
    if (requestedUrls.contains(address)) {
        qROPrivDebug() << "Connection already requested for " << address.toString();
        return true;
    }

    requestedUrls.insert(address);

    ClientIoDevice *connection = QtROClientFactory::instance()->create(address, q);
    if (!connection) {
        qROPrivWarning() << "Could not create ClientIoDevice for client. Invalid url/scheme provided?" << address;
        return false;
    }

    qROPrivDebug() << "Opening connection to" << address.toString();
    qROPrivDebug() << "Replica Connection isValid" << connection->isOpen();
    QObject::connect(connection, SIGNAL(shouldReconnect(ClientIoDevice*)), q, SLOT(onShouldReconnect(ClientIoDevice*)));
    connection->connectToServer();
    QObject::connect(connection, SIGNAL(readyRead()), &clientRead, SLOT(map()));
    clientRead.setMapping(connection, connection);
    return true;
}

bool QRemoteObjectNodePrivate::hasInstance(const QString &name)
{
    if (!replicas.contains(name))
        return false;

    QSharedPointer<QReplicaPrivateInterface> rep = replicas.value(name).toStrongRef();
    if (!rep) { //already deleted
        replicas.remove(name);
        return false;
    }

    return true;
}

void QRemoteObjectNodePrivate::onRemoteObjectSourceAdded(const QRemoteObjectSourceLocation &entry)
{
    qROPrivDebug() << "onRemoteObjectSourceAdded" << entry << replicas << replicas.contains(entry.first);
    if (!entry.first.isEmpty()) {
        QRemoteObjectSourceLocations locs = registry->sourceLocations();
        locs[entry.first] = entry.second;
        //TODO Is there a way to extend QRemoteObjectSourceLocations in place?
        registry->d_ptr->setProperty(0, QVariant::fromValue(locs));
        qROPrivDebug() << "onRemoteObjectSourceAdded, now locations =" << locs;
    }
    if (replicas.contains(entry.first)) //We have a replica waiting on this remoteObject
    {
        QSharedPointer<QReplicaPrivateInterface> rep = replicas.value(entry.first).toStrongRef();
        if (!rep) { //replica has been deleted, remove from list
            replicas.remove(entry.first);
            return;
        }

        initConnection(entry.second.hostUrl);

        qROPrivDebug() << "Called initConnection due to new RemoteObjectSource added via registry" << entry.first;
    }
}

void QRemoteObjectNodePrivate::onRemoteObjectSourceRemoved(const QRemoteObjectSourceLocation &entry)
{
    if (!entry.first.isEmpty()) {
        QRemoteObjectSourceLocations locs = registry->sourceLocations();
        locs.remove(entry.first);
        registry->d_ptr->setProperty(0, QVariant::fromValue(locs));
    }
}

void QRemoteObjectNodePrivate::onRegistryInitialized()
{
    qROPrivDebug() << "Registry Initialized" << remoteObjectAddresses();

    QHashIterator<QString, QRemoteObjectSourceLocationInfo> i(remoteObjectAddresses());
    while (i.hasNext()) {
        i.next();
        if (replicas.contains(i.key())) //We have a replica waiting on this remoteObject
        {
            QSharedPointer<QReplicaPrivateInterface> rep = replicas.value(i.key()).toStrongRef();
            if (rep && !requestedUrls.contains(i.value().hostUrl))
                initConnection(i.value().hostUrl);
            else if (!rep) //replica has been deleted, remove from list
                replicas.remove(i.key());

            continue;
        }
    }
}

void QRemoteObjectNodePrivate::onShouldReconnect(ClientIoDevice *ioDevice)
{
    Q_Q(QRemoteObjectNode);

    Q_FOREACH (const QString &remoteObject, ioDevice->remoteObjects()) {
        connectedSources.remove(remoteObject);
        ioDevice->removeSource(remoteObject);
        if (replicas.contains(remoteObject)) { //We have a replica waiting on this remoteObject
            QSharedPointer<QConnectedReplicaPrivate> rep = qSharedPointerCast<QConnectedReplicaPrivate>(replicas.value(remoteObject).toStrongRef());
            if (rep && !rep->connectionToSource.isNull()) {
                rep->setDisconnected();
            } else if (!rep) {
                replicas.remove(remoteObject);
            }
        }
    }
    if (requestedUrls.contains(ioDevice->url())) {
        // Only try to reconnect to URLs requested via connectToNode
        // If we connected via registry, wait for the registry to see the node/source again
        pendingReconnect.insert(ioDevice);
        if (!reconnectTimer.isActive()) {
            reconnectTimer.start(retryInterval, q);
            qROPrivDebug() << "Starting reconnect timer";
        }
    } else {
        qROPrivDebug() << "Url" << ioDevice->url().toDisplayString().toLatin1()
                       << "lost.  We will reconnect Replicas if they reappear on the Registry.";
    }
}

//This version of handleNewAcquire creates a QConnectedReplica. If this is a
//host node, the QRemoteObjectHostBasePrivate overload is called instead.
QReplicaPrivateInterface *QRemoteObjectNodePrivate::handleNewAcquire(const QMetaObject *meta, QRemoteObjectReplica *instance, const QString &name)
{
    Q_Q(QRemoteObjectNode);
    QConnectedReplicaPrivate *rp = new QConnectedReplicaPrivate(name, meta, q);
    rp->configurePrivate(instance);
    if (connectedSources.contains(name)) { //Either we have a peer connections, or existing connection via registry
        if (checkSignatures(rp->m_objectSignature, connectedSources[name].objectSignature))
            rp->setConnection(connectedSources[name].device);
        else
            rp->setState(QRemoteObjectReplica::SignatureMismatch);
    } else if (remoteObjectAddresses().contains(name)) { //No existing connection, but we know we can connect via registry
        initConnection(remoteObjectAddresses()[name].hostUrl); //This will try the connection, and if successful, the remoteObjects will be sent
                                              //The link to the replica will be handled then
    }
    return rp;
}

//Host Nodes can use the more efficient InProcess Replica if we (this Node) hold the Source for the
//requested Replica.  If not, fall back to the Connected Replica case.
QReplicaPrivateInterface *QRemoteObjectHostBasePrivate::handleNewAcquire(const QMetaObject *meta, QRemoteObjectReplica *instance, const QString &name)
{
    QMap<QString, QRemoteObjectSource*>::const_iterator mapIt;
    if (remoteObjectIo && map_contains(remoteObjectIo->m_remoteObjects, name, mapIt)) {
        Q_Q(QRemoteObjectHostBase);
        QInProcessReplicaPrivate *rp = new QInProcessReplicaPrivate(name, meta, q);
        rp->configurePrivate(instance);
        connectReplica(mapIt.value()->m_object, instance);
        rp->connectionToSource = mapIt.value();
        return rp;
    }
    return QRemoteObjectNodePrivate::handleNewAcquire(meta, instance, name);
}

void QRemoteObjectNodePrivate::onClientRead(QObject *obj)
{
    using namespace QRemoteObjectPackets;
    ClientIoDevice *connection = qobject_cast<ClientIoDevice*>(obj);
    QRemoteObjectPacketTypeEnum packetType;
    Q_ASSERT(connection);

    do {

        if (!connection->read(packetType, rxName))
            return;

        switch (packetType) {
        case ObjectList:
        {
            deserializeObjectListPacket(connection->stream(), rxObjects);
            qROPrivDebug() << "newObjects:" << rxObjects;
            Q_FOREACH (const auto &remoteObject, rxObjects) {
                qROPrivDebug() << "  connectedSources.contains(" << remoteObject << ")" << connectedSources.contains(remoteObject.name) << replicas.contains(remoteObject.name);
                if (!connectedSources.contains(remoteObject.name)) {
                    connectedSources[remoteObject.name] = SourceInfo{connection, remoteObject.typeName, remoteObject.signature};
                    connection->addSource(remoteObject.name);
                    if (replicas.contains(remoteObject.name)) { //We have a replica waiting on this remoteObject
                        QSharedPointer<QConnectedReplicaPrivate> rep = qSharedPointerCast<QConnectedReplicaPrivate>(replicas.value(remoteObject.name).toStrongRef());
                        if (!rep || checkSignatures(remoteObject.signature, rep->m_objectSignature)) {
                            if (rep && rep->connectionToSource.isNull()) {
                                qROPrivDebug() << "Test" << remoteObject<<replicas.keys();
                                qROPrivDebug() << rep;
                                rep->setConnection(connection);
                            } else if (!rep) { //replica has been deleted, remove from list
                                replicas.remove(remoteObject.name);
                            }
                        } else {
                            if (rep)
                                rep->setState(QRemoteObjectReplica::SignatureMismatch);
                        }
                        continue;
                    }
                }
            }
            break;
        }
        case InitPacket:
        {
            qROPrivDebug() << "InitObject-->" << rxName << this;
            QSharedPointer<QConnectedReplicaPrivate> rep = qSharedPointerCast<QConnectedReplicaPrivate>(replicas.value(rxName).toStrongRef());
            //Use m_rxArgs (a QVariantList to hold the properties QVariantList)
            deserializeInitPacket(connection->stream(), rxArgs);
            if (rep)
            {
                rep->initialize(rxArgs);
            } else { //replica has been deleted, remove from list
                replicas.remove(rxName);
            }
            break;
        }
        case InitDynamicPacket:
        {
            qROPrivDebug() << "InitObject-->" << rxName << this;
            QMetaObjectBuilder builder;
            builder.setClassName("QRemoteObjectDynamicReplica");
            builder.setSuperClass(&QRemoteObjectReplica::staticMetaObject);
            builder.setFlags(QMetaObjectBuilder::DynamicMetaObject);
            deserializeInitDynamicPacket(connection->stream(), builder, rxArgs);
            QSharedPointer<QConnectedReplicaPrivate> rep = qSharedPointerCast<QConnectedReplicaPrivate>(replicas.value(rxName).toStrongRef());
            if (rep)
            {
                rep->initializeMetaObject(builder, rxArgs);
            } else { //replica has been deleted, remove from list
                replicas.remove(rxName);
            }
            break;
        }
        case RemoveObject:
        {
            qROPrivDebug() << "RemoveObject-->" << rxName << this;
            connectedSources.remove(rxName);
            connection->removeSource(rxName);
            if (replicas.contains(rxName)) { //We have a replica waiting on this remoteObject
                QSharedPointer<QConnectedReplicaPrivate> rep = qSharedPointerCast<QConnectedReplicaPrivate>(replicas.value(rxName).toStrongRef());
                if (rep && !rep->connectionToSource.isNull()) {
                    rep->connectionToSource.clear();
                    rep->setState(QRemoteObjectReplica::Suspect);
                } else if (!rep) {
                    replicas.remove(rxName);
                }
            }
            break;
        }
        case PropertyChangePacket:
        {
            int propertyIndex;
            deserializePropertyChangePacket(connection->stream(), propertyIndex, rxValue);
            QSharedPointer<QRemoteObjectReplicaPrivate> rep = qSharedPointerCast<QRemoteObjectReplicaPrivate>(replicas.value(rxName).toStrongRef());
            if (rep) {
                const QMetaProperty property = rep->m_metaObject->property(propertyIndex + rep->m_metaObject->propertyOffset());
                rep->setProperty(propertyIndex, deserializedProperty(rxValue, property));
            } else { //replica has been deleted, remove from list
                replicas.remove(rxName);
            }
            break;
        }
        case InvokePacket:
        {
            int call, index, serialId, propertyIndex;
            deserializeInvokePacket(connection->stream(), call, index, rxArgs, serialId, propertyIndex);
            QSharedPointer<QRemoteObjectReplicaPrivate> rep = qSharedPointerCast<QRemoteObjectReplicaPrivate>(replicas.value(rxName).toStrongRef());
            if (rep) {
                static QVariant null(QMetaType::QObjectStar, (void*)0);
                QVariant paramValue;
                // Qt usually supports 9 arguments, so ten should be usually safe
                QVarLengthArray<void*, 10> param(rxArgs.size() + 1);
                param[0] = null.data(); //Never a return value
                if (rxArgs.size()) {
                    for (int i = 0; i < rxArgs.size(); i++) {
                        param[i + 1] = const_cast<void *>(rxArgs[i].data());
                    }
                } else if (propertyIndex != -1) {
                    param.resize(2);
                    paramValue = rep->getProperty(propertyIndex);
                    param[1] = paramValue.data();
                }
                qROPrivDebug() << "Replica Invoke-->" << rxName << rep->m_metaObject->method(index+rep->m_signalOffset).name() << index << rep->m_signalOffset;
                QMetaObject::activate(rep.data(), rep->metaObject(), index+rep->m_signalOffset, param.data());
            } else { //replica has been deleted, remove from list
                replicas.remove(rxName);
            }
            break;
        }
        case InvokeReplyPacket:
        {
            int ackedSerialId;
            deserializeInvokeReplyPacket(connection->stream(), ackedSerialId, rxValue);
            QSharedPointer<QRemoteObjectReplicaPrivate> rep = qSharedPointerCast<QRemoteObjectReplicaPrivate>(replicas.value(rxName).toStrongRef());
            if (rep) {
                qROPrivDebug() << "Received InvokeReplyPacket ack'ing serial id:" << ackedSerialId;
                rep->notifyAboutReply(ackedSerialId, rxValue);
            } else { //replica has been deleted, remove from list
                replicas.remove(rxName);
            }
            break;
        }
        case AddObject:
        case Invalid:
            qROPrivWarning() << "Unexpected packet received";
        }
    } while (connection->bytesAvailable()); // have bytes left over, so do another iteration
}

/*!
    \class QRemoteObjectNode
    \inmodule QtRemoteObjects
    \brief A node on a Qt Remote Objects network.

    The QRemoteObjectNode class provides an entry point to a QtRemoteObjects
    network. A network can be as simple as two nodes, or an arbitrarily complex
    set of processes and devices.

    A QRemoteObjectNode does not have a url that other nodes can connect to,
    and thus is able to acquire replicas only. It is not able to share source
    objects (only QRemoteObjectHost and QRemoteObjectRegistryHost Nodes can
    share).

    Nodes may connect to each other directly using \l connectToNode, or
    they can use the QRemoteObjectRegistry to simplify connections.

    The QRemoteObjectRegistry is a special replica available to every node that
    connects to the Registry Url. It knows how to connect to every
    QRemoteObjectSource object on the network.

    \sa QRemoteObjectHost, QRemoteObjectRegistryHost
*/

/*!
    \class QRemoteObjectHostBase
    \inmodule QtRemoteObjects
    \brief The QRemoteObjectHostBase class provides base functionality common to \l
    {QRemoteObjectHost} {Host} and \l {QRemoteObjectRegistryHost} {RegistryHost} classes.

    QRemoteObjectHostBase is a base class that cannot be instantiated directly.
    It provides the enableRemoting and disableRemoting functionality shared by
    all host nodes (\l {QRemoteObjectHost} {Host} and \l
    {QRemoteObjectRegistryHost} {RegistryHost}) as well as the logic required
    to expose \l {Source} objects on the Remote Objects network.
*/

/*!
    \class QRemoteObjectHost
    \inmodule QtRemoteObjects
    \brief A (Host) Node on a Qt Remote Objects network.

    The QRemoteObjectHost class provides an entry point to a QtRemoteObjects
    network. A network can be as simple as two nodes, or an arbitrarily complex
    set of processes and devices.

    QRemoteObjectHosts have the same capabilities as QRemoteObjectNodes, but
    they can also be connected to and can share source objects on the network.

    Nodes may connect to each other directly using \l connectToNode, or they
    can use the QRemoteObjectRegistry to simplify connections.

    The QRemoteObjectRegistry is a special replica available to every node that
    connects to the uegistry Url. It knows how to connect to every
    QRemoteObjectSource object on the network.

    \sa QRemoteObjectNode, QRemoteObjectRegistryHost
*/

/*!
    \class QRemoteObjectRegistryHost
    \inmodule QtRemoteObjects
    \brief A (Host/Registry) node on a Qt Remote Objects network.

    The QRemoteObjectRegistryHost class provides an entry point to a QtRemoteObjects
    network. A network can be as simple as two Nodes, or an arbitrarily complex
    set of processes and devices.

    A QRemoteObjectRegistryHost has the same capability that a
    QRemoteObjectHost has (which includes everything a QRemoteObjectNode
    supports), and in addition is the owner of the Registry. Any
    QRemoteObjectHost node that connects to this Node will have all of their
    Source objects made available by the Registry.

    Nodes only support connection to one \l registry, calling \l
    QRemoteObjectNode::setRegistryUrl when a Registry is already set is
    considered an error. For something like a secure and insecure network
    (where different Registries would be applicable), the recommendation is to
    create separate Nodes to connect to each, in effect creating two
    independent Qt Remote Objects networks.

    Nodes may connect to each other directly using \l connectToNode, or they
    can use the QRemoteObjectRegistry to simplify connections.

    The QRemoteObjectRegistry is a special Replica available to every Node that
    connects to the Registry Url. It knows how to connect to every
    QRemoteObjectSource object on the network.

    \sa QRemoteObjectNode, QRemoteObjectHost
*/

/*!
    \enum QRemoteObjectNode::ErrorCode

    This enum type specifies the various error codes associated with QRemoteObjectNode errors:

    \value NoError No error.
    \value RegistryNotAcquired The registry could not be acquired.
    \value RegistryAlreadyHosted The registry is already defined and hosting Sources.
    \value NodeIsNoServer The given QRemoteObjectNode is not a host node.
    \value ServerAlreadyCreated The host node has already been initialized.
    \value UnintendedRegistryHosting An attempt was made to create a host QRemoteObjectNode and connect to itself as the registry.
    \value OperationNotValidOnClientNode The attempted operation is not valid on a client QRemoteObjectNode.
    \value SourceNotRegistered The given QRemoteObjectSource is not registered on this node.
    \value MissingObjectName The given QObject does not have objectName() set.
    \value HostUrlInvalid The given url has an invalid or unrecognized scheme.
*/

/*!
    \fn ObjectType *QRemoteObjectNode::acquire(const QString &name)

    Returns a pointer to a Replica of type ObjectType (which is a template
    parameter and must inherit from \l QRemoteObjectReplica). That is, the
    template parameter must be a \l {repc} generated type. The \a name
    parameter can be used to specify the \a name given to the object
    during the QRemoteObjectHost::enableRemoting() call.
*/

void QRemoteObjectNodePrivate::initialize()
{
    Q_Q(QRemoteObjectNode);
    qRegisterMetaType<QRemoteObjectNode *>();
    qRegisterMetaType<QRemoteObjectNode::ErrorCode>();
    qRegisterMetaType<QAbstractSocket::SocketError>(); //For queued qnx error()
    qRegisterMetaTypeStreamOperators<QVector<int> >();
    QObject::connect(&clientRead, SIGNAL(mapped(QObject*)), q, SLOT(onClientRead(QObject*)));
}

bool QRemoteObjectNodePrivate::checkSignatures(const QByteArray &a, const QByteArray &b)
{
    // if any of a or b is empty it means it's a dynamic ojects or an item model
    if (a.isEmpty() || b.isEmpty())
        return true;
    return a == b;
}


void QRemoteObjectNode::persistProperties(const QString &repName, const QByteArray &repSig, const QVariantList &props)
{
    Q_D(QRemoteObjectNode);
    if (d->persistedStore) {
        d->persistedStore->saveProperties(repName, repSig, props);
    } else {
        qCWarning(QT_REMOTEOBJECT) << qPrintable(objectName()) << "Unable to store persisted properties for" << repName;
        qCWarning(QT_REMOTEOBJECT) << "    No persisted store set.";
    }
}

QVariantList QRemoteObjectNode::retrieveProperties(const QString &repName, const QByteArray &repSig)
{
    Q_D(QRemoteObjectNode);
    if (d->persistedStore) {
        return d->persistedStore->restoreProperties(repName, repSig);
    }
    qCWarning(QT_REMOTEOBJECT) << qPrintable(objectName()) << "Unable to retrieve persisted properties for" << repName;
    qCWarning(QT_REMOTEOBJECT) << "    No persisted store set.";
    return QVariantList();
}

/*!
    Default constructor for QRemoteObjectNode with the given \a parent. A Node
    constructed in this manner can not be connected to, and thus can not expose
    Source objects on the network. It also will not include a \l
    QRemoteObjectRegistry, unless set manually using setRegistryUrl.

    \sa connectToNode, setRegistryUrl
*/
QRemoteObjectNode::QRemoteObjectNode(QObject *parent)
    : QObject(*new QRemoteObjectNodePrivate, parent)
{
    Q_D(QRemoteObjectNode);
    d->initialize();
}

/*!
    QRemoteObjectNode connected to a {QRemoteObjectRegistry} {Registry}. A Node
    constructed in this manner can not be connected to, and thus can not expose
    Source objects on the network. Finding and connecting to other (Host) Nodes
    is handled by the QRemoteObjectRegistry specified by \a registryAddress.

    \sa connectToNode, setRegistryUrl, QRemoteObjectHost, QRemoteObjectRegistryHost
*/
QRemoteObjectNode::QRemoteObjectNode(const QUrl &registryAddress, QObject *parent)
    : QObject(*new QRemoteObjectNodePrivate, parent)
{
    Q_D(QRemoteObjectNode);
    d->initialize();
    setRegistryUrl(registryAddress);
}

QRemoteObjectNode::QRemoteObjectNode(QRemoteObjectNodePrivate &dptr, QObject *parent)
    : QObject(dptr, parent)
{
    Q_D(QRemoteObjectNode);
    d->initialize();
}

/*!
    \internal This is a base class for both QRemoteObjectHost and
    QRemoteObjectRegistryHost to provide the shared features/functions for
    sharing \l Source objects.
*/
QRemoteObjectHostBase::QRemoteObjectHostBase(QRemoteObjectHostBasePrivate &d, QObject *parent)
    : QRemoteObjectNode(d, parent)
{ }

/*!
    Constructs a new QRemoteObjectHost Node (i.e., a Node that supports
    exposing \l Source objects on the QtRO network) with the given \a parent.
    This constructor is meant specific to support QML in the future as it will
    not be available to connect to until \l setHostUrl() is called.

    \sa setHostUrl(), setRegistryUrl()
*/
QRemoteObjectHost::QRemoteObjectHost(QObject *parent)
    : QRemoteObjectHostBase(*new QRemoteObjectHostPrivate, parent)
{ }

/*!
    Constructs a new QRemoteObjectHost Node (i.e., a Node that supports
    exposing \l Source objects on the QtRO network) with address \a address. If
    set, \a registryAddress will be used to connect to the \l
    QRemoteObjectRegistry at the provided address.

    \sa setHostUrl(), setRegistryUrl()
*/
QRemoteObjectHost::QRemoteObjectHost(const QUrl &address, const QUrl &registryAddress, QObject *parent)
    : QRemoteObjectHostBase(*new QRemoteObjectHostPrivate, parent)
{
    if (!address.isEmpty()) {
        if (!setHostUrl(address))
            return;
    }

    if (!registryAddress.isEmpty())
        setRegistryUrl(registryAddress);
}

/*!
    Constructs a new QRemoteObjectHost Node (i.e., a Node that supports
    exposing \l Source objects on the QtRO network) with a url of \a
    address and the given \a parent. This overload is provided as a convenience for specifying a
    QObject parent without providing a registry address.

    \sa setHostUrl(), setRegistryUrl()
*/
QRemoteObjectHost::QRemoteObjectHost(const QUrl &address, QObject *parent)
    : QRemoteObjectHostBase(*new QRemoteObjectHostPrivate, parent)
{
    if (!address.isEmpty())
        setHostUrl(address);
}

/*!
    \internal QRemoteObjectHost::QRemoteObjectHost
*/
QRemoteObjectHost::QRemoteObjectHost(QRemoteObjectHostPrivate &d, QObject *parent)
    : QRemoteObjectHostBase(d, parent)
{ }

QRemoteObjectHost::~QRemoteObjectHost() {}

/*!
    Constructs a new QRemoteObjectRegistryHost Node with the given \a parent. RegistryHost Nodes have
    the same functionality as \l QRemoteObjectHost Nodes, except rather than
    being able to connect to a \l QRemoteObjectRegistry, the provided Host QUrl
    (\a registryAddress) becomes the address of the registry for other Nodes to
    connect to.
*/
QRemoteObjectRegistryHost::QRemoteObjectRegistryHost(const QUrl &registryAddress, QObject *parent)
    : QRemoteObjectHostBase(*new QRemoteObjectRegistryHostPrivate, parent)
{
    if (registryAddress.isEmpty())
        return;

    setRegistryUrl(registryAddress);
}

/*!
    \internal
*/
QRemoteObjectRegistryHost::QRemoteObjectRegistryHost(QRemoteObjectRegistryHostPrivate &d, QObject *parent)
    : QRemoteObjectHostBase(d, parent)
{ }

QRemoteObjectRegistryHost::~QRemoteObjectRegistryHost() {}

QRemoteObjectNode::~QRemoteObjectNode()
{ }

/*!
    Sets \a name as the internal name for this Node.  This
    is then output as part of the logging (if enabled).
    This is primarily useful if you merge log data from multiple nodes.
*/
void QRemoteObjectNode::setName(const QString &name)
{
    setObjectName(name);
}

/*!
    Similar to QObject::setObjectName() (which this method calls), but this
    version also applies the \a name to internal classes as well, which are
    used in some of the debugging output.
*/
void QRemoteObjectHostBase::setName(const QString &name)
{
    Q_D(QRemoteObjectHostBase);
    setObjectName(name);
    if (d->remoteObjectIo)
        d->remoteObjectIo->setObjectName(name);
}

/*!
    \internal The HostBase version of this method is protected so the method
    isn't exposed on RegistryHost nodes.
*/
QUrl QRemoteObjectHostBase::hostUrl() const
{
    Q_D(const QRemoteObjectHostBase);
    return d->remoteObjectIo->serverAddress();
}

/*!
    \internal The HostBase version of this method is protected so the method
    isn't exposed on RegistryHost nodes.
*/
bool QRemoteObjectHostBase::setHostUrl(const QUrl &hostAddress)
{
    Q_D(QRemoteObjectHostBase);
    if (d->remoteObjectIo) {
        d->setLastError(ServerAlreadyCreated);
        return false;
    }
    else if (d->isInitialized.loadAcquire()) {
        d->setLastError(RegistryAlreadyHosted);
        return false;
    }

    d->remoteObjectIo = new QRemoteObjectSourceIo(hostAddress, this);
    if (d->remoteObjectIo->m_server.isNull()) { //Invalid url/scheme
        d->setLastError(HostUrlInvalid);
        delete d->remoteObjectIo;
        d->remoteObjectIo = 0;
        return false;
    }

    //If we've given a name to the node, set it on the sourceIo as well
    if (!objectName().isEmpty())
        d->remoteObjectIo->setObjectName(objectName());
    //Since we don't know whether setHostUrl or setRegistryUrl/setRegistryHost will be called first,
    //break it into two pieces.  setHostUrl connects the RemoteObjectSourceIo->[add/remove]RemoteObjectSource to QRemoteObjectReplicaNode->[add/remove]RemoteObjectSource
    //setRegistry* calls appropriately connect RemoteObjecSourcetIo->[add/remove]RemoteObjectSource to the registry when it is created
    QObject::connect(d->remoteObjectIo, SIGNAL(remoteObjectAdded(QRemoteObjectSourceLocation)), this, SIGNAL(remoteObjectAdded(QRemoteObjectSourceLocation)));
    QObject::connect(d->remoteObjectIo, SIGNAL(remoteObjectRemoved(QRemoteObjectSourceLocation)), this, SIGNAL(remoteObjectRemoved(QRemoteObjectSourceLocation)));

    return true;
}

/*!
    Returns the host address for the QRemoteObjectNode as a QUrl. If the Node
    is not a Host node, it return an empty QUrl.

    \sa setHostUrl()
*/
QUrl QRemoteObjectHost::hostUrl() const
{
    return QRemoteObjectHostBase::hostUrl();
}

/*!
    Sets the \a hostAddress for a host QRemoteObjectNode.

    Returns \c true if the Host address is set, otherwise \c false.
*/
bool QRemoteObjectHost::setHostUrl(const QUrl &hostAddress)
{
    return QRemoteObjectHostBase::setHostUrl(hostAddress);
}

/*!
    This method can be used to set the address of this Node to \a registryUrl
    (used for other Nodes to connect to this one), if the QUrl isn't set in the
    constructor. Since this Node becomes the Registry, calling this setter
    method causes this Node to use the url as the host address. All other
    Node's use the \l {QRemoteObjectNode::setRegistryUrl} method initiate a
    connection to the Registry.

    \sa QRemoteObjectRegistryHost(), QRemoteObjectNode::setRegistryUrl
*/
bool QRemoteObjectRegistryHost::setRegistryUrl(const QUrl &registryUrl)
{
    Q_D(QRemoteObjectRegistryHost);
    if (setHostUrl(registryUrl)) {
        if (!d->remoteObjectIo) {
            d->setLastError(ServerAlreadyCreated);
            return false;
        } else if (d->isInitialized.loadAcquire() || d->registry) {
            d->setLastError(RegistryAlreadyHosted);
            return false;
        }

        QRegistrySource *remoteObject = new QRegistrySource(this);
        enableRemoting(remoteObject);
        d->registryAddress = d->remoteObjectIo->serverAddress();
        d->registrySource = remoteObject;
        //Connect RemoteObjectSourceIo->remoteObject[Added/Removde] to the registry Slot
        QObject::connect(this, SIGNAL(remoteObjectAdded(QRemoteObjectSourceLocation)), d->registrySource, SLOT(addSource(QRemoteObjectSourceLocation)));
        QObject::connect(this, SIGNAL(remoteObjectRemoved(QRemoteObjectSourceLocation)), d->registrySource, SLOT(removeSource(QRemoteObjectSourceLocation)));
        QObject::connect(d->remoteObjectIo, SIGNAL(serverRemoved(QUrl)),d->registrySource, SLOT(removeServer(QUrl)));
        //onAdd/Remove update the known remoteObjects list in the RegistrySource, so no need to connect to the RegistrySource remoteObjectAdded/Removed signals
        d->setRegistry(acquire<QRemoteObjectRegistry>());
        return true;
    }
    return false;
}

/*!
    Returns the last error set.
*/
QRemoteObjectNode::ErrorCode QRemoteObjectNode::lastError() const
{
    Q_D(const QRemoteObjectNode);
    return d->lastError;
}

/*!
    \property QRemoteObjectNode::registryUrl
    \brief The address of the \l {QRemoteObjectRegistry} {Registry} used by this node.

    This will be an empty QUrl if there is no registry in use.
*/
QUrl QRemoteObjectNode::registryUrl() const
{
    Q_D(const QRemoteObjectNode);
    return d->registryAddress;
}

bool QRemoteObjectNode::setRegistryUrl(const QUrl &registryAddress)
{
    Q_D(QRemoteObjectNode);
    if (d->isInitialized.loadAcquire() || d->registry) {
        d->setLastError(RegistryAlreadyHosted);
        return false;
    }

    if (!connectToNode(registryAddress)) {
        d->setLastError(RegistryNotAcquired);
        return false;
    }

    d->registryAddress = registryAddress;
    d->setRegistry(acquire<QRemoteObjectRegistry>());
    //Connect remoteObject[Added/Removed] to the registry Slot
    QObject::connect(this, SIGNAL(remoteObjectAdded(QRemoteObjectSourceLocation)), d->registry, SLOT(addSource(QRemoteObjectSourceLocation)));
    QObject::connect(this, SIGNAL(remoteObjectRemoved(QRemoteObjectSourceLocation)), d->registry, SLOT(removeSource(QRemoteObjectSourceLocation)));
    return true;
}

void QRemoteObjectNodePrivate::setRegistry(QRemoteObjectRegistry *reg)
{
    Q_Q(QRemoteObjectNode);
    registry = reg;
    reg->setParent(q);
    //Make sure when we get the registry initialized, we update our replicas
    QObject::connect(reg, SIGNAL(initialized()), q, SLOT(onRegistryInitialized()));
    //Make sure we handle new RemoteObjectSources on Registry...
    QObject::connect(reg, SIGNAL(remoteObjectAdded(QRemoteObjectSourceLocation)), q, SLOT(onRemoteObjectSourceAdded(QRemoteObjectSourceLocation)));
    QObject::connect(reg, SIGNAL(remoteObjectRemoved(QRemoteObjectSourceLocation)), q, SLOT(onRemoteObjectSourceRemoved(QRemoteObjectSourceLocation)));
}

/*!
    Blocks until this Node's \l Registry is initialized or \a timeout (in
    milliseconds) expires. Returns \c true if the \l Registry is successfully
    initialized upon return, or \c false otherwise.
*/
bool QRemoteObjectNode::waitForRegistry(int timeout)
{
    Q_D(QRemoteObjectNode);
    if (!d->registry) {
        qCWarning(QT_REMOTEOBJECT) << qPrintable(objectName()) << "waitForRegistry() error: No valid registry url set";
        return false;
    }

    return d->registry->waitForSource(timeout);
}

/*!
    Connects a client node to the host node at \a address.

    Connections will remain valid until the host node is deleted or no longer
    accessible over a network.

    Once a client is connected to a host, valid Replicas can then be acquired
    if the corresponding Source is being remoted.

    Return \c true on success, \c false otherwise (usually an unrecognized url,
    or connecting to already connected address).
*/
bool QRemoteObjectNode::connectToNode(const QUrl &address)
{
    Q_D(QRemoteObjectNode);
    if (!d->initConnection(address)) {
        d->setLastError(RegistryNotAcquired);
        return false;
    }
    return true;
}

/*!
    \fn void QRemoteObjectNode::remoteObjectAdded(const QRemoteObjectSourceLocation &loc)

    This signal is emitted whenever a new \l {Source} object is added to
    the Registry. The signal will not be emitted if there is no Registry set
    (i.e., Sources over connections made via connectToNode directly). The \a
    loc parameter contains the information about the added Source, including
    name, type and the QUrl of the hosting Node.

    \sa remoteObjectRemoved, instances
*/

/*!
    \fn void remoteObjectRemoved(const QRemoteObjectSourceLocation &loc)

    This signal is emitted whenever there is a known \l {Source} object is
    removed from the Registry. The signal will not be emitted if there is no
    Registry set (i.e., Sources over connections made via connectToNode
    directly). The \a loc parameter contains the information about the removed
    Source, including name, type and the QUrl of the hosting Node.

    \sa remoteObjectAdded, instances
*/

/*!
    \fn QStringList QRemoteObjectNode::instances() const

    This templated function (taking a \l repc generated type as the template parameter) will
    return the list of names of every instance of that type on the Remote
    Objects network. For example, if you have a Shape class defined in a .rep file,
    and Circle and Square classes inherit from the Source definition, they can
    be shared on the Remote Objects network using \l {QRemoteObjectHostBase::enableRemoting} {enableRemoting}.
    \code
        Square square;
        Circle circle;
        myHost.enableRemoting(&square, "Square");
        myHost.enableRemoting(&circle, "Circle");
    \endcode
    Then instance can be used to find the available instances of Shape.
    \code
        QStringList instances = clientNode.instances<Shape>();
        // will return a QStringList containing "Circle" and "Square"
        auto instance1 = clientNode.acquire<Shape>("Circle");
        auto instance2 = clientNode.acquire<Shape>("Square");
        ...
    \endcode
*/

/*!
    \overload instances()

    This convenience function provides the same result as the templated
    version, but takes the name of the \l {Source} class as a parameter (\a
    typeName) rather than deriving it from the class type.
*/
QStringList QRemoteObjectNode::instances(const QString &typeName) const
{
    Q_D(const QRemoteObjectNode);
    QStringList names;
    for (auto it = d->connectedSources.cbegin(), end = d->connectedSources.cend(); it != end; ++it) {
        if (it.value().typeName == typeName) {
            names << it.key();
        }
    }
    return names;
}

/*!
    \keyword dynamic acquire
    Returns a QRemoteObjectDynamicReplica of the Source \a name.
*/
QRemoteObjectDynamicReplica *QRemoteObjectNode::acquireDynamic(const QString &name)
{
    return new QRemoteObjectDynamicReplica(this, name);
}

/*!
    Enables a host node to dynamically provide remote access to the QObject \a
    object. Client nodes connected to the node
    hosting this object may obtain Replicas of this Source.

    The \a name defines the lookup-name under which the QObject can be acquired
    using \l QRemoteObjectNode::acquire() . If not explicitly set then the name
    given in the QCLASSINFO_REMOTEOBJECT_TYPE will be used. If no such macro
    was defined for the QObject then the \l QObject::objectName() is used.

    Returns \c false if the current node is a client node, or if the QObject is already
    registered to be remoted, and \c true if remoting is successfully enabled
    for the dynamic QObject.

    \sa disableRemoting()
*/
bool QRemoteObjectHostBase::enableRemoting(QObject *object, const QString &name)
{
    Q_D(QRemoteObjectHostBase);
    if (!d->remoteObjectIo) {
        d->setLastError(OperationNotValidOnClientNode);
        return false;
    }

    const QMetaObject *meta = object->metaObject();
    QString _name = name;
    QString typeName;
    const int ind = meta->indexOfClassInfo(QCLASSINFO_REMOTEOBJECT_TYPE);
    if (ind != -1) { //We have an object created from repc or at least with QCLASSINFO defined
        typeName = QString::fromLatin1(meta->classInfo(ind).value());
        if (_name.isEmpty())
            _name = typeName;
        while (true) {
            Q_ASSERT(meta->superClass()); //This recurses to QObject, which doesn't have QCLASSINFO_REMOTEOBJECT_TYPE
            if (ind != meta->superClass()->indexOfClassInfo(QCLASSINFO_REMOTEOBJECT_TYPE)) //At the point we don't find the same QCLASSINFO_REMOTEOBJECT_TYPE,
                            //we have the metaobject we should work from
                break;
            meta = meta->superClass();
        }
    } else { //This is a passed in QObject, use its API
        if (_name.isEmpty()) {
            _name = object->objectName();
            if (_name.isEmpty()) {
                d->setLastError(MissingObjectName);
                qCWarning(QT_REMOTEOBJECT) << qPrintable(objectName()) << "enableRemoting() Error: Unable to Replicate an object that does not have objectName() set.";
                return false;
            }
        }
    }
    return d->remoteObjectIo->enableRemoting(object, meta, _name, typeName);
}

/*!
    This overload of enableRemoting() is specific to \l QAbstractItemModel types
    (or any type derived from \l QAbstractItemModel). This is useful if you want
    to have a model and the HMI for the model in different processes.

    The three required parameters are the \a model itself, the \a name by which
    to lookup the model, and the \a roles that should be exposed on the Replica
    side. If you want to synchronize selection between \l Source and \l
    Replica, the optional \a selectionModel parameter can be used. This is only
    recommended when using a single Replica.

    Behind the scenes, Qt Remote Objects batches data() lookups and prefetches
    data when possible to make the model interaction as responsive as possible.

    Returns \c false if the current node is a client node, or if the QObject is already
    registered to be remoted, and \c true if remoting is successfully enabled
    for the QAbstractItemModel.

    \sa disableRemoting()
 */
bool QRemoteObjectHostBase::enableRemoting(QAbstractItemModel *model, const QString &name, const QVector<int> roles, QItemSelectionModel *selectionModel)
{
    //This looks complicated, but hopefully there is a way to have an adapter be a template
    //parameter and this makes sure that is supported.
    QObject *adapter = QAbstractItemModelSourceAdapter::staticMetaObject.newInstance(Q_ARG(QAbstractItemModel*, model),
                                                                                     Q_ARG(QItemSelectionModel*, selectionModel),
                                                                                     Q_ARG(QVector<int>, roles));
    QAbstractItemAdapterSourceAPI<QAbstractItemModel, QAbstractItemModelSourceAdapter> *api =
        new QAbstractItemAdapterSourceAPI<QAbstractItemModel, QAbstractItemModelSourceAdapter>(name);
    return enableRemoting(model, api, adapter);
}

/*!
    \fn bool QRemoteObjectHostBase::enableRemoting(ObjectType *object)

    This templated function overload enables a host node to provide remote
    access to a QObject \a object with a specified (and compile-time checked)
    interface. Client nodes connected to the node hosting this object may
    obtain Replicas of this Source.

    This is best illustrated by example:
    \snippet doc_src_remoteobjects.cpp api_source

    Here the MinuteTimerSourceAPI is the set of Signals/Slots/Properties
    defined by the TimeModel.rep file. Compile time checks are made to verify
    the input QObject can expose the requested API, it will fail to compile
    otherwise. This allows a subset of \a object 's interface to be exposed,
    and allows the types of conversions supported by Signal/Slot connections.

    Returns \c false if the current node is a client node, or if the QObject is
    already registered to be remoted, and \c true if remoting is successfully
    enabled for the QObject.

    \sa disableRemoting()
*/

/*!
    \internal Enables a host node to provide remote access to a QObject \a
    object with the API defined by \a api. Client nodes connected to the node
    hosting this object may obtain Replicas of this Source.

    Returns \c false if the current node is a client node, or if the QObject is
    already registered to be remoted, and \c true if remoting is successfully
    enabled for the QObject.

    \sa disableRemoting()
*/
bool QRemoteObjectHostBase::enableRemoting(QObject *object, const SourceApiMap *api, QObject *adapter)
{
    Q_D(QRemoteObjectHostBase);
    api->modelSetup(this);
    return d->remoteObjectIo->enableRemoting(object, api, adapter);
}

/*!
    Disables remote access for the QObject \a remoteObject. Returns \c false if
    the current node is a client node or if the \a remoteObject is not
    registered, and returns \c true if remoting is successfully disabled for
    the Source object.

    \warning Replicas of this object will no longer be valid after calling this method.

    \sa enableRemoting()
*/
bool QRemoteObjectHostBase::disableRemoting(QObject *remoteObject)
{
    Q_D(QRemoteObjectHostBase);
    if (!d->remoteObjectIo) {
        d->setLastError(OperationNotValidOnClientNode);
        return false;
    }

    if (!d->remoteObjectIo->disableRemoting(remoteObject)) {
        d->setLastError(SourceNotRegistered);
        return false;
    }

    return true;
}

/*!
 Returns a pointer to a Replica which is specifically derived from \l
 QAbstractItemModel. The \a name provided must match the name used with the
 matching \l {QRemoteObjectHostBase::enableRemoting} {enableRemoting} that put
 the Model on the network. The returned \c model will be empty until it is
 initialized with the \l Source.
 */
QAbstractItemModelReplica *QRemoteObjectNode::acquireModel(const QString &name)
{
    QAbstractItemModelReplicaPrivate *rep = acquire<QAbstractItemModelReplicaPrivate>(name);
    return new QAbstractItemModelReplica(rep);
}

QRemoteObjectHostBasePrivate::QRemoteObjectHostBasePrivate()
    : QRemoteObjectNodePrivate()
    , remoteObjectIo(nullptr)
{ }

QRemoteObjectHostPrivate::QRemoteObjectHostPrivate()
    : QRemoteObjectHostBasePrivate()
{ }

QRemoteObjectRegistryHostPrivate::QRemoteObjectRegistryHostPrivate()
    : QRemoteObjectHostBasePrivate()
    , registrySource(nullptr)
{ }

QT_END_NAMESPACE

#include "moc_qremoteobjectnode.cpp"
