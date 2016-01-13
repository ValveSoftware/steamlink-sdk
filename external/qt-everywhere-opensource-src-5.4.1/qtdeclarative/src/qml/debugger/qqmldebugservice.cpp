/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmldebugservice_p.h"
#include "qqmldebugservice_p_p.h"
#include "qqmldebugserver_p.h"
#include <private/qqmldata_p.h>
#include <private/qqmlcontext_p.h>

#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtCore/QFileInfo>

QT_BEGIN_NAMESPACE

QQmlDebugServicePrivate::QQmlDebugServicePrivate()
{
}

QQmlDebugService::QQmlDebugService(const QString &name, float version, QObject *parent)
    : QObject(*(new QQmlDebugServicePrivate), parent)
{
    QQmlDebugServer::instance(); // create it when it isn't there yet.

    Q_D(QQmlDebugService);
    d->name = name;
    d->version = version;
    d->state = QQmlDebugService::NotConnected;
}

QQmlDebugService::QQmlDebugService(QQmlDebugServicePrivate &dd,
                                                   const QString &name, float version, QObject *parent)
    : QObject(dd, parent)
{
    Q_D(QQmlDebugService);
    d->name = name;
    d->version = version;
    d->state = QQmlDebugService::NotConnected;
}

/**
  Registers the service. This should be called in the constructor of the inherited class. From
  then on the service might get asynchronous calls to messageReceived().
  */
QQmlDebugService::State QQmlDebugService::registerService()
{
    Q_D(QQmlDebugService);
    QQmlDebugServer *server = QQmlDebugServer::instance();

    if (!server)
        return NotConnected;

    if (server->serviceNames().contains(d->name)) {
        qWarning() << "QQmlDebugService: Conflicting plugin name" << d->name;
    } else {
        server->addService(this);
    }
    return state();
}

QQmlDebugService::~QQmlDebugService()
{
    if (QQmlDebugServer *inst = QQmlDebugServer::instance())
        inst->removeService(this);
}

QString QQmlDebugService::name() const
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

namespace {

struct ObjectReference
{
    QPointer<QObject> object;
    int id;
};

struct ObjectReferenceHash
{
    ObjectReferenceHash() : nextId(0) {}

    QHash<QObject *, ObjectReference> objects;
    QHash<int, QObject *> ids;

    int nextId;
};

}
Q_GLOBAL_STATIC(ObjectReferenceHash, objectReferenceHash)


/*!
    Returns a unique id for \a object.  Calling this method multiple times
    for the same object will return the same id.
*/
int QQmlDebugService::idForObject(QObject *object)
{
    if (!object)
        return -1;

    ObjectReferenceHash *hash = objectReferenceHash();
    QHash<QObject *, ObjectReference>::Iterator iter =
            hash->objects.find(object);

    if (iter == hash->objects.end()) {
        int id = hash->nextId++;

        hash->ids.insert(id, object);
        iter = hash->objects.insert(object, ObjectReference());
        iter->object = object;
        iter->id = id;
    } else if (iter->object != object) {
        int id = hash->nextId++;

        hash->ids.remove(iter->id);

        hash->ids.insert(id, object);
        iter->object = object;
        iter->id = id;
    }
    return iter->id;
}

/*!
    Returns the object for unique \a id.  If the object has not previously been
    assigned an id, through idForObject(), then 0 is returned.  If the object
    has been destroyed, 0 is returned.
*/
QObject *QQmlDebugService::objectForId(int id)
{
    ObjectReferenceHash *hash = objectReferenceHash();

    QHash<int, QObject *>::Iterator iter = hash->ids.find(id);
    if (iter == hash->ids.end())
        return 0;


    QHash<QObject *, ObjectReference>::Iterator objIter =
            hash->objects.find(*iter);
    Q_ASSERT(objIter != hash->objects.end());

    if (objIter->object == 0) {
        hash->ids.erase(iter);
        hash->objects.erase(objIter);
        // run a loop to remove other invalid objects
        removeInvalidObjectsFromHash();
        return 0;
    } else {
        return *iter;
    }
}

/*!
    Returns a list of objects matching the given filename, line and column.
*/
QList<QObject*> QQmlDebugService::objectForLocationInfo(const QString &filename,
                                                 int lineNumber, int columnNumber)
{
    ObjectReferenceHash *hash = objectReferenceHash();
    QList<QObject*> objects;
    QHash<int, QObject *>::Iterator iter = hash->ids.begin();
    while (iter != hash->ids.end()) {
        QHash<QObject *, ObjectReference>::Iterator objIter =
                hash->objects.find(*iter);
        Q_ASSERT(objIter != hash->objects.end());

        if (objIter->object == 0) {
            iter = hash->ids.erase(iter);
            hash->objects.erase(objIter);
        } else {
            QQmlData *ddata = QQmlData::get(iter.value());
            if (ddata && ddata->outerContext) {
                if (QFileInfo(ddata->outerContext->urlString).fileName() == filename &&
                    ddata->lineNumber == lineNumber &&
                    ddata->columnNumber >= columnNumber) {
                    objects << *iter;
                }
            }
            ++iter;
        }
    }
    return objects;
}

void QQmlDebugService::removeInvalidObjectsFromHash()
{
    ObjectReferenceHash *hash = objectReferenceHash();
    QHash<int, QObject *>::Iterator iter = hash->ids.begin();
    while (iter != hash->ids.end()) {
        QHash<QObject *, ObjectReference>::Iterator objIter =
                hash->objects.find(*iter);
        Q_ASSERT(objIter != hash->objects.end());

        if (objIter->object == 0) {
            iter = hash->ids.erase(iter);
            hash->objects.erase(objIter);
        } else {
            ++iter;
        }
    }
}

void QQmlDebugService::clearObjectsFromHash()
{
    ObjectReferenceHash *hash = objectReferenceHash();
    hash->ids.clear();
    hash->objects.clear();
}

bool QQmlDebugService::isDebuggingEnabled()
{
    return QQmlDebugServer::instance() != 0;
}

bool QQmlDebugService::hasDebuggingClient()
{
    return QQmlDebugServer::instance() != 0
            && QQmlDebugServer::instance()->hasDebuggingClient();
}

bool QQmlDebugService::blockingMode()
{
    return QQmlDebugServer::instance() != 0
            && QQmlDebugServer::instance()->blockingMode();
}

QString QQmlDebugService::objectToString(QObject *obj)
{
    if(!obj)
        return QStringLiteral("NULL");

    QString objectName = obj->objectName();
    if(objectName.isEmpty())
        objectName = QStringLiteral("<unnamed>");

    QString rv = QString::fromUtf8(obj->metaObject()->className()) +
            QLatin1String(": ") + objectName;

    return rv;
}

void QQmlDebugService::sendMessage(const QByteArray &message)
{
    sendMessages(QList<QByteArray>() << message);
}

void QQmlDebugService::sendMessages(const QList<QByteArray> &messages)
{
    if (state() != Enabled)
        return;

    if (QQmlDebugServer *inst = QQmlDebugServer::instance())
        inst->sendMessages(this, messages);
}

void QQmlDebugService::stateAboutToBeChanged(State)
{
}

void QQmlDebugService::stateChanged(State)
{
}

void QQmlDebugService::messageReceived(const QByteArray &)
{
}

void QQmlDebugService::engineAboutToBeAdded(QQmlEngine *engine)
{
    emit attachedToEngine(engine);
}

void QQmlDebugService::engineAboutToBeRemoved(QQmlEngine *engine)
{
    emit detachedFromEngine(engine);
}

void QQmlDebugService::engineAdded(QQmlEngine *)
{
}

void QQmlDebugService::engineRemoved(QQmlEngine *)
{
}

QQmlDebugStream::QQmlDebugStream()
    : QDataStream()
{
    setVersion(QQmlDebugServer::s_dataStreamVersion);
}

QQmlDebugStream::QQmlDebugStream(QIODevice *d)
    : QDataStream(d)
{
    setVersion(QQmlDebugServer::s_dataStreamVersion);
}

QQmlDebugStream::QQmlDebugStream(QByteArray *ba, QIODevice::OpenMode flags)
    : QDataStream(ba, flags)
{
    setVersion(QQmlDebugServer::s_dataStreamVersion);
}

QQmlDebugStream::QQmlDebugStream(const QByteArray &ba)
    : QDataStream(ba)
{
    setVersion(QQmlDebugServer::s_dataStreamVersion);
}

QT_END_NAMESPACE
