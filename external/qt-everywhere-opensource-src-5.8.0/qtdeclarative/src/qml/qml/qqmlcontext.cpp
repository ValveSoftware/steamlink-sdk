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

#include "qqmlcontext.h"
#include "qqmlcontext_p.h"
#include "qqmlcomponentattached_p.h"

#include "qqmlcomponent_p.h"
#include "qqmlexpression_p.h"
#include "qqmlengine_p.h"
#include "qqmlengine.h"
#include "qqmlinfo.h"
#include "qqmlabstracturlinterceptor.h"

#include <qjsengine.h>
#include <QtCore/qvarlengtharray.h>
#include <private/qmetaobject_p.h>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

QQmlContextPrivate::QQmlContextPrivate()
: data(0), notifyIndex(-1)
{
}

/*!
    \class QQmlContext
    \brief The QQmlContext class defines a context within a QML engine.
    \inmodule QtQml

    Contexts allow data to be exposed to the QML components instantiated by the
    QML engine.

    Each QQmlContext contains a set of properties, distinct from its QObject
    properties, that allow data to be explicitly bound to a context by name.  The
    context properties are defined and updated by calling
    QQmlContext::setContextProperty().  The following example shows a Qt model
    being bound to a context and then accessed from a QML file.

    \code
    QQmlEngine engine;
    QStringListModel modelData;
    QQmlContext *context = new QQmlContext(engine.rootContext());
    context->setContextProperty("myModel", &modelData);

    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nListView { model: myModel }", QUrl());
    QObject *window = component.create(context);
    \endcode

    Note it is the responsibility of the creator to delete any QQmlContext it
    constructs. If the \c context object in the example is no longer needed when the
    \c window component instance is destroyed, the \c context must be destroyed explicitly.
    The simplest way to ensure this is to set \c window as the parent of \c context.

    To simplify binding and maintaining larger data sets, a context object can be set
    on a QQmlContext.  All the properties of the context object are available
    by name in the context, as though they were all individually added through calls
    to QQmlContext::setContextProperty().  Changes to the property's values are
    detected through the property's notify signal.  Setting a context object is both
    faster and easier than manually adding and maintaining context property values.

    The following example has the same effect as the previous one, but it uses a context
    object.

    \code
    class MyDataSet : ... {
        ...
        Q_PROPERTY(QAbstractItemModel *myModel READ model NOTIFY modelChanged)
        ...
    };

    MyDataSet myDataSet;
    QQmlEngine engine;
    QQmlContext *context = new QQmlContext(engine.rootContext());
    context->setContextObject(&myDataSet);

    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nListView { model: myModel }", QUrl());
    component.create(context);
    \endcode

    All properties added explicitly by QQmlContext::setContextProperty() take
    precedence over the context object's properties.

    \section2 The Context Hierarchy

    Contexts form a hierarchy. The root of this hierarchy is the QML engine's
    \l {QQmlEngine::rootContext()}{root context}. Child contexts inherit
    the context properties of their parents; if a child context sets a context property
    that already exists in its parent, the new context property overrides that of the
    parent.

    The following example defines two contexts - \c context1 and \c context2.  The
    second context overrides the "b" context property inherited from the first with a
    new value.

    \code
    QQmlEngine engine;
    QQmlContext *context1 = new QQmlContext(engine.rootContext());
    QQmlContext *context2 = new QQmlContext(context1);

    context1->setContextProperty("a", 12);
    context1->setContextProperty("b", 12);

    context2->setContextProperty("b", 15);
    \endcode

    While QML objects instantiated in a context are not strictly owned by that
    context, their bindings are.  If a context is destroyed, the property bindings of
    outstanding QML objects will stop evaluating.

    \warning Setting the context object or adding new context properties after an object
    has been created in that context is an expensive operation (essentially forcing all bindings
    to reevaluate). Thus whenever possible you should complete "setup" of the context
    before using it to create any objects.

    \sa {qtqml-cppintegration-exposecppattributes.html}{Exposing Attributes of C++ Types to QML}
*/

/*! \internal */
QQmlContext::QQmlContext(QQmlEngine *e, bool)
: QObject(*(new QQmlContextPrivate))
{
    Q_D(QQmlContext);
    d->data = new QQmlContextData(this);

    d->data->engine = e;
}

/*!
    Create a new QQmlContext as a child of \a engine's root context, and the
    QObject \a parent.
*/
QQmlContext::QQmlContext(QQmlEngine *engine, QObject *parent)
: QObject(*(new QQmlContextPrivate), parent)
{
    Q_D(QQmlContext);
    d->data = new QQmlContextData(this);

    d->data->setParent(engine?QQmlContextData::get(engine->rootContext()):0);
}

/*!
    Create a new QQmlContext with the given \a parentContext, and the
    QObject \a parent.
*/
QQmlContext::QQmlContext(QQmlContext *parentContext, QObject *parent)
: QObject(*(new QQmlContextPrivate), parent)
{
    Q_D(QQmlContext);
    d->data = new QQmlContextData(this);

    d->data->setParent(parentContext?QQmlContextData::get(parentContext):0);
}

/*!
    \internal
*/
QQmlContext::QQmlContext(QQmlContextData *data)
: QObject(*(new QQmlContextPrivate), 0)
{
    Q_D(QQmlContext);
    d->data = data;
}

/*!
    Destroys the QQmlContext.

    Any expressions, or sub-contexts dependent on this context will be
    invalidated, but not destroyed (unless they are parented to the QQmlContext
    object).
 */
QQmlContext::~QQmlContext()
{
    Q_D(QQmlContext);

    if (!d->data->isInternal)
        d->data->destroy();
}

/*!
    Returns whether the context is valid.

    To be valid, a context must have a engine, and it's contextObject(), if any,
    must not have been deleted.
*/
bool QQmlContext::isValid() const
{
    Q_D(const QQmlContext);
    return d->data && d->data->isValid();
}

/*!
    Return the context's QQmlEngine, or 0 if the context has no QQmlEngine or the
    QQmlEngine was destroyed.
*/
QQmlEngine *QQmlContext::engine() const
{
    Q_D(const QQmlContext);
    return d->data->engine;
}

/*!
    Return the context's parent QQmlContext, or 0 if this context has no
    parent or if the parent has been destroyed.
*/
QQmlContext *QQmlContext::parentContext() const
{
    Q_D(const QQmlContext);
    return d->data->parent?d->data->parent->asQQmlContext():0;
}

/*!
    Return the context object, or 0 if there is no context object.
*/
QObject *QQmlContext::contextObject() const
{
    Q_D(const QQmlContext);
    return d->data->contextObject;
}

/*!
    Set the context \a object.
*/
void QQmlContext::setContextObject(QObject *object)
{
    Q_D(QQmlContext);

    QQmlContextData *data = d->data;

    if (data->isInternal) {
        qWarning("QQmlContext: Cannot set context object for internal context.");
        return;
    }

    if (!isValid()) {
        qWarning("QQmlContext: Cannot set context object on invalid context.");
        return;
    }

    data->contextObject = object;
    data->refreshExpressions();
}

/*!
    Set a the \a value of the \a name property on this context.
*/
void QQmlContext::setContextProperty(const QString &name, const QVariant &value)
{
    Q_D(QQmlContext);
    if (d->notifyIndex == -1)
        d->notifyIndex = QMetaObjectPrivate::absoluteSignalCount(&QQmlContext::staticMetaObject);

    QQmlContextData *data = d->data;

    if (data->isInternal) {
        qWarning("QQmlContext: Cannot set property on internal context.");
        return;
    }

    if (!isValid()) {
        qWarning("QQmlContext: Cannot set property on invalid context.");
        return;
    }

    if (data->engine) {
        bool ok;
        QObject *o = QQmlEnginePrivate::get(data->engine)->toQObject(value, &ok);
        if (ok) {
            setContextProperty(name, o);
            return;
        }
    }

    QV4::IdentifierHash<int> &properties = data->detachedPropertyNames();
    int idx = properties.value(name);
    if (idx == -1) {
        properties.add(name, data->idValueCount + d->propertyValues.count());
        d->propertyValues.append(value);

        data->refreshExpressions();
    } else {
        d->propertyValues[idx] = value;
        QMetaObject::activate(this, d->notifyIndex, idx, 0);
    }
}

/*!
    Set the \a value of the \a name property on this context.

    QQmlContext does \b not take ownership of \a value.
*/
void QQmlContext::setContextProperty(const QString &name, QObject *value)
{
    Q_D(QQmlContext);
    if (d->notifyIndex == -1)
        d->notifyIndex = QMetaObjectPrivate::absoluteSignalCount(&QQmlContext::staticMetaObject);

    QQmlContextData *data = d->data;

    if (data->isInternal) {
        qWarning("QQmlContext: Cannot set property on internal context.");
        return;
    }

    if (!isValid()) {
        qWarning("QQmlContext: Cannot set property on invalid context.");
        return;
    }

    QV4::IdentifierHash<int> &properties = data->detachedPropertyNames();
    int idx = properties.value(name);

    if (idx == -1) {
        properties.add(name, data->idValueCount + d->propertyValues.count());
        d->propertyValues.append(QVariant::fromValue(value));

        data->refreshExpressions();
    } else {
        d->propertyValues[idx] = QVariant::fromValue(value);
        QMetaObject::activate(this, d->notifyIndex, idx, 0);
    }
}

/*!
  Returns the value of the \a name property for this context
  as a QVariant.
 */
QVariant QQmlContext::contextProperty(const QString &name) const
{
    Q_D(const QQmlContext);
    QVariant value;
    int idx = -1;

    QQmlContextData *data = d->data;

    const QV4::IdentifierHash<int> &properties = data->propertyNames();
    if (properties.count())
        idx = properties.value(name);

    if (idx == -1) {
        if (data->contextObject) {
            QObject *obj = data->contextObject;
            QQmlPropertyData local;
            QQmlPropertyData *property =
                QQmlPropertyCache::property(data->engine, obj, name, data, local);

            if (property) value = obj->metaObject()->property(property->coreIndex()).read(obj);
        }
        if (!value.isValid() && parentContext())
            value = parentContext()->contextProperty(name);
    } else {
        if (idx >= d->propertyValues.count())
            value = QVariant::fromValue(data->idValues[idx - d->propertyValues.count()].data());
        else
            value = d->propertyValues[idx];
    }

    return value;
}

/*!
Returns the name of \a object in this context, or an empty string if \a object
is not named in the context.  Objects are named by setContextProperty(), or by ids in
the case of QML created contexts.

If the object has multiple names, the first is returned.
*/
QString QQmlContext::nameForObject(QObject *object) const
{
    Q_D(const QQmlContext);

    return d->data->findObjectId(object);
}

/*!
    Resolves the URL \a src relative to the URL of the
    containing component.

    \sa QQmlEngine::baseUrl(), setBaseUrl()
*/
QUrl QQmlContext::resolvedUrl(const QUrl &src)
{
    Q_D(QQmlContext);
    return d->data->resolvedUrl(src);
}

QUrl QQmlContextData::resolvedUrl(const QUrl &src)
{
    QQmlContextData *ctxt = this;

    QUrl resolved;
    if (src.isRelative() && !src.isEmpty()) {
        if (ctxt) {
            while(ctxt) {
                if (ctxt->url().isValid())
                    break;
                else
                    ctxt = ctxt->parent;
            }

            if (ctxt)
                resolved = ctxt->url().resolved(src);
            else if (engine)
                resolved = engine->baseUrl().resolved(src);
        }
    } else {
        resolved = src;
    }

    if (resolved.isEmpty()) //relative but no ctxt
        return resolved;

    if (engine && engine->urlInterceptor())
        resolved = engine->urlInterceptor()->intercept(resolved, QQmlAbstractUrlInterceptor::UrlString);
    return resolved;
}


/*!
    Explicitly sets the url resolvedUrl() will use for relative references to \a baseUrl.

    Calling this function will override the url of the containing
    component used by default.

    \sa resolvedUrl()
*/
void QQmlContext::setBaseUrl(const QUrl &baseUrl)
{
    Q_D(QQmlContext);

    d->data->baseUrl = baseUrl;
    d->data->baseUrlString = baseUrl.toString();
}

/*!
    Returns the base url of the component, or the containing component
    if none is set.
*/
QUrl QQmlContext::baseUrl() const
{
    Q_D(const QQmlContext);
    const QQmlContextData* data = d->data;
    while (data && data->url().isEmpty())
        data = data->parent;

    if (data)
        return data->url();
    else
        return QUrl();
}

int QQmlContextPrivate::context_count(QQmlListProperty<QObject> *prop)
{
    QQmlContext *context = static_cast<QQmlContext*>(prop->object);
    QQmlContextPrivate *d = QQmlContextPrivate::get(context);
    int contextProperty = (int)(quintptr)prop->data;

    if (d->propertyValues.at(contextProperty).userType() != qMetaTypeId<QList<QObject*> >()) {
        return 0;
    } else {
        return ((const QList<QObject> *)d->propertyValues.at(contextProperty).constData())->count();
    }
}

QObject *QQmlContextPrivate::context_at(QQmlListProperty<QObject> *prop, int index)
{
    QQmlContext *context = static_cast<QQmlContext*>(prop->object);
    QQmlContextPrivate *d = QQmlContextPrivate::get(context);
    int contextProperty = (int)(quintptr)prop->data;

    if (d->propertyValues.at(contextProperty).userType() != qMetaTypeId<QList<QObject*> >()) {
        return 0;
    } else {
        return ((const QList<QObject*> *)d->propertyValues.at(contextProperty).constData())->at(index);
    }
}


QQmlContextData::QQmlContextData()
    : QQmlContextData(nullptr)
{
}

QQmlContextData::QQmlContextData(QQmlContext *ctxt)
: parent(0), engine(0), isInternal(false), ownedByParent(false), isJSContext(false),
  isPragmaLibraryContext(false), unresolvedNames(false), hasEmittedDestruction(false), isRootObjectInCreation(false),
  publicContext(ctxt), activeVMEData(0), componentObjectIndex(-1),
  contextObject(0), childContexts(0), nextChild(0), prevChild(0),
  expressions(0), contextObjects(0), contextGuards(0), idValues(0), idValueCount(0), linkedContext(0),
  componentAttached(0)
{
}

void QQmlContextData::emitDestruction()
{
    if (!hasEmittedDestruction) {
        hasEmittedDestruction = true;

        // Emit the destruction signal - must be emitted before invalidate so that the
        // context is still valid if bindings or resultant expression evaluation requires it
        if (engine) {
            while (componentAttached) {
                QQmlComponentAttached *a = componentAttached;
                componentAttached = a->next;
                if (componentAttached) componentAttached->prev = &componentAttached;

                a->next = 0;
                a->prev = 0;

                emit a->destruction();
            }

            QQmlContextData * child = childContexts;
            while (child) {
                child->emitDestruction();
                child = child->nextChild;
            }
        }
    }
}

void QQmlContextData::invalidate()
{
    emitDestruction();

    while (childContexts) {
        if (childContexts->ownedByParent) {
            childContexts->destroy();
        } else {
            childContexts->invalidate();
        }
    }

    if (prevChild) {
        *prevChild = nextChild;
        if (nextChild) nextChild->prevChild = prevChild;
        nextChild = 0;
        prevChild = 0;
    }

    engine = 0;
    parent = 0;
}

void QQmlContextData::clearContext()
{
    emitDestruction();

    QQmlJavaScriptExpression *expression = expressions;
    while (expression) {
        QQmlJavaScriptExpression *nextExpression = expression->m_nextExpression;

        expression->m_prevExpression = 0;
        expression->m_nextExpression = 0;

        expression->setContext(0);

        expression = nextExpression;
    }
    expressions = 0;
}

void QQmlContextData::destroy()
{
    if (linkedContext)
        linkedContext->destroy();

    if (engine) invalidate();

    clearContext();

    while (contextObjects) {
        QQmlData *co = contextObjects;
        contextObjects = contextObjects->nextContextObject;

        co->context = 0;
        co->outerContext = 0;
        co->nextContextObject = 0;
        co->prevContextObject = 0;
    }

    QQmlGuardedContextData *contextGuard = contextGuards;
    while (contextGuard) {
        QQmlGuardedContextData *next = contextGuard->m_next;
        contextGuard->m_next = 0;
        contextGuard->m_prev = 0;
        contextGuard->m_contextData = 0;
        contextGuard = next;
    }
    contextGuards = 0;

    delete [] idValues;

    if (isInternal)
        delete publicContext;

    delete this;
}

void QQmlContextData::setParent(QQmlContextData *p, bool parentTakesOwnership)
{
    if (p) {
        parent = p;
        engine = p->engine;
        nextChild = p->childContexts;
        if (nextChild) nextChild->prevChild = &nextChild;
        prevChild = &p->childContexts;
        p->childContexts = this;
        ownedByParent = parentTakesOwnership;
    }
}

void QQmlContextData::refreshExpressionsRecursive(QQmlJavaScriptExpression *expression)
{
    QQmlJavaScriptExpression::DeleteWatcher w(expression);

    if (expression->m_nextExpression)
        refreshExpressionsRecursive(expression->m_nextExpression);

    if (!w.wasDeleted())
        expression->refresh();
}

static inline bool expressions_to_run(QQmlContextData *ctxt, bool isGlobalRefresh)
{
    return ctxt->expressions && (!isGlobalRefresh || ctxt->unresolvedNames);
}

void QQmlContextData::refreshExpressionsRecursive(bool isGlobal)
{
    // For efficiency, we try and minimize the number of guards we have to create
    if (expressions_to_run(this, isGlobal) && (nextChild || childContexts)) {
        QQmlGuardedContextData guard(this);

        if (childContexts)
            childContexts->refreshExpressionsRecursive(isGlobal);

        if (guard.isNull()) return;

        if (nextChild)
            nextChild->refreshExpressionsRecursive(isGlobal);

        if (guard.isNull()) return;

        if (expressions_to_run(this, isGlobal))
            refreshExpressionsRecursive(expressions);

    } else if (expressions_to_run(this, isGlobal)) {

        refreshExpressionsRecursive(expressions);

    } else if (nextChild && childContexts) {

        QQmlGuardedContextData guard(this);

        childContexts->refreshExpressionsRecursive(isGlobal);

        if (!guard.isNull() && nextChild)
            nextChild->refreshExpressionsRecursive(isGlobal);

    } else if (nextChild) {

        nextChild->refreshExpressionsRecursive(isGlobal);

    } else if (childContexts) {

        childContexts->refreshExpressionsRecursive(isGlobal);

    }
}

// Refreshes all expressions that could possibly depend on this context.  Refreshing flushes all
// context-tree dependent caches in the expressions, and should occur every time the context tree
// *structure* (not values) changes.
void QQmlContextData::refreshExpressions()
{
    bool isGlobal = (parent == 0);

    // For efficiency, we try and minimize the number of guards we have to create
    if (expressions_to_run(this, isGlobal) && childContexts) {
        QQmlGuardedContextData guard(this);

        childContexts->refreshExpressionsRecursive(isGlobal);

        if (!guard.isNull() && expressions_to_run(this, isGlobal))
            refreshExpressionsRecursive(expressions);

    } else if (expressions_to_run(this, isGlobal)) {

        refreshExpressionsRecursive(expressions);

    } else if (childContexts) {

        childContexts->refreshExpressionsRecursive(isGlobal);

    }
}

void QQmlContextData::addObject(QObject *o)
{
    QQmlData *data = QQmlData::get(o, true);

    Q_ASSERT(data->context == 0);

    data->context = this;
    data->outerContext = this;

    data->nextContextObject = contextObjects;
    if (data->nextContextObject)
        data->nextContextObject->prevContextObject = &data->nextContextObject;
    data->prevContextObject = &contextObjects;
    contextObjects = data;
}

void QQmlContextData::setIdProperty(int idx, QObject *obj)
{
    idValues[idx] = obj;
    idValues[idx].context = this;
}

QString QQmlContextData::findObjectId(const QObject *obj) const
{
    const QV4::IdentifierHash<int> &properties = propertyNames();
    if (propertyNameCache.isEmpty())
        return QString();

    for (int ii = 0; ii < idValueCount; ii++) {
        if (idValues[ii] == obj)
            return properties.findId(ii);
    }

    if (publicContext) {
        QQmlContextPrivate *p = QQmlContextPrivate::get(publicContext);
        for (int ii = 0; ii < p->propertyValues.count(); ++ii)
            if (p->propertyValues.at(ii) == QVariant::fromValue(const_cast<QObject *>(obj)))
                return properties.findId(ii);
    }

    if (linkedContext)
        return linkedContext->findObjectId(obj);
    return QString();
}

QQmlContext *QQmlContextData::asQQmlContext()
{
    if (!publicContext)
        publicContext = new QQmlContext(this);
    return publicContext;
}

QQmlContextPrivate *QQmlContextData::asQQmlContextPrivate()
{
    return QQmlContextPrivate::get(asQQmlContext());
}

void QQmlContextData::initFromTypeCompilationUnit(const QQmlRefPointer<QV4::CompiledData::CompilationUnit> &unit, int subComponentIndex)
{
    typeCompilationUnit = unit;
    componentObjectIndex = subComponentIndex == -1 ? typeCompilationUnit->data->indexOfRootObject : subComponentIndex;
    Q_ASSERT(!idValues);
    idValueCount = typeCompilationUnit->data->objectAt(componentObjectIndex)->nNamedObjectsInComponent;
    idValues = new ContextGuard[idValueCount];
}

const QV4::IdentifierHash<int> &QQmlContextData::propertyNames() const
{
    if (propertyNameCache.isEmpty()) {
        if (typeCompilationUnit)
            propertyNameCache = typeCompilationUnit->namedObjectsPerComponent(componentObjectIndex);
        else
            propertyNameCache = QV4::IdentifierHash<int>(QV8Engine::getV4(engine));
    }
    return propertyNameCache;
}

QV4::IdentifierHash<int> &QQmlContextData::detachedPropertyNames()
{
    propertyNames();
    propertyNameCache.detach();
    return propertyNameCache;
}

QUrl QQmlContextData::url() const
{
    if (typeCompilationUnit)
        return typeCompilationUnit->url();
    return baseUrl;
}

QString QQmlContextData::urlString() const
{
    if (typeCompilationUnit)
        return typeCompilationUnit->fileName();
    return baseUrlString;
}

QT_END_NAMESPACE
