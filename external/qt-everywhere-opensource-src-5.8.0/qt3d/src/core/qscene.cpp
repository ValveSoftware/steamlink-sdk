/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qscene_p.h"
#include <QHash>
#include <QReadLocker>
#include <Qt3DCore/qnode.h>
#include <Qt3DCore/private/qlockableobserverinterface_p.h>
#include <Qt3DCore/private/qobservableinterface_p.h>
#include <Qt3DCore/private/qnode_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

class QScenePrivate
{
public:
    QScenePrivate(QAspectEngine *engine)
        : m_engine(engine)
        , m_arbiter(nullptr)
    {
    }

    QAspectEngine *m_engine;
    QHash<QNodeId, QNode *> m_nodeLookupTable;
    QMultiHash<QNodeId, QNodeId> m_componentToEntities;
    QMultiHash<QNodeId, QObservableInterface *> m_observablesLookupTable;
    QHash<QObservableInterface *, QNodeId> m_observableToUuid;
    QLockableObserverInterface *m_arbiter;
    mutable QReadWriteLock m_lock;
};


QScene::QScene(QAspectEngine *engine)
    : d_ptr(new QScenePrivate(engine))
{
}

QScene::~QScene()
{
}

QAspectEngine *QScene::engine() const
{
    Q_D(const QScene);
    return d->m_engine;
}

// Called by any thread
void QScene::addObservable(QObservableInterface *observable, QNodeId id)
{
    Q_D(QScene);
    QWriteLocker lock(&d->m_lock);
    d->m_observablesLookupTable.insert(id, observable);
    d->m_observableToUuid.insert(observable, id);
    if (d->m_arbiter != nullptr)
        observable->setArbiter(d->m_arbiter);
}

// Called by main thread only
void QScene::addObservable(QNode *observable)
{
    Q_D(QScene);
    if (observable != nullptr) {
        QWriteLocker lock(&d->m_lock);
        d->m_nodeLookupTable.insert(observable->id(), observable);
        if (d->m_arbiter != nullptr)
            observable->d_func()->setArbiter(d->m_arbiter);
    }
}

// Called by any thread
void QScene::removeObservable(QObservableInterface *observable, QNodeId id)
{
    Q_D(QScene);
    QWriteLocker lock(&d->m_lock);
    d->m_observablesLookupTable.remove(id, observable);
    d->m_observableToUuid.remove(observable);
    observable->setArbiter(nullptr);
}

// Called by main thread
void QScene::removeObservable(QNode *observable)
{
    Q_D(QScene);
    if (observable != nullptr) {
        QWriteLocker lock(&d->m_lock);
        QNodeId nodeUuid = observable->id();
        const auto p = d->m_observablesLookupTable.equal_range(nodeUuid); // must be non-const equal_range to ensure p.second stays valid
        auto it = p.first;
        while (it != p.second) {
            it.value()->setArbiter(nullptr);
            d->m_observableToUuid.remove(it.value());
            it = d->m_observablesLookupTable.erase(it);
        }
        d->m_nodeLookupTable.remove(nodeUuid);
        observable->d_func()->setArbiter(nullptr);
    }
}

// Called by any thread
QObservableList QScene::lookupObservables(QNodeId id) const
{
    Q_D(const QScene);
    QReadLocker lock(&d->m_lock);
    return d->m_observablesLookupTable.values(id);
}

// Called by any thread
QNode *QScene::lookupNode(QNodeId id) const
{
    Q_D(const QScene);
    QReadLocker lock(&d->m_lock);
    return d->m_nodeLookupTable.value(id);
}

QVector<QNode *> QScene::lookupNodes(const QVector<QNodeId> &ids) const
{
    Q_D(const QScene);
    QReadLocker lock(&d->m_lock);
    QVector<QNode *> nodes(ids.size());
    int index = 0;
    for (QNodeId id : ids)
        nodes[index++] = d->m_nodeLookupTable.value(id);
    return nodes;
}

QNodeId QScene::nodeIdFromObservable(QObservableInterface *observable) const
{
    Q_D(const QScene);
    QReadLocker lock(&d->m_lock);
    return d->m_observableToUuid.value(observable);
}

void QScene::setArbiter(QLockableObserverInterface *arbiter)
{
    Q_D(QScene);
    d->m_arbiter = arbiter;
}

QLockableObserverInterface *QScene::arbiter() const
{
    Q_D(const QScene);
    return d->m_arbiter;
}

QVector<QNodeId> QScene::entitiesForComponent(QNodeId id) const
{
    Q_D(const QScene);
    QReadLocker lock(&d->m_lock);
    QVector<QNodeId> result;
    const auto p = d->m_componentToEntities.equal_range(id);
    for (auto it = p.first; it != p.second; ++it)
        result.push_back(*it);
    return result;
}

void QScene::addEntityForComponent(QNodeId componentUuid, QNodeId entityUuid)
{
    Q_D(QScene);
    QWriteLocker lock(&d->m_lock);
    d->m_componentToEntities.insert(componentUuid, entityUuid);
}

void QScene::removeEntityForComponent(QNodeId componentUuid, QNodeId entityUuid)
{
    Q_D(QScene);
    QWriteLocker lock(&d->m_lock);
    d->m_componentToEntities.remove(componentUuid, entityUuid);
}

bool QScene::hasEntityForComponent(QNodeId componentUuid, QNodeId entityUuid)
{
    Q_D(QScene);
    QReadLocker lock(&d->m_lock);
    return d->m_componentToEntities.values(componentUuid).contains(entityUuid);
}

} // Qt3D

QT_END_NAMESPACE
