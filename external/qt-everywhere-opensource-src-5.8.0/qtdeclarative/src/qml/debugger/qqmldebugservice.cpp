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

#include "qqmldebugservice_p.h"
#include "qqmldebugconnector_p.h"
#include <private/qqmldata_p.h>
#include <private/qqmlcontext_p.h>

#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtCore/QFileInfo>

QT_BEGIN_NAMESPACE

class QQmlDebugServer;

class QQmlDebugServicePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQmlDebugService)
public:
    QQmlDebugServicePrivate(const QString &name, float version);

    const QString name;
    const float version;
    QQmlDebugService::State state;
};

QQmlDebugServicePrivate::QQmlDebugServicePrivate(const QString &name, float version) :
    name(name), version(version), state(QQmlDebugService::NotConnected)
{
}

QQmlDebugService::QQmlDebugService(const QString &name, float version, QObject *parent)
    : QObject(*(new QQmlDebugServicePrivate(name, version)), parent)
{
    Q_D(QQmlDebugService);
    QQmlDebugConnector *server = QQmlDebugConnector::instance();

    if (!server)
        return;

    if (server->service(d->name)) {
        qWarning() << "QQmlDebugService: Conflicting plugin name" << d->name;
    } else {
        server->addService(d->name, this);
    }
}

QQmlDebugService::~QQmlDebugService()
{
    Q_D(QQmlDebugService);
    QQmlDebugConnector *server = QQmlDebugConnector::instance();

    if (!server)
        return;

    if (server->service(d->name) != this)
        qWarning() << "QQmlDebugService: Plugin" << d->name << "is not registered.";
    else
        server->removeService(d->name);
}

const QString &QQmlDebugService::name() const
{
    Q_D(const QQmlDebugService);
    return d->name;
}

float QQmlDebugService::version() const
{
    Q_D(const QQmlDebugService);
    return d->version;
}

QQmlDebugService::State QQmlDebugService::state() const
{
    Q_D(const QQmlDebugService);
    return d->state;
}

void QQmlDebugService::setState(QQmlDebugService::State newState)
{
    Q_D(QQmlDebugService);
    d->state = newState;
}

namespace {
class ObjectReferenceHash : public QObject
{
    Q_OBJECT
public:
    ObjectReferenceHash() : nextId(0) {}

    QHash<QObject *, int> objects;
    QHash<int, QObject *> ids;

    int nextId;

    void remove(QObject *obj);
};
}
Q_GLOBAL_STATIC(ObjectReferenceHash, objectReferenceHash)

void ObjectReferenceHash::remove(QObject *obj)
{
    QHash<QObject *, int>::Iterator iter = objects.find(obj);
    if (iter != objects.end()) {
        ids.remove(iter.value());
        objects.erase(iter);
    }
}

/*!
    Returns a unique id for \a object.  Calling this method multiple times
    for the same object will return the same id.
*/
int QQmlDebugService::idForObject(QObject *object)
{
    if (!object)
        return -1;

    ObjectReferenceHash *hash = objectReferenceHash();
    QHash<QObject *, int>::Iterator iter = hash->objects.find(object);

    if (iter == hash->objects.end()) {
        int id = hash->nextId++;
        hash->ids.insert(id, object);
        iter = hash->objects.insert(object, id);
        connect(object, &QObject::destroyed, hash, &ObjectReferenceHash::remove);
    }
    return iter.value();
}

/*!
    Returns the mapping of objects to unique \a ids, created through calls to idForObject().
*/
const QHash<int, QObject *> &QQmlDebugService::objectsForIds()
{
    return objectReferenceHash()->ids;
}

QT_END_NAMESPACE

#include "qqmldebugservice.moc"
