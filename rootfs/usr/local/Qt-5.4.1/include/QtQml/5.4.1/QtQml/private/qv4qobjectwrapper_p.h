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

#ifndef QV4QOBJECTWRAPPER_P_H
#define QV4QOBJECTWRAPPER_P_H

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

#include <QtCore/qglobal.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qpair.h>
#include <QtCore/qhash.h>
#include <private/qhashedstring_p.h>
#include <private/qqmldata_p.h>
#include <private/qqmlpropertycache_p.h>
#include <private/qintrusivelist_p.h>

#include <private/qv4value_inl_p.h>
#include <private/qv4functionobject_p.h>

QT_BEGIN_NAMESPACE

class QObject;
class QQmlData;
class QQmlPropertyCache;
class QQmlPropertyData;

namespace QV4 {
struct QObjectSlotDispatcher;

struct Q_QML_EXPORT QObjectWrapper : public QV4::Object
{
    struct Data : QV4::Object::Data {
        Data(ExecutionEngine *engine, QObject *object);
        QPointer<QObject> object;
    };
    V4_OBJECT(QV4::Object)

    enum RevisionMode { IgnoreRevision, CheckRevision };

    static void initializeBindings(ExecutionEngine *engine);

    QObject *object() const { return d()->object.data(); }

    ReturnedValue getQmlProperty(ExecutionContext *ctx, QQmlContextData *qmlContext, String *name, RevisionMode revisionMode, bool *hasProperty = 0, bool includeImports = false);
    static ReturnedValue getQmlProperty(ExecutionContext *ctx, QQmlContextData *qmlContext, QObject *object, String *name, RevisionMode revisionMode, bool *hasProperty = 0);

    static bool setQmlProperty(ExecutionContext *ctx, QQmlContextData *qmlContext, QObject *object, String *name, RevisionMode revisionMode, const ValueRef value);

    static ReturnedValue wrap(ExecutionEngine *engine, QObject *object);

    using Object::get;

    static ReturnedValue getProperty(QObject *object, ExecutionContext *ctx, int propertyIndex, bool captureRequired);
    void setProperty(ExecutionContext *ctx, int propertyIndex, const ValueRef value);

protected:
    static bool isEqualTo(Managed *that, Managed *o);

private:
    static ReturnedValue getProperty(QObject *object, ExecutionContext *ctx, QQmlPropertyData *property, bool captureRequired = true);
    static void setProperty(QObject *object, ExecutionContext *ctx, QQmlPropertyData *property, const ValueRef value);

    static ReturnedValue create(ExecutionEngine *engine, QObject *object);

    QQmlPropertyData *findProperty(ExecutionEngine *engine, QQmlContextData *qmlContext, String *name, RevisionMode revisionMode, QQmlPropertyData *local) const;

    static ReturnedValue get(Managed *m, String *name, bool *hasProperty);
    static void put(Managed *m, String *name, const ValueRef value);
    static PropertyAttributes query(const Managed *, String *name);
    static void advanceIterator(Managed *m, ObjectIterator *it, String *&name, uint *index, Property *p, PropertyAttributes *attributes);
    static void markObjects(Managed *that, QV4::ExecutionEngine *e);
    static void destroy(Managed *that);

    static ReturnedValue method_connect(CallContext *ctx);
    static ReturnedValue method_disconnect(CallContext *ctx);
};

struct Q_QML_EXPORT QObjectMethod : public QV4::FunctionObject
{
    struct Data : QV4::FunctionObject::Data {
        Data(QV4::ExecutionContext *scope, QObject *object, int index, const ValueRef qmlGlobal);
        QPointer<QObject> object;
        int index;
        QV4::PersistentValue qmlGlobal;
    };
    V4_OBJECT(QV4::FunctionObject)

    enum { DestroyMethod = -1, ToStringMethod = -2 };

    static ReturnedValue create(QV4::ExecutionContext *scope, QObject *object, int index, const ValueRef qmlGlobal = Primitive::undefinedValue());

    int methodIndex() const { return d()->index; }
    QObject *object() const { return d()->object.data(); }

    QV4::ReturnedValue method_toString(QV4::ExecutionContext *ctx);
    QV4::ReturnedValue method_destroy(QV4::ExecutionContext *ctx, const ValueRef args, int argc);

    static ReturnedValue call(Managed *, CallData *callData);

    ReturnedValue callInternal(CallData *callData);

    static void destroy(Managed *that)
    {
        static_cast<QObjectMethod *>(that)->d()->~Data();
    }
};

struct QmlSignalHandler : public QV4::Object
{
    struct Data : QV4::Object::Data {
        Data(ExecutionEngine *engine, QObject *object, int signalIndex);
        QPointer<QObject> object;
        int signalIndex;
    };
    V4_OBJECT(QV4::Object)


    int signalIndex() const { return d()->signalIndex; }
    QObject *object() const { return d()->object.data(); }

private:

    static void destroy(Managed *that)
    {
        static_cast<QmlSignalHandler *>(that)->d()->~Data();
    }
};

class MultiplyWrappedQObjectMap : public QObject,
                                  private QHash<QObject*, Object*>
{
    Q_OBJECT
public:
    typedef QHash<QObject*, Object*>::ConstIterator ConstIterator;
    typedef QHash<QObject*, Object*>::Iterator Iterator;

    ConstIterator begin() const { return QHash<QObject*, Object*>::constBegin(); }
    Iterator begin() { return QHash<QObject*, Object*>::begin(); }
    ConstIterator end() const { return QHash<QObject*, Object*>::constEnd(); }
    Iterator end() { return QHash<QObject*, Object*>::end(); }

    void insert(QObject *key, Object *value);
    Object *value(QObject *key) const { return QHash<QObject*, Object*>::value(key, 0); }
    Iterator erase(Iterator it);
    void remove(QObject *key);

private Q_SLOTS:
    void removeDestroyedObject(QObject*);
};

}

QT_END_NAMESPACE

#endif // QV4QOBJECTWRAPPER_P_H


