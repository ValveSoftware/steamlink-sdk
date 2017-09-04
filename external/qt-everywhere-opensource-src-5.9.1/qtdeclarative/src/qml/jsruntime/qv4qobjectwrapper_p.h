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

#include <private/qv4value_p.h>
#include <private/qv4functionobject_p.h>

QT_BEGIN_NAMESPACE

class QObject;
class QQmlData;
class QQmlPropertyCache;
class QQmlPropertyData;

namespace QV4 {
struct QObjectSlotDispatcher;

namespace Heap {

struct QQmlValueTypeWrapper;

struct Q_QML_EXPORT QObjectWrapper : Object {
    void init(QObject *object)
    {
        Object::init();
        qObj.init(object);
    }

    void destroy() {
        qObj.destroy();
        Object::destroy();
    }

    QObject *object() const { return qObj.data(); }

private:
    QQmlQPointer<QObject> qObj;
};

struct QObjectMethod : FunctionObject {
    void init(QV4::ExecutionContext *scope);
    void destroy()
    {
        setPropertyCache(nullptr);
        qObj.destroy();
        FunctionObject::destroy();
    }

    QQmlPropertyCache *propertyCache() const { return _propertyCache; }
    void setPropertyCache(QQmlPropertyCache *c) {
        if (c)
            c->addref();
        if (_propertyCache)
            _propertyCache->release();
        _propertyCache = c;
    }

    Pointer<QQmlValueTypeWrapper> valueTypeWrapper;

    const QMetaObject *metaObject();
    QObject *object() const { return qObj.data(); }
    void setObject(QObject *o) { qObj = o; }

private:
    QQmlQPointer<QObject> qObj;
    QQmlPropertyCache *_propertyCache;

public:
    int index;
};

struct QMetaObjectWrapper : FunctionObject {
    const QMetaObject* metaObject;
    QQmlPropertyData *constructors;
    int constructorCount;

    void init(const QMetaObject* metaObject);
    void destroy();
    void ensureConstructorsCache();
};

struct QmlSignalHandler : Object {
    void init(QObject *object, int signalIndex);
    void destroy() {
        qObj.destroy();
        Object::destroy();
    }
    int signalIndex;

    QObject *object() const { return qObj.data(); }
    void setObject(QObject *o) { qObj = o; }

private:
    QQmlQPointer<QObject> qObj;
};

}

struct Q_QML_EXPORT QObjectWrapper : public Object
{
    V4_OBJECT2(QObjectWrapper, Object)
    V4_NEEDS_DESTROY

    enum RevisionMode { IgnoreRevision, CheckRevision };

    static void initializeBindings(ExecutionEngine *engine);

    QObject *object() const { return d()->object(); }

    ReturnedValue getQmlProperty(QQmlContextData *qmlContext, String *name, RevisionMode revisionMode, bool *hasProperty = 0, bool includeImports = false) const;
    static ReturnedValue getQmlProperty(ExecutionEngine *engine, QQmlContextData *qmlContext, QObject *object, String *name, RevisionMode revisionMode, bool *hasProperty = 0);

    static bool setQmlProperty(ExecutionEngine *engine, QQmlContextData *qmlContext, QObject *object, String *name, RevisionMode revisionMode, const Value &value);

    static ReturnedValue wrap(ExecutionEngine *engine, QObject *object);
    static void markWrapper(QObject *object, ExecutionEngine *engine);

    using Object::get;

    static ReturnedValue getProperty(ExecutionEngine *engine, QObject *object, int propertyIndex, bool captureRequired);
    static void setProperty(ExecutionEngine *engine, QObject *object, int propertyIndex, const Value &value);
    void setProperty(ExecutionEngine *engine, int propertyIndex, const Value &value);

    void destroyObject(bool lastCall);

protected:
    static bool isEqualTo(Managed *that, Managed *o);

    static ReturnedValue getProperty(ExecutionEngine *engine, QObject *object, QQmlPropertyData *property, bool captureRequired = true);
    static void setProperty(ExecutionEngine *engine, QObject *object, QQmlPropertyData *property, const Value &value);

    static ReturnedValue create(ExecutionEngine *engine, QObject *object);

    QQmlPropertyData *findProperty(ExecutionEngine *engine, QQmlContextData *qmlContext, String *name, RevisionMode revisionMode, QQmlPropertyData *local) const;

    static ReturnedValue get(const Managed *m, String *name, bool *hasProperty);
    static void put(Managed *m, String *name, const Value &value);
    static PropertyAttributes query(const Managed *, String *name);
    static void advanceIterator(Managed *m, ObjectIterator *it, Value *name, uint *index, Property *p, PropertyAttributes *attributes);
    static void markObjects(Heap::Base *that, QV4::ExecutionEngine *e);

    static void method_connect(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_disconnect(const BuiltinFunction *, Scope &scope, CallData *callData);

private:
    Q_NEVER_INLINE static ReturnedValue wrap_slowPath(ExecutionEngine *engine, QObject *object);
};

inline ReturnedValue QObjectWrapper::wrap(ExecutionEngine *engine, QObject *object)
{
    if (Q_UNLIKELY(QQmlData::wasDeleted(object)))
        return QV4::Encode::null();

    auto ddata = QQmlData::get(object);
    if (Q_LIKELY(ddata && ddata->jsEngineId == engine->m_engineId && !ddata->jsWrapper.isUndefined())) {
        // We own the JS object
        return ddata->jsWrapper.value();
    }

    return wrap_slowPath(engine, object);
}

struct QQmlValueTypeWrapper;

struct Q_QML_EXPORT QObjectMethod : public QV4::FunctionObject
{
    V4_OBJECT2(QObjectMethod, QV4::FunctionObject)
    V4_NEEDS_DESTROY

    enum { DestroyMethod = -1, ToStringMethod = -2 };

    static ReturnedValue create(QV4::ExecutionContext *scope, QObject *object, int index);
    static ReturnedValue create(QV4::ExecutionContext *scope, const QQmlValueTypeWrapper *valueType, int index);

    int methodIndex() const { return d()->index; }
    QObject *object() const { return d()->object(); }

    QV4::ReturnedValue method_toString(QV4::ExecutionContext *ctx) const;
    QV4::ReturnedValue method_destroy(QV4::ExecutionContext *ctx, const Value *args, int argc) const;

    static void call(const Managed *, Scope &scope, CallData *callData);

    void callInternal(CallData *callData, Scope &scope) const;

    static void markObjects(Heap::Base *that, QV4::ExecutionEngine *e);

    static QPair<QObject *, int> extractQtMethod(const QV4::FunctionObject *function);
};


struct Q_QML_EXPORT QMetaObjectWrapper : public QV4::FunctionObject
{
    V4_OBJECT2(QMetaObjectWrapper, QV4::FunctionObject)
    V4_NEEDS_DESTROY

    static ReturnedValue create(ExecutionEngine *engine, const QMetaObject* metaObject);
    static void construct(const Managed *, Scope &scope, CallData *callData);
    static bool isEqualTo(Managed *a, Managed *b);

    const QMetaObject *metaObject() const { return d()->metaObject; }

private:
    void init(ExecutionEngine *engine);
    ReturnedValue constructInternal(CallData *callData) const;
    ReturnedValue callConstructor(const QQmlPropertyData &data, QV4::ExecutionEngine *engine, QV4::CallData *callArgs) const;
    ReturnedValue callOverloadedConstructor(QV4::ExecutionEngine *engine, QV4::CallData *callArgs) const;

};

struct Q_QML_EXPORT QmlSignalHandler : public QV4::Object
{
    V4_OBJECT2(QmlSignalHandler, QV4::Object)
    V4_PROTOTYPE(signalHandlerPrototype)
    V4_NEEDS_DESTROY

    int signalIndex() const { return d()->signalIndex; }
    QObject *object() const { return d()->object(); }

    static void initProto(ExecutionEngine *v4);
};

class MultiplyWrappedQObjectMap : public QObject,
                                  private QHash<QObject*, QV4::WeakValue>
{
    Q_OBJECT
public:
    typedef QHash<QObject*, QV4::WeakValue>::ConstIterator ConstIterator;
    typedef QHash<QObject*, QV4::WeakValue>::Iterator Iterator;

    ConstIterator begin() const { return QHash<QObject*, QV4::WeakValue>::constBegin(); }
    Iterator begin() { return QHash<QObject*, QV4::WeakValue>::begin(); }
    ConstIterator end() const { return QHash<QObject*, QV4::WeakValue>::constEnd(); }
    Iterator end() { return QHash<QObject*, QV4::WeakValue>::end(); }

    void insert(QObject *key, Heap::Object *value);
    ReturnedValue value(QObject *key) const { return QHash<QObject*, QV4::WeakValue>::value(key).value(); }
    Iterator erase(Iterator it);
    void remove(QObject *key);
    void mark(QObject *key, ExecutionEngine *engine);

private Q_SLOTS:
    void removeDestroyedObject(QObject*);
};

}

QT_END_NAMESPACE

#endif // QV4QOBJECTWRAPPER_P_H


