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

#include "qpostman_p.h"
#include <private/qobject_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qlockableobserverinterface_p.h>
#include <Qt3DCore/qnode.h>
#include <Qt3DCore/private/qnode_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

class QPostmanPrivate : public QObjectPrivate
{
public:
    QPostmanPrivate()
        : QObjectPrivate()
        , m_scene(nullptr)
    {
    }

    Q_DECLARE_PUBLIC(QPostman)
    QScene *m_scene;
    std::vector<QSceneChangePtr> m_batch;
};

QPostman::QPostman(QObject *parent)
    : QObject(*new QPostmanPrivate, parent)
{
    qRegisterMetaType<QSceneChangePtr >("QSceneChangePtr");
}

QPostman::~QPostman()
{
}

void QPostman::setScene(QScene *scene)
{
    Q_D(QPostman);
    d->m_scene = scene;
}

static inline QMetaMethod notifyFrontendNodeMethod()
{
    int idx = QPostman::staticMetaObject.indexOfMethod("notifyFrontendNode(QSceneChangePtr)");
    Q_ASSERT(idx != -1);
    return QPostman::staticMetaObject.method(idx);
}

void QPostman::sceneChangeEvent(const QSceneChangePtr &e)
{
    static const QMetaMethod notifyFrontendNode = notifyFrontendNodeMethod();
    notifyFrontendNode.invoke(this, Q_ARG(QSceneChangePtr, e));
}

static inline QMetaMethod submitChangeBatchMethod()
{
    int idx = QPostman::staticMetaObject.indexOfMethod("submitChangeBatch()");
    Q_ASSERT(idx != -1);
    return QPostman::staticMetaObject.method(idx);
}

/*!
 * This will start or append \a change to a batch of changes from frontend
 * nodes. Once the batch is complete, when the event loop returns, the batch is
 * sent to the QChangeArbiter to notify the backend aspects.
 */
void QPostman::notifyBackend(const QSceneChangePtr &change)
{
    // If batch in progress
    // add change
    // otherwise start batch
    // by calling a queued slot
    Q_D(QPostman);
    if (d->m_batch.empty()) {
        static const QMetaMethod submitChangeBatch = submitChangeBatchMethod();
        submitChangeBatch.invoke(this, Qt::QueuedConnection);
    }
    d->m_batch.push_back(change);
}

void QPostman::notifyFrontendNode(const QSceneChangePtr &e)
{
    Q_D(QPostman);
    if (!e.isNull() && d->m_scene != nullptr) {
        QNode *n = d->m_scene->lookupNode(e->subjectId());
        if (n != nullptr)
            n->sceneChangeEvent(e);
    }
}

void QPostman::submitChangeBatch()
{
    Q_D(QPostman);
    QLockableObserverInterface *arbiter = nullptr;
    if (d->m_scene && (arbiter = d->m_scene->arbiter()) != nullptr) {
        arbiter->sceneChangeEventWithLock(d->m_batch);
        d->m_batch.clear();
    }
}

} //Qt3D

QT_END_NAMESPACE
