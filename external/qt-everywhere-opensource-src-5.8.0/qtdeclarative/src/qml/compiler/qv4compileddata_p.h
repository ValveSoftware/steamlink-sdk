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
#ifndef QV4COMPILEDDATA_P_H
#define QV4COMPILEDDATA_P_H

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

#include <QtCore/qstring.h>
#include <QVector>
#include <QStringList>
#include <QHash>
#include <QUrl>

#include <private/qv4value_p.h>
#include <private/qv4executableallocator_p.h>
#include <private/qqmlrefcount_p.h>
#include <private/qqmlnullablevalue_p.h>
#include <private/qv4identifier_p.h>
#include <private/qflagpointer_p.h>
#include <private/qjson_p.h>
#ifndef V4_BOOTSTRAP
#include <private/qqmltypenamecache_p.h>
#include <private/qqmlpropertycache_p.h>
#endif

QT_BEGIN_NAMESPACE

// Bump this whenever the compiler data structures change in an incompatible way.
#define QV4_DATA_STRUCTURE_VERSION 0x07

class QIODevice;
class QQmlPropertyCache;
class QQmlPropertyData;
class QQmlTypeNameCache;
class QQmlScriptData;
class QQmlType;
class QQmlEngine;

namespace QmlIR {
struct Document;
}

namespace QV4 {
namespace IR {
struct Function;
}

struct Function;
class EvalISelFactory;
class CompilationUnitMapper;

namespace CompiledData {

typedef QJsonPrivate::q_littleendian<qint16> LEInt16;
typedef QJsonPrivate::q_littleendian<quint16> LEUInt16;
typedef QJsonPrivate::q_littleendian<quint32> LEUInt32;
typedef QJsonPrivate::q_littleendian<qint32> LEInt32;
typedef QJsonPrivate::q_littleendian<quint64> LEUInt64;
typedef QJsonPrivate::q_littleendian<qint64> LEInt64;

struct String;
struct Function;
struct Lookup;
struct RegExp;
struct Unit;

template <typename ItemType, typename Container, const ItemType *(Container::*IndexedGetter)(int index) const>
struct TableIterator
{
    TableIterator(const Container *container, int index) : container(container), index(index) {}
    const Container *container;
    int index;

    const ItemType *operator->() { return (container->*IndexedGetter)(index); }
    void operator++() { ++index; }
    bool operator==(const TableIterator &rhs) const { return index == rhs.index; }
    bool operator!=(const TableIterator &rhs) const { return index != rhs.index; }
};

#if defined(Q_CC_MSVC) || defined(Q_CC_GNU)
#pragma pack(push, 1)
#endif

struct Location
{
    union {
        QJsonPrivate::qle_bitfield<0, 20> line;
        QJsonPrivate::qle_bitfield<20, 12> column;
    };

    Location() { line = 0; column = 0; }

    inline bool operator<(const Location &other) const {
        return line < other.line ||
               (line == other.line && column < other.column);
    }
};

struct RegExp
{
    enum Flags : unsigned int {
        RegExp_Global     = 0x01,
        RegExp_IgnoreCase = 0x02,
        RegExp_Multiline  = 0x04
    };
    union {
        QJsonPrivate::qle_bitfield<0, 4> flags;
        QJsonPrivate::qle_bitfield<4, 28> stringIndex;
    };

    RegExp() { flags = 0; stringIndex = 0; }
};

struct Lookup
{
    enum Type : unsigned int {
        Type_Getter = 0x0,
        Type_Setter = 0x1,
        Type_GlobalGetter = 2,
        Type_IndexedGetter = 3,
        Type_IndexedSetter = 4
    };

    union {
        QJsonPrivate::qle_bitfield<0, 4> type_and_flags;
        QJsonPrivate::qle_bitfield<4, 28> nameIndex;
    };

    Lookup() { type_and_flags = 0; nameIndex = 0; }
};

struct JSClassMember
{
    union {
        QJsonPrivate::qle_bitfield<0, 31> nameOffset;
        QJsonPrivate::qle_bitfield<31, 1> isAccessor;
    };

    JSClassMember() { nameOffset = 0; isAccessor = 0; }
};

struct JSClass
{
    LEUInt32 nMembers;
    // JSClassMember[nMembers]

    static int calculateSize(int nMembers) { return (sizeof(JSClass) + nMembers * sizeof(JSClassMember) + 7) & ~7; }
};

struct String
{
    LEInt32 size;
    // uint16 strdata[]

    static int calculateSize(const QString &str) {
        return (sizeof(String) + str.length() * sizeof(quint16) + 7) & ~0x7;
    }
};

// Function is aligned on an 8-byte boundary to make sure there are no bus errors or penalties
// for unaligned access. The ordering of the fields is also from largest to smallest.
struct Function
{
    enum Flags : unsigned int {
        HasDirectEval       = 0x1,
        UsesArgumentsObject = 0x2,
        IsStrict            = 0x4,
        IsNamedExpression   = 0x8,
        HasCatchOrWith      = 0x10
    };

    // Absolute offset into file where the code for this function is located. Only used when the function
    // is serialized.
    LEUInt64 codeOffset;
    LEUInt64 codeSize;

    LEUInt32 nameIndex;
    LEUInt32 nFormals;
    LEUInt32 formalsOffset;
    LEUInt32 nLocals;
    LEUInt32 localsOffset;
    LEUInt32 nInnerFunctions;
    Location location;

    // Qml Extensions Begin
    LEUInt32 nDependingIdObjects;
    LEUInt32 dependingIdObjectsOffset; // Array of resolved ID objects
    LEUInt32 nDependingContextProperties;
    LEUInt32 dependingContextPropertiesOffset; // Array of int pairs (property index and notify index)
    LEUInt32 nDependingScopeProperties;
    LEUInt32 dependingScopePropertiesOffset; // Array of int pairs (property index and notify index)
    // Qml Extensions End

//    quint32 formalsIndex[nFormals]
//    quint32 localsIndex[nLocals]
//    quint32 offsetForInnerFunctions[nInnerFunctions]
//    Function[nInnerFunctions]

    // Keep all unaligned data at the end
    quint8 flags;

    const LEUInt32 *formalsTable() const { return reinterpret_cast<const LEUInt32 *>(reinterpret_cast<const char *>(this) + formalsOffset); }
    const LEUInt32 *localsTable() const { return reinterpret_cast<const LEUInt32 *>(reinterpret_cast<const char *>(this) + localsOffset); }
    const LEUInt32 *qmlIdObjectDependencyTable() const { return reinterpret_cast<const LEUInt32 *>(reinterpret_cast<const char *>(this) + dependingIdObjectsOffset); }
    const LEUInt32 *qmlContextPropertiesDependencyTable() const { return reinterpret_cast<const LEUInt32 *>(reinterpret_cast<const char *>(this) + dependingContextPropertiesOffset); }
    const LEUInt32 *qmlScopePropertiesDependencyTable() const { return reinterpret_cast<const LEUInt32 *>(reinterpret_cast<const char *>(this) + dependingScopePropertiesOffset); }

    // --- QQmlPropertyCacheCreator interface
    const LEUInt32 *formalsBegin() const { return formalsTable(); }
    const LEUInt32 *formalsEnd() const { return formalsTable() + nFormals; }
    // ---

    inline bool hasQmlDependencies() const { return nDependingIdObjects > 0 || nDependingContextProperties > 0 || nDependingScopeProperties > 0; }

    static int calculateSize(int nFormals, int nLocals, int nInnerfunctions, int nIdObjectDependencies, int nPropertyDependencies) {
        return (sizeof(Function) + (nFormals + nLocals + nInnerfunctions + nIdObjectDependencies + 2 * nPropertyDependencies) * sizeof(quint32) + 7) & ~0x7;
    }
};

// Qml data structures

struct Q_QML_EXPORT TranslationData {
    LEUInt32 commentIndex;
    LEInt32 number;
};

struct Q_QML_PRIVATE_EXPORT Binding
{
    LEUInt32 propertyNameIndex;

    enum ValueType : unsigned int {
        Type_Invalid,
        Type_Boolean,
        Type_Number,
        Type_String,
        Type_Translation,
        Type_TranslationById,
        Type_Script,
        Type_Object,
        Type_AttachedProperty,
        Type_GroupProperty
    };

    enum Flags : unsigned int {
        IsSignalHandlerExpression = 0x1,
        IsSignalHandlerObject = 0x2,
        IsOnAssignment = 0x4,
        InitializerForReadOnlyDeclaration = 0x8,
        IsResolvedEnum = 0x10,
        IsListItem = 0x20,
        IsBindingToAlias = 0x40,
        IsDeferredBinding = 0x80,
        IsCustomParserBinding = 0x100,
    };

    union {
        QJsonPrivate::qle_bitfield<0, 16> flags;
        QJsonPrivate::qle_bitfield<16, 16> type;
    };
    union {
        bool b;
        quint64 doubleValue; // do not access directly, needs endian protected access
        LEUInt32 compiledScriptIndex; // used when Type_Script
        LEUInt32 objectIndex;
        TranslationData translationData; // used when Type_Translation
    } value;
    LEUInt32 stringIndex; // Set for Type_String, Type_Translation and Type_Script (the latter because of script strings)

    Location location;
    Location valueLocation;

    bool isValueBinding() const
    {
        if (type == Type_AttachedProperty
            || type == Type_GroupProperty)
            return false;
        if (flags & IsSignalHandlerExpression
            || flags & IsSignalHandlerObject)
            return false;
        return true;
    }

    bool isValueBindingNoAlias() const { return isValueBinding() && !(flags & IsBindingToAlias); }
    bool isValueBindingToAlias() const { return isValueBinding() && (flags & IsBindingToAlias); }

    bool isSignalHandler() const
    {
        if (flags & IsSignalHandlerExpression || flags & IsSignalHandlerObject) {
            Q_ASSERT(!isValueBinding());
            Q_ASSERT(!isAttachedProperty());
            Q_ASSERT(!isGroupProperty());
            return true;
        }
        return false;
    }

    bool isAttachedProperty() const
    {
        if (type == Type_AttachedProperty) {
            Q_ASSERT(!isValueBinding());
            Q_ASSERT(!isSignalHandler());
            Q_ASSERT(!isGroupProperty());
            return true;
        }
        return false;
    }

    bool isGroupProperty() const
    {
        if (type == Type_GroupProperty) {
            Q_ASSERT(!isValueBinding());
            Q_ASSERT(!isSignalHandler());
            Q_ASSERT(!isAttachedProperty());
            return true;
        }
        return false;
    }

    static QString escapedString(const QString &string);

    bool evaluatesToString() const { return type == Type_String || type == Type_Translation || type == Type_TranslationById; }

    QString valueAsString(const Unit *unit) const;
    QString valueAsScriptString(const Unit *unit) const;
    double valueAsNumber() const
    {
        if (type != Type_Number)
            return 0.0;
        quint64 intval = qFromLittleEndian<quint64>(value.doubleValue);
        double d;
        memcpy(&d, &intval, sizeof(double));
        return d;
    }
    void setNumberValueInternal(double d)
    {
        quint64 intval;
        memcpy(&intval, &d, sizeof(double));
        value.doubleValue = qToLittleEndian<quint64>(intval);
    }

    bool valueAsBoolean() const
    {
        if (type == Type_Boolean)
            return value.b;
        return false;
    }

};

struct Parameter
{
    LEUInt32 nameIndex;
    LEUInt32 type;
    LEUInt32 customTypeNameIndex;
    Location location;
};

struct Signal
{
    LEUInt32 nameIndex;
    LEUInt32 nParameters;
    Location location;
    // Parameter parameters[1];

    const Parameter *parameterAt(int idx) const {
        return reinterpret_cast<const Parameter*>(this + 1) + idx;
    }

    static int calculateSize(int nParameters) {
        return (sizeof(Signal)
                + nParameters * sizeof(Parameter)
                + 7) & ~0x7;
    }

    // --- QQmlPropertyCacheCceatorInterface
    const Parameter *parametersBegin() const { return parameterAt(0); }
    const Parameter *parametersEnd() const { return parameterAt(nParameters); }
    int parameterCount() const { return nParameters; }
    // ---
};

struct Property
{
    enum Type : unsigned int { Var = 0, Variant, Int, Bool, Real, String, Url, Color,
                Font, Time, Date, DateTime, Rect, Point, Size,
                Vector2D, Vector3D, Vector4D, Matrix4x4, Quaternion,
                Custom, CustomList };

    enum Flags : unsigned int {
        IsReadOnly = 0x1
    };

    LEUInt32 nameIndex;
    union {
        QJsonPrivate::qle_bitfield<0, 31> type;
        QJsonPrivate::qle_bitfield<31, 1> flags; // readonly
    };
    LEUInt32 customTypeNameIndex; // If type >= Custom
    Location location;
};

struct Alias {
    enum Flags : unsigned int {
        IsReadOnly = 0x1,
        Resolved = 0x2,
        AliasPointsToPointerObject = 0x4
    };
    union {
        QJsonPrivate::qle_bitfield<0, 29> nameIndex;
        QJsonPrivate::qle_bitfield<29, 3> flags;
    };
    union {
        LEUInt32 idIndex; // string index
        QJsonPrivate::qle_bitfield<0, 31> targetObjectId; // object id index (in QQmlContextData::idValues)
        QJsonPrivate::qle_bitfield<31, 1> aliasToLocalAlias;
    };
    union {
        LEUInt32 propertyNameIndex; // string index
        LEInt32 encodedMetaPropertyIndex;
        LEUInt32 localAliasIndex; // index in list of aliases local to the object (if targetObjectId == objectId)
    };
    Location location;
    Location referenceLocation;

    bool isObjectAlias() const {
        Q_ASSERT(flags & Resolved);
        return encodedMetaPropertyIndex == -1;
    }
};

struct Object
{
    enum Flags : unsigned int {
        NoFlag = 0x0,
        IsComponent = 0x1, // object was identified to be an explicit or implicit component boundary
        HasDeferredBindings = 0x2, // any of the bindings are deferred
        HasCustomParserBindings = 0x4
    };

    // Depending on the use, this may be the type name to instantiate before instantiating this
    // object. For grouped properties the type name will be empty and for attached properties
    // it will be the name of the attached type.
    LEUInt32 inheritedTypeNameIndex;
    LEUInt32 idNameIndex;
    union {
        QJsonPrivate::qle_bitfield<0, 15> flags;
        QJsonPrivate::qle_bitfield<15, 1> defaultPropertyIsAlias;
        QJsonPrivate::qle_signedbitfield<16, 16> id;
    };
    LEInt32 indexOfDefaultPropertyOrAlias; // -1 means no default property declared in this object
    LEUInt32 nFunctions;
    LEUInt32 offsetToFunctions;
    LEUInt32 nProperties;
    LEUInt32 offsetToProperties;
    LEUInt32 nAliases;
    LEUInt32 offsetToAliases;
    LEUInt32 nSignals;
    LEUInt32 offsetToSignals; // which in turn will be a table with offsets to variable-sized Signal objects
    LEUInt32 nBindings;
    LEUInt32 offsetToBindings;
    LEUInt32 nNamedObjectsInComponent;
    LEUInt32 offsetToNamedObjectsInComponent;
    Location location;
    Location locationOfIdProperty;
//    Function[]
//    Property[]
//    Signal[]
//    Binding[]

    static int calculateSizeExcludingSignals(int nFunctions, int nProperties, int nAliases, int nSignals, int nBindings, int nNamedObjectsInComponent)
    {
        return ( sizeof(Object)
                 + nFunctions * sizeof(quint32)
                 + nProperties * sizeof(Property)
                 + nAliases * sizeof(Alias)
                 + nSignals * sizeof(quint32)
                 + nBindings * sizeof(Binding)
                 + nNamedObjectsInComponent * sizeof(int)
                 + 0x7
               ) & ~0x7;
    }

    const LEUInt32 *functionOffsetTable() const
    {
        return reinterpret_cast<const LEUInt32*>(reinterpret_cast<const char *>(this) + offsetToFunctions);
    }

    const Property *propertyTable() const
    {
        return reinterpret_cast<const Property*>(reinterpret_cast<const char *>(this) + offsetToProperties);
    }

    const Alias *aliasTable() const
    {
        return reinterpret_cast<const Alias*>(reinterpret_cast<const char *>(this) + offsetToAliases);
    }

    const Binding *bindingTable() const
    {
        return reinterpret_cast<const Binding*>(reinterpret_cast<const char *>(this) + offsetToBindings);
    }

    const Signal *signalAt(int idx) const
    {
        const LEUInt32 *offsetTable = reinterpret_cast<const LEUInt32*>((reinterpret_cast<const char *>(this)) + offsetToSignals);
        const LEUInt32 offset = offsetTable[idx];
        return reinterpret_cast<const Signal*>(reinterpret_cast<const char*>(this) + offset);
    }

    const LEUInt32 *namedObjectsInComponentTable() const
    {
        return reinterpret_cast<const LEUInt32*>(reinterpret_cast<const char *>(this) + offsetToNamedObjectsInComponent);
    }

    // --- QQmlPropertyCacheCreator interface
    int propertyCount() const { return nProperties; }
    int aliasCount() const { return nAliases; }
    int signalCount() const { return nSignals; }
    int functionCount() const { return nFunctions; }

    const Binding *bindingsBegin() const { return bindingTable(); }
    const Binding *bindingsEnd() const { return bindingTable() + nBindings; }

    const Property *propertiesBegin() const { return propertyTable(); }
    const Property *propertiesEnd() const { return propertyTable() + nProperties; }

    const Alias *aliasesBegin() const { return aliasTable(); }
    const Alias *aliasesEnd() const { return aliasTable() + nAliases; }

    typedef TableIterator<Signal, Object, &Object::signalAt> SignalIterator;
    SignalIterator signalsBegin() const { return SignalIterator(this, 0); }
    SignalIterator signalsEnd() const { return SignalIterator(this, nSignals); }

    int namedObjectsInComponentCount() const { return nNamedObjectsInComponent; }
    // ---
};

struct Import
{
    enum ImportType : unsigned int {
        ImportLibrary = 0x1,
        ImportFile = 0x2,
        ImportScript = 0x3
    };
    LEUInt32 type;

    LEUInt32 uriIndex;
    LEUInt32 qualifierIndex;

    LEInt32 majorVersion;
    LEInt32 minorVersion;

    Location location;

    Import() { type = 0; uriIndex = 0; qualifierIndex = 0; majorVersion = 0; minorVersion = 0; }
};

static const char magic_str[] = "qv4cdata";

struct Unit
{
    // DO NOT CHANGE THESE FIELDS EVER
    char magic[8];
    LEUInt32 version;
    LEUInt32 qtVersion;
    LEInt64 sourceTimeStamp;
    LEUInt32 unitSize; // Size of the Unit and any depending data.
    // END DO NOT CHANGE THESE FIELDS EVER

    char md5Checksum[16]; // checksum of all bytes following this field.
    void generateChecksum();

    LEUInt32 architectureIndex; // string index to QSysInfo::buildAbi()
    LEUInt32 codeGeneratorIndex;
    char dependencyMD5Checksum[16];

    enum : unsigned int {
        IsJavascript = 0x1,
        IsQml = 0x2,
        StaticData = 0x4, // Unit data persistent in memory?
        IsSingleton = 0x8,
        IsSharedLibrary = 0x10, // .pragma shared?
        ContainsMachineCode = 0x20 // used to determine if we need to mmap with execute permissions
    };
    LEUInt32 flags;
    LEUInt32 stringTableSize;
    LEUInt32 offsetToStringTable;
    LEUInt32 functionTableSize;
    LEUInt32 offsetToFunctionTable;
    LEUInt32 lookupTableSize;
    LEUInt32 offsetToLookupTable;
    LEUInt32 regexpTableSize;
    LEUInt32 offsetToRegexpTable;
    LEUInt32 constantTableSize;
    LEUInt32 offsetToConstantTable;
    LEUInt32 jsClassTableSize;
    LEUInt32 offsetToJSClassTable;
    LEInt32 indexOfRootFunction;
    LEUInt32 sourceFileIndex;

    /* QML specific fields */
    LEUInt32 nImports;
    LEUInt32 offsetToImports;
    LEUInt32 nObjects;
    LEUInt32 offsetToObjects;
    LEUInt32 indexOfRootObject;

    const Import *importAt(int idx) const {
        return reinterpret_cast<const Import*>((reinterpret_cast<const char *>(this)) + offsetToImports + idx * sizeof(Import));
    }

    const Object *objectAt(int idx) const {
        const LEUInt32 *offsetTable = reinterpret_cast<const LEUInt32*>((reinterpret_cast<const char *>(this)) + offsetToObjects);
        const LEUInt32 offset = offsetTable[idx];
        return reinterpret_cast<const Object*>(reinterpret_cast<const char*>(this) + offset);
    }

    bool isSingleton() const {
        return flags & Unit::IsSingleton;
    }
    /* end QML specific fields*/

    QString stringAt(int idx) const {
        const LEUInt32 *offsetTable = reinterpret_cast<const LEUInt32*>((reinterpret_cast<const char *>(this)) + offsetToStringTable);
        const LEUInt32 offset = offsetTable[idx];
        const String *str = reinterpret_cast<const String*>(reinterpret_cast<const char *>(this) + offset);
        if (str->size == 0)
            return QString();
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        const QChar *characters = reinterpret_cast<const QChar *>(str + 1);
        // Too risky to do this while we unmap disk backed compilation but keep pointers to string
        // data in the identifier tables.
        //        if (flags & StaticData)
        //            return QString::fromRawData(characters, str->size);
        return QString(characters, str->size);
#else
        const LEUInt16 *characters = reinterpret_cast<const LEUInt16 *>(str + 1);
        QString qstr(str->size, Qt::Uninitialized);
        QChar *ch = qstr.data();
        for (int i = 0; i < str->size; ++i)
             ch[i] = QChar(characters[i]);
         return qstr;
#endif
    }

    const LEUInt32 *functionOffsetTable() const { return reinterpret_cast<const LEUInt32*>((reinterpret_cast<const char *>(this)) + offsetToFunctionTable); }

    const Function *functionAt(int idx) const {
        const LEUInt32 *offsetTable = functionOffsetTable();
        const LEUInt32 offset = offsetTable[idx];
        return reinterpret_cast<const Function*>(reinterpret_cast<const char *>(this) + offset);
    }

    const Lookup *lookupTable() const { return reinterpret_cast<const Lookup*>(reinterpret_cast<const char *>(this) + offsetToLookupTable); }
    const RegExp *regexpAt(int index) const {
        return reinterpret_cast<const RegExp*>(reinterpret_cast<const char *>(this) + offsetToRegexpTable + index * sizeof(RegExp));
    }
    const LEUInt64 *constants() const {
        return reinterpret_cast<const LEUInt64*>(reinterpret_cast<const char *>(this) + offsetToConstantTable);
    }

    const JSClassMember *jsClassAt(int idx, int *nMembers) const {
        const LEUInt32 *offsetTable = reinterpret_cast<const LEUInt32 *>(reinterpret_cast<const char *>(this) + offsetToJSClassTable);
        const LEUInt32 offset = offsetTable[idx];
        const char *ptr = reinterpret_cast<const char *>(this) + offset;
        const JSClass *klass = reinterpret_cast<const JSClass *>(ptr);
        *nMembers = klass->nMembers;
        return reinterpret_cast<const JSClassMember*>(ptr + sizeof(JSClass));
    }
};

#if defined(Q_CC_MSVC) || defined(Q_CC_GNU)
#pragma pack(pop)
#endif

struct TypeReference
{
    TypeReference(const Location &loc)
        : location(loc)
        , needsCreation(false)
        , errorWhenNotFound(false)
    {}
    Location location; // first use
    bool needsCreation : 1; // whether the type needs to be creatable or not
    bool errorWhenNotFound: 1;
};

// Map from name index to location of first use.
struct TypeReferenceMap : QHash<int, TypeReference>
{
    TypeReference &add(int nameIndex, const Location &loc) {
        Iterator it = find(nameIndex);
        if (it != end())
            return *it;
        return *insert(nameIndex, loc);
    }

    template <typename CompiledObject>
    void collectFromObject(const CompiledObject *obj)
    {
        if (obj->inheritedTypeNameIndex != 0) {
            TypeReference &r = this->add(obj->inheritedTypeNameIndex, obj->location);
            r.needsCreation = true;
            r.errorWhenNotFound = true;
        }

        auto prop = obj->propertiesBegin();
        auto propEnd = obj->propertiesEnd();
        for ( ; prop != propEnd; ++prop) {
            if (prop->type >= QV4::CompiledData::Property::Custom) {
                // ### FIXME: We could report the more accurate location here by using prop->location, but the old
                // compiler can't and the tests expect it to be the object location right now.
                TypeReference &r = this->add(prop->customTypeNameIndex, obj->location);
                r.errorWhenNotFound = true;
            }
        }

        auto binding = obj->bindingsBegin();
        auto bindingEnd = obj->bindingsEnd();
        for ( ; binding != bindingEnd; ++binding) {
            if (binding->type == QV4::CompiledData::Binding::Type_AttachedProperty)
                this->add(binding->propertyNameIndex, binding->location);
        }
    }

    template <typename Iterator>
    void collectFromObjects(Iterator it, Iterator end)
    {
        for (; it != end; ++it)
            collectFromObject(*it);
    }
};

#ifndef V4_BOOTSTRAP
struct ResolvedTypeReference
{
    ResolvedTypeReference()
        : type(0)
        , majorVersion(0)
        , minorVersion(0)
        , isFullyDynamicType(false)
    {}

    QQmlType *type;
    QQmlRefPointer<QQmlPropertyCache> typePropertyCache;
    QQmlRefPointer<QV4::CompiledData::CompilationUnit> compilationUnit;

    int majorVersion;
    int minorVersion;
    // Types such as QQmlPropertyMap can add properties dynamically at run-time and
    // therefore cannot have a property cache installed when instantiated.
    bool isFullyDynamicType;

    QQmlPropertyCache *propertyCache() const;
    QQmlPropertyCache *createPropertyCache(QQmlEngine *);
    bool addToHash(QCryptographicHash *hash, QQmlEngine *engine);

    void doDynamicTypeCheck();
};
// map from name index
// While this could be a hash, a map is chosen here to provide a stable
// order, which is used to calculating a check-sum on dependent meta-objects.
struct ResolvedTypeReferenceMap: public QMap<int, ResolvedTypeReference*>
{
    bool addToHash(QCryptographicHash *hash, QQmlEngine *engine) const;
};
#else
struct ResolvedTypeReferenceMap {};
#endif

// index is per-object binding index
typedef QVector<QQmlPropertyData*> BindingPropertyData;

// This is how this hooks into the existing structures:

//VM::Function
//    CompilationUnit * (for functions that need to clean up)
//    CompiledData::Function *compiledFunction

struct Q_QML_PRIVATE_EXPORT CompilationUnit : public QQmlRefCount
{
#ifdef V4_BOOTSTRAP
    CompilationUnit()
        : data(0)
    {}
    virtual ~CompilationUnit() {}
#else
    CompilationUnit();
    virtual ~CompilationUnit();
#endif

    const Unit *data;

    // Called only when building QML, when we build the header for JS first and append QML data
    virtual QV4::CompiledData::Unit *createUnitData(QmlIR::Document *irDocument);

#ifndef V4_BOOTSTRAP
    ExecutionEngine *engine;
    QString fileName() const { return data->stringAt(data->sourceFileIndex); }
    QUrl url() const { if (m_url.isNull) m_url = QUrl(fileName()); return m_url; }

    QV4::Heap::String **runtimeStrings; // Array
    QV4::Lookup *runtimeLookups;
    QV4::Value *runtimeRegularExpressions;
    QV4::InternalClass **runtimeClasses;
    QVector<QV4::Function *> runtimeFunctions;
    mutable QQmlNullableValue<QUrl> m_url;

    // QML specific fields
    QQmlPropertyCacheVector propertyCaches;
    QQmlPropertyCache *rootPropertyCache() const { return propertyCaches.at(data->indexOfRootObject); }

    QQmlRefPointer<QQmlTypeNameCache> importCache;

    // index is object index. This allows fast access to the
    // property data when initializing bindings, avoiding expensive
    // lookups by string (property name).
    QVector<BindingPropertyData> bindingPropertyDataPerObject;

    // mapping from component object index (CompiledData::Unit object index that points to component) to identifier hash of named objects
    // this is initialized on-demand by QQmlContextData
    QHash<int, IdentifierHash<int>> namedObjectsPerComponentCache;
    IdentifierHash<int> namedObjectsPerComponent(int componentObjectIndex);

    // pointers either to data->constants() or little-endian memory copy.
    const Value* constants;

    void finalize(QQmlEnginePrivate *engine);

    int totalBindingsCount; // Number of bindings used in this type
    int totalParserStatusCount; // Number of instantiated types that are QQmlParserStatus subclasses
    int totalObjectCount; // Number of objects explicitly instantiated

    QVector<QQmlScriptData *> dependentScripts;
    ResolvedTypeReferenceMap resolvedTypes;

    bool verifyChecksum(QQmlEngine *engine,
                        const ResolvedTypeReferenceMap &dependentTypes) const;

    int metaTypeId;
    int listMetaTypeId;
    bool isRegisteredWithEngine;

    QScopedPointer<CompilationUnitMapper> backingFile;

    // --- interface for QQmlPropertyCacheCreator
    typedef Object CompiledObject;
    int objectCount() const { return data->nObjects; }
    int rootObjectIndex() const { return data->indexOfRootObject; }
    const Object *objectAt(int index) const { return data->objectAt(index); }
    QString stringAt(int index) const { return data->stringAt(index); }

    struct FunctionIterator
    {
        FunctionIterator(const Unit *unit, const Object *object, int index) : unit(unit), object(object), index(index) {}
        const Unit *unit;
        const Object *object;
        int index;

        const Function *operator->() const { return unit->functionAt(object->functionOffsetTable()[index]); }
        void operator++() { ++index; }
        bool operator==(const FunctionIterator &rhs) const { return index == rhs.index; }
        bool operator!=(const FunctionIterator &rhs) const { return index != rhs.index; }
    };
    FunctionIterator objectFunctionsBegin(const Object *object) const { return FunctionIterator(data, object, 0); }
    FunctionIterator objectFunctionsEnd(const Object *object) const { return FunctionIterator(data, object, object->nFunctions); }
    // ---

    QV4::Function *linkToEngine(QV4::ExecutionEngine *engine);
    void unlink();

    void markObjects(QV4::ExecutionEngine *e);

    void destroy() Q_DECL_OVERRIDE;

    bool saveToDisk(const QUrl &unitUrl, QString *errorString);
    bool loadFromDisk(const QUrl &url, EvalISelFactory *iselFactory, QString *errorString);

protected:
    virtual void linkBackendToEngine(QV4::ExecutionEngine *engine) = 0;
    virtual void prepareCodeOffsetsForDiskStorage(CompiledData::Unit *unit);
    virtual bool saveCodeToDisk(QIODevice *device, const CompiledData::Unit *unit, QString *errorString);
    virtual bool memoryMapCode(QString *errorString);
#endif // V4_BOOTSTRAP
};

}

}

Q_DECLARE_TYPEINFO(QV4::CompiledData::JSClassMember, Q_PRIMITIVE_TYPE);

QT_END_NAMESPACE

#endif
