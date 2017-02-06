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

#include "qqmlpropertycache_p.h"

#include <private/qqmlengine_p.h>
#include <private/qqmlbinding_p.h>
#include <private/qqmlvmemetaobject_p.h>
#include <private/qv8engine_p.h>

#include <private/qmetaobject_p.h>
#include <private/qmetaobjectbuilder_p.h>

#include <private/qv4value_p.h>

#include <QtCore/qdebug.h>
#include <QtCore/QCryptographicHash>

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

    flags.isConstant = p.isConstant();
    flags.isWritable = p.isWritable();
    flags.isResettable = p.isResettable();
    flags.isFinal = p.isFinal();

    if (p.isEnumType())
        flags.type = QQmlPropertyData::Flags::EnumType;

    return flags;
}

// Flags that do depend on the property's QMetaProperty::userType() and thus are slow to
// load
static void flagsForPropertyType(int propType, QQmlEngine *engine, QQmlPropertyData::Flags &flags)
{
    Q_ASSERT(propType != -1);

    if (propType == QMetaType::QObjectStar) {
        flags.type = QQmlPropertyData::Flags::QObjectDerivedType;
    } else if (propType == QMetaType::QVariant) {
        flags.type = QQmlPropertyData::Flags::QVariantType;
    } else if (propType < static_cast<int>(QVariant::UserType)) {
        // nothing to do
    } else if (propType == qMetaTypeId<QQmlBinding *>()) {
        flags.type = QQmlPropertyData::Flags::QmlBindingType;
    } else if (propType == qMetaTypeId<QJSValue>()) {
        flags.type = QQmlPropertyData::Flags::QJSValueType;
    } else if (propType == qMetaTypeId<QQmlV4Handle>()) {
        flags.type = QQmlPropertyData::Flags::V4HandleType;
    } else {
        QQmlMetaType::TypeCategory cat =
            engine ? QQmlEnginePrivate::get(engine)->typeCategory(propType)
                   : QQmlMetaType::typeCategory(propType);

        if (cat == QQmlMetaType::Object || QMetaType::typeFlags(propType) & QMetaType::PointerToQObject)
            flags.type = QQmlPropertyData::Flags::QObjectDerivedType;
        else if (cat == QQmlMetaType::List)
            flags.type = QQmlPropertyData::Flags::QListType;
    }
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
    auto flags = fastFlagsForProperty(p);
    flagsForPropertyType(p.userType(), engine, flags);
    return flags;
}

void QQmlPropertyData::lazyLoad(const QMetaProperty &p)
{
    setCoreIndex(p.propertyIndex());
    setNotifyIndex(QMetaObjectPrivate::signalIndex(p.notifySignal()));
    Q_ASSERT(p.revision() <= Q_INT16_MAX);
    setRevision(p.revision());

    setFlags(fastFlagsForProperty(p));

    int type = static_cast<int>(p.type());
    if (type == QMetaType::QObjectStar) {
        setPropType(type);
        _flags.type = Flags::QObjectDerivedType;
    } else if (type == QMetaType::QVariant) {
        setPropType(type);
        _flags.type = Flags::QVariantType;
    } else if (type == QVariant::UserType || type == -1) {
        _flags.notFullyResolved = true;
    } else {
        setPropType(type);
    }
}

void QQmlPropertyData::load(const QMetaProperty &p, QQmlEngine *engine)
{
    setPropType(p.userType());
    setCoreIndex(p.propertyIndex());
    setNotifyIndex(QMetaObjectPrivate::signalIndex(p.notifySignal()));
    setFlags(fastFlagsForProperty(p));
    flagsForPropertyType(propType(), engine, _flags);
    Q_ASSERT(p.revision() <= Q_INT16_MAX);
    setRevision(p.revision());
}

void QQmlPropertyData::load(const QMetaMethod &m)
{
    setCoreIndex(m.methodIndex());
    setArguments(nullptr);

    setPropType(m.returnType());

    _flags.type = Flags::FunctionType;
    if (m.methodType() == QMetaMethod::Signal)
        _flags.isSignal = true;
    else if (m.methodType() == QMetaMethod::Constructor) {
        _flags.isConstructor = true;
        setPropType(QMetaType::QObjectStar);
    }

    if (m.parameterCount()) {
        _flags.hasArguments = true;
        if ((m.parameterCount() == 1) && (m.parameterTypes().first() == "QQmlV4Function*")) {
            _flags.isV4Function = true;
        }
    }

    if (m.attributes() & QMetaMethod::Cloned)
        _flags.isCloned = true;

    Q_ASSERT(m.revision() <= Q_INT16_MAX);
    setRevision(m.revision());
}

void QQmlPropertyData::lazyLoad(const QMetaMethod &m)
{
    setCoreIndex(m.methodIndex());
    setPropType(QMetaType::Void);
    setArguments(nullptr);
    _flags.type = Flags::FunctionType;
    if (m.methodType() == QMetaMethod::Signal)
        _flags.isSignal = true;
    else if (m.methodType() == QMetaMethod::Constructor) {
        _flags.isConstructor = true;
        setPropType(QMetaType::QObjectStar);
    }

    const char *returnType = m.typeName();
    if (!returnType)
        returnType = "\0";
    if ((*returnType != 'v') || (qstrcmp(returnType+1, "oid") != 0)) {
        _flags.notFullyResolved = true;
    }

    const int paramCount = m.parameterCount();
    if (paramCount) {
        _flags.hasArguments = true;
        if ((paramCount == 1) && (m.parameterTypes().first() == "QQmlV4Function*")) {
            _flags.isV4Function = true;
        }
    }

    if (m.attributes() & QMetaMethod::Cloned)
        _flags.isCloned = true;

    Q_ASSERT(m.revision() <= Q_INT16_MAX);
    setRevision(m.revision());
}

/*!
Creates a new empty QQmlPropertyCache.
*/
QQmlPropertyCache::QQmlPropertyCache(QV4::ExecutionEngine *e)
    : engine(e), _parent(0), propertyIndexCacheStart(0), methodIndexCacheStart(0),
      signalHandlerIndexCacheStart(0), _hasPropertyOverrides(false), _ownMetaObject(false),
      _metaObject(0), argumentsCache(0), _jsFactoryMethodIndex(-1)
{
    Q_ASSERT(engine);
}

/*!
Creates a new QQmlPropertyCache of \a metaObject.
*/
QQmlPropertyCache::QQmlPropertyCache(QV4::ExecutionEngine *e, const QMetaObject *metaObject)
    : QQmlPropertyCache(e)
{
    Q_ASSERT(metaObject);

    update(metaObject);
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

    if (_ownMetaObject) free(const_cast<QMetaObject *>(_metaObject));
    _metaObject = 0;
    _parent = 0;
    engine = 0;
}

void QQmlPropertyCache::destroy()
{
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

QQmlPropertyCache *QQmlPropertyCache::copyAndReserve(int propertyCount, int methodCount,
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
void QQmlPropertyCache::appendProperty(const QString &name, QQmlPropertyData::Flags flags,
                                       int coreIndex, int propType, int notifyIndex)
{
    QQmlPropertyData data;
    data.setPropType(propType);
    data.setCoreIndex(coreIndex);
    data.setNotifyIndex(notifyIndex);
    data.setFlags(flags);

    QQmlPropertyData *old = findNamedProperty(name);
    if (old)
        data.markAsOverrideOf(old);

    int index = propertyIndexCache.count();
    propertyIndexCache.append(data);

    setNamedProperty(name, index + propertyOffset(), propertyIndexCache.data() + index, (old != 0));
}

void QQmlPropertyCache::appendSignal(const QString &name, QQmlPropertyData::Flags flags,
                                     int coreIndex, const int *types,
                                     const QList<QByteArray> &names)
{
    QQmlPropertyData data;
    data.setPropType(QVariant::Invalid);
    data.setCoreIndex(coreIndex);
    data.setFlags(flags);
    data.setArguments(nullptr);

    QQmlPropertyData handler = data;
    handler._flags.isSignalHandler = true;

    if (types) {
        int argumentCount = *types;
        QQmlPropertyCacheMethodArguments *args = createArgumentsObject(argumentCount, names);
        ::memcpy(args->arguments, types, (argumentCount + 1) * sizeof(int));
        args->argumentsValid = true;
        data.setArguments(args);
    }

    QQmlPropertyData *old = findNamedProperty(name);
    if (old)
        data.markAsOverrideOf(old);

    int methodIndex = methodIndexCache.count();
    methodIndexCache.append(data);

    int signalHandlerIndex = signalHandlerIndexCache.count();
    signalHandlerIndexCache.append(handler);

    QString handlerName = QLatin1String("on") + name;
    handlerName[2] = handlerName.at(2).toUpper();

    setNamedProperty(name, methodIndex + methodOffset(), methodIndexCache.data() + methodIndex, (old != 0));
    setNamedProperty(handlerName, signalHandlerIndex + signalOffset(), signalHandlerIndexCache.data() + signalHandlerIndex, (old != 0));
}

void QQmlPropertyCache::appendMethod(const QString &name, QQmlPropertyData::Flags flags,
                                     int coreIndex, const QList<QByteArray> &names)
{
    int argumentCount = names.count();

    QQmlPropertyData data;
    data.setPropType(QMetaType::QVariant);
    data.setCoreIndex(coreIndex);

    QQmlPropertyCacheMethodArguments *args = createArgumentsObject(argumentCount, names);
    for (int ii = 0; ii < argumentCount; ++ii)
        args->arguments[ii + 1] = QMetaType::QVariant;
    args->argumentsValid = true;
    data.setArguments(args);

    data.setFlags(flags);

    QQmlPropertyData *old = findNamedProperty(name);
    if (old)
        data.markAsOverrideOf(old);

    int methodIndex = methodIndexCache.count();
    methodIndexCache.append(data);

    setNamedProperty(name, methodIndex + methodOffset(), methodIndexCache.data() + methodIndex, (old != 0));
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

QQmlPropertyData *QQmlPropertyCache::defaultProperty() const
{
    return property(defaultPropertyName(), 0, 0);
}

void QQmlPropertyCache::setParent(QQmlPropertyCache *newParent)
{
    if (_parent == newParent)
        return;
    if (_parent)
        _parent->release();
    _parent = newParent;
    _parent->addref();
}

QQmlPropertyCache *
QQmlPropertyCache::copyAndAppend(const QMetaObject *metaObject,
                                 QQmlPropertyData::Flags propertyFlags,
                                 QQmlPropertyData::Flags methodFlags,
                                 QQmlPropertyData::Flags signalFlags)
{
    return copyAndAppend(metaObject, -1, propertyFlags, methodFlags, signalFlags);
}

QQmlPropertyCache *
QQmlPropertyCache::copyAndAppend(const QMetaObject *metaObject,
                                 int revision,
                                 QQmlPropertyData::Flags propertyFlags,
                                 QQmlPropertyData::Flags methodFlags,
                                 QQmlPropertyData::Flags signalFlags)
{
    Q_ASSERT(QMetaObjectPrivate::get(metaObject)->revision >= 4);

    // Reserve enough space in the name hash for all the methods (including signals), all the
    // signal handlers and all the properties.  This assumes no name clashes, but this is the
    // common case.
    QQmlPropertyCache *rv = copy(QMetaObjectPrivate::get(metaObject)->methodCount +
                                         QMetaObjectPrivate::get(metaObject)->signalCount +
                                         QMetaObjectPrivate::get(metaObject)->propertyCount);

    rv->append(metaObject, revision, propertyFlags, methodFlags, signalFlags);

    return rv;
}

void QQmlPropertyCache::append(const QMetaObject *metaObject,
                               int revision,
                               QQmlPropertyData::Flags propertyFlags,
                               QQmlPropertyData::Flags methodFlags,
                               QQmlPropertyData::Flags signalFlags)
{
    Q_UNUSED(revision);

    _metaObject = metaObject;

    bool dynamicMetaObject = isDynamicMetaObject(metaObject);

    allowedRevisionCache.append(0);

    int methodCount = metaObject->methodCount();
    Q_ASSERT(QMetaObjectPrivate::get(metaObject)->revision >= 4);
    int signalCount = metaObjectSignalCount(metaObject);
    int classInfoCount = QMetaObjectPrivate::get(metaObject)->classInfoCount;

    if (classInfoCount) {
        int classInfoOffset = metaObject->classInfoOffset();
        for (int ii = 0; ii < classInfoCount; ++ii) {
            int idx = ii + classInfoOffset;
            QMetaClassInfo mci = metaObject->classInfo(idx);
            const char *name = mci.name();
            if (0 == qstrcmp(name, "DefaultProperty")) {
                _defaultPropertyName = QString::fromUtf8(mci.value());
            } else if (0 == qstrcmp(name, "qt_QmlJSWrapperFactoryMethod")) {
                const char * const factoryMethod = mci.value();
                _jsFactoryMethodIndex = metaObject->indexOfSlot(factoryMethod);
                if (_jsFactoryMethodIndex != -1)
                    _jsFactoryMethodIndex -= metaObject->methodOffset();
            }
        }
    }

    //Used to block access to QObject::destroyed() and QObject::deleteLater() from QML
    static const int destroyedIdx1 = QObject::staticMetaObject.indexOfSignal("destroyed(QObject*)");
    static const int destroyedIdx2 = QObject::staticMetaObject.indexOfSignal("destroyed()");
    static const int deleteLaterIdx = QObject::staticMetaObject.indexOfSlot("deleteLater()");
    // These indices don't apply to gadgets, so don't block them.
    const bool preventDestruction = metaObject->superClass() || metaObject == &QObject::staticMetaObject;

    int methodOffset = metaObject->methodOffset();
    int signalOffset = signalCount - QMetaObjectPrivate::get(metaObject)->signalCount;

    // update() should have reserved enough space in the vector that this doesn't cause a realloc
    // and invalidate the stringCache.
    methodIndexCache.resize(methodCount - methodIndexCacheStart);
    signalHandlerIndexCache.resize(signalCount - signalHandlerIndexCacheStart);
    int signalHandlerIndex = signalOffset;
    for (int ii = methodOffset; ii < methodCount; ++ii) {
        if (preventDestruction && (ii == destroyedIdx1 || ii == destroyedIdx2 || ii == deleteLaterIdx))
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

        if (m.methodType() == QMetaMethod::Signal)
            data->setFlags(signalFlags);
        else
            data->setFlags(methodFlags);

        data->lazyLoad(m);
        data->_flags.isDirect = !dynamicMetaObject;

        Q_ASSERT((allowedRevisionCache.count() - 1) < Q_INT16_MAX);
        data->setMetaObjectOffset(allowedRevisionCache.count() - 1);

        if (data->isSignal()) {
            sigdata = &signalHandlerIndexCache[signalHandlerIndex - signalHandlerIndexCacheStart];
            *sigdata = *data;
            sigdata->_flags.isSignalHandler = true;
        }

        QQmlPropertyData *old = 0;

        if (utf8) {
            QHashedString methodName(QString::fromUtf8(rawName, cptr - rawName));
            if (StringCache::mapped_type *it = stringCache.value(methodName))
                old = it->second;
            setNamedProperty(methodName, ii, data, (old != 0));

            if (data->isSignal()) {
                QHashedString on(QLatin1String("on") % methodName.at(0).toUpper() % methodName.midRef(1));
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
            if (old->isFunction() && old->coreIndex() >= methodOffset)
                data->_flags.isOverload = true;

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

        data->setFlags(propertyFlags);
        data->lazyLoad(p);

        data->_flags.isDirect = !dynamicMetaObject;

        Q_ASSERT((allowedRevisionCache.count() - 1) < Q_INT16_MAX);
        data->setMetaObjectOffset(allowedRevisionCache.count() - 1);

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

        bool isGadget = true;
        for (const QMetaObject *it = metaObject; it != nullptr; it = it->superClass()) {
            if (it == &QObject::staticMetaObject)
                isGadget = false;
        }

        if (isGadget) // always dispatch over a 'normal' meta-call so the QQmlValueType can intercept
            data->_flags.isDirect = false;
        else
            data->trySetStaticMetaCallFunction(metaObject->d.static_metacall, ii - propOffset);
        if (old)
            data->markAsOverrideOf(old);
    }
}

void QQmlPropertyCache::resolve(QQmlPropertyData *data) const
{
    Q_ASSERT(data->notFullyResolved());
    data->_flags.notFullyResolved = false;

    const QMetaObject *mo = firstCppMetaObject();
    if (data->isFunction()) {
        auto metaMethod = mo->method(data->coreIndex());
        const char *retTy = metaMethod.typeName();
        if (!retTy)
            retTy = "\0";
        data->setPropType(QMetaType::type(retTy));
    } else {
        auto metaProperty = mo->property(data->coreIndex());
        data->setPropType(QMetaType::type(metaProperty.typeName()));
    }

    if (!data->isFunction()) {
        if (data->propType() == QMetaType::UnknownType) {
            QQmlPropertyCache *p = _parent;
            while (p && (!mo || _ownMetaObject)) {
                mo = p->_metaObject;
                p = p->_parent;
            }

            int propOffset = mo->propertyOffset();
            if (mo && data->coreIndex() < propOffset + mo->propertyCount()) {
                while (data->coreIndex() < propOffset) {
                    mo = mo->superClass();
                    propOffset = mo->propertyOffset();
                }

                int registerResult = -1;
                void *argv[] = { &registerResult };
                mo->static_metacall(QMetaObject::RegisterPropertyMetaType, data->coreIndex() - propOffset, argv);
                data->setPropType(registerResult == -1 ? QMetaType::UnknownType : registerResult);
            }
        }
        flagsForPropertyType(data->propType(), engine->qmlEngine(), data->_flags);
    }
}

void QQmlPropertyCache::updateRecur(const QMetaObject *metaObject)
{
    if (!metaObject)
        return;

    updateRecur(metaObject->superClass());

    append(metaObject, -1);
}

void QQmlPropertyCache::update(const QMetaObject *metaObject)
{
    Q_ASSERT(metaObject);
    stringCache.clear();

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

    updateRecur(metaObject);
}

/*! \internal
    invalidates and updates the PropertyCache if the QMetaObject has changed.
    This function is used in the tooling to update dynamic properties.
*/
void QQmlPropertyCache::invalidate(const QMetaObject *metaObject)
{
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
        append(metaObject, -1);
    } else {
        propertyIndexCacheStart = 0;
        methodIndexCacheStart = 0;
        signalHandlerIndexCacheStart = 0;
        update(metaObject);
    }
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

inline int maximumIndexForProperty(QQmlPropertyData *prop, const int methodCount, const int signalCount, const int propertyCount)
{
    return prop->isFunction() ? methodCount
                              : prop->isSignalHandler() ? signalCount
                                                        : propertyCount;
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
            const int methodCount = vmemo->methodCount();
            const int signalCount = vmemo->signalCount();
            const int propertyCount = vmemo->propertyCount();

            // Ensure that the property we resolve to is accessible from this meta-object
            do {
                const StringCache::mapped_type &property(it.value());

                if (property.first < maximumIndexForProperty(property.second, methodCount, signalCount, propertyCount)) {
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
    if (!metaObject || coreIndex() == -1)
        return QString();

    if (isFunction()) {
        QMetaMethod m = metaObject->method(coreIndex());

        return QString::fromUtf8(m.name().constData());
    } else {
        QMetaProperty p = metaObject->property(coreIndex());
        return QString::fromUtf8(p.name());
    }
}

void QQmlPropertyData::markAsOverrideOf(QQmlPropertyData *predecessor)
{
    setOverrideIndexIsProperty(!predecessor->isFunction());
    setOverrideIndex(predecessor->coreIndex());

    predecessor->_flags.isOverridden = true;
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

QString QQmlPropertyCache::signalParameterStringForJS(QV4::ExecutionEngine *engine, const QList<QByteArray> &parameterNameList, QString *errorString)
{
    bool unnamedParameter = false;
    const QSet<QString> &illegalNames = engine->v8Engine->illegalNames();
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
    // These indices don't apply to gadgets, so don't block them.
    const bool preventDestruction = metaObject->superClass() || metaObject == &QObject::staticMetaObject;

    const QByteArray propertyName = property.toUtf8();

    int methodCount = metaObject->methodCount();
    for (int ii = methodCount - 1; ii >= 0; --ii) {
        if (preventDestruction && (ii == destroyedIdx1 || ii == destroyedIdx2 || ii == deleteLaterIdx))
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
qQmlPropertyCacheProperty(QJSEngine *engine, QObject *obj, T name,
                          QQmlContextData *context, QQmlPropertyData &local)
{
    QQmlPropertyCache *cache = 0;

    QQmlData *ddata = QQmlData::get(obj, false);

    if (ddata && ddata->propertyCache) {
        cache = ddata->propertyCache;
    } else if (engine) {
        QJSEnginePrivate *ep = QJSEnginePrivate::get(engine);
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
QQmlPropertyCache::property(QJSEngine *engine, QObject *obj, const QV4::String *name,
                            QQmlContextData *context, QQmlPropertyData &local)
{
    return qQmlPropertyCacheProperty<const QV4::String *>(engine, obj, name, context, local);
}

QQmlPropertyData *
QQmlPropertyCache::property(QJSEngine *engine, QObject *obj,
                                    const QString &name, QQmlContextData *context, QQmlPropertyData &local)
{
    return qQmlPropertyCacheProperty<const QString &>(engine, obj, name, context, local);
}

QQmlPropertyData *
QQmlPropertyCache::property(QJSEngine *engine, QObject *obj, const QLatin1String &name,
                            QQmlContextData *context, QQmlPropertyData &local)
{
    return qQmlPropertyCacheProperty<const QLatin1String &>(engine, obj, name, context, local);
}

// these two functions are copied from qmetaobject.cpp
static inline const QMetaObjectPrivate *priv(const uint* data)
{ return reinterpret_cast<const QMetaObjectPrivate*>(data); }

static inline const QByteArray stringData(const QMetaObject *mo, int index)
{
    Q_ASSERT(priv(mo->d.data)->revision >= 7);
    const QByteArrayDataPtr data = { const_cast<QByteArrayData*>(&mo->d.stringdata[index]) };
    Q_ASSERT(data.ptr->ref.isStatic());
    Q_ASSERT(data.ptr->alloc == 0);
    Q_ASSERT(data.ptr->capacityReserved == 0);
    Q_ASSERT(data.ptr->size >= 0);
    return data;
}

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
        return lhs.second->coreIndex() < rhs.second->coreIndex();
    } };

    struct Insert { static void in(QQmlPropertyCache *This,
                                   QList<QPair<QString, QQmlPropertyData *> > &properties,
                                   QList<QPair<QString, QQmlPropertyData *> > &methods,
                                   StringCache::ConstIterator iter, QQmlPropertyData *data) {
        if (data->isSignalHandler())
            return;

        if (data->isFunction()) {
            if (data->coreIndex() < This->methodIndexCacheStart)
                return;

            QPair<QString, QQmlPropertyData *> entry = qMakePair((QString)iter.key(), data);
            // Overrides can cause the entry to already exist
            if (!methods.contains(entry)) methods.append(entry);

            data = This->overrideData(data);
            if (data && !data->isFunction()) Insert::in(This, properties, methods, iter, data);
        } else {
            if (data->coreIndex() < This->propertyIndexCacheStart)
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

    for (StringCache::ConstIterator iter = stringCache.begin(), cend = stringCache.end(); iter != cend; ++iter)
        Insert::in(this, properties, methods, iter, iter.value().second);

    Q_ASSERT(properties.count() == propertyIndexCache.count());
    Q_ASSERT(methods.count() == methodIndexCache.count());

    std::sort(properties.begin(), properties.end(), Sort::lt);
    std::sort(methods.begin(), methods.end(), Sort::lt);

    for (int ii = 0; ii < properties.count(); ++ii) {
        QQmlPropertyData *data = properties.at(ii).second;

        int notifierId = -1;
        if (data->notifyIndex() != -1)
            notifierId = data->notifyIndex() - signalHandlerIndexCacheStart;

        QMetaPropertyBuilder property = builder.addProperty(properties.at(ii).first.toUtf8(),
                                                            QMetaType::typeName(data->propType()),
                                                            notifierId);

        property.setReadable(true);
        property.setWritable(data->isWritable());
        property.setResettable(data->isResettable());
    }

    for (int ii = 0; ii < methods.count(); ++ii) {
        QQmlPropertyData *data = methods.at(ii).second;

        QByteArray returnType;
        if (data->propType() != 0)
            returnType = QMetaType::typeName(data->propType());

        QByteArray signature;
        // '+=' reserves extra capacity. Follow-up appending will be probably free.
        signature += methods.at(ii).first.toUtf8() + '(';

        QQmlPropertyCacheMethodArguments *arguments = 0;
        if (data->hasArguments()) {
            arguments = (QQmlPropertyCacheMethodArguments *)data->arguments();
            Q_ASSERT(arguments->argumentsValid);
            for (int ii = 0; ii < arguments->arguments[0]; ++ii) {
                if (ii != 0) signature.append(',');
                signature.append(QMetaType::typeName(arguments->arguments[1 + ii]));
            }
        }

        signature.append(')');

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
        if (dp && dp->coreIndex() >= propertyIndexCacheStart) {
            Q_ASSERT(!dp->isFunction());
            builder.addClassInfo("DefaultProperty", _defaultPropertyName.toUtf8());
        }
    }
}

namespace {
template <typename StringVisitor, typename TypeInfoVisitor>
int visitMethods(const QMetaObject &mo, int methodOffset, int methodCount,
                 StringVisitor visitString, TypeInfoVisitor visitTypeInfo)
{
    const int intsPerMethod = 5;

    int fieldsForParameterData = 0;

    bool hasRevisionedMethods = false;

    for (int i = 0; i < methodCount; ++i) {
        const int handle = methodOffset + i * intsPerMethod;

        const uint flags = mo.d.data[handle + 4];
        if (flags & MethodRevisioned)
            hasRevisionedMethods = true;

        visitString(mo.d.data[handle + 0]); // name
        visitString(mo.d.data[handle + 3]); // tag

        const int argc = mo.d.data[handle + 1];
        const int paramIndex = mo.d.data[handle + 2];

        fieldsForParameterData += argc * 2; // type and name
        fieldsForParameterData += 1; // + return type

        // return type + args
        for (int i = 0; i < 1 + argc; ++i) {
            // type name (maybe)
            visitTypeInfo(mo.d.data[paramIndex + i]);

            // parameter name
            if (i > 0)
                visitString(mo.d.data[paramIndex + argc + i]);
        }
    }

    int fieldsForRevisions = 0;
    if (hasRevisionedMethods)
        fieldsForRevisions = methodCount;

    return methodCount * intsPerMethod + fieldsForRevisions + fieldsForParameterData;
}

template <typename StringVisitor, typename TypeInfoVisitor>
int visitProperties(const QMetaObject &mo, StringVisitor visitString, TypeInfoVisitor visitTypeInfo)
{
    const QMetaObjectPrivate *const priv = reinterpret_cast<const QMetaObjectPrivate*>(mo.d.data);
    const int intsPerProperty = 3;

    bool hasRevisionedProperties = false;
    bool hasNotifySignals = false;

    for (int i = 0; i < priv->propertyCount; ++i) {
        const int handle = priv->propertyData + i * intsPerProperty;

        const auto flags = mo.d.data[handle + 2];
        if (flags & Revisioned) {
            hasRevisionedProperties = true;
        }
        if (flags & Notify)
            hasNotifySignals = true;

        visitString(mo.d.data[handle]); // name
        visitTypeInfo(mo.d.data[handle + 1]);
    }

    int fieldsForPropertyRevisions = 0;
    if (hasRevisionedProperties)
        fieldsForPropertyRevisions = priv->propertyCount;

    int fieldsForNotifySignals = 0;
    if (hasNotifySignals)
        fieldsForNotifySignals = priv->propertyCount;

    return priv->propertyCount * intsPerProperty + fieldsForPropertyRevisions
            + fieldsForNotifySignals;
}

template <typename StringVisitor>
int visitClassInfo(const QMetaObject &mo, StringVisitor visitString)
{
    const QMetaObjectPrivate *const priv = reinterpret_cast<const QMetaObjectPrivate*>(mo.d.data);
    const int intsPerClassInfo = 2;

    for (int i = 0; i < priv->classInfoCount; ++i) {
        const int handle = priv->classInfoData + i * intsPerClassInfo;

        visitString(mo.d.data[handle]); // key
        visitString(mo.d.data[handle + 1]); // value
    }

    return priv->classInfoCount * intsPerClassInfo;
}

template <typename StringVisitor>
int visitEnumerations(const QMetaObject &mo, StringVisitor visitString)
{
    const QMetaObjectPrivate *const priv = reinterpret_cast<const QMetaObjectPrivate*>(mo.d.data);
    const int intsPerEnumerator = 4;

    int fieldCount = priv->enumeratorCount * intsPerEnumerator;

    for (int i = 0; i < priv->enumeratorCount; ++i) {
        const uint *enumeratorData = mo.d.data + priv->enumeratorData + i * intsPerEnumerator;

        const uint keyCount = enumeratorData[2];
        fieldCount += keyCount * 2;

        visitString(enumeratorData[0]); // name

        const uint keyOffset = enumeratorData[3];

        for (uint j = 0; j < keyCount; ++j) {
            visitString(mo.d.data[keyOffset + 2 * j]);
        }
    }

    return fieldCount;
}

template <typename StringVisitor>
int countMetaObjectFields(const QMetaObject &mo, StringVisitor stringVisitor)
{
    const QMetaObjectPrivate *const priv = reinterpret_cast<const QMetaObjectPrivate*>(mo.d.data);

    const auto typeInfoVisitor = [&stringVisitor](uint typeInfo) {
        if (typeInfo & IsUnresolvedType)
            stringVisitor(typeInfo & TypeNameIndexMask);
    };

    int fieldCount = MetaObjectPrivateFieldCount;

    fieldCount += visitMethods(mo, priv->methodData, priv->methodCount, stringVisitor,
                               typeInfoVisitor);
    fieldCount += visitMethods(mo, priv->constructorData, priv->constructorCount, stringVisitor,
                               typeInfoVisitor);

    fieldCount += visitProperties(mo, stringVisitor, typeInfoVisitor);
    fieldCount += visitClassInfo(mo, stringVisitor);
    fieldCount += visitEnumerations(mo, stringVisitor);

    return fieldCount;
}

} // anonymous namespace

bool QQmlPropertyCache::determineMetaObjectSizes(const QMetaObject &mo, int *fieldCount,
                                                 int *stringCount)
{
    const QMetaObjectPrivate *priv = reinterpret_cast<const QMetaObjectPrivate*>(mo.d.data);
    if (priv->revision != 7) {
        return false;
    }

    uint highestStringIndex = 0;
    const auto stringIndexVisitor = [&highestStringIndex](uint index) {
        highestStringIndex = qMax(highestStringIndex, index);
    };

    *fieldCount = countMetaObjectFields(mo, stringIndexVisitor);
    *stringCount = highestStringIndex + 1;

    return true;
}

bool QQmlPropertyCache::addToHash(QCryptographicHash &hash, const QMetaObject &mo)
{
    int fieldCount = 0;
    int stringCount = 0;
    if (!determineMetaObjectSizes(mo, &fieldCount, &stringCount)) {
        return false;
    }

    hash.addData(reinterpret_cast<const char *>(mo.d.data), fieldCount * sizeof(uint));
    for (int i = 0; i < stringCount; ++i) {
        hash.addData(stringData(&mo, i));
    }

    return true;
}

QByteArray QQmlPropertyCache::checksum(bool *ok)
{
    if (!_checksum.isEmpty()) {
        *ok = true;
        return _checksum;
    }

    // Generate a checksum on the meta-object data only on C++ types.
    if (!_metaObject || _ownMetaObject) {
        *ok = false;
        return _checksum;
    }

    QCryptographicHash hash(QCryptographicHash::Md5);

    if (_parent) {
        hash.addData(_parent->checksum(ok));
        if (!*ok)
            return QByteArray();
    }

    if (!addToHash(hash, *createMetaObject())) {
        *ok = false;
        return QByteArray();
    }

    _checksum = hash.result();
    *ok = !_checksum.isEmpty();
    return _checksum;
}

/*! \internal
    \a index MUST be in the signal index range (see QObjectPrivate::signalIndex()).
    This is different from QMetaMethod::methodIndex().
*/
QList<QByteArray> QQmlPropertyCache::signalParameterNames(int index) const
{
    QQmlPropertyData *signalData = signal(index);
    if (signalData && signalData->hasArguments()) {
        QQmlPropertyCacheMethodArguments *args = (QQmlPropertyCacheMethodArguments *)signalData->arguments();
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

void QQmlMetaObject::resolveGadgetMethodOrPropertyIndex(QMetaObject::Call type, const QMetaObject **metaObject, int *index)
{
    int offset;

    switch (type) {
    case QMetaObject::ReadProperty:
    case QMetaObject::WriteProperty:
    case QMetaObject::ResetProperty:
    case QMetaObject::QueryPropertyDesignable:
    case QMetaObject::QueryPropertyEditable:
    case QMetaObject::QueryPropertyScriptable:
    case QMetaObject::QueryPropertyStored:
    case QMetaObject::QueryPropertyUser:
        offset = (*metaObject)->propertyOffset();
        while (*index < offset) {
            *metaObject = (*metaObject)->superClass();
            offset = (*metaObject)->propertyOffset();
        }
        break;
    case QMetaObject::InvokeMetaMethod:
        offset = (*metaObject)->methodOffset();
        while (*index < offset) {
            *metaObject = (*metaObject)->superClass();
            offset = (*metaObject)->methodOffset();
        }
        break;
    default:
        offset = 0;
        Q_UNIMPLEMENTED();
        offset = INT_MAX;
    }

    *index -= offset;
}

QQmlPropertyCache *QQmlMetaObject::propertyCache(QQmlEnginePrivate *e) const
{
    if (_m.isNull()) return 0;
    if (_m.isT1()) return _m.asT1();
    else return e->cache(_m.asT2());
}

int QQmlMetaObject::methodReturnType(const QQmlPropertyData &data, QByteArray *unknownTypeError) const
{
    Q_ASSERT(!_m.isNull() && data.coreIndex() >= 0);

    int type = data.propType();

    const char *propTypeName = 0;

    if (type == QMetaType::UnknownType) {
        // Find the return type name from the method info
        QMetaMethod m;

        if (_m.isT1()) {
            QQmlPropertyCache *c = _m.asT1();
            Q_ASSERT(data.coreIndex() < c->methodIndexCacheStart + c->methodIndexCache.count());

            while (data.coreIndex() < c->methodIndexCacheStart)
                c = c->_parent;

            const QMetaObject *metaObject = c->createMetaObject();
            Q_ASSERT(metaObject);
            m = metaObject->method(data.coreIndex());
        } else {
            m = _m.asT2()->method(data.coreIndex());
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
        type = EnumType(metaObject(), propTypeName, type);
    }

    if (type == QMetaType::UnknownType) {
        if (unknownTypeError) *unknownTypeError = propTypeName;
    }

    return type;
}

int *QQmlMetaObject::methodParameterTypes(int index, ArgTypeStorage *argStorage,
                                          QByteArray *unknownTypeError) const
{
    Q_ASSERT(!_m.isNull() && index >= 0);

    if (_m.isT1()) {
        typedef QQmlPropertyCacheMethodArguments A;

        QQmlPropertyCache *c = _m.asT1();
        Q_ASSERT(index < c->methodIndexCacheStart + c->methodIndexCache.count());

        while (index < c->methodIndexCacheStart)
            c = c->_parent;

        QQmlPropertyData *rv = const_cast<QQmlPropertyData *>(&c->methodIndexCache.at(index - c->methodIndexCacheStart));

        if (rv->arguments() && static_cast<A *>(rv->arguments())->argumentsValid)
            return static_cast<A *>(rv->arguments())->arguments;

        const QMetaObject *metaObject = c->createMetaObject();
        Q_ASSERT(metaObject);
        QMetaMethod m = metaObject->method(index);

        int argc = m.parameterCount();
        if (!rv->arguments()) {
            A *args = c->createArgumentsObject(argc, m.parameterNames());
            rv->setArguments(args);
        }
        A *args = static_cast<A *>(rv->arguments());

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
                type = EnumType(metaObject, argTypeNames.at(ii), type);
            }
            if (type == QMetaType::UnknownType) {
                if (unknownTypeError) *unknownTypeError = argTypeNames.at(ii);
                return 0;
            }
            args->arguments[ii + 1] = type;
        }
        args->argumentsValid = true;
        return static_cast<A *>(rv->arguments())->arguments;

    } else {
        QMetaMethod m = _m.asT2()->method(index);
        return methodParameterTypes(m, argStorage, unknownTypeError);

    }
}

int *QQmlMetaObject::methodParameterTypes(const QMetaMethod &m, ArgTypeStorage *argStorage,
                                          QByteArray *unknownTypeError) const
{
    Q_ASSERT(argStorage);

    int argc = m.parameterCount();
    argStorage->resize(argc + 1);
    argStorage->operator[](0) = argc;
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
            type = EnumType(_m.asT2(), argTypeNames.at(ii), type);
        }
        if (type == QMetaType::UnknownType) {
            if (unknownTypeError) *unknownTypeError = argTypeNames.at(ii);
            return 0;
        }
        argStorage->operator[](ii + 1) = type;
    }

    return argStorage->data();
}

void QQmlObjectOrGadget::metacall(QMetaObject::Call type, int index, void **argv) const
{
    if (ptr.isNull()) {
        const QMetaObject *metaObject = _m.asT2();
        metaObject->d.static_metacall(0, type, index, argv);
    }
    else if (ptr.isT1()) {
        QMetaObject::metacall(ptr.asT1(), type, index, argv);
    }
    else {
        const QMetaObject *metaObject = _m.asT1()->metaObject();
        QQmlMetaObject::resolveGadgetMethodOrPropertyIndex(type, &metaObject, &index);
        metaObject->d.static_metacall(reinterpret_cast<QObject*>(ptr.asT2()), type, index, argv);
    }
}

int *QQmlStaticMetaObject::constructorParameterTypes(int index, ArgTypeStorage *dummy,
                                                     QByteArray *unknownTypeError) const
{
    QMetaMethod m = _m.asT2()->constructor(index);
    return methodParameterTypes(m, dummy, unknownTypeError);
}

QT_END_NAMESPACE
