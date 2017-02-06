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

#ifndef QQMLPROPERTYCACHE_P_H
#define QQMLPROPERTYCACHE_P_H

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

#include <private/qqmlrefcount_p.h>
#include <private/qflagpointer_p.h>
#include "qqmlcleanup_p.h"
#include "qqmlnotifier_p.h"
#include <private/qqmlpropertyindex_p.h>

#include <private/qhashedstring_p.h>
#include <QtCore/qvarlengtharray.h>
#include <QtCore/qvector.h>

#include <private/qv4value_p.h>

#include <limits>

QT_BEGIN_NAMESPACE

class QCryptographicHash;
class QMetaProperty;
class QQmlEngine;
class QJSEngine;
class QQmlPropertyData;
class QMetaObjectBuilder;
class QQmlPropertyCacheMethodArguments;
class QQmlVMEMetaObject;
template <typename T> class QQmlPropertyCacheCreator;
template <typename T> class QQmlPropertyCacheAliasCreator;

// We have this somewhat awful split between RawData and Data so that RawData can be
// used in unions.  In normal code, you should always use Data which initializes RawData
// to an invalid state on construction.
class QQmlPropertyRawData
{
public:
    typedef QObjectPrivate::StaticMetaCallFunction StaticMetaCallFunction;

    struct Flags {
        enum Types {
            OtherType            = 0,
            FunctionType         = 1, // Is an invokable
            QObjectDerivedType   = 2, // Property type is a QObject* derived type
            EnumType             = 3, // Property type is an enum
            QListType            = 4, // Property type is a QML list
            QmlBindingType       = 5, // Property type is a QQmlBinding*
            QJSValueType         = 6, // Property type is a QScriptValue
            V4HandleType         = 7, // Property type is a QQmlV4Handle
            VarPropertyType      = 8, // Property type is a "var" property of VMEMO
            QVariantType         = 9  // Property is a QVariant
        };

        // The _otherBits (which "pad" the Flags struct to align it nicely) are used
        // to store the relative property index. It will only get used when said index fits. See
        // trySetStaticMetaCallFunction for details.
        // (Note: this padding is done here, because certain compilers have surprising behavior
        // when an enum is declared in-between two bit fields.)
        enum { BitsLeftInFlags = 10 };
        unsigned _otherBits       : BitsLeftInFlags; // align to 32 bits

        // Can apply to all properties, except IsFunction
        unsigned isConstant       : 1; // Has CONST flag
        unsigned isWritable       : 1; // Has WRITE function
        unsigned isResettable     : 1; // Has RESET function
        unsigned isAlias          : 1; // Is a QML alias to another property
        unsigned isFinal          : 1; // Has FINAL flag
        unsigned isOverridden     : 1; // Is overridden by a extension property
        unsigned isDirect         : 1; // Exists on a C++ QMetaObject

        unsigned type             : 4; // stores an entry of Types

        // Apply only to IsFunctions
        unsigned isVMEFunction    : 1; // Function was added by QML
        unsigned hasArguments     : 1; // Function takes arguments
        unsigned isSignal         : 1; // Function is a signal
        unsigned isVMESignal      : 1; // Signal was added by QML
        unsigned isV4Function     : 1; // Function takes QQmlV4Function* args
        unsigned isSignalHandler  : 1; // Function is a signal handler
        unsigned isOverload       : 1; // Function is an overload of another function
        unsigned isCloned         : 1; // The function was marked as cloned
        unsigned isConstructor    : 1; // The function was marked is a constructor

        // Internal QQmlPropertyCache flags
        unsigned notFullyResolved : 1; // True if the type data is to be lazily resolved
        unsigned overrideIndexIsProperty: 1;

        inline Flags();
        inline bool operator==(const Flags &other) const;
        inline void copyPropertyTypeFlags(Flags from);
    };

    Flags flags() const { return _flags; }
    void setFlags(Flags f)
    {
        unsigned otherBits = _flags._otherBits;
        _flags = f;
        _flags._otherBits = otherBits;
    }

    bool isValid() const { return coreIndex() != -1; }

    bool isConstant() const { return _flags.isConstant; }
    bool isWritable() const { return _flags.isWritable; }
    void setWritable(bool onoff) { _flags.isWritable = onoff; }
    bool isResettable() const { return _flags.isResettable; }
    bool isAlias() const { return _flags.isAlias; }
    bool isFinal() const { return _flags.isFinal; }
    bool isOverridden() const { return _flags.isOverridden; }
    bool isDirect() const { return _flags.isDirect; }
    bool hasStaticMetaCallFunction() const { return staticMetaCallFunction() != nullptr; }
    bool isFunction() const { return _flags.type == Flags::FunctionType; }
    bool isQObject() const { return _flags.type == Flags::QObjectDerivedType; }
    bool isEnum() const { return _flags.type == Flags::EnumType; }
    bool isQList() const { return _flags.type == Flags::QListType; }
    bool isQmlBinding() const { return _flags.type == Flags::QmlBindingType; }
    bool isQJSValue() const { return _flags.type == Flags::QJSValueType; }
    bool isV4Handle() const { return _flags.type == Flags::V4HandleType; }
    bool isVarProperty() const { return _flags.type == Flags::VarPropertyType; }
    bool isQVariant() const { return _flags.type == Flags::QVariantType; }
    bool isVMEFunction() const { return _flags.isVMEFunction; }
    bool hasArguments() const { return _flags.hasArguments; }
    bool isSignal() const { return _flags.isSignal; }
    bool isVMESignal() const { return _flags.isVMESignal; }
    bool isV4Function() const { return _flags.isV4Function; }
    bool isSignalHandler() const { return _flags.isSignalHandler; }
    bool isOverload() const { return _flags.isOverload; }
    void setOverload(bool onoff) { _flags.isOverload = onoff; }
    bool isCloned() const { return _flags.isCloned; }
    bool isConstructor() const { return _flags.isConstructor; }

    bool hasOverride() const { return overrideIndex() >= 0; }
    bool hasRevision() const { return revision() != 0; }

    bool isFullyResolved() const { return !_flags.notFullyResolved; }

    int propType() const { Q_ASSERT(isFullyResolved()); return _propType; }
    void setPropType(int pt)
    {
        Q_ASSERT(pt >= 0);
        Q_ASSERT(pt <= std::numeric_limits<qint16>::max());
        _propType = quint16(pt);
    }

    int notifyIndex() const { return _notifyIndex; }
    void setNotifyIndex(int idx)
    {
        Q_ASSERT(idx >= std::numeric_limits<qint16>::min());
        Q_ASSERT(idx <= std::numeric_limits<qint16>::max());
        _notifyIndex = qint16(idx);
    }

    bool overrideIndexIsProperty() const { return _flags.overrideIndexIsProperty; }
    void setOverrideIndexIsProperty(bool onoff) { _flags.overrideIndexIsProperty = onoff; }

    int overrideIndex() const { return _overrideIndex; }
    void setOverrideIndex(int idx)
    {
        Q_ASSERT(idx >= std::numeric_limits<qint16>::min());
        Q_ASSERT(idx <= std::numeric_limits<qint16>::max());
        _overrideIndex = qint16(idx);
    }

    int coreIndex() const { return _coreIndex; }
    void setCoreIndex(int idx)
    {
        Q_ASSERT(idx >= std::numeric_limits<qint16>::min());
        Q_ASSERT(idx <= std::numeric_limits<qint16>::max());
        _coreIndex = qint16(idx);
    }

    int revision() const { return _revision; }
    void setRevision(int rev)
    {
        Q_ASSERT(rev >= std::numeric_limits<qint16>::min());
        Q_ASSERT(rev <= std::numeric_limits<qint16>::max());
        _revision = qint16(rev);
    }

    QQmlPropertyCacheMethodArguments *arguments() const { return _arguments; }
    void setArguments(QQmlPropertyCacheMethodArguments *args) { _arguments = args; }

    int metaObjectOffset() const { return _metaObjectOffset; }
    void setMetaObjectOffset(int off)
    {
        Q_ASSERT(off >= std::numeric_limits<qint16>::min());
        Q_ASSERT(off <= std::numeric_limits<qint16>::max());
        _metaObjectOffset = qint16(off);
    }

    StaticMetaCallFunction staticMetaCallFunction() const { return _staticMetaCallFunction; }
    void trySetStaticMetaCallFunction(StaticMetaCallFunction f, unsigned relativePropertyIndex)
    {
        if (relativePropertyIndex < (1 << Flags::BitsLeftInFlags) - 1) {
            _flags._otherBits = relativePropertyIndex;
            _staticMetaCallFunction = f;
        }
    }
    quint16 relativePropertyIndex() const { Q_ASSERT(hasStaticMetaCallFunction()); return _flags._otherBits; }

private:
    Flags _flags;
    qint16 _coreIndex;
    quint16 _propType;

    // The notify index is in the range returned by QObjectPrivate::signalIndex().
    // This is different from QMetaMethod::methodIndex().
    qint16 _notifyIndex;
    qint16 _overrideIndex;

    qint16 _revision;
    qint16 _metaObjectOffset;

    QQmlPropertyCacheMethodArguments *_arguments;
    StaticMetaCallFunction _staticMetaCallFunction;

    friend class QQmlPropertyData;
    friend class QQmlPropertyCache;
};

#if QT_POINTER_SIZE == 4
Q_STATIC_ASSERT(sizeof(QQmlPropertyRawData) == 24);
#else // QT_POINTER_SIZE == 8
Q_STATIC_ASSERT(sizeof(QQmlPropertyRawData) == 32);
#endif

class QQmlPropertyData : public QQmlPropertyRawData
{
public:
    enum WriteFlag {
        BypassInterceptor = 0x01,
        DontRemoveBinding = 0x02,
        RemoveBindingOnAliasWrite = 0x04
    };
    Q_DECLARE_FLAGS(WriteFlags, WriteFlag)

    inline QQmlPropertyData();
    inline QQmlPropertyData(const QQmlPropertyRawData &);

    inline bool operator==(const QQmlPropertyRawData &);

    static Flags flagsForProperty(const QMetaProperty &, QQmlEngine *engine = 0);
    void load(const QMetaProperty &, QQmlEngine *engine = 0);
    void load(const QMetaMethod &);
    QString name(QObject *) const;
    QString name(const QMetaObject *) const;

    void markAsOverrideOf(QQmlPropertyData *predecessor);

    inline void readProperty(QObject *target, void *property) const
    {
        void *args[] = { property, 0 };
        readPropertyWithArgs(target, args);
    }

    inline void readPropertyWithArgs(QObject *target, void *args[]) const
    {
        if (hasStaticMetaCallFunction())
            staticMetaCallFunction()(target, QMetaObject::ReadProperty, relativePropertyIndex(), args);
        else if (isDirect())
            target->qt_metacall(QMetaObject::ReadProperty, coreIndex(), args);
        else
            QMetaObject::metacall(target, QMetaObject::ReadProperty, coreIndex(), args);
    }

    bool writeProperty(QObject *target, void *value, WriteFlags flags) const
    {
        int status = -1;
        void *argv[] = { value, 0, &status, &flags };
        if (flags.testFlag(BypassInterceptor) && hasStaticMetaCallFunction())
            staticMetaCallFunction()(target, QMetaObject::WriteProperty, relativePropertyIndex(), argv);
        else if (flags.testFlag(BypassInterceptor) && isDirect())
            target->qt_metacall(QMetaObject::WriteProperty, coreIndex(), argv);
        else
            QMetaObject::metacall(target, QMetaObject::WriteProperty, coreIndex(), argv);
        return true;
    }

    static Flags defaultSignalFlags()
    {
        Flags f;
        f.isSignal = true;
        f.type = Flags::FunctionType;
        f.isVMESignal = true;
        return f;
    }

    static Flags defaultSlotFlags()
    {
        Flags f;
        f.type = Flags::FunctionType;
        f.isVMEFunction = true;
        return f;
    }

private:
    friend class QQmlPropertyCache;
    void lazyLoad(const QMetaProperty &);
    void lazyLoad(const QMetaMethod &);
    bool notFullyResolved() const { return _flags.notFullyResolved; }
};

class QQmlPropertyCacheMethodArguments;
class Q_QML_PRIVATE_EXPORT QQmlPropertyCache : public QQmlRefCount, public QQmlCleanup
{
public:
    QQmlPropertyCache(QV4::ExecutionEngine *);
    QQmlPropertyCache(QV4::ExecutionEngine *, const QMetaObject *);
    virtual ~QQmlPropertyCache();

    void update(const QMetaObject *);
    void invalidate(const QMetaObject *);
    // Used by qmlpuppet. Remove as soon Creator requires Qt 5.5.
    void invalidate(void *, const QMetaObject *mo) { invalidate(mo); }

    QQmlPropertyCache *copy();

    QQmlPropertyCache *copyAndAppend(const QMetaObject *,
                QQmlPropertyRawData::Flags propertyFlags = QQmlPropertyData::Flags(),
                QQmlPropertyRawData::Flags methodFlags = QQmlPropertyData::Flags(),
                QQmlPropertyRawData::Flags signalFlags = QQmlPropertyData::Flags());
    QQmlPropertyCache *copyAndAppend(const QMetaObject *, int revision,
                QQmlPropertyRawData::Flags propertyFlags = QQmlPropertyData::Flags(),
                QQmlPropertyRawData::Flags methodFlags = QQmlPropertyData::Flags(),
                QQmlPropertyRawData::Flags signalFlags = QQmlPropertyData::Flags());

    QQmlPropertyCache *copyAndReserve(int propertyCount,
                                      int methodCount, int signalCount);
    void appendProperty(const QString &, QQmlPropertyRawData::Flags flags, int coreIndex,
                        int propType, int notifyIndex);
    void appendSignal(const QString &, QQmlPropertyRawData::Flags, int coreIndex,
                      const int *types = 0, const QList<QByteArray> &names = QList<QByteArray>());
    void appendMethod(const QString &, QQmlPropertyData::Flags flags, int coreIndex,
                      const QList<QByteArray> &names = QList<QByteArray>());

    const QMetaObject *metaObject() const;
    const QMetaObject *createMetaObject();
    const QMetaObject *firstCppMetaObject() const;

    template<typename K>
    QQmlPropertyData *property(const K &key, QObject *object, QQmlContextData *context) const
    {
        return findProperty(stringCache.find(key), object, context);
    }

    QQmlPropertyData *property(int) const;
    QQmlPropertyData *method(int) const;
    QQmlPropertyData *signal(int index) const;
    int methodIndexToSignalIndex(int) const;

    QString defaultPropertyName() const;
    QQmlPropertyData *defaultProperty() const;
    QQmlPropertyCache *parent() const;
    // is used by the Qml Designer
    void setParent(QQmlPropertyCache *newParent);

    inline QQmlPropertyData *overrideData(QQmlPropertyData *) const;
    inline bool isAllowedInRevision(QQmlPropertyData *) const;

    static QQmlPropertyData *property(QJSEngine *, QObject *, const QString &,
                                              QQmlContextData *, QQmlPropertyData &);
    static QQmlPropertyData *property(QJSEngine *, QObject *, const QLatin1String &,
                                      QQmlContextData *, QQmlPropertyData &);
    static QQmlPropertyData *property(QJSEngine *, QObject *, const QV4::String *,
                                              QQmlContextData *, QQmlPropertyData &);

    //see QMetaObjectPrivate::originalClone
    int originalClone(int index);
    static int originalClone(QObject *, int index);

    QList<QByteArray> signalParameterNames(int index) const;
    static QString signalParameterStringForJS(QV4::ExecutionEngine *engine, const QList<QByteArray> &parameterNameList, QString *errorString = 0);

    const char *className() const;

    inline int propertyCount() const;
    inline int propertyOffset() const;
    inline int methodCount() const;
    inline int methodOffset() const;
    inline int signalCount() const;
    inline int signalOffset() const;

    static bool isDynamicMetaObject(const QMetaObject *);

    void toMetaObjectBuilder(QMetaObjectBuilder &);

    inline bool callJSFactoryMethod(QObject *object, void **args) const;

    static bool determineMetaObjectSizes(const QMetaObject &mo, int *fieldCount, int *stringCount);
    static bool addToHash(QCryptographicHash &hash, const QMetaObject &mo);

    QByteArray checksum(bool *ok);

protected:
    virtual void destroy();
    virtual void clear();

private:
    friend class QQmlEnginePrivate;
    friend class QQmlCompiler;
    template <typename T> friend class QQmlPropertyCacheCreator;
    template <typename T> friend class QQmlPropertyCacheAliasCreator;
    friend class QQmlComponentAndAliasResolver;
    friend class QQmlMetaObject;

    inline QQmlPropertyCache *copy(int reserve);

    void append(const QMetaObject *, int revision,
                QQmlPropertyRawData::Flags propertyFlags = QQmlPropertyRawData::Flags(),
                QQmlPropertyRawData::Flags methodFlags = QQmlPropertyData::Flags(),
                QQmlPropertyRawData::Flags signalFlags = QQmlPropertyData::Flags());

    QQmlPropertyCacheMethodArguments *createArgumentsObject(int count, const QList<QByteArray> &names);

    typedef QVector<QQmlPropertyData> IndexCache;
    typedef QStringMultiHash<QPair<int, QQmlPropertyData *> > StringCache;
    typedef QVector<int> AllowedRevisionCache;

    QQmlPropertyData *findProperty(StringCache::ConstIterator it, QObject *, QQmlContextData *) const;
    QQmlPropertyData *findProperty(StringCache::ConstIterator it, const QQmlVMEMetaObject *, QQmlContextData *) const;

    QQmlPropertyData *ensureResolved(QQmlPropertyData*) const;

    Q_NEVER_INLINE void resolve(QQmlPropertyData *) const;
    void updateRecur(const QMetaObject *);

    template<typename K>
    QQmlPropertyData *findNamedProperty(const K &key)
    {
        StringCache::mapped_type *it = stringCache.value(key);
        return it ? it->second : 0;
    }

    template<typename K>
    void setNamedProperty(const K &key, int index, QQmlPropertyData *data, bool isOverride)
    {
        stringCache.insert(key, qMakePair(index, data));
        _hasPropertyOverrides |= isOverride;
    }

public:
    QV4::ExecutionEngine *engine;

private:
    QQmlPropertyCache *_parent;
    int propertyIndexCacheStart;
    int methodIndexCacheStart;
    int signalHandlerIndexCacheStart;

    IndexCache propertyIndexCache;
    IndexCache methodIndexCache;
    IndexCache signalHandlerIndexCache;
    StringCache stringCache;
    AllowedRevisionCache allowedRevisionCache;

    bool _hasPropertyOverrides : 1;
    bool _ownMetaObject : 1;
    const QMetaObject *_metaObject;
    QByteArray _dynamicClassName;
    QByteArray _dynamicStringData;
    QString _defaultPropertyName;
    QQmlPropertyCacheMethodArguments *argumentsCache;
    int _jsFactoryMethodIndex;
    QByteArray _checksum;
};

// QQmlMetaObject serves as a wrapper around either QMetaObject or QQmlPropertyCache.
// This is necessary as we delay creation of QMetaObject for synthesized QObjects, but
// we don't want to needlessly generate QQmlPropertyCaches every time we encounter a
// QObject type used in assignment or when we don't have a QQmlEngine etc.
//
// This class does NOT reference the propertycache.
class QQmlEnginePrivate;
class Q_QML_EXPORT QQmlMetaObject
{
public:
    typedef QVarLengthArray<int, 9> ArgTypeStorage;

    inline QQmlMetaObject();
    inline QQmlMetaObject(QObject *);
    inline QQmlMetaObject(const QMetaObject *);
    inline QQmlMetaObject(QQmlPropertyCache *);
    inline QQmlMetaObject(const QQmlMetaObject &);

    inline QQmlMetaObject &operator=(const QQmlMetaObject &);

    inline bool isNull() const;

    inline const char *className() const;
    inline int propertyCount() const;

    inline bool hasMetaObject() const;
    inline const QMetaObject *metaObject() const;

    QQmlPropertyCache *propertyCache(QQmlEnginePrivate *) const;

    int methodReturnType(const QQmlPropertyData &data, QByteArray *unknownTypeError) const;
    int *methodParameterTypes(int index, ArgTypeStorage *argStorage,
                              QByteArray *unknownTypeError) const;

    static bool canConvert(const QQmlMetaObject &from, const QQmlMetaObject &to);

    // static_metacall (on Gadgets) doesn't call the base implementation and therefore
    // we need a helper to find the correct meta object and property/method index.
    static void resolveGadgetMethodOrPropertyIndex(QMetaObject::Call type, const QMetaObject **metaObject, int *index);

protected:
    QBiPointer<QQmlPropertyCache, const QMetaObject> _m;
    int *methodParameterTypes(const QMetaMethod &method, ArgTypeStorage *argStorage,
                              QByteArray *unknownTypeError) const;

};

class QQmlObjectOrGadget: public QQmlMetaObject
{
public:
    QQmlObjectOrGadget(QObject *obj)
        : QQmlMetaObject(obj),
          ptr(obj)
    {}
    QQmlObjectOrGadget(QQmlPropertyCache *propertyCache, void *gadget)
        : QQmlMetaObject(propertyCache)
        , ptr(gadget)
    {}

    void metacall(QMetaObject::Call type, int index, void **argv) const;

private:
    QBiPointer<QObject, void> ptr;

protected:
    QQmlObjectOrGadget(const QMetaObject* metaObject)
        : QQmlMetaObject(metaObject)
    {}

};

class QQmlStaticMetaObject : public QQmlObjectOrGadget {
public:
    QQmlStaticMetaObject(const QMetaObject* metaObject)
        : QQmlObjectOrGadget(metaObject)
    {}
    int *constructorParameterTypes(int index, ArgTypeStorage *dummy, QByteArray *unknownTypeError) const;
};

QQmlPropertyRawData::Flags::Flags()
    : _otherBits(0)
    , isConstant(false)
    , isWritable(false)
    , isResettable(false)
    , isAlias(false)
    , isFinal(false)
    , isOverridden(false)
    , isDirect(false)
    , type(OtherType)
    , isVMEFunction(false)
    , hasArguments(false)
    , isSignal(false)
    , isVMESignal(false)
    , isV4Function(false)
    , isSignalHandler(false)
    , isOverload(false)
    , isCloned(false)
    , isConstructor(false)
    , notFullyResolved(false)
    , overrideIndexIsProperty(false)
{}

bool QQmlPropertyRawData::Flags::operator==(const QQmlPropertyRawData::Flags &other) const
{
    return isConstant == other.isConstant &&
            isWritable == other.isWritable &&
            isResettable == other.isResettable &&
            isAlias == other.isAlias &&
            isFinal == other.isFinal &&
            isOverridden == other.isOverridden &&
            type == other.type &&
            isVMEFunction == other.isVMEFunction &&
            hasArguments == other.hasArguments &&
            isSignal == other.isSignal &&
            isVMESignal == other.isVMESignal &&
            isV4Function == other.isV4Function &&
            isSignalHandler == other.isSignalHandler &&
            isOverload == other.isOverload &&
            isCloned == other.isCloned &&
            isConstructor == other.isConstructor &&
            notFullyResolved == other.notFullyResolved &&
            overrideIndexIsProperty == other.overrideIndexIsProperty;
}

void QQmlPropertyRawData::Flags::copyPropertyTypeFlags(QQmlPropertyRawData::Flags from)
{
    switch (from.type) {
    case QObjectDerivedType:
    case EnumType:
    case QListType:
    case QmlBindingType:
    case QJSValueType:
    case V4HandleType:
    case QVariantType:
        type = from.type;
    }
}

QQmlPropertyData::QQmlPropertyData()
{
    setCoreIndex(-1);
    setPropType(0);
    setNotifyIndex(-1);
    setOverrideIndex(-1);
    setRevision(0);
    setMetaObjectOffset(-1);
    setArguments(nullptr);
    trySetStaticMetaCallFunction(nullptr, 0);
}

QQmlPropertyData::QQmlPropertyData(const QQmlPropertyRawData &d)
{
    *(static_cast<QQmlPropertyRawData *>(this)) = d;
}

bool QQmlPropertyData::operator==(const QQmlPropertyRawData &other)
{
    return flags() == other.flags() &&
           propType() == other.propType() &&
           coreIndex() == other.coreIndex() &&
           notifyIndex() == other.notifyIndex() &&
           revision() == other.revision();
}

inline QQmlPropertyData *QQmlPropertyCache::ensureResolved(QQmlPropertyData *p) const
{
    if (p && Q_UNLIKELY(p->notFullyResolved()))
        resolve(p);

    return p;
}

// Returns this property cache's metaObject.  May be null if it hasn't been created yet.
inline const QMetaObject *QQmlPropertyCache::metaObject() const
{
    return _metaObject;
}

// Returns the first C++ type's QMetaObject - that is, the first QMetaObject not created by
// QML
inline const QMetaObject *QQmlPropertyCache::firstCppMetaObject() const
{
    while (_parent && (_metaObject == 0 || _ownMetaObject))
        return _parent->firstCppMetaObject();
    return _metaObject;
}

inline QQmlPropertyData *QQmlPropertyCache::property(int index) const
{
    if (index < 0 || index >= (propertyIndexCacheStart + propertyIndexCache.count()))
        return 0;

    if (index < propertyIndexCacheStart)
        return _parent->property(index);

    QQmlPropertyData *rv = const_cast<QQmlPropertyData *>(&propertyIndexCache.at(index - propertyIndexCacheStart));
    return ensureResolved(rv);
}

inline QQmlPropertyData *QQmlPropertyCache::method(int index) const
{
    if (index < 0 || index >= (methodIndexCacheStart + methodIndexCache.count()))
        return 0;

    if (index < methodIndexCacheStart)
        return _parent->method(index);

    QQmlPropertyData *rv = const_cast<QQmlPropertyData *>(&methodIndexCache.at(index - methodIndexCacheStart));
    return ensureResolved(rv);
}

/*! \internal
    \a index MUST be in the signal index range (see QObjectPrivate::signalIndex()).
    This is different from QMetaMethod::methodIndex().
*/
inline QQmlPropertyData *QQmlPropertyCache::signal(int index) const
{
    if (index < 0 || index >= (signalHandlerIndexCacheStart + signalHandlerIndexCache.count()))
        return 0;

    if (index < signalHandlerIndexCacheStart)
        return _parent->signal(index);

    QQmlPropertyData *rv = const_cast<QQmlPropertyData *>(&methodIndexCache.at(index - signalHandlerIndexCacheStart));
    Q_ASSERT(rv->isSignal() || rv->coreIndex() == -1);
    return ensureResolved(rv);
}

inline int QQmlPropertyCache::methodIndexToSignalIndex(int index) const
{
    if (index < 0 || index >= (methodIndexCacheStart + methodIndexCache.count()))
        return index;

    if (index < methodIndexCacheStart)
        return _parent->methodIndexToSignalIndex(index);

    return index - methodIndexCacheStart + signalHandlerIndexCacheStart;
}

// Returns the name of the default property for this cache
inline QString QQmlPropertyCache::defaultPropertyName() const
{
    return _defaultPropertyName;
}

inline QQmlPropertyCache *QQmlPropertyCache::parent() const
{
    return _parent;
}

QQmlPropertyData *
QQmlPropertyCache::overrideData(QQmlPropertyData *data) const
{
    if (!data->hasOverride())
        return 0;

    if (data->overrideIndexIsProperty())
        return property(data->overrideIndex());
    else
        return method(data->overrideIndex());
}

bool QQmlPropertyCache::isAllowedInRevision(QQmlPropertyData *data) const
{
    return (data->metaObjectOffset() == -1 && data->revision() == 0) ||
           (allowedRevisionCache[data->metaObjectOffset()] >= data->revision());
}

int QQmlPropertyCache::propertyCount() const
{
    return propertyIndexCacheStart + propertyIndexCache.count();
}

int QQmlPropertyCache::propertyOffset() const
{
    return propertyIndexCacheStart;
}

int QQmlPropertyCache::methodCount() const
{
    return methodIndexCacheStart + methodIndexCache.count();
}

int QQmlPropertyCache::methodOffset() const
{
    return methodIndexCacheStart;
}

int QQmlPropertyCache::signalCount() const
{
    return signalHandlerIndexCacheStart + signalHandlerIndexCache.count();
}

int QQmlPropertyCache::signalOffset() const
{
    return signalHandlerIndexCacheStart;
}

bool QQmlPropertyCache::callJSFactoryMethod(QObject *object, void **args) const
{
    if (_jsFactoryMethodIndex != -1) {
        _metaObject->d.static_metacall(object, QMetaObject::InvokeMetaMethod, _jsFactoryMethodIndex, args);
        return true;
    }
    if (_parent)
        return _parent->callJSFactoryMethod(object, args);
    return false;
}

QQmlMetaObject::QQmlMetaObject()
{
}

QQmlMetaObject::QQmlMetaObject(QObject *o)
{
    if (o) {
        QQmlData *ddata = QQmlData::get(o, false);
        if (ddata && ddata->propertyCache) _m = ddata->propertyCache;
        else _m = o->metaObject();
    }
}

QQmlMetaObject::QQmlMetaObject(const QMetaObject *m)
: _m(m)
{
}

QQmlMetaObject::QQmlMetaObject(QQmlPropertyCache *m)
: _m(m)
{
}

QQmlMetaObject::QQmlMetaObject(const QQmlMetaObject &o)
: _m(o._m)
{
}

QQmlMetaObject &QQmlMetaObject::operator=(const QQmlMetaObject &o)
{
    _m = o._m;
    return *this;
}

bool QQmlMetaObject::isNull() const
{
    return _m.isNull();
}

const char *QQmlMetaObject::className() const
{
    if (_m.isNull()) {
        return 0;
    } else if (_m.isT1()) {
        return _m.asT1()->className();
    } else {
        return _m.asT2()->className();
    }
}

int QQmlMetaObject::propertyCount() const
{
    if (_m.isNull()) {
        return 0;
    } else if (_m.isT1()) {
        return _m.asT1()->propertyCount();
    } else {
        return _m.asT2()->propertyCount();
    }
}

bool QQmlMetaObject::hasMetaObject() const
{
    return _m.isT2() || (!_m.isNull() && _m.asT1()->metaObject());
}

const QMetaObject *QQmlMetaObject::metaObject() const
{
    if (_m.isNull()) return 0;
    if (_m.isT1()) return _m.asT1()->createMetaObject();
    else return _m.asT2();
}

class QQmlPropertyCacheVector
{
public:
    QQmlPropertyCacheVector() {}
    QQmlPropertyCacheVector(QQmlPropertyCacheVector &&other)
        : data(std::move(other.data)) {}
    QQmlPropertyCacheVector &operator=(QQmlPropertyCacheVector &&other) {
        QVector<QFlagPointer<QQmlPropertyCache>> moved(std::move(other.data));
        data.swap(moved);
        return *this;
    }

    ~QQmlPropertyCacheVector() { clear(); }
    void resize(int size) { return data.resize(size); }
    int count() const { return data.count(); }
    void clear()
    {
        for (int i = 0; i < data.count(); ++i) {
            if (QQmlPropertyCache *cache = data.at(i).data())
                cache->release();
        }
        data.clear();
    }

    void append(QQmlPropertyCache *cache) { cache->addref(); data.append(cache); }
    QQmlPropertyCache *at(int index) const { return data.at(index).data(); }
    void set(int index, QQmlPropertyCache *replacement) {
        if (QQmlPropertyCache *oldCache = data.at(index).data()) {
            if (replacement == oldCache)
                return;
            oldCache->release();
        }
        data[index] = replacement;
        replacement->addref();
    }

    void setNeedsVMEMetaObject(int index) { data[index].setFlag(); }
    bool needsVMEMetaObject(int index) const { return data.at(index).flag(); }
private:
    Q_DISABLE_COPY(QQmlPropertyCacheVector)
    QVector<QFlagPointer<QQmlPropertyCache>> data;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QQmlPropertyData::WriteFlags)

QT_END_NAMESPACE

#endif // QQMLPROPERTYCACHE_P_H
