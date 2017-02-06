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

#include "qv4compileddata_p.h"
#include "qv4jsir_p.h"
#include <private/qv4value_p.h>
#ifndef V4_BOOTSTRAP
#include <private/qv4engine_p.h>
#include <private/qv4function_p.h>
#include <private/qv4objectproto_p.h>
#include <private/qv4lookup_p.h>
#include <private/qv4regexpobject_p.h>
#include <private/qqmlpropertycache_p.h>
#include <private/qqmltypeloader_p.h>
#include <private/qqmlengine_p.h>
#include "qv4compilationunitmapper_p.h"
#include <QQmlPropertyMap>
#include <QDateTime>
#include <QSaveFile>
#include <QFile>
#include <QFileInfo>
#include <QScopedValueRollback>
#include <QStandardPaths>
#include <QDir>
#endif
#include <private/qqmlirbuilder_p.h>
#include <QCoreApplication>
#include <QCryptographicHash>

#include <algorithm>

#if defined(QT_BUILD_INTERNAL)
#if defined(Q_OS_UNIX) && !defined(QT_NO_DYNAMIC_CAST)
#include <dlfcn.h>
#endif
#endif

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace CompiledData {

#ifndef V4_BOOTSTRAP
CompilationUnit::CompilationUnit()
    : data(0)
    , engine(0)
    , runtimeStrings(0)
    , runtimeLookups(0)
    , runtimeRegularExpressions(0)
    , runtimeClasses(0)
    , totalBindingsCount(0)
    , totalParserStatusCount(0)
    , totalObjectCount(0)
    , metaTypeId(-1)
    , listMetaTypeId(-1)
    , isRegisteredWithEngine(false)
{}

CompilationUnit::~CompilationUnit()
{
    unlink();
    if (data && !(data->flags & QV4::CompiledData::Unit::StaticData))
        free(const_cast<Unit *>(data));
    data = 0;
}

QV4::Function *CompilationUnit::linkToEngine(ExecutionEngine *engine)
{
    this->engine = engine;
    engine->compilationUnits.insert(this);

    Q_ASSERT(!runtimeStrings);
    Q_ASSERT(data);
    runtimeStrings = (QV4::Heap::String **)malloc(data->stringTableSize * sizeof(QV4::Heap::String*));
    // memset the strings to 0 in case a GC run happens while we're within the loop below
    memset(runtimeStrings, 0, data->stringTableSize * sizeof(QV4::Heap::String*));
    for (uint i = 0; i < data->stringTableSize; ++i)
        runtimeStrings[i] = engine->newIdentifier(data->stringAt(i));

    runtimeRegularExpressions = new QV4::Value[data->regexpTableSize];
    // memset the regexps to 0 in case a GC run happens while we're within the loop below
    memset(runtimeRegularExpressions, 0, data->regexpTableSize * sizeof(QV4::Value));
    for (uint i = 0; i < data->regexpTableSize; ++i) {
        const CompiledData::RegExp *re = data->regexpAt(i);
        int flags = 0;
        if (re->flags & CompiledData::RegExp::RegExp_Global)
            flags |= IR::RegExp::RegExp_Global;
        if (re->flags & CompiledData::RegExp::RegExp_IgnoreCase)
            flags |= IR::RegExp::RegExp_IgnoreCase;
        if (re->flags & CompiledData::RegExp::RegExp_Multiline)
            flags |= IR::RegExp::RegExp_Multiline;
        runtimeRegularExpressions[i] = engine->newRegExpObject(data->stringAt(re->stringIndex), flags);
    }

    if (data->lookupTableSize) {
        runtimeLookups = new QV4::Lookup[data->lookupTableSize];
        memset(runtimeLookups, 0, data->lookupTableSize * sizeof(QV4::Lookup));
        const CompiledData::Lookup *compiledLookups = data->lookupTable();
        for (uint i = 0; i < data->lookupTableSize; ++i) {
            QV4::Lookup *l = runtimeLookups + i;

            Lookup::Type type = Lookup::Type(uint(compiledLookups[i].type_and_flags));
            if (type == CompiledData::Lookup::Type_Getter)
                l->getter = QV4::Lookup::getterGeneric;
            else if (type == CompiledData::Lookup::Type_Setter)
                l->setter = QV4::Lookup::setterGeneric;
            else if (type == CompiledData::Lookup::Type_GlobalGetter)
                l->globalGetter = QV4::Lookup::globalGetterGeneric;
            else if (type == CompiledData::Lookup::Type_IndexedGetter)
                l->indexedGetter = QV4::Lookup::indexedGetterGeneric;
            else if (type == CompiledData::Lookup::Type_IndexedSetter)
                l->indexedSetter = QV4::Lookup::indexedSetterGeneric;

            for (int j = 0; j < QV4::Lookup::Size; ++j)
                l->classList[j] = 0;
            l->level = -1;
            l->index = UINT_MAX;
            l->nameIndex = compiledLookups[i].nameIndex;
            if (type == CompiledData::Lookup::Type_IndexedGetter || type == CompiledData::Lookup::Type_IndexedSetter)
                l->engine = engine;
        }
    }

    if (data->jsClassTableSize) {
        runtimeClasses = (QV4::InternalClass**)malloc(data->jsClassTableSize * sizeof(QV4::InternalClass*));
        for (uint i = 0; i < data->jsClassTableSize; ++i) {
            int memberCount = 0;
            const CompiledData::JSClassMember *member = data->jsClassAt(i, &memberCount);
            QV4::InternalClass *klass = engine->emptyClass;
            for (int j = 0; j < memberCount; ++j, ++member)
                klass = klass->addMember(runtimeStrings[member->nameOffset]->identifier, member->isAccessor ? QV4::Attr_Accessor : QV4::Attr_Data);

            runtimeClasses[i] = klass;
        }
    }

#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    Value *bigEndianConstants = new Value[data->constantTableSize];
    const LEUInt64 *littleEndianConstants = data->constants();
    for (uint i = 0; i < data->constantTableSize; ++i)
        bigEndianConstants[i] = Value::fromReturnedValue(littleEndianConstants[i]);
    constants = bigEndianConstants;
#else
    constants = reinterpret_cast<const Value*>(data->constants());
#endif

    linkBackendToEngine(engine);

    if (data->indexOfRootFunction != -1)
        return runtimeFunctions[data->indexOfRootFunction];
    else
        return 0;
}

void CompilationUnit::unlink()
{
    if (engine)
        engine->compilationUnits.erase(engine->compilationUnits.find(this));

    if (isRegisteredWithEngine) {
        Q_ASSERT(data && quint32(propertyCaches.count()) > data->indexOfRootObject && propertyCaches.at(data->indexOfRootObject));
        QQmlEnginePrivate *qmlEngine = QQmlEnginePrivate::get(propertyCaches.at(data->indexOfRootObject)->engine);
        qmlEngine->unregisterInternalCompositeType(this);
        isRegisteredWithEngine = false;
    }

    propertyCaches.clear();

    for (int ii = 0; ii < dependentScripts.count(); ++ii)
        dependentScripts.at(ii)->release();
    dependentScripts.clear();

    importCache = nullptr;

    qDeleteAll(resolvedTypes);
    resolvedTypes.clear();

    engine = 0;
    free(runtimeStrings);
    runtimeStrings = 0;
    delete [] runtimeLookups;
    runtimeLookups = 0;
    delete [] runtimeRegularExpressions;
    runtimeRegularExpressions = 0;
    free(runtimeClasses);
    runtimeClasses = 0;
    qDeleteAll(runtimeFunctions);
    runtimeFunctions.clear();
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    delete [] constants;
#endif
}

void CompilationUnit::markObjects(QV4::ExecutionEngine *e)
{
    for (uint i = 0; i < data->stringTableSize; ++i)
        if (runtimeStrings[i])
            runtimeStrings[i]->mark(e);
    if (runtimeRegularExpressions) {
        for (uint i = 0; i < data->regexpTableSize; ++i)
            runtimeRegularExpressions[i].mark(e);
    }
}

void CompilationUnit::destroy()
{
    QQmlEngine *qmlEngine = 0;
    if (engine && engine->v8Engine)
        qmlEngine = engine->v8Engine->engine();
    if (qmlEngine)
        QQmlEnginePrivate::deleteInEngineThread(qmlEngine, this);
    else
        delete this;
}

IdentifierHash<int> CompilationUnit::namedObjectsPerComponent(int componentObjectIndex)
{
    auto it = namedObjectsPerComponentCache.find(componentObjectIndex);
    if (it == namedObjectsPerComponentCache.end()) {
        IdentifierHash<int> namedObjectCache(engine);
        const CompiledData::Object *component = data->objectAt(componentObjectIndex);
        const LEUInt32 *namedObjectIndexPtr = component->namedObjectsInComponentTable();
        for (quint32 i = 0; i < component->nNamedObjectsInComponent; ++i, ++namedObjectIndexPtr) {
            const CompiledData::Object *namedObject = data->objectAt(*namedObjectIndexPtr);
            namedObjectCache.add(runtimeStrings[namedObject->idNameIndex], namedObject->id);
        }
        it = namedObjectsPerComponentCache.insert(componentObjectIndex, namedObjectCache);
    }
    return *it;
}

void CompilationUnit::finalize(QQmlEnginePrivate *engine)
{
    // Add to type registry of composites
    if (propertyCaches.needsVMEMetaObject(data->indexOfRootObject))
        engine->registerInternalCompositeType(this);
    else {
        const QV4::CompiledData::Object *obj = objectAt(data->indexOfRootObject);
        auto *typeRef = resolvedTypes.value(obj->inheritedTypeNameIndex);
        Q_ASSERT(typeRef);
        if (typeRef->compilationUnit) {
            metaTypeId = typeRef->compilationUnit->metaTypeId;
            listMetaTypeId = typeRef->compilationUnit->listMetaTypeId;
        } else {
            metaTypeId = typeRef->type->typeId();
            listMetaTypeId = typeRef->type->qListTypeId();
        }
    }

    // Collect some data for instantiation later.
    int bindingCount = 0;
    int parserStatusCount = 0;
    int objectCount = 0;
    for (quint32 i = 0; i < data->nObjects; ++i) {
        const QV4::CompiledData::Object *obj = data->objectAt(i);
        bindingCount += obj->nBindings;
        if (auto *typeRef = resolvedTypes.value(obj->inheritedTypeNameIndex)) {
            if (QQmlType *qmlType = typeRef->type) {
                if (qmlType->parserStatusCast() != -1)
                    ++parserStatusCount;
            }
            ++objectCount;
            if (typeRef->compilationUnit) {
                bindingCount += typeRef->compilationUnit->totalBindingsCount;
                parserStatusCount += typeRef->compilationUnit->totalParserStatusCount;
                objectCount += typeRef->compilationUnit->totalObjectCount;
            }
        }
    }

    totalBindingsCount = bindingCount;
    totalParserStatusCount = parserStatusCount;
    totalObjectCount = objectCount;
}

bool CompilationUnit::verifyChecksum(QQmlEngine *engine,
                                     const ResolvedTypeReferenceMap &dependentTypes) const
{
    if (dependentTypes.isEmpty()) {
        for (size_t i = 0; i < sizeof(data->dependencyMD5Checksum); ++i) {
            if (data->dependencyMD5Checksum[i] != 0)
                return false;
        }
        return true;
    }
    QCryptographicHash hash(QCryptographicHash::Md5);
    if (!dependentTypes.addToHash(&hash, engine))
        return false;
    QByteArray checksum = hash.result();
    Q_ASSERT(checksum.size() == sizeof(data->dependencyMD5Checksum));
    return memcmp(data->dependencyMD5Checksum, checksum.constData(),
                  sizeof(data->dependencyMD5Checksum)) == 0;
}

static QString cacheFilePath(const QUrl &url)
{
    const QString localSourcePath = QQmlFile::urlToLocalFileOrQrc(url);
    const QString localCachePath = localSourcePath + QLatin1Char('c');
    if (QFileInfo(QFileInfo(localSourcePath).dir().absolutePath()).isWritable())
        return localCachePath;
    QCryptographicHash fileNameHash(QCryptographicHash::Sha1);
    fileNameHash.addData(localSourcePath.toUtf8());
    QString directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QLatin1String("/qmlcache/");
    QDir::root().mkpath(directory);
    return directory + QString::fromUtf8(fileNameHash.result().toHex()) + QLatin1Char('.') + QFileInfo(localCachePath).completeSuffix();
}

bool CompilationUnit::saveToDisk(const QUrl &unitUrl, QString *errorString)
{
    errorString->clear();

    if (data->sourceTimeStamp == 0) {
        *errorString = QStringLiteral("Missing time stamp for source file");
        return false;
    }

    if (!QQmlFile::isLocalFile(unitUrl)) {
        *errorString = QStringLiteral("File has to be a local file.");
        return false;
    }

    // Foo.qml -> Foo.qmlc
    QSaveFile cacheFile(cacheFilePath(unitUrl));
    if (!cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *errorString = cacheFile.errorString();
        return false;
    }

    QByteArray modifiedUnit;
    modifiedUnit.resize(data->unitSize);
    memcpy(modifiedUnit.data(), data, data->unitSize);
    const char *dataPtr = modifiedUnit.data();
    Unit *unitPtr;
    memcpy(&unitPtr, &dataPtr, sizeof(unitPtr));
    unitPtr->flags |= Unit::StaticData;

    prepareCodeOffsetsForDiskStorage(unitPtr);

    qint64 headerWritten = cacheFile.write(modifiedUnit);
    if (headerWritten != modifiedUnit.size()) {
        *errorString = cacheFile.errorString();
        return false;
    }

    if (!saveCodeToDisk(&cacheFile, unitPtr, errorString))
        return false;

    if (!cacheFile.commit()) {
        *errorString = cacheFile.errorString();
        return false;
    }

    return true;
}

bool CompilationUnit::loadFromDisk(const QUrl &url, EvalISelFactory *iselFactory, QString *errorString)
{
    if (!QQmlFile::isLocalFile(url)) {
        *errorString = QStringLiteral("File has to be a local file.");
        return false;
    }

    const QString sourcePath = QQmlFile::urlToLocalFileOrQrc(url);
    QScopedPointer<CompilationUnitMapper> cacheFile(new CompilationUnitMapper());

    CompiledData::Unit *mappedUnit = cacheFile->open(cacheFilePath(url), sourcePath, errorString);
    if (!mappedUnit)
        return false;

    const Unit * const oldDataPtr = (data && !(data->flags & QV4::CompiledData::Unit::StaticData)) ? data : nullptr;
    QScopedValueRollback<const Unit *> dataPtrChange(data, mappedUnit);

    {
        const QString foundArchitecture = stringAt(data->architectureIndex);
        const QString expectedArchitecture = QSysInfo::buildAbi();
        if (foundArchitecture != expectedArchitecture) {
            *errorString = QString::fromUtf8("Architecture mismatch. Found %1 expected %2").arg(foundArchitecture).arg(expectedArchitecture);
            return false;
        }
    }

    {
        const QString foundCodeGenerator = stringAt(data->codeGeneratorIndex);
        const QString expectedCodeGenerator = iselFactory->codeGeneratorName;
        if (foundCodeGenerator != expectedCodeGenerator) {
            *errorString = QString::fromUtf8("Code generator mismatch. Found code generated by %1 but expected %2").arg(foundCodeGenerator).arg(expectedCodeGenerator);
            return false;
        }
    }

    if (!memoryMapCode(errorString))
        return false;

    dataPtrChange.commit();
    free(const_cast<Unit*>(oldDataPtr));
    backingFile.reset(cacheFile.take());
    return true;
}

void CompilationUnit::prepareCodeOffsetsForDiskStorage(Unit *unit)
{
    Q_UNUSED(unit);
}

bool CompilationUnit::saveCodeToDisk(QIODevice *device, const Unit *unit, QString *errorString)
{
    Q_UNUSED(device);
    Q_UNUSED(unit);
    *errorString = QStringLiteral("Saving code to disk is not supported in this configuration");
    return false;
}

bool CompilationUnit::memoryMapCode(QString *errorString)
{
    *errorString = QStringLiteral("Missing code mapping backend");
    return false;
}
#endif // V4_BOOTSTRAP

Unit *CompilationUnit::createUnitData(QmlIR::Document *irDocument)
{
    return irDocument->jsGenerator.generateUnit(QV4::Compiler::JSUnitGenerator::GenerateWithoutStringTable);
}

QString Binding::valueAsString(const Unit *unit) const
{
    switch (type) {
    case Type_Script:
    case Type_String:
        return unit->stringAt(stringIndex);
    case Type_Boolean:
        return value.b ? QStringLiteral("true") : QStringLiteral("false");
    case Type_Number:
        return QString::number(valueAsNumber());
    case Type_Invalid:
        return QString();
#ifdef QT_NO_TRANSLATION
    case Type_TranslationById:
    case Type_Translation:
        return unit->stringAt(stringIndex);
#else
    case Type_TranslationById: {
        QByteArray id = unit->stringAt(stringIndex).toUtf8();
        return qtTrId(id.constData(), value.translationData.number);
    }
    case Type_Translation: {
        // This code must match that in the qsTr() implementation
        const QString &path = unit->stringAt(unit->sourceFileIndex);
        int lastSlash = path.lastIndexOf(QLatin1Char('/'));
        QStringRef context = (lastSlash > -1) ? path.midRef(lastSlash + 1, path.length() - lastSlash - 5)
                                              : QStringRef();
        QByteArray contextUtf8 = context.toUtf8();
        QByteArray comment = unit->stringAt(value.translationData.commentIndex).toUtf8();
        QByteArray text = unit->stringAt(stringIndex).toUtf8();
        return QCoreApplication::translate(contextUtf8.constData(), text.constData(),
                                           comment.constData(), value.translationData.number);
    }
#endif
    default:
        break;
    }
    return QString();
}

//reverse of Lexer::singleEscape()
QString Binding::escapedString(const QString &string)
{
    QString tmp = QLatin1String("\"");
    for (int i = 0; i < string.length(); ++i) {
        const QChar &c = string.at(i);
        switch (c.unicode()) {
        case 0x08:
            tmp += QLatin1String("\\b");
            break;
        case 0x09:
            tmp += QLatin1String("\\t");
            break;
        case 0x0A:
            tmp += QLatin1String("\\n");
            break;
        case 0x0B:
            tmp += QLatin1String("\\v");
            break;
        case 0x0C:
            tmp += QLatin1String("\\f");
            break;
        case 0x0D:
            tmp += QLatin1String("\\r");
            break;
        case 0x22:
            tmp += QLatin1String("\\\"");
            break;
        case 0x27:
            tmp += QLatin1String("\\\'");
            break;
        case 0x5C:
            tmp += QLatin1String("\\\\");
            break;
        default:
            tmp += c;
            break;
        }
    }
    tmp += QLatin1Char('\"');
    return tmp;
}

QString Binding::valueAsScriptString(const Unit *unit) const
{
    if (type == Type_String)
        return escapedString(unit->stringAt(stringIndex));
    else
        return valueAsString(unit);
}

#ifndef V4_BOOTSTRAP
/*!
Returns the property cache, if one alread exists.  The cache is not referenced.
*/
QQmlPropertyCache *ResolvedTypeReference::propertyCache() const
{
    if (type)
        return typePropertyCache;
    else
        return compilationUnit->rootPropertyCache();
}

/*!
Returns the property cache, creating one if it doesn't already exist.  The cache is not referenced.
*/
QQmlPropertyCache *ResolvedTypeReference::createPropertyCache(QQmlEngine *engine)
{
    if (typePropertyCache) {
        return typePropertyCache;
    } else if (type) {
        typePropertyCache = QQmlEnginePrivate::get(engine)->cache(type->metaObject());
        return typePropertyCache;
    } else {
        return compilationUnit->rootPropertyCache();
    }
}

bool ResolvedTypeReference::addToHash(QCryptographicHash *hash, QQmlEngine *engine)
{
    if (type) {
        bool ok = false;
        hash->addData(createPropertyCache(engine)->checksum(&ok));
        return ok;
    }
    hash->addData(compilationUnit->data->md5Checksum, sizeof(compilationUnit->data->md5Checksum));
    return true;
}

template <typename T>
bool qtTypeInherits(const QMetaObject *mo) {
    while (mo) {
        if (mo == &T::staticMetaObject)
            return true;
        mo = mo->superClass();
    }
    return false;
}

void ResolvedTypeReference::doDynamicTypeCheck()
{
    const QMetaObject *mo = 0;
    if (typePropertyCache)
        mo = typePropertyCache->firstCppMetaObject();
    else if (type)
        mo = type->metaObject();
    else if (compilationUnit)
        mo = compilationUnit->rootPropertyCache()->firstCppMetaObject();
    isFullyDynamicType = qtTypeInherits<QQmlPropertyMap>(mo);
}

#if defined(QT_BUILD_INTERNAL)

static QByteArray ownLibraryChecksum()
{
    static QByteArray libraryChecksum;
    static bool checksumInitialized = false;
    if (checksumInitialized)
        return libraryChecksum;
    checksumInitialized = true;
#if defined(Q_OS_UNIX) && !defined(QT_NO_DYNAMIC_CAST)
    Dl_info libInfo;
    if (dladdr(reinterpret_cast<const void *>(&ownLibraryChecksum), &libInfo) != 0) {
        QFile library(QFile::decodeName(libInfo.dli_fname));
        if (library.open(QIODevice::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha1);
            hash.addData(&library);
            libraryChecksum = hash.result();
        }
    }
#else
    // Not implemented.
#endif
    return libraryChecksum;
}

#endif

bool ResolvedTypeReferenceMap::addToHash(QCryptographicHash *hash, QQmlEngine *engine) const
{
    for (auto it = constBegin(), end = constEnd(); it != end; ++it) {
        if (!it.value()->addToHash(hash, engine))
            return false;
    }

    // This is a bit of a hack to make development easier. When hacking on the code generator
    // the cache files may end up being re-used. To avoid that we also add the checksum of
    // the QtQml library.
#if defined(QT_BUILD_INTERNAL)
    hash->addData(ownLibraryChecksum());
#endif

    return true;
}

#endif

void Unit::generateChecksum()
{
#ifndef V4_BOOTSTRAP
    QCryptographicHash hash(QCryptographicHash::Md5);

    const int checksummableDataOffset = qOffsetOf(QV4::CompiledData::Unit, md5Checksum) + sizeof(md5Checksum);

    const char *dataPtr = reinterpret_cast<const char *>(this) + checksummableDataOffset;
    hash.addData(dataPtr, unitSize - checksummableDataOffset);

    QByteArray checksum = hash.result();
    Q_ASSERT(checksum.size() == sizeof(md5Checksum));
    memcpy(md5Checksum, checksum.constData(), sizeof(md5Checksum));
#else
    memset(md5Checksum, 0, sizeof(md5Checksum));
#endif
}

}

}

QT_END_NAMESPACE
