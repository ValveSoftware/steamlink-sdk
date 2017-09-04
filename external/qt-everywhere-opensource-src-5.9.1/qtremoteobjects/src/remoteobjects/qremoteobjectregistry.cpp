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

#include "qremoteobjectregistry.h"
#include "qremoteobjectreplica_p.h"

#include <QSet>
#include <QDataStream>


QT_BEGIN_NAMESPACE

/*!
    \class QRemoteObjectRegistry
    \inmodule QtRemoteObjects
    \brief A class holding information about \l {Source} objects available on the Qt Remote Objects network

    The Registry is a special Source/Replica pair held by a \l
    {QRemoteObjectNode} {node} itself. It knows about all other \l {Source}s
    available on the network, and simplifies the process of connecting to other
    \l {QRemoteObjectNode} {node}s.
*/
QRemoteObjectRegistry::QRemoteObjectRegistry() : QRemoteObjectReplica()
{
    connect(this, &QRemoteObjectRegistry::stateChanged, this, &QRemoteObjectRegistry::pushToRegistryIfNeeded);
}

QRemoteObjectRegistry::QRemoteObjectRegistry(QRemoteObjectNode *node, const QString &name)
    : QRemoteObjectReplica(ConstructWithNode)
{
    connect(this, &QRemoteObjectRegistry::stateChanged, this, &QRemoteObjectRegistry::pushToRegistryIfNeeded);
    initializeNode(node, name);
}

/*!
    \fn void QRemoteObjectRegistry::remoteObjectAdded(const QRemoteObjectSourceLocation &entry)

    This signal is emitted whenever a new source location is added to the registry.

    \a entry is a QRemoteObjectSourceLocation, a typedef for QPair<QString, QUrl>.

    \sa remoteObjectRemoved()
*/

/*!
    \fn void QRemoteObjectRegistry::remoteObjectRemoved(const QRemoteObjectSourceLocation &entry)

    This signal is emitted whenever a Source location is removed from the Registry.

    \a entry is a QRemoteObjectSourceLocation, a typedef for QPair<QString, QUrl>.

    \sa remoteObjectAdded()
*/

/*!
    \property QRemoteObjectRegistry::sourceLocations
    \brief The set of sources known to the registry.

    This property is a QRemoteObjectSourceLocations, which is a typedef for QHash<QString, QUrl>.  Each known \l Source is the QString key, while the url for the host node is the corresponding value for that key in the hash.
*/

/*!
    Destructor for QRemoteObjectRegistry.
*/
QRemoteObjectRegistry::~QRemoteObjectRegistry()
{}

void QRemoteObjectRegistry::registerMetatypes()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;
    qRegisterMetaType<QRemoteObjectSourceLocation>();
    qRegisterMetaTypeStreamOperators<QRemoteObjectSourceLocation>();
    qRegisterMetaType<QRemoteObjectSourceLocations>();
    qRegisterMetaTypeStreamOperators<QRemoteObjectSourceLocations>();
}

void QRemoteObjectRegistry::initialize()
{
    QRemoteObjectRegistry::registerMetatypes();
    QVariantList properties;
    properties.reserve(3);
    properties << QVariant::fromValue(QRemoteObjectSourceLocations());
    properties << QVariant::fromValue(QRemoteObjectSourceLocation());
    properties << QVariant::fromValue(QRemoteObjectSourceLocation());
    setProperties(properties);
}

/*!
    Returns a QRemoteObjectSourceLocations object, which includes the name
    and additional information of all sources known to the registry.
*/
QRemoteObjectSourceLocations QRemoteObjectRegistry::sourceLocations() const
{
    return propAsVariant(0).value<QRemoteObjectSourceLocations>();
}

/*!
    \internal
*/
void QRemoteObjectRegistry::addSource(const QRemoteObjectSourceLocation &entry)
{
    if (hostedSources.contains(entry.first)) {
        qCWarning(QT_REMOTEOBJECT) << "Node warning: ignoring source" << entry.first
                                   << "as this node already has a source by that name.";
        return;
    }
    hostedSources.insert(entry.first, entry.second);
    if (state() != QRemoteObjectReplica::State::Valid)
        return;

    if (sourceLocations().contains(entry.first)) {
        qCWarning(QT_REMOTEOBJECT) << "Node warning: ignoring source" << entry.first
                                   << "as another source (" << sourceLocations()[entry.first]
                                   << ") has already registered that name.";
        return;
    }
    qCDebug(QT_REMOTEOBJECT) << "An entry was added to the registry - Sending to source" << entry.first << entry.second;
    // This does not set any data to avoid a coherency problem between client and server
    static int index = QRemoteObjectRegistry::staticMetaObject.indexOfMethod("addSource(QRemoteObjectSourceLocation)");
    QVariantList args;
    args << QVariant::fromValue(entry);
    send(QMetaObject::InvokeMetaMethod, index, args);
}

/*!
    \internal
*/
void QRemoteObjectRegistry::removeSource(const QRemoteObjectSourceLocation &entry)
{
    if (!hostedSources.contains(entry.first))
        return;
    hostedSources.remove(entry.first);
    if (state() != QRemoteObjectReplica::State::Valid)
        return;

    qCDebug(QT_REMOTEOBJECT) << "An entry was removed from the registry - Sending to source" << entry.first << entry.second;
    // This does not set any data to avoid a coherency problem between client and server
    static int index = QRemoteObjectRegistry::staticMetaObject.indexOfMethod("removeSource(QRemoteObjectSourceLocation)");
    QVariantList args;
    args << QVariant::fromValue(entry);
    send(QMetaObject::InvokeMetaMethod, index, args);
}

/*!
    \internal
    This internal function supports the edge case where the \l Registry
    is connected after \l Source objects are added to this \l Node, or
    the connection to the \l Registry is lost. When connected/reconnected, this
    function synchronizes local \l Source objects with the \l Registry.
*/
void QRemoteObjectRegistry::pushToRegistryIfNeeded()
{
    if (state() != QRemoteObjectReplica::State::Valid)
        return;
    const QSet<QString> myLocs = QSet<QString>::fromList(hostedSources.keys());
    if (myLocs.empty())
        return;
    const QSet<QString> registryLocs = QSet<QString>::fromList(sourceLocations().keys());
    foreach (const QString &loc, myLocs & registryLocs) {
        qCWarning(QT_REMOTEOBJECT) << "Node warning: Ignoring Source" << loc << "as another source ("
                                   << sourceLocations()[loc] << ") has already registered that name.";
        hostedSources.remove(loc);
        return;
    }
    //Sources that need to be pushed to the registry...
    foreach (const QString &loc, myLocs - registryLocs) {
        static int index = QRemoteObjectRegistry::staticMetaObject.indexOfMethod("addSource(QRemoteObjectSourceLocation)");
        QVariantList args;
        args << QVariant::fromValue(QRemoteObjectSourceLocation(loc, hostedSources[loc]));
        send(QMetaObject::InvokeMetaMethod, index, args);
    }
}

QT_END_NAMESPACE
