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

#include "qqmlpropertycache_p.h"

#include <private/qqmlengine_p.h>
#include <private/qqmlbinding_p.h>
#include <private/qqmlvmemetaobject_p.h>
#include <private/qv8engine_p.h>

#include <private/qmetaobject_p.h>
#include <private/qqmlaccessors_p.h>
#include <private/qmetaobjectbuilder_p.h>

#include <private/qv4value_inl_p.h>

#include <QtCore/qdebug.h>

#include <ctype.h> // for toupper
#include <limits.h>
#include <algorithm>

#ifdef Q_CC_MSVC
// nonstandard extension used : zero-sized array in struct/union.
#  pragma warning( disable : 4200 )
#endif

QT_BEGIN_NAMESPACE

#define Q_INT16_MAX 32767

class QQmlPropertyCacheMethodArguments
{
public:
    QQmlPropertyCacheMethodArguments *next;

    //for signal handler rewrites
    QString *signalParameterStringForJS;
    int parameterError:1;
    int argumentsValid:1;

    QList<QByteArray> *names;
    int arguments[0];
};

// Flags that do *NOT* depend on the property's QMetaProperty::userType() and thus are quick
// to load
static QQmlPropertyData::Flags fastFlagsForProperty(const QMetaProperty &p)
{
    QQmlPropertyData::Flags flags;

    if (p.isConstant())
        flags |= QQmlPropertyData::IsConstant;
    if (p.isWritable())
        flags |= QQmlPropertyData::IsWritable;
    if (p.isResettable())
        flags |= QQmlPropertyData::IsResettable;
    if (p.isFinal())
        flags |= QQmlPropertyData::IsFinal;
    if (p.isEnumType())
        flags |= QQmlPropertyData::IsEnumType;

    return flags;
}

// Flags that do depend on the property's QMetaProperty::userType() and thus are slow to
// load
static QQmlPropertyData::Flags flagsForPropertyType(int propType, QQmlEngine *engine)
{
    Q_ASSERT(propType != -1);

    QQmlPropertyData::Flags flags;

    if (propType == QMetaType::QObjectStar) {
        flags |= QQmlPropertyData::IsQObjectDerived;
    } else if (propType == QMetaType::QVariant) {
        flags |= QQmlPropertyData::IsQVariant;
    } else if (propType < (int)QVariant::UserType) {
    } else if (propType == qMetaTypeId<QQmlBinding *>()) {
        flags |= QQmlPropertyData::IsQmlBinding;
    } else if (propType == qMetaTypeId<QJSValue>()) {
        flags |= QQmlPropertyData::IsQJSValue;
    } else if (propType == qMetaTypeId<QQmlV4Handle>()) {
        flags |= QQmlPropertyData::IsV4Handle;
    } else {
        QQmlMetaType::TypeCategory cat =
            engine ? QQmlEnginePrivate::get(engine)->typeCategory(propType)
                   : QQmlMetaType::typeCategory(propType);

        if (cat == QQmlMetaType::Object || QMetaType::typeFlags(propType) & QMetaType::PointerToQObject)
            flags |= QQmlPropertyData::IsQObjectDerived;
        else if (cat == QQmlMetaType::List)
            flags |= QQmlPropertyData::IsQList;
    }

    return flags;
}

static int metaObjectSignalCount(const QMetaObject *metaObject)
{
    int signalCount = 0;
    for (const QMetaObject *obj = metaObject; obj; obj = obj->superClass())
        signalCount += QMetaObjectPrivate::get(obj)->signalCount;
    return signalCount;
}

QQmlPropertyData::Flags
QQmlPropertyData::flagsForProperty(const QMetaProperty &p, QQmlEngine *engine)
{
    return fastFlagsForProperty(p) | flagsForPropertyType(p.userType(), engine);
}

void QQmlPropertyData::lazyLoad(const QMetaProperty &p, QQmlEngine *engine)
{
    Q_UNUSED(engine);

    coreIndex = p.propertyIndex();
    notifyIndex = QMetaObjectPrivate::signalIndex(p.notifySignal());
    Q_ASSERT(p.revision() <= Q_INT16_MAX);
    revision = p.revision();

    flags = fastFlagsForProperty(p);

    int type = p.type();
    if (type == QMetaType::QObjectStar) {
        propType = type;
        flags |= QQmlPropertyData::IsQObjectDerived;
    } else if (type == QMetaType::QVariant) {
        propType = type;
        flags |= QQmlPropertyData::IsQVariant;
    } else if (type == QVariant::UserType || type == -1) {
        propTypeName = p.typeName();
        flags |= QQmlPropertyData::NotFullyResolved;
    } else {
        propType = type;
    }
}

void QQmlPropertyData::load(const QMetaProperty &p, QQmlEngine *engine)
{
    propType = p.userType();
    coreIndex = p.propertyIndex();
    notifyIndex = QMetaObjectPrivate::signalIndex(p.notifySignal());
    flags = fastFlagsForProperty(p) | flagsForPropertyType(propType, engine);
    Q_ASSERT(p.revision() <= Q_INT16_MAX);
    revision = p.revision();
}

void QQmlPropertyData::load(const QMetaMethod &m)
{
    coreIndex = m.methodIndex();
    arguments = 0;
    flags |= IsFunction;
    if (m.methodType() == QMetaMethod::Signal)
        flags |= IsSignal;
    propType = m.returnType();

    if (m.parameterCount()) {
        flags |= HasArguments;
        if ((m.parameterCount() == 1) && (m.parameterTypes().first() == "QQmlV4Function*")) {
            flags |= IsV4Function;
        }
    }

    if (m.attributes() & QMetaMethod::Cloned)
        flags |= IsCloned;

    Q_ASSERT(m.revision() <= Q_INT16_MAX);
    revision = m.revision();
}

void QQmlPropertyData::lazyLoad(const QMetaMethod &m)
{
    coreIndex = m.methodIndex();
    arguments = 0;
    flags |= IsFunction;
    if (m.methodType() == QMetaMethod::Signal)
        flags |= IsSignal;
    propType = QMetaType::Void;

    const char *returnType = m.typeName();
    if (!returnType)
        returnType = "\0";
    if ((*returnType != 'v') || (qstrcmp(returnType+1, "oid") != 0)) {
        propTypeName = returnType;
        flags |= NotFullyResolved;
    }

    if (m.parameterCount()) {
        flags |= HasArguments;
        if ((m.parameterCount() == 1) && (m.parameterTypes().first() == "QQmlV4Function*")) {
            flags |= IsV4Function;
        }
    }

    if (m.attributes() & QMetaMethod::Cloned)
        flags |= IsCloned;

    Q_ASSERT(m.revision() <= Q_INT16_MAX);
    revision = m.revision();
}

/*!
Creates a new empty QQmlPropertyCache.
*/
QQmlPropertyCache::QQmlPropertyCache(QQmlEngine *e)
: engine(e), _parent(0), propertyIndexCacheStart(0), methodIndexCacheStart(0),
  signalHandlerIndexCacheStart(0), _hasPropertyOverrides(false), _ownMetaObject(false),
  _metaObject(0), argumentsCache(0)
{
    Q_ASSERT(engine);
}

/*!
Creates a new QQmlPropertyCache of \a metaObject.
*/
QQmlPropertyCache::QQmlPropertyCache(QQmlEngine *e, const QMetaObject *metaObject)
: engine(e), _parent(0), propertyIndexCacheStart(0), methodIndexCacheStart(0),
  signalHandlerIndexCacheStart(0), _hasPropertyOverrides(false), _ownMetaObject(false),
  _metaObject(0), argumentsCache(0)
{
    Q_ASSERT(engine);
    Q_ASSERT(metaObject);

    update(engine, metaObject);
}

QQmlPropertyCache::~QQmlPropertyCache()
{
    clear();

    QQmlPropertyCacheMethodArguments *args = argumentsCache;
    while (args) {
        QQmlPropertyCacheMethodArguments *next = args->next;
        if (args->signalParameterStringForJS) delete args->signalParameterStringForJS;
        if (args->names) delete args->names;
        free(args);
        args = next;
    }

    // We must clear this prior to releasing the parent incase it is a
    // linked hash
    stringCache.clear();
    if (_parent) _parent->release();

    if (_ownMetaObject) free((void *)_metaObject);
    _metaObject = 0;
    _parent = 0;
    engine = 0;
}

void QQmlPropertyCache::destroy()
{
    Q_ASSERT(engine);
    delete this;
}

// This is inherited from QQmlCleanup, so it should only clear the things
// that are tied to the specific QQmlEngine.
void QQmlPropertyCache::clear()
{
    engine = 0;
}

QQmlPropertyCache *QQmlPropertyCache::copy(int reserve)
{
    QQmlPropertyCache *cache = new QQmlPropertyCache(engine);
    cache->_parent = this;
    cache->_parent->addref();
    cache->propertyIndexCacheStart = propertyIndexCache.count() + propertyIndexCacheStart;
    cache->methodIndexCacheStart = methodIndexCache.count() + methodIndexCacheStart;
    cache->signalHandlerIndexCacheStart = signalHandlerIndexCache.count() + signalHandlerIndexCacheStart;
    cache->stringCache.linkAndReserve(stringCache, reserve);
    cache->allowedRevisionCache = allowedRevisionCache;
    cache->_metaObject = _metaObject;
    cache->_defaultPropertyName = _defaultPropertyName;

    return cache;
}

QQmlPropertyCache *QQmlPropertyCache::copy()
{
    return copy(0);
}

QQmlPropertyCache *QQmlPropertyCache::copyAndReserve(QQmlEngine *, int propertyCount, int methodCount,
                                                     int signalCount)
{
    QQmlPropertyCache *rv = copy(propertyCount + methodCount + signalCount);
    rv->propertyIndexCache.reserve(propertyCount);
    rv->methodIndexCache.reserve(methodCount);
    rv->signalHandlerIndexCache.reserve(signalCount);
    rv->_metaObject = 0;

    return rv;
}

/*! \internal

    \a notifyIndex MUST be in the signal index range (see QObjectPrivate::signalIndex()).
    This is different from QMetaMethod::methodIndex().
*/
void QQmlPropertyCache::appendProperty(const QString &name,
                                       quint32 flags, int coreIndex, int propType, int notifyIndex)
{
    QQmlPropertyData data;
    data.propType = propType;
    data.coreIndex = coreIndex;
    data.notifyIndex = notifyIndex;
    data.flags = flags;

    QQmlPropertyData *old = findNamedProperty(name);
    if (old)
        data.markAsOverrideOf(old);

    int index = propertyIndexCache.count();
    propertyIndexCache.append(data);

    setNamedProperty(name, index + propertyOffset(), propertyIndexCache.data() + index, (old != 0));
}

void QQmlPropertyCache::appendProperty(const QHashedCStringRef &name,
                                       quint32 flags, int coreIndex, int propType, int notifyIndex)
{
    QQmlPropertyData data;
    data.propType = propType;
    data.coreIndex = coreIndex;
    data.notifyIndex = notifyIndex;
    data.flags = flags;

    QQmlPropertyData *old = findNamedProperty(name);
    if (old)
        data.markAsOverrideOf(old);

    int index = propertyIndexCache.count();
    propertyIndexCache.append(data);

    setNamedProperty(name, index + propertyOffset(), propertyIndexCache.data() + index, (old != 0));
}

void QQmlPropertyCache::appendSignal(const QString &name, quint32 flags, int coreIndex,
                                     const int *types, const QList<QByteArray> &names)
{
    QQmlPropertyData data;
    data.propType = QVariant::Invalid;
    data.coreIndex = coreIndex;
    data.flags = flags;
    data.arguments = 0;

    QQmlPropertyData handler = data;
    handler.flags |= QQmlPropertyData::IsSignalHandler;

    if (types) {
        int argumentCount = *types;
        QQmlPropertyCacheMethodArguments *args = createArgumentsObject(argumentCount, names);
        ::memcpy(args->arguments, types, (argumentCount + 1) * sizeof(int));
        args->argumentsValid = true;
        data.arguments = args;
    }

    QQmlPropertyData *old = findNamedProperty(name);
    if (old)
        data.markAsOverrideOf(old);

    int methodIndex = methodIndexCache.count();
    methodIndexCache.append(data);

    int signalHandlerIndex = signalHandlerIndexCache.count();
    signalHandlerIndexCache.append(handler);

    QString handlerName = QLatin1String("on") + name;
    handlerName[2] = handlerName[2].toUpper();

    setNamedProperty(name, methodIndex + methodOffset(), methodIndexCache.data() + methodIndex, (old != 0));
    setNamedProperty(handlerName, signalHandlerIndex + signalOffset(), signalHandlerIndexCache.data() + signalHandlerIndex, (old != 0));
}

void QQmlPropertyCache::appendSignal(const QHashedCStringRef &name, quint32 flags, int coreIndex,
                                     const int *types, const QList<QByteArray> &names)
{
    QQmlPropertyData data;
    data.propType = QVariant::Invalid;
    data.coreIndex = coreIndex;
    data.flags = flags;
    data.arguments = 0;

    QQmlPropertyData handler = data;
    handler.flags |= QQmlPropertyData::IsSignalHandler;

    if (types) {
        int argumentCount = *types;
        QQmlPropertyCacheMethodArguments *args = createArgumentsObject(argumentCount, names);
        ::memcpy(args->arguments, types, (argumentCount + 1) * sizeof(int));
        args->argumentsValid = true;
        data.arguments = args;
    }

    QQmlPropertyData *old = findNamedProperty(name);
    if (old)
        data.markAsOverrideOf(old);

    int methodIndex = methodIndexCache.count();
    methodIndexCache.append(data);

    int signalHandlerIndex = signalHandlerIndexCache.count();
    signalHandlerIndexCache.append(handler);

    QString handlerName = QLatin1String("on") + name.toUtf16();
    handlerName[2] = handlerName[2].toUpper();

    setNamedProperty(name, methodIndex + methodOffset(), methodIndexCache.data() + methodIndex, (old != 0));
    setNamedProperty(handlerName, signalHandlerIndex + signalOffset(), signalHandlerIndexCache.data() + signalHandlerIndex, (old != 0));
}

void QQmlPropertyCache::appendMethod(const QString &name, quint32 flags, int coreIndex,
                                     const QList<QByteArray> &names)
{
    int argumentCount = names.count();

    QQmlPropertyData data;
    data.propType = QMetaType::QVariant;
    data.coreIndex = coreIndex;

    QQmlPropertyCacheMethodArguments *args = createArgumentsObject(argumentCount, names);
    for (int ii = 0; ii < argumentCount; ++ii)
        args->arguments[ii + 1] = QMetaType::QVariant;
    args->argumentsValid = true;
    data.arguments = args;

    data.flags = flags;

    QQmlPropertyData *old = findNamedProperty(name);
    if (old)
        data.markAsOverrideOf(old);

    int methodIndex = methodIndexCache.count();
    methodIndexCache.append(data);

    setNamedProperty(name, methodIndex + methodOffset(), methodIndexCache.data() + methodIndex, (old != 0));
}

void QQmlPropertyCache::appendMethod(const QHashedCStringRef &name, quint32 flags, int coreIndex,
                                     const QList<QByteArray> &names)
{
    int argumentCount = names.count();

    QQmlPropertyData data;
    data.propType = QMetaType::QVariant;
    data.coreIndex = coreIndex;

    QQmlPropertyCacheMethodArguments *args = createArgumentsObject(argumentCount, names);
    for (int ii = 0; ii < argumentCount; ++ii)
        args->arguments[ii + 1] = QMetaType::QVariant;
    args->argumentsValid = true;
    data.arguments = args;

    data.flags = flags;

    QQmlPropertyData *old = findNamedProperty(name);
    if (old)
        data.markAsOverrideOf(old);

    int methodIndex = methodIndexCache.count();
    methodIndexCache.append(data);

    setNamedProperty(name, methodIndex + methodOffset(), methodIndexCache.data() + methodIndex, (old != 0));
}

// Returns this property cache's metaObject.  May be null if it hasn't been created yet.
const QMetaObject *QQmlPropertyCache::metaObject() const
{
    return _metaObject;
}

// Returns this property cache's metaObject, creating it if necessary.
const QMetaObject *QQmlPropertyCache::createMetaObject()
{
    if (!_metaObject) {
        _ownMetaObject = true;

        QMetaObjectBuilder builder;
        toMetaObjectBuilder(builder);
        builder.setSuperClass(_parent->createMetaObject());
        _metaObject = builder.toMetaObject();
    }

    return _metaObject;
}

// Returns the name of the default property for this cache
QString QQmlPropertyCache::defaultPropertyName() const
{
    return _defaultPropertyName;
}

QQmlPropertyData *QQmlPropertyCache::defaultProperty() const
{
    return property(defaultPropertyName(), 0, 0);
}

QQmlPropertyCache *QQmlPropertyCache::parent() const
{
    return _parent;
}

void QQmlPropertyCache::setParent(QQmlPropertyCache *newParent)
{
    _parent = newParent;
}

// Returns the first C++ type's QMetaObject - that is, the first QMetaObject not created by
// QML
const QMetaObject *QQmlPropertyCache::firstCppMetaObject() const
{
    while (_parent && (_metaObject == 0 || _ownMetaObject))
        return _parent->firstCppMetaObject();
    return _metaObject;
}

QQmlPropertyCache *
QQmlPropertyCache::copyAndAppend(QQmlEngine *engine, const QMetaObject *metaObject,
                                         QQmlPropertyData::Flag propertyFlags,
                                         QQmlPropertyData::Flag methodFlags,
                                         QQmlPropertyData::Flag signalFlags)
{
    return copyAndAppend(engine, metaObject, -1, propertyFlags, methodFlags, signalFlags);
}

QQmlPropertyCache *
QQmlPropertyCache::copyAndAppend(QQmlEngine *engine, const QMetaObject *metaObject,
                                         int revision,
                                         QQmlPropertyData::Flag propertyFlags,
                                         QQmlPropertyData::Flag methodFlags,
                                         QQmlPropertyData::Flag signalFlags)
{
    Q_ASSERT(QMetaObjectPrivate::get(metaObject)->revision >= 4);

    // Reserve enough space in the name hash for all the methods (including signals), all the
    // signal handlers and all the properties.  This assumes no name clashes, but this is the
    // common case.
    QQmlPropertyCache *rv = copy(QMetaObjectPrivate::get(metaObject)->methodCount +
                                         QMetaObjectPrivate::get(metaObject)->signalCount +
                                         QMetaObjectPrivate::get(metaObject)->propertyCount);

    rv->append(engine, metaObject, revision, propertyFlags, methodFlags, signalFlags);

    return rv;
}

void QQmlPropertyCache::append(QQmlEngine *engine, const QMetaObject *metaObject,
                                       int revision,
                                       QQmlPropertyData::Flag propertyFlags,
                                       QQmlPropertyData::Flag methodFlags,
                                       QQmlPropertyData::Flag signalFlags)
{
    Q_UNUSED(revision);

    _metaObject = metaObject;

    bool dynamicMetaObject = isDynamicMetaObject(metaObject);

    allowedRevisionCache.append(0);

    int methodCount = metaObject->methodCount();
    Q_ASSERT(QMetaObjectPrivate::get(metaObject)->revision >= 4);
    int signalCount = metaObjectSignalCount(metaObject);
    int classInfoCount = QMetaObjectPrivate::get(metaObject)->classInfoCount;

    QQmlAccessorProperties::Properties accessorProperties;

    if (classInfoCount) {
        int classInfoOffset = metaObject->classInfoOffset();
        bool hasFastProperty = false;
        for (int ii = 0; ii < classInfoCount; ++ii) {
            int idx = ii + classInfoOffset;

            if (0 == qstrcmp(metaObject->classInfo(idx).name(), "qt_HasQmlAccessors")) {
                hasFastProperty = true;
            } else if (0 == qstrcmp(metaObject->classInfo(idx).name(), "DefaultProperty")) {
                _defaultPropertyName = QString::fromUtf8(metaObject->classInfo(idx).value());
            }
        }

        if (hasFastProperty) {
            accessorProperties = QQmlAccessorProperties::properties(metaObject);
            if (accessorProperties.count == 0)
                qFatal("QQmlPropertyCache: %s has FastProperty class info, but has not "
                       "installed property accessors", metaObject->className());
        } else {
#ifndef QT_NO_DEBUG
            accessorProperties = QQmlAccessorProperties::properties(metaObject);
            if (accessorProperties.count != 0)
                qFatal("QQmlPropertyCache: %s has fast property accessors, but is missing "
                       "FastProperty class info", metaObject->className());
#endif
        }
    }

    //Used to block access to QObject::destroyed() and QObject::deleteLater() from QML
    static const int destroyedIdx1 = QObject::staticMetaObject.indexOfSignal("destroyed(QObject*)");
    static const int destroyedIdx2 = QObject::staticMetaObject.indexOfSignal("destroyed()");
    static const int deleteLaterIdx = QObject::staticMetaObject.indexOfSlot("deleteLater()");

    int methodOffset = metaObject->methodOffset();
    int signalOffset = signalCount - QMetaObjectPrivate::get(metaObject)->signalCount;

    // update() should have reserved enough space in the vector that this doesn't cause a realloc
    // and invalidate the stringCache.
    methodIndexCache.resize(methodCount - methodIndexCacheStart);
    signalHandlerIndexCache.resize(signalCount - signalHandlerIndexCacheStart);
    int signalHandlerIndex = signalOffset;
    for (int ii = methodOffset; ii < methodCount; ++ii) {
        if (ii == destroyedIdx1 || ii == destroyedIdx2 || ii == deleteLaterIdx)
            continue;
        QMetaMethod m = metaObject->method(ii);
        if (m.access() == QMetaMethod::Private)
            continue;

        // Extract method name
        // It's safe to keep the raw name pointer
        Q_ASSERT(QMetaObjectPrivate::get(metaObject)->revision >= 7);
        const char *rawName = m.name().constData();
        const char *cptr = rawName;
        char utf8 = 0;
        while (*cptr) {
            utf8 |= *cptr & 0x80;
            ++cptr;
        }

        QQmlPropertyData *data = &methodIndexCache[ii - methodIndexCacheStart];
        QQmlPropertyData *sigdata = 0;

        data->lazyLoad(m);

        if (data->isSignal())
            data->flags |= signalFlags;
        else
            data->flags |= methodFlags;

        if (!dynamicMetaObject)
            data->flags |= QQmlPropertyData::IsDirect;

        Q_ASSERT((allowedRevisionCache.count() - 1) < Q_INT16_MAX);
        data->metaObjectOffset = allowedRevisionCache.count() - 1;

        if (data->isSignal()) {
            sigdata = &signalHandlerIndexCache[signalHandlerIndex - signalHandlerIndexCacheStart];
            *sigdata = *data;
            sigdata->flags |= QQmlPropertyData::IsSignalHandler;
        }

        QQmlPropertyData *old = 0;

        if (utf8) {
            QHashedString methodName(QString::fromUtf8(rawName, cptr - rawName));
            if (StringCache::mapped_type *it = stringCache.value(methodName))
                old = it->second;
            setNamedProperty(methodName, ii, data, (old != 0));

            if (data->isSignal()) {
                QHashedString on(QStringLiteral("on") % methodName.at(0).toUpper() % methodName.midRef(1));
                setNamedProperty(on, ii, sigdata, (old != 0));
                ++signalHandlerIndex;
            }
        } else {
            QHashedCStringRef methodName(rawName, cptr - rawName);
            if (StringCache::mapped_type *it = stringCache.value(methodName))
                old = it->second;
            setNamedProperty(methodName, ii, data, (old != 0));

            if (data->isSignal()) {
                int length = methodName.length();

                QVarLengthArray<char, 128> str(length+3);
                str[0] = 'o';
                str[1] = 'n';
                str[2] = toupper(rawName[0]);
                if (length > 1)
                    memcpy(&str[3], &rawName[1], length - 1);
                str[length + 2] = '\0';

                QHashedString on(QString::fromLatin1(str.data()));
                setNamedProperty(on, ii, data, (old != 0));
                ++signalHandlerIndex;
            }
        }

        if (old) {
            // We only overload methods in the same class, exactly like C++
            if (old->isFunction() && old->coreIndex >= methodOffset)
                data->flags |= QQmlPropertyData::IsOverload;

            data->markAsOverrideOf(old);
        }
    }

    int propCount = metaObject->propertyCount();
    int propOffset = metaObject->propertyOffset();

    // update() should have reserved enough space in the vector that this doesn't cause a realloc
    // and invalidate the stringCache.
    propertyIndexCache.resize(propCount - propertyIndexCacheStart);
    for (int ii = propOffset; ii < propCount; ++ii) {
        QMetaProperty p = metaObject->property(ii);
        if (!p.isScriptable())
            continue;

        const char *str = p.name();
        char utf8 = 0;
        const char *cptr = str;
        while (*cptr != 0) {
            utf8 |= *cptr & 0x80;
            ++cptr;
        }

        QQmlPropertyData *data = &propertyIndexCache[ii - propertyIndexCacheStart];

        data->lazyLoad(p, engine);
        data->flags |= propertyFlags;

        if (!dynamicMetaObject)
            data->flags |= QQmlPropertyData::IsDirect;

        Q_ASSERT((allowedRevisionCache.count() - 1) < Q_INT16_MAX);
        data->metaObjectOffset = allowedRevisionCache.count() - 1;

        QQmlPropertyData *old = 0;

        if (utf8) {
            QHashedString propName(QString::fromUtf8(str, cptr - str));
            if (StringCache::mapped_type *it = stringCache.value(propName))
                old = it->second;
            setNamedProperty(propName, ii, data, (old != 0));
        } else {
            QHashedCStringRef propName(str, cptr - str);
            if (StringCache::mapped_type *it = stringCache.value(propName))
                old = it->second;
            setNamedProperty(propName, ii, data, (old != 0));
        }

        QQmlAccessorProperties::Property *accessorProperty = accessorProperties.property(str);

        // Fast properties may not be overrides or revisioned
        Q_ASSERT(accessorProperty == 0 || (old == 0 && data->revision == 0));

        if (accessorProperty) {
            data->flags |= QQmlPropertyData::HasAccessors;
            data->accessors = accessorProperty->accessors;
            data->accessorData = accessorProperty->data;
        } else if (old) {
            data->markAsOverrideOf(old);
        }
    }
}

QQmlPropertyData *QQmlPropertyCache::ensureResolved(QQmlPropertyData *p) const
{
    if (p && p->notFullyResolved())
        resolve(p);

    return p;
}

void QQmlPropertyCache::resolve(QQmlPropertyData *data) const
{
    Q_ASSERT(data->notFullyResolved());

    data->propType = QMetaType::type(data->propTypeName);

    if (!data->isFunction())
        data->flags |= flagsForPropertyType(data->propType, engine);

    data->flags &= ~QQmlPropertyData::NotFullyResolved;
}

void QQmlPropertyCache::updateRecur(QQmlEngine *engine, const QMetaObject *metaObject)
{
    if (!metaObject)
        return;

    updateRecur(engine, metaObject->superClass());

    append(engine, metaObject, -1);
}

void QQmlPropertyCache::update(QQmlEngine *engine, const QMetaObject *metaObject)
{
    Q_ASSERT(engine);
    Q_ASSERT(metaObject);
    Q_ASSERT(stringCache.isEmpty());

    // Preallocate enough space in the index caches for all the properties/methods/signals that
    // are not cached in a parent cache so that the caches never need to be reallocated as this
    // would invalidate pointers stored in the stringCache.
    int pc = metaObject->propertyCount();
    int mc = metaObject->methodCount();
    int sc = metaObjectSignalCount(metaObject);
    propertyIndexCache.reserve(pc - propertyIndexCacheStart);
    methodIndexCache.reserve(mc - methodIndexCacheStart);
    signalHandlerIndexCache.reserve(sc - signalHandlerIndexCacheStart);

    // Reserve enough space in the stringCache for all properties/methods/signals including those
    // cached in a parent cache.
    stringCache.reserve(pc + mc + sc);

    updateRecur(engine,metaObject);
}

/*! \internal
    invalidates and updates the PropertyCache if the QMetaObject has changed.
    This function is used in the tooling to update dynamic properties.
*/
void QQmlPropertyCache::invalidate(QQmlEngine *engine, const QMetaObject *metaObject)
{
    stringCache.clear();
    propertyIndexCache.clear();
    methodIndexCache.clear();
    signalHandlerIndexCache.clear();

    _hasPropertyOverrides = false;
    argumentsCache = 0;

    int pc = metaObject->propertyCount();
    int mc = metaObject->methodCount();
    int sc = metaObjectSignalCount(metaObject);
    int reserve = pc + mc + sc;

    if (parent()) {
        propertyIndexCacheStart = parent()->propertyIndexCache.count() + parent()->propertyIndexCacheStart;
        methodIndexCacheStart = parent()->methodIndexCache.count() + parent()->methodIndexCacheStart;
        signalHandlerIndexCacheStart = parent()->signalHandlerIndexCache.count() + parent()->signalHandlerIndexCacheStart;
        stringCache.linkAndReserve(parent()->stringCache, reserve);
        append(engine, metaObject, -1);
    } else {
        propertyIndexCacheStart = 0;
        methodIndexCacheStart = 0;
        signalHandlerIndexCacheStart = 0;
        update(engine, metaObject);
    }
}

/*! \internal
    \a index MUST be in the signal index range (see QObjectPrivate::signalIndex()).
    This is different from QMetaMethod::methodIndex().
*/
QQmlPropertyData *
QQmlPropertyCache::signal(int index, QQmlPropertyCache **c) const
{
    if (index < 0 || index >= (signalHandlerIndexCacheStart + signalHandlerIndexCache.count()))
        return 0;

    if (index < signalHandlerIndexCacheStart)
        return _parent->signal(index, c);

    QQmlPropertyData *rv = const_cast<QQmlPropertyData *>(&methodIndexCache.at(index - signalHandlerIndexCacheStart));
    if (rv->notFullyResolved()) resolve(rv);
    Q_ASSERT(rv->isSignal() || rv->coreIndex == -1);
    if (c) *c = const_cast<QQmlPropertyCache *>(this);
    return rv;
}

int QQmlPropertyCache::methodIndexToSignalIndex(int index) const
{
    if (index < 0 || index >= (methodIndexCacheStart + methodIndexCache.count()))
        return index;

    if (index < methodIndexCacheStart)
        return _parent->methodIndexToSignalIndex(index);

    return index - methodIndexCacheStart + signalHandlerIndexCacheStart;
}

QQmlPropertyData *
QQmlPropertyCache::property(int index) const
{
    if (index < 0 || index >= (propertyIndexCacheStart + propertyIndexCache.count()))
        return 0;

    if (index < propertyIndexCacheStart)
        return _parent->property(index);

    QQmlPropertyData *rv = const_cast<QQmlPropertyData *>(&propertyIndexCache.at(index - propertyIndexCacheStart));
    return ensureResolved(rv);
}

QQmlPropertyData *
QQmlPropertyCache::method(int index) const
{
    if (index < 0 || index >= (methodIndexCacheStart + methodIndexCache.count()))
        return 0;

    if (index < methodIndexCacheStart)
        return _parent->method(index);

    QQmlPropertyData *rv = const_cast<QQmlPropertyData *>(&methodIndexCache.at(index - methodIndexCacheStart));
    return ensureResolved(rv);
}

QQmlPropertyData *QQmlPropertyCache::findProperty(StringCache::ConstIterator it, QObject *object, QQmlContextData *context) const
{
    QQmlData *data = (object ? QQmlData::get(object) : 0);
    const QQmlVMEMetaObject *vmemo = (data && data->hasVMEMetaObject ? static_cast<const QQmlVMEMetaObject *>(object->metaObject()) : 0);
    return findProperty(it, vmemo, context);
}

namespace {

inline bool contextHasNoExtensions(QQmlContextData *context)
{
    // This context has no extension if its parent is the engine's rootContext,
    // which has children but no imports
    return (!context->parent || !context->parent->imports);
}

inline int maximumIndexForProperty(QQmlPropertyData *prop, const QQmlVMEMetaObject *vmemo)
{
    return (prop->isFunction() ? vmemo->methodCount()
                               : prop->isSignalHandler() ? vmemo->signalCount()
                                                         : vmemo->propertyCount());
}

}

QQmlPropertyData *QQmlPropertyCache::findProperty(StringCache::ConstIterator it, const QQmlVMEMetaObject *vmemo, QQmlContextData *context) const
{
    StringCache::ConstIterator end = stringCache.end();

    if (it != end) {
        QQmlPropertyData *result = it.value().second;

        // If there exists a typed property (not a function or signal handler), of the
        // right name available to the specified context, we need to return that
        // property rather than any subsequent override

        if (vmemo && context && !contextHasNoExtensions(context)) {
            // Find the meta-object that corresponds to the supplied context
            do {
                if (vmemo->ctxt == context)
                    break;

                vmemo = vmemo->parentVMEMetaObject();
            } while (vmemo);
        }

        if (vmemo) {
            // Ensure that the property we resolve to is accessible from this meta-object
            do {
                const StringCache::mapped_type &property(it.value());

                if (property.first < maximumIndexForProperty(property.second, vmemo)) {
                    // This property is available in the specified context
                    if (property.second->isFunction() || property.second->isSignalHandler()) {
                        // Prefer the earlier resolution
                    } else {
                        // Prefer the typed property to any previous property found
                        result = property.second;
                    }
                    break;
                }

                // See if there is a better candidate
                it = stringCache.findNext(it);
            } while (it != end);
        }

        return ensureResolved(result);
    }

    return 0;
}

QString QQmlPropertyData::name(QObject *object) const
{
    if (!object)
        return QString();

    return name(object->metaObject());
}

QString QQmlPropertyData::name(const QMetaObject *metaObject) const
{
    if (!metaObject || coreIndex == -1)
        return QString();

    if (flags & IsFunction) {
        QMetaMethod m = metaObject->method(coreIndex);

        return QString::fromUtf8(m.name().constData());
    } else {
        QMetaProperty p = metaObject->property(coreIndex);
        return QString::fromUtf8(p.name());
    }
}

void QQmlPropertyData::markAsOverrideOf(QQmlPropertyData *predecessor)
{
    overrideIndexIsProperty = !predecessor->isFunction();
    overrideIndex = predecessor->coreIndex;

    predecessor->flags |= QQmlPropertyData::IsOverridden;
}

QStringList QQmlPropertyCache::propertyNames() const
{
    QStringList keys;
    for (StringCache::ConstIterator iter = stringCache.begin(); iter != stringCache.end(); ++iter)
        keys.append(iter.key());
    return keys;
}

struct StaticQtMetaObject : public QObject
{
    static const QMetaObject *get()
        { return &staticQtMetaObject; }
};

static int EnumType(const QMetaObject *metaobj, const QByteArray &str, int type)
{
    QByteArray scope;
    QByteArray name;
    int scopeIdx = str.lastIndexOf("::");
    if (scopeIdx != -1) {
        scope = str.left(scopeIdx);
        name = str.mid(scopeIdx + 2);
    } else {
        name = str;
    }
    const QMetaObject *meta;
    if (scope == "Qt")
        meta = StaticQtMetaObject::get();
    else
        meta = metaobj;
    for (int i = meta->enumeratorCount() - 1; i >= 0; --i) {
        QMetaEnum m = meta->enumerator(i);
        if ((m.name() == name) && (scope.isEmpty() || (m.scope() == scope)))
            return QVariant::Int;
    }
    return type;
}

QQmlPropertyCacheMethodArguments *QQmlPropertyCache::createArgumentsObject(int argc, const QList<QByteArray> &names)
{
    typedef QQmlPropertyCacheMethodArguments A;
    A *args = static_cast<A *>(malloc(sizeof(A) + (argc + 1) * sizeof(int)));
    args->arguments[0] = argc;
    args->argumentsValid = false;
    args->signalParameterStringForJS = 0;
    args->parameterError = false;
    args->names = argc ? new QList<QByteArray>(names) : 0;
    args->next = argumentsCache;
    argumentsCache = args;
    return args;
}

/*! \internal
    \a index MUST be in the signal index range (see QObjectPrivate::signalIndex()).
    This is different from QMetaMethod::methodIndex().
*/
QString QQmlPropertyCache::signalParameterStringForJS(int index, QString *errorString)
{
    QQmlPropertyCache *c = 0;
    QQmlPropertyData *signalData = signal(index, &c);
    if (!signalData)
        return QString();

    typedef QQmlPropertyCacheMethodArguments A;

    if (signalData->arguments) {
        A *arguments = static_cast<A *>(signalData->arguments);
        if (arguments->signalParameterStringForJS) {
            if (arguments->parameterError) {
                if (errorString)
                    *errorString = *arguments->signalParameterStringForJS;
                return QString();
            }
            return *arguments->signalParameterStringForJS;
        }
    }

    QList<QByteArray> parameterNameList = signalParameterNames(index);

    if (!signalData->arguments) {
        A *args = c->createArgumentsObject(parameterNameList.count(), parameterNameList);
        signalData->arguments = args;
    }

    QString error;
    QString parameters = signalParameterStringForJS(engine, parameterNameList, &error);

    A *arguments = static_cast<A *>(signalData->arguments);
    arguments->signalParameterStringForJS = new QString(!error.isEmpty() ? error : parameters);
    if (!error.isEmpty()) {
        arguments->parameterError = true;
        if (errorString)
            *errorString = *arguments->signalParameterStringForJS;
        return QString();
    }
    return *arguments->signalParameterStringForJS;
}

QString QQmlPropertyCache::signalParameterStringForJS(QQmlEngine *engine, const QList<QByteArray> &parameterNameList, QString *errorString)
{
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine);
    bool unnamedParameter = false;
    const QSet<QString> &illegalNames = ep->v8engine()->illegalNames();
    QString error;
    QString parameters;

    for (int i = 0; i < parameterNameList.count(); ++i) {
        if (i > 0)
            parameters += QLatin1Char(',');
        const QByteArray &param = parameterNameList.at(i);
        if (param.isEmpty())
            unnamedParameter = true;
        else if (unnamedParameter) {
            if (errorString)
                *errorString = QCoreApplication::translate("QQmlRewrite", "Signal uses unnamed parameter followed by named parameter.");
            return QString();
        } else if (illegalNames.contains(QString::fromUtf8(param))) {
            if (errorString)
                *errorString = QCoreApplication::translate("QQmlRewrite", "Signal parameter \"%1\" hides global variable.").arg(QString::fromUtf8(param));
            return QString();
        }
        parameters += QString::fromUtf8(param);
    }

    return parameters;
}

// Returns an array of the arguments for method \a index.  The first entry in the array
// is the number of arguments.
int *QQmlPropertyCache::methodParameterTypes(QObject *object, int index,
                                                     QVarLengthArray<int, 9> &dummy,
                                                     QByteArray *unknownTypeError)
{
    Q_ASSERT(object && index >= 0);

    QQmlData *ddata = QQmlData::get(object, false);

    if (ddata && ddata->propertyCache) {
        typedef QQmlPropertyCacheMethodArguments A;

        QQmlPropertyCache *c = ddata->propertyCache;
        Q_ASSERT(index < c->methodIndexCacheStart + c->methodIndexCache.count());

        while (index < c->methodIndexCacheStart)
            c = c->_parent;

        QQmlPropertyData *rv = const_cast<QQmlPropertyData *>(&c->methodIndexCache.at(index - c->methodIndexCacheStart));

        if (rv->arguments && static_cast<A *>(rv->arguments)->argumentsValid)
            return static_cast<A *>(rv->arguments)->arguments;

        const QMetaObject *metaObject = c->createMetaObject();
        Q_ASSERT(metaObject);
        QMetaMethod m = metaObject->method(index);

        int argc = m.parameterCount();
        if (!rv->arguments) {
            A *args = c->createArgumentsObject(argc, m.parameterNames());
            rv->arguments = args;
        }
        A *args = static_cast<A *>(rv->arguments);

        QList<QByteArray> argTypeNames; // Only loaded if needed

        for (int ii = 0; ii < argc; ++ii) {
            int type = m.parameterType(ii);
            QMetaType::TypeFlags flags = QMetaType::typeFlags(type);
            if (flags & QMetaType::IsEnumeration)
                type = QVariant::Int;
            else if (type == QMetaType::UnknownType ||
                     (type >= (int)QVariant::UserType && !(flags & QMetaType::PointerToQObject) &&
                      type != qMetaTypeId<QJSValue>())) {
                //the UserType clause is to catch registered QFlags
                if (argTypeNames.isEmpty())
                    argTypeNames = m.parameterTypes();
                type = EnumType(object->metaObject(), argTypeNames.at(ii), type);
            }
            if (type == QMetaType::UnknownType) {
                if (unknownTypeError) *unknownTypeError = argTypeNames.at(ii);
                return 0;
            }
            args->arguments[ii + 1] = type;
        }
        args->argumentsValid = true;
        return static_cast<A *>(rv->arguments)->arguments;

    } else {
        QMetaMethod m = object->metaObject()->method(index);
        int argc = m.parameterCount();
        dummy.resize(argc + 1);
        dummy[0] = argc;
        QList<QByteArray> argTypeNames; // Only loaded if needed

        for (int ii = 0; ii < argc; ++ii) {
            int type = m.parameterType(ii);
            QMetaType::TypeFlags flags = QMetaType::typeFlags(type);
            if (flags & QMetaType::IsEnumeration)
                type = QVariant::Int;
            else if (type == QMetaType::UnknownType ||
                     (type >= (int)QVariant::UserType && !(flags & QMetaType::PointerToQObject) &&
                      type != qMetaTypeId<QJSValue>())) {
                //the UserType clause is to catch registered QFlags)
                if (argTypeNames.isEmpty())
                    argTypeNames = m.parameterTypes();
                type = EnumType(object->metaObject(), argTypeNames.at(ii), type);
            }
            if (type == QMetaType::UnknownType) {
                if (unknownTypeError) *unknownTypeError = argTypeNames.at(ii);
                return 0;
            }
            dummy[ii + 1] = type;
        }

        return dummy.data();
    }
}

// Returns the return type of the method.
int QQmlPropertyCache::methodReturnType(QObject *object, const QQmlPropertyData &data,
                                        QByteArray *unknownTypeError)
{
    Q_ASSERT(object && data.coreIndex >= 0);

    int type = data.propType;

    const char *propTypeName = 0;

    if (type == QMetaType::UnknownType) {
        // Find the return type name from the method info
        QMetaMethod m;

        QQmlData *ddata = QQmlData::get(object, false);
        if (ddata && ddata->propertyCache) {
            QQmlPropertyCache *c = ddata->propertyCache;
            Q_ASSERT(data.coreIndex < c->methodIndexCacheStart + c->methodIndexCache.count());

            while (data.coreIndex < c->methodIndexCacheStart)
                c = c->_parent;

            const QMetaObject *metaObject = c->createMetaObject();
            Q_ASSERT(metaObject);
            m = metaObject->method(data.coreIndex);
        } else {
            m = object->metaObject()->method(data.coreIndex);
        }

        type = m.returnType();
        propTypeName = m.typeName();
    }

    QMetaType::TypeFlags flags = QMetaType::typeFlags(type);
    if (flags & QMetaType::IsEnumeration) {
        type = QVariant::Int;
    } else if (type == QMetaType::UnknownType ||
               (type >= (int)QVariant::UserType && !(flags & QMetaType::PointerToQObject) &&
               type != qMetaTypeId<QJSValue>())) {
        //the UserType clause is to catch registered QFlags
        type = EnumType(object->metaObject(), propTypeName, type);
    }

    if (type == QMetaType::UnknownType) {
        if (unknownTypeError) *unknownTypeError = propTypeName;
    }

    return type;
}

int QQmlPropertyCache::originalClone(int index)
{
    while (signal(index)->isCloned())
        --index;
    return index;
}

int QQmlPropertyCache::originalClone(QObject *object, int index)
{
    QQmlData *data = QQmlData::get(object, false);
    if (data && data->propertyCache) {
        QQmlPropertyCache *cache = data->propertyCache;
        while (cache->signal(index)->isCloned())
            --index;
    } else {
        while (QMetaObjectPrivate::signal(object->metaObject(), index).attributes() & QMetaMethod::Cloned)
            --index;
    }
    return index;
}

QQmlPropertyData qQmlPropertyCacheCreate(const QMetaObject *metaObject, const QString &property)
{
    Q_ASSERT(metaObject);

    QQmlPropertyData rv;

    /* It's important to check the method list before checking for properties;
     * otherwise, if the meta object is dynamic, a property will be created even
     * if not found and it might obscure a method having the same name. */

    //Used to block access to QObject::destroyed() and QObject::deleteLater() from QML
    static const int destroyedIdx1 = QObject::staticMetaObject.indexOfSignal("destroyed(QObject*)");
    static const int destroyedIdx2 = QObject::staticMetaObject.indexOfSignal("destroyed()");
    static const int deleteLaterIdx = QObject::staticMetaObject.indexOfSlot("deleteLater()");

    const QByteArray propertyName = property.toUtf8();

    int methodCount = metaObject->methodCount();
    for (int ii = methodCount - 1; ii >= 0; --ii) {
        if (ii == destroyedIdx1 || ii == destroyedIdx2 || ii == deleteLaterIdx)
            continue;
        QMetaMethod m = metaObject->method(ii);
        if (m.access() == QMetaMethod::Private)
            continue;

        if (m.name() == propertyName) {
            rv.load(m);
            return rv;
        }
    }

    {
        const QMetaObject *cmo = metaObject;
        while (cmo) {
            int idx = cmo->indexOfProperty(propertyName);
            if (idx != -1) {
                QMetaProperty p = cmo->property(idx);
                if (p.isScriptable()) {
                    rv.load(p);
                    return rv;
                } else {
                    bool changed = false;
                    while (cmo && cmo->propertyOffset() >= idx) {
                        cmo = cmo->superClass();
                        changed = true;
                    }
                    /* If the "cmo" variable didn't change, set it to 0 to
                     * avoid running into an infinite loop */
                    if (!changed) cmo = 0;
                }
            } else {
                cmo = 0;
            }
        }
    }
    return rv;
}

inline const QString &qQmlPropertyCacheToString(const QString &string)
{
    return string;
}

inline QString qQmlPropertyCacheToString(const QV4::String *string)
{
    return string->toQString();
}

template<typename T>
QQmlPropertyData *
qQmlPropertyCacheProperty(QQmlEngine *engine, QObject *obj, T name,
                          QQmlContextData *context, QQmlPropertyData &local)
{
    QQmlPropertyCache *cache = 0;

    QQmlData *ddata = QQmlData::get(obj, false);

    if (ddata && ddata->propertyCache) {
        cache = ddata->propertyCache;
    } else if (engine) {
        QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine);
        cache = ep->cache(obj);
        if (cache) {
            ddata = QQmlData::get(obj, true);
            cache->addref();
            ddata->propertyCache = cache;
        }
    }

    QQmlPropertyData *rv = 0;

    if (cache) {
        rv = cache->property(name, obj, context);
    } else {
        local = qQmlPropertyCacheCreate(obj->metaObject(), qQmlPropertyCacheToString(name));
        if (local.isValid())
            rv = &local;
    }

    return rv;
}

QQmlPropertyData *
QQmlPropertyCache::property(QQmlEngine *engine, QObject *obj, const QV4::String *name,
                            QQmlContextData *context, QQmlPropertyData &local)
{
    return qQmlPropertyCacheProperty<const QV4::String *>(engine, obj, name, context, local);
}

QQmlPropertyData *
QQmlPropertyCache::property(QQmlEngine *engine, QObject *obj,
                                    const QString &name, QQmlContextData *context, QQmlPropertyData &local)
{
    return qQmlPropertyCacheProperty<const QString &>(engine, obj, name, context, local);
}

static inline const QMetaObjectPrivate *priv(const uint* data)
{ return reinterpret_cast<const QMetaObjectPrivate*>(data); }

bool QQmlPropertyCache::isDynamicMetaObject(const QMetaObject *mo)
{
    return priv(mo->d.data)->revision >= 3 && priv(mo->d.data)->flags & DynamicMetaObject;
}

const char *QQmlPropertyCache::className() const
{
    if (!_ownMetaObject && _metaObject)
        return _metaObject->className();
    else
        return _dynamicClassName.constData();
}

void QQmlPropertyCache::toMetaObjectBuilder(QMetaObjectBuilder &builder)
{
    struct Sort { static bool lt(const QPair<QString, QQmlPropertyData *> &lhs,
                                 const QPair<QString, QQmlPropertyData *> &rhs) {
        return lhs.second->coreIndex < rhs.second->coreIndex;
    } };

    struct Insert { static void in(QQmlPropertyCache *This,
                                   QList<QPair<QString, QQmlPropertyData *> > &properties,
                                   QList<QPair<QString, QQmlPropertyData *> > &methods,
                                   StringCache::ConstIterator iter, QQmlPropertyData *data) {
        if (data->isSignalHandler())
            return;

        if (data->isFunction()) {
            if (data->coreIndex < This->methodIndexCacheStart)
                return;

            QPair<QString, QQmlPropertyData *> entry = qMakePair((QString)iter.key(), data);
            // Overrides can cause the entry to already exist
            if (!methods.contains(entry)) methods.append(entry);

            data = This->overrideData(data);
            if (data && !data->isFunction()) Insert::in(This, properties, methods, iter, data);
        } else {
            if (data->coreIndex < This->propertyIndexCacheStart)
                return;

            QPair<QString, QQmlPropertyData *> entry = qMakePair((QString)iter.key(), data);
            // Overrides can cause the entry to already exist
            if (!properties.contains(entry)) properties.append(entry);

            data = This->overrideData(data);
            if (data) Insert::in(This, properties, methods, iter, data);
        }

    } };

    builder.setClassName(_dynamicClassName);

    QList<QPair<QString, QQmlPropertyData *> > properties;
    QList<QPair<QString, QQmlPropertyData *> > methods;

    for (StringCache::ConstIterator iter = stringCache.begin(); iter != stringCache.end(); ++iter)
        Insert::in(this, properties, methods, iter, iter.value().second);

    Q_ASSERT(properties.count() == propertyIndexCache.count());
    Q_ASSERT(methods.count() == methodIndexCache.count());

    std::sort(properties.begin(), properties.end(), Sort::lt);
    std::sort(methods.begin(), methods.end(), Sort::lt);

    for (int ii = 0; ii < properties.count(); ++ii) {
        QQmlPropertyData *data = properties.at(ii).second;

        int notifierId = -1;
        if (data->notifyIndex != -1)
            notifierId = data->notifyIndex - signalHandlerIndexCacheStart;

        QMetaPropertyBuilder property = builder.addProperty(properties.at(ii).first.toUtf8(),
                                                            QMetaType::typeName(data->propType),
                                                            notifierId);

        property.setReadable(true);
        property.setWritable(data->isWritable());
        property.setResettable(data->isResettable());
    }

    for (int ii = 0; ii < methods.count(); ++ii) {
        QQmlPropertyData *data = methods.at(ii).second;

        QByteArray returnType;
        if (data->propType != 0)
            returnType = QMetaType::typeName(data->propType);

        QByteArray signature = methods.at(ii).first.toUtf8() + "(";

        QQmlPropertyCacheMethodArguments *arguments = 0;
        if (data->hasArguments()) {
            arguments = (QQmlPropertyCacheMethodArguments *)data->arguments;
            Q_ASSERT(arguments->argumentsValid);
            for (int ii = 0; ii < arguments->arguments[0]; ++ii) {
                if (ii != 0) signature.append(",");
                signature.append(QMetaType::typeName(arguments->arguments[1 + ii]));
            }
        }

        signature.append(")");

        QMetaMethodBuilder method;
        if (data->isSignal()) {
            method = builder.addSignal(signature);
        } else {
            method = builder.addSlot(signature);
        }
        method.setAccess(QMetaMethod::Public);

        if (arguments && arguments->names)
            method.setParameterNames(*arguments->names);

        if (!returnType.isEmpty())
            method.setReturnType(returnType);
    }

    if (!_defaultPropertyName.isEmpty()) {
        QQmlPropertyData *dp = property(_defaultPropertyName, 0, 0);
        if (dp && dp->coreIndex >= propertyIndexCacheStart) {
            Q_ASSERT(!dp->isFunction());
            builder.addClassInfo("DefaultProperty", _defaultPropertyName.toUtf8());
        }
    }
}

/*! \internal
    \a index MUST be in the signal index range (see QObjectPrivate::signalIndex()).
    This is different from QMetaMethod::methodIndex().
*/
QList<QByteArray> QQmlPropertyCache::signalParameterNames(int index) const
{
    QQmlPropertyData *signalData = signal(index);
    if (signalData && signalData->hasArguments()) {
        QQmlPropertyCacheMethodArguments *args = (QQmlPropertyCacheMethodArguments *)signalData->arguments;
        if (args && args->names)
            return *args->names;
        const QMetaMethod &method = QMetaObjectPrivate::signal(firstCppMetaObject(), index);
        return method.parameterNames();
    }
    return QList<QByteArray>();
}

// Returns true if \a from is assignable to a property of type \a to
bool QQmlMetaObject::canConvert(const QQmlMetaObject &from, const QQmlMetaObject &to)
{
    Q_ASSERT(!from.isNull() && !to.isNull());

    struct I { static bool equal(const QMetaObject *lhs, const QMetaObject *rhs) {
        return lhs == rhs || (lhs && rhs && lhs->d.stringdata == rhs->d.stringdata);
    } };

    const QMetaObject *tom = to._m.isT1()?to._m.asT1()->metaObject():to._m.asT2();
    if (tom == &QObject::staticMetaObject) return true;

    if (from._m.isT1() && to._m.isT1()) { // QQmlPropertyCache -> QQmlPropertyCache
        QQmlPropertyCache *fromp = from._m.asT1();
        QQmlPropertyCache *top = to._m.asT1();

        while (fromp) {
            if (fromp == top) return true;
            fromp = fromp->parent();
        }
    } else if (from._m.isT1() && to._m.isT2()) { // QQmlPropertyCache -> QMetaObject
        QQmlPropertyCache *fromp = from._m.asT1();

        while (fromp) {
            const QMetaObject *fromm = fromp->metaObject();
            if (fromm && I::equal(fromm, tom)) return true;
            fromp = fromp->parent();
        }
    } else if (from._m.isT2() && to._m.isT1()) { // QMetaObject -> QQmlPropertyCache
        const QMetaObject *fromm = from._m.asT2();

        if (!tom) return false;

        while (fromm) {
            if (I::equal(fromm, tom)) return true;
            fromm = fromm->superClass();
        }
    } else { // QMetaObject -> QMetaObject
        const QMetaObject *fromm = from._m.asT2();

        while (fromm) {
            if (I::equal(fromm, tom)) return true;
            fromm = fromm->superClass();
        }
    }

    return false;
}

QQmlPropertyCache *QQmlMetaObject::propertyCache(QQmlEnginePrivate *e) const
{
    if (_m.isNull()) return 0;
    if (_m.isT1()) return _m.asT1();
    else return e->cache(_m.asT2());
}

QT_END_NAMESPACE
