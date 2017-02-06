/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmllistmodelworkeragent_p.h"
#include "qqmllistmodel_p_p.h"
#include <private/qqmldata_p.h>
#include <private/qqmlengine_p.h>
#include <qqmlinfo.h>

#include <QtCore/qcoreevent.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>


QT_BEGIN_NAMESPACE


void QQmlListModelWorkerAgent::Data::clearChange(int uid)
{
    for (int i=0 ; i < changes.count() ; ++i) {
        if (changes[i].modelUid == uid) {
            changes.removeAt(i);
            --i;
        }
    }
}

void QQmlListModelWorkerAgent::Data::insertChange(int uid, int index, int count)
{
    Change c = { uid, Change::Inserted, index, count, 0, QVector<int>() };
    changes << c;
}

void QQmlListModelWorkerAgent::Data::removeChange(int uid, int index, int count)
{
    Change c = { uid, Change::Removed, index, count, 0, QVector<int>() };
    changes << c;
}

void QQmlListModelWorkerAgent::Data::moveChange(int uid, int index, int count, int to)
{
    Change c = { uid, Change::Moved, index, count, to, QVector<int>() };
    changes << c;
}

void QQmlListModelWorkerAgent::Data::changedChange(int uid, int index, int count, const QVector<int> &roles)
{
    Change c = { uid, Change::Changed, index, count, 0, roles };
    changes << c;
}

QQmlListModelWorkerAgent::QQmlListModelWorkerAgent(QQmlListModel *model)
: m_ref(1), m_orig(model), m_copy(new QQmlListModel(model, this))
{
}

QQmlListModelWorkerAgent::~QQmlListModelWorkerAgent()
{
    mutex.lock();
    syncDone.wakeAll();
    mutex.unlock();
}

void QQmlListModelWorkerAgent::setEngine(QV4::ExecutionEngine *eng)
{
    m_copy->m_engine = eng;
}

void QQmlListModelWorkerAgent::addref()
{
    m_ref.ref();
}

void QQmlListModelWorkerAgent::release()
{
    bool del = !m_ref.deref();

    if (del)
        deleteLater();
}

void QQmlListModelWorkerAgent::modelDestroyed()
{
    m_orig = 0;
}

int QQmlListModelWorkerAgent::count() const
{
    return m_copy->count();
}

void QQmlListModelWorkerAgent::clear()
{
    m_copy->clear();
}

void QQmlListModelWorkerAgent::remove(QQmlV4Function *args)
{
    m_copy->remove(args);
}

void QQmlListModelWorkerAgent::append(QQmlV4Function *args)
{
    m_copy->append(args);
}

void QQmlListModelWorkerAgent::insert(QQmlV4Function *args)
{
    m_copy->insert(args);
}

QQmlV4Handle QQmlListModelWorkerAgent::get(int index) const
{
    return m_copy->get(index);
}

void QQmlListModelWorkerAgent::set(int index, const QQmlV4Handle &value)
{
    m_copy->set(index, value);
}

void QQmlListModelWorkerAgent::setProperty(int index, const QString& property, const QVariant& value)
{
    m_copy->setProperty(index, property, value);
}

void QQmlListModelWorkerAgent::move(int from, int to, int count)
{
    m_copy->move(from, to, count);
}

void QQmlListModelWorkerAgent::sync()
{
    Sync *s = new Sync(data, m_copy);
    data.changes.clear();

    mutex.lock();
    QCoreApplication::postEvent(this, s);
    syncDone.wait(&mutex);
    mutex.unlock();
}

bool QQmlListModelWorkerAgent::event(QEvent *e)
{
    if (e->type() == QEvent::User) {
        bool cc = false;
        QMutexLocker locker(&mutex);
        if (m_orig) {
            Sync *s = static_cast<Sync *>(e);
            const QList<Change> &changes = s->data.changes;

            cc = m_orig->count() != s->list->count();

            QHash<int, QQmlListModel *> targetModelDynamicHash;
            QHash<int, ListModel *> targetModelStaticHash;

            Q_ASSERT(m_orig->m_dynamicRoles == s->list->m_dynamicRoles);
            if (m_orig->m_dynamicRoles)
                QQmlListModel::sync(s->list, m_orig, &targetModelDynamicHash);
            else
                ListModel::sync(s->list->m_listModel, m_orig->m_listModel, &targetModelStaticHash);

            for (int ii = 0; ii < changes.count(); ++ii) {
                const Change &change = changes.at(ii);

                QQmlListModel *model = 0;
                if (m_orig->m_dynamicRoles) {
                    model = targetModelDynamicHash.value(change.modelUid);
                } else {
                    ListModel *lm = targetModelStaticHash.value(change.modelUid);
                    if (lm)
                        model = lm->m_modelCache;
                }

                if (model) {
                    switch (change.type) {
                    case Change::Inserted:
                        model->beginInsertRows(
                                    QModelIndex(), change.index, change.index + change.count - 1);
                        model->endInsertRows();
                        break;
                    case Change::Removed:
                        model->beginRemoveRows(
                                    QModelIndex(), change.index, change.index + change.count - 1);
                        model->endRemoveRows();
                        break;
                    case Change::Moved:
                        model->beginMoveRows(
                                    QModelIndex(),
                                    change.index,
                                    change.index + change.count - 1,
                                    QModelIndex(),
                                    change.to > change.index ? change.to + change.count : change.to);
                        model->endMoveRows();
                        break;
                    case Change::Changed:
                        emit model->dataChanged(
                                    model->createIndex(change.index, 0),
                                    model->createIndex(change.index + change.count - 1, 0),
                                    change.roles);
                        break;
                    }
                }
            }
        }

        syncDone.wakeAll();
        locker.unlock();

        if (cc)
            emit m_orig->countChanged();
        return true;
    }

    return QObject::event(e);
}

QT_END_NAMESPACE

