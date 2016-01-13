/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "qqmltypecompiler_p.h"

#include <private/qqmlirbuilder_p.h>
#include <private/qqmlobjectcreator_p.h>
#include <private/qqmlcustomparser_p.h>
#include <private/qqmlvmemetaobject_p.h>
#include <private/qqmlcomponent_p.h>
#include <private/qqmlstringconverters_p.h>
#include <private/qv4ssa_p.h>

#define COMPILE_EXCEPTION(token, desc) \
    { \
        recordError((token)->location, desc); \
        return false; \
    }

QT_BEGIN_NAMESPACE

QQmlTypeCompiler::QQmlTypeCompiler(QQmlEnginePrivate *engine, QQmlCompiledData *compiledData, QQmlTypeData *typeData, QmlIR::Document *parsedQML)
    : engine(engine)
    , compiledData(compiledData)
    , typeData(typeData)
    , document(parsedQML)
{
}

bool QQmlTypeCompiler::compile()
{
    compiledData->importCache = new QQmlTypeNameCache;

    foreach (const QString &ns, typeData->namespaces())
        compiledData->importCache->add(ns);

    // Add any Composite Singletons that were used to the import cache
    foreach (const QQmlTypeData::TypeReference &singleton, typeData->compositeSingletons())
        compiledData->importCache->add(singleton.type->qmlTypeName(), singleton.type->sourceUrl(), singleton.prefix);

    typeData->imports().populateCache(compiledData->importCache);

    const QHash<int, QQmlTypeData::TypeReference> &resolvedTypes = typeData->resolvedTypeRefs();
    for (QHash<int, QQmlTypeData::TypeReference>::ConstIterator resolvedType = resolvedTypes.constBegin(), end = resolvedTypes.constEnd();
         resolvedType != end; ++resolvedType) {
        QScopedPointer<QQmlCompiledData::TypeReference> ref(new QQmlCompiledData::TypeReference);
        QQmlType *qmlType = resolvedType->type;
        if (resolvedType->typeData) {
            if (resolvedType->needsCreation && qmlType->isCompositeSingleton()) {
                QQmlError error;
                QString reason = tr("Composite Singleton Type %1 is not creatable.").arg(qmlType->qmlTypeName());
                error.setDescription(reason);
                error.setColumn(resolvedType->location.column);
                error.setLine(resolvedType->location.line);
                recordError(error);
                return false;
            }
            ref->component = resolvedType->typeData->compiledData();
            ref->component->addref();
        } else if (qmlType) {
            ref->type = qmlType;
            Q_ASSERT(ref->type);

            if (resolvedType->needsCreation && !ref->type->isCreatable()) {
                QQmlError error;
                QString reason = ref->type->noCreationReason();
                if (reason.isEmpty())
                    reason = tr("Element is not creatable.");
                error.setDescription(reason);
                error.setColumn(resolvedType->location.column);
                error.setLine(resolvedType->location.line);
                recordError(error);
                return false;
            }

            if (ref->type->containsRevisionedAttributes()) {
                QQmlError cacheError;
                ref->typePropertyCache = engine->cache(ref->type,
                                                       resolvedType->minorVersion,
                                                       cacheError);
                if (!ref->typePropertyCache) {
                    cacheError.setColumn(resolvedType->location.column);
                    cacheError.setLine(resolvedType->location.line);
                    recordError(cacheError);
                    return false;
                }
                ref->typePropertyCache->addref();
            }
        }
        ref->majorVersion = resolvedType->majorVersion;
        ref->minorVersion = resolvedType->minorVersion;
        ref->doDynamicTypeCheck();
        compiledData->resolvedTypes.insert(resolvedType.key(), ref.take());
    }

    // Build property caches and VME meta object data

    for (QHash<int, QQmlCompiledData::TypeReference*>::ConstIterator it = compiledData->resolvedTypes.constBegin(), end = compiledData->resolvedTypes.constEnd();
         it != end; ++it) {
        QQmlCustomParser *customParser = (*it)->type ? (*it)->type->customParser() : 0;
        if (customParser)
            customParsers.insert(it.key(), customParser);
    }

    compiledData->metaObjects.reserve(document->objects.count());
    compiledData->propertyCaches.reserve(document->objects.count());

    {
        QQmlPropertyCacheCreator propertyCacheBuilder(this);
        if (!propertyCacheBuilder.buildMetaObjects())
            return false;
    }

    {
        QQmlDefaultPropertyMerger merger(this);
        merger.mergeDefaultProperties();
    }

    {
        SignalHandlerConverter converter(this);
        if (!converter.convertSignalHandlerExpressionsToFunctionDeclarations())
            return false;
    }

    {
        QQmlEnumTypeResolver enumResolver(this);
        if (!enumResolver.resolveEnumBindings())
            return false;
    }

    {
        QQmlCustomParserScriptIndexer cpi(this);
        cpi.annotateBindingsWithScriptStrings();
    }

    {
        QQmlAliasAnnotator annotator(this);
        annotator.annotateBindingsToAliases();
    }

    // Collect imported scripts
    const QList<QQmlTypeData::ScriptReference> &scripts = typeData->resolvedScripts();
    compiledData->scripts.reserve(scripts.count());
    for (int scriptIndex = 0; scriptIndex < scripts.count(); ++scriptIndex) {
        const QQmlTypeData::ScriptReference &script = scripts.at(scriptIndex);

        QString qualifier = script.qualifier;
        QString enclosingNamespace;

        const int lastDotIndex = qualifier.lastIndexOf(QLatin1Char('.'));
        if (lastDotIndex != -1) {
            enclosingNamespace = qualifier.left(lastDotIndex);
            qualifier = qualifier.mid(lastDotIndex+1);
        }

        compiledData->importCache->add(qualifier, scriptIndex, enclosingNamespace);
        QQmlScriptData *scriptData = script.script->scriptData();
        scriptData->addref();
        compiledData->scripts << scriptData;
    }

    // Resolve component boundaries and aliases

    {
        // Scan for components, determine their scopes and resolve aliases within the scope.
        QQmlComponentAndAliasResolver resolver(this);
        if (!resolver.resolve())
            return false;
    }

    // Compile JS binding expressions and signal handlers
    if (!document->javaScriptCompilationUnit) {
        {
            // We can compile script strings ahead of time, but they must be compiled
            // without type optimizations as their scope is always entirely dynamic.
            QQmlScriptStringScanner sss(this);
            sss.scan();
        }

        QmlIR::JSCodeGen v4CodeGenerator(typeData->finalUrlString(), document->code, &document->jsModule, &document->jsParserEngine, document->program, compiledData->importCache, &document->jsGenerator.stringTable);
        QQmlJSCodeGenerator jsCodeGen(this, &v4CodeGenerator);
        if (!jsCodeGen.generateCodeForComponents())
            return false;

        QQmlJavaScriptBindingExpressionSimplificationPass pass(this);
        pass.reduceTranslationBindings();

        QV4::ExecutionEngine *v4 = engine->v4engine();
        QScopedPointer<QV4::EvalInstructionSelection> isel(v4->iselFactory->create(engine, v4->executableAllocator, &document->jsModule, &document->jsGenerator));
        isel->setUseFastLookups(false);
        isel->setUseTypeInference(true);
        document->javaScriptCompilationUnit = isel->compile(/*generated unit data*/false);
    }

    // Generate QML compiled type data structures

    QmlIR::QmlUnitGenerator qmlGenerator;
    QV4::CompiledData::Unit *qmlUnit = qmlGenerator.generate(*document);

    Q_ASSERT(document->javaScriptCompilationUnit);
    // The js unit owns the data and will free the qml unit.
    document->javaScriptCompilationUnit->data = qmlUnit;

    compiledData->compilationUnit = document->javaScriptCompilationUnit;

    // Add to type registry of composites
    if (compiledData->isCompositeType())
        engine->registerInternalCompositeType(compiledData);
    else {
        const QV4::CompiledData::Object *obj = qmlUnit->objectAt(qmlUnit->indexOfRootObject);
        QQmlCompiledData::TypeReference *typeRef = compiledData->resolvedTypes.value(obj->inheritedTypeNameIndex);
        Q_ASSERT(typeRef);
        if (typeRef->component) {
            compiledData->metaTypeId = typeRef->component->metaTypeId;
            compiledData->listMetaTypeId = typeRef->component->listMetaTypeId;
        } else {
            compiledData->metaTypeId = typeRef->type->typeId();
            compiledData->listMetaTypeId = typeRef->type->qListTypeId();
        }
    }

    // Sanity check property bindings
    QQmlPropertyValidator validator(this);
    if (!validator.validate())
        return false;

    // Collect some data for instantiation later.
    int bindingCount = 0;
    int parserStatusCount = 0;
    int objectCount = 0;
    for (quint32 i = 0; i < qmlUnit->nObjects; ++i) {
        const QV4::CompiledData::Object *obj = qmlUnit->objectAt(i);
        bindingCount += obj->nBindings;
        if (QQmlCompiledData::TypeReference *typeRef = compiledData->resolvedTypes.value(obj->inheritedTypeNameIndex)) {
            if (QQmlType *qmlType = typeRef->type) {
                if (qmlType->parserStatusCast() != -1)
                    ++parserStatusCount;
            }
            ++objectCount;
            if (typeRef->component) {
                bindingCount += typeRef->component->totalBindingsCount;
                parserStatusCount += typeRef->component->totalParserStatusCount;
                objectCount += typeRef->component->totalObjectCount;
            }
        }
    }

    compiledData->totalBindingsCount = bindingCount;
    compiledData->totalParserStatusCount = parserStatusCount;
    compiledData->totalObjectCount = objectCount;

    Q_ASSERT(compiledData->propertyCaches.count() == static_cast<int>(compiledData->compilationUnit->data->nObjects));

    return errors.isEmpty();
}

void QQmlTypeCompiler::recordError(const QQmlError &error)
{
    QQmlError e = error;
    e.setUrl(url());
    errors << e;
}

QString QQmlTypeCompiler::stringAt(int idx) const
{
    return document->stringAt(idx);
}

int QQmlTypeCompiler::registerString(const QString &str)
{
    return document->jsGenerator.registerString(str);
}

QV4::IR::Module *QQmlTypeCompiler::jsIRModule() const
{
    return &document->jsModule;
}

const QV4::CompiledData::Unit *QQmlTypeCompiler::qmlUnit() const
{
    return compiledData->compilationUnit->data;
}

const QQmlImports *QQmlTypeCompiler::imports() const
{
    return &typeData->imports();
}

QHash<int, QQmlCompiledData::TypeReference*> *QQmlTypeCompiler::resolvedTypes()
{
    return &compiledData->resolvedTypes;
}

QList<QmlIR::Object *> *QQmlTypeCompiler::qmlObjects()
{
    return &document->objects;
}

int QQmlTypeCompiler::rootObjectIndex() const
{
    return document->indexOfRootObject;
}

void QQmlTypeCompiler::setPropertyCaches(const QVector<QQmlPropertyCache *> &caches)
{
    compiledData->propertyCaches = caches;
    Q_ASSERT(caches.count() >= document->indexOfRootObject);
    if (compiledData->rootPropertyCache)
        compiledData->rootPropertyCache->release();
    compiledData->rootPropertyCache = caches.at(document->indexOfRootObject);
    compiledData->rootPropertyCache->addref();
}

const QVector<QQmlPropertyCache *> &QQmlTypeCompiler::propertyCaches() const
{
    return compiledData->propertyCaches;
}

void QQmlTypeCompiler::setVMEMetaObjects(const QVector<QByteArray> &metaObjects)
{
    Q_ASSERT(compiledData->metaObjects.isEmpty());
    compiledData->metaObjects = metaObjects;
}

QVector<QByteArray> *QQmlTypeCompiler::vmeMetaObjects() const
{
    return &compiledData->metaObjects;
}

QHash<int, int> *QQmlTypeCompiler::objectIndexToIdForRoot()
{
    return &compiledData->objectIndexToIdForRoot;
}

QHash<int, QHash<int, int> > *QQmlTypeCompiler::objectIndexToIdPerComponent()
{
    return &compiledData->objectIndexToIdPerComponent;
}

QHash<int, QBitArray> *QQmlTypeCompiler::customParserBindings()
{
    return &compiledData->customParserBindings;
}

QQmlJS::MemoryPool *QQmlTypeCompiler::memoryPool()
{
    return document->jsParserEngine.pool();
}

QStringRef QQmlTypeCompiler::newStringRef(const QString &string)
{
    return document->jsParserEngine.newStringRef(string);
}

const QV4::Compiler::StringTableGenerator *QQmlTypeCompiler::stringPool() const
{
    return &document->jsGenerator.stringTable;
}

void QQmlTypeCompiler::setDeferredBindingsPerObject(const QHash<int, QBitArray> &deferredBindingsPerObject)
{
    compiledData->deferredBindingsPerObject = deferredBindingsPerObject;
}

QString QQmlTypeCompiler::bindingAsString(const QmlIR::Object *object, int scriptIndex) const
{
    return object->bindingAsString(document, scriptIndex);
}

QQmlCompilePass::QQmlCompilePass(QQmlTypeCompiler *typeCompiler)
    : compiler(typeCompiler)
{
}

void QQmlCompilePass::recordError(const QV4::CompiledData::Location &location, const QString &description)
{
    QQmlError error;
    error.setLine(location.line);
    error.setColumn(location.column);
    error.setDescription(description);
    compiler->recordError(error);
}

static QAtomicInt classIndexCounter(0);

QQmlPropertyCacheCreator::QQmlPropertyCacheCreator(QQmlTypeCompiler *typeCompiler)
    : QQmlCompilePass(typeCompiler)
    , enginePrivate(typeCompiler->enginePrivate())
    , qmlObjects(*typeCompiler->qmlObjects())
    , imports(typeCompiler->imports())
    , resolvedTypes(typeCompiler->resolvedTypes())
{
}

QQmlPropertyCacheCreator::~QQmlPropertyCacheCreator()
{
    for (int i = 0; i < propertyCaches.count(); ++i)
        if (QQmlPropertyCache *cache = propertyCaches.at(i))
            cache->release();
    propertyCaches.clear();
}

bool QQmlPropertyCacheCreator::buildMetaObjects()
{
    propertyCaches.resize(qmlObjects.count());
    vmeMetaObjects.resize(qmlObjects.count());

    if (!buildMetaObjectRecursively(compiler->rootObjectIndex(), /*referencing object*/-1, /*instantiating binding*/0))
        return false;

    compiler->setVMEMetaObjects(vmeMetaObjects);
    compiler->setPropertyCaches(propertyCaches);
    propertyCaches.clear();

    return true;
}

bool QQmlPropertyCacheCreator::buildMetaObjectRecursively(int objectIndex, int referencingObjectIndex, const QV4::CompiledData::Binding *instantiatingBinding)
{
    const QmlIR::Object *obj = qmlObjects.at(objectIndex);

    QQmlPropertyCache *baseTypeCache = 0;
    QQmlPropertyData *instantiatingProperty = 0;
    if (instantiatingBinding && instantiatingBinding->type == QV4::CompiledData::Binding::Type_GroupProperty) {
        Q_ASSERT(referencingObjectIndex >= 0);
        QQmlPropertyCache *parentCache = propertyCaches.at(referencingObjectIndex);
        Q_ASSERT(parentCache);
        Q_ASSERT(instantiatingBinding->propertyNameIndex != 0);

        bool notInRevision = false;
        instantiatingProperty = QmlIR::PropertyResolver(parentCache).property(stringAt(instantiatingBinding->propertyNameIndex), &notInRevision);
        if (instantiatingProperty) {
            if (instantiatingProperty->isQObject()) {
                baseTypeCache = enginePrivate->rawPropertyCacheForType(instantiatingProperty->propType);
                Q_ASSERT(baseTypeCache);
            } else if (QQmlValueType *vt = QQmlValueTypeFactory::valueType(instantiatingProperty->propType)) {
                baseTypeCache = enginePrivate->cache(vt->metaObject());
                Q_ASSERT(baseTypeCache);
            }
        }
    }

    bool needVMEMetaObject = obj->propertyCount() != 0 || obj->signalCount() != 0 || obj->functionCount() != 0;
    if (!needVMEMetaObject) {
        for (const QmlIR::Binding *binding = obj->firstBinding(); binding; binding = binding->next) {
            if (binding->type == QV4::CompiledData::Binding::Type_Object && (binding->flags & QV4::CompiledData::Binding::IsOnAssignment)) {

                // On assignments are implemented using value interceptors, which require a VME meta object.
                needVMEMetaObject = true;

                // If the on assignment is inside a group property, we need to distinguish between QObject based
                // group properties and value type group properties. For the former the base type is derived from
                // the property that references us, for the latter we only need a meta-object on the referencing object
                // because interceptors can't go to the shared value type instances.
                if (instantiatingProperty && QQmlValueTypeFactory::isValueType(instantiatingProperty->propType)) {
                    needVMEMetaObject = false;
                    if (!ensureMetaObject(referencingObjectIndex))
                        return false;
                }
                break;
            }
        }
    }

    if (obj->inheritedTypeNameIndex != 0) {
        QQmlCompiledData::TypeReference *typeRef = resolvedTypes->value(obj->inheritedTypeNameIndex);
        Q_ASSERT(typeRef);

        if (typeRef->isFullyDynamicType) {
            if (obj->propertyCount() > 0) {
                recordError(obj->location, tr("Fully dynamic types cannot declare new properties."));
                return false;
            }
            if (obj->signalCount() > 0) {
                recordError(obj->location, tr("Fully dynamic types cannot declare new signals."));
                return false;
            }
            if (obj->functionCount() > 0) {
                recordError(obj->location, tr("Fully Dynamic types cannot declare new functions."));
                return false;
            }
        }

        baseTypeCache = typeRef->createPropertyCache(QQmlEnginePrivate::get(enginePrivate));
        Q_ASSERT(baseTypeCache);
    } else if (instantiatingBinding && instantiatingBinding->isAttachedProperty()) {
        QQmlCompiledData::TypeReference *typeRef = resolvedTypes->value(instantiatingBinding->propertyNameIndex);
        Q_ASSERT(typeRef);
        const QMetaObject *attachedMo = typeRef->type ? typeRef->type->attachedPropertiesType() : 0;
        if (!attachedMo) {
            recordError(instantiatingBinding->location, tr("Non-existent attached object"));
            return false;
        }
        baseTypeCache = enginePrivate->cache(attachedMo);
        Q_ASSERT(baseTypeCache);
    }

    if (baseTypeCache) {
        if (needVMEMetaObject) {
            if (!createMetaObject(objectIndex, obj, baseTypeCache))
                return false;
        } else {
            propertyCaches[objectIndex] = baseTypeCache;
            baseTypeCache->addref();
        }
    }

    if (propertyCaches.at(objectIndex)) {
        for (const QmlIR::Binding *binding = obj->firstBinding(); binding; binding = binding->next)
            if (binding->type >= QV4::CompiledData::Binding::Type_Object) {
                if (!buildMetaObjectRecursively(binding->value.objectIndex, objectIndex, binding))
                    return false;
            }
    }

    return true;
}

bool QQmlPropertyCacheCreator::ensureMetaObject(int objectIndex)
{
    if (!vmeMetaObjects.at(objectIndex).isEmpty())
        return true;
    const QmlIR::Object *obj = qmlObjects.at(objectIndex);
    QQmlCompiledData::TypeReference *typeRef = resolvedTypes->value(obj->inheritedTypeNameIndex);
    Q_ASSERT(typeRef);
    QQmlPropertyCache *baseTypeCache = typeRef->createPropertyCache(QQmlEnginePrivate::get(enginePrivate));
    return createMetaObject(objectIndex, obj, baseTypeCache);
}

bool QQmlPropertyCacheCreator::createMetaObject(int objectIndex, const QmlIR::Object *obj, QQmlPropertyCache *baseTypeCache)
{
    QQmlPropertyCache *cache = baseTypeCache->copyAndReserve(QQmlEnginePrivate::get(enginePrivate),
                                                             obj->propertyCount(),
                                                             obj->functionCount() + obj->propertyCount() + obj->signalCount(),
                                                             obj->signalCount() + obj->propertyCount());
    propertyCaches[objectIndex] = cache;

    struct TypeData {
        QV4::CompiledData::Property::Type dtype;
        int metaType;
    } builtinTypes[] = {
        { QV4::CompiledData::Property::Var, QMetaType::QVariant },
        { QV4::CompiledData::Property::Variant, QMetaType::QVariant },
        { QV4::CompiledData::Property::Int, QMetaType::Int },
        { QV4::CompiledData::Property::Bool, QMetaType::Bool },
        { QV4::CompiledData::Property::Real, QMetaType::Double },
        { QV4::CompiledData::Property::String, QMetaType::QString },
        { QV4::CompiledData::Property::Url, QMetaType::QUrl },
        { QV4::CompiledData::Property::Color, QMetaType::QColor },
        { QV4::CompiledData::Property::Font, QMetaType::QFont },
        { QV4::CompiledData::Property::Time, QMetaType::QTime },
        { QV4::CompiledData::Property::Date, QMetaType::QDate },
        { QV4::CompiledData::Property::DateTime, QMetaType::QDateTime },
        { QV4::CompiledData::Property::Rect, QMetaType::QRectF },
        { QV4::CompiledData::Property::Point, QMetaType::QPointF },
        { QV4::CompiledData::Property::Size, QMetaType::QSizeF },
        { QV4::CompiledData::Property::Vector2D, QMetaType::QVector2D },
        { QV4::CompiledData::Property::Vector3D, QMetaType::QVector3D },
        { QV4::CompiledData::Property::Vector4D, QMetaType::QVector4D },
        { QV4::CompiledData::Property::Matrix4x4, QMetaType::QMatrix4x4 },
        { QV4::CompiledData::Property::Quaternion, QMetaType::QQuaternion }
    };
    static const uint builtinTypeCount = sizeof(builtinTypes) / sizeof(TypeData);

    QByteArray newClassName;

    if (objectIndex == compiler->rootObjectIndex()) {
        QString path = compiler->url().path();
        int lastSlash = path.lastIndexOf(QLatin1Char('/'));
        if (lastSlash > -1) {
            QString nameBase = path.mid(lastSlash + 1, path.length()-lastSlash-5);
            if (!nameBase.isEmpty() && nameBase.at(0).isUpper())
                newClassName = nameBase.toUtf8() + "_QMLTYPE_" +
                               QByteArray::number(classIndexCounter.fetchAndAddRelaxed(1));
        }
    }
    if (newClassName.isEmpty()) {
        newClassName = QQmlMetaObject(baseTypeCache).className();
        newClassName.append("_QML_");
        newClassName.append(QByteArray::number(classIndexCounter.fetchAndAddRelaxed(1)));
    }

    cache->_dynamicClassName = newClassName;

    int aliasCount = 0;
    int varPropCount = 0;

    QmlIR::PropertyResolver resolver(baseTypeCache);

    for (const QmlIR::Property *p = obj->firstProperty(); p; p = p->next) {
        if (p->type == QV4::CompiledData::Property::Alias)
            aliasCount++;
        else if (p->type == QV4::CompiledData::Property::Var)
            varPropCount++;

        // No point doing this for both the alias and non alias cases
        bool notInRevision = false;
        QQmlPropertyData *d = resolver.property(stringAt(p->nameIndex), &notInRevision);
        if (d && d->isFinal())
            COMPILE_EXCEPTION(p, tr("Cannot override FINAL property"));
    }

    typedef QQmlVMEMetaData VMD;

    QByteArray &dynamicData = vmeMetaObjects[objectIndex] = QByteArray(sizeof(QQmlVMEMetaData)
                                                              + obj->propertyCount() * sizeof(VMD::PropertyData)
                                                              + obj->functionCount() * sizeof(VMD::MethodData)
                                                              + aliasCount * sizeof(VMD::AliasData), 0);

    int effectivePropertyIndex = cache->propertyIndexCacheStart;
    int effectiveMethodIndex = cache->methodIndexCacheStart;

    // For property change signal override detection.
    // We prepopulate a set of signal names which already exist in the object,
    // and throw an error if there is a signal/method defined as an override.
    QSet<QString> seenSignals;
    seenSignals << QStringLiteral("destroyed") << QStringLiteral("parentChanged") << QStringLiteral("objectNameChanged");
    QQmlPropertyCache *parentCache = cache;
    while ((parentCache = parentCache->parent())) {
        if (int pSigCount = parentCache->signalCount()) {
            int pSigOffset = parentCache->signalOffset();
            for (int i = pSigOffset; i < pSigCount; ++i) {
                QQmlPropertyData *currPSig = parentCache->signal(i);
                // XXX TODO: find a better way to get signal name from the property data :-/
                for (QQmlPropertyCache::StringCache::ConstIterator iter = parentCache->stringCache.begin();
                        iter != parentCache->stringCache.end(); ++iter) {
                    if (currPSig == (*iter).second) {
                        seenSignals.insert(iter.key());
                        break;
                    }
                }
            }
        }
    }

    // First set up notify signals for properties - first normal, then var, then alias
    enum { NSS_Normal = 0, NSS_Var = 1, NSS_Alias = 2 };
    for (int ii = NSS_Normal; ii <= NSS_Alias; ++ii) { // 0 == normal, 1 == var, 2 == alias

        if (ii == NSS_Var && varPropCount == 0) continue;
        else if (ii == NSS_Alias && aliasCount == 0) continue;

        for (const QmlIR::Property *p = obj->firstProperty(); p; p = p->next) {
            if ((ii == NSS_Normal && (p->type == QV4::CompiledData::Property::Alias ||
                                      p->type == QV4::CompiledData::Property::Var)) ||
                ((ii == NSS_Var) && (p->type != QV4::CompiledData::Property::Var)) ||
                ((ii == NSS_Alias) && (p->type != QV4::CompiledData::Property::Alias)))
                continue;

            quint32 flags = QQmlPropertyData::IsSignal | QQmlPropertyData::IsFunction |
                            QQmlPropertyData::IsVMESignal;

            QString changedSigName = stringAt(p->nameIndex) + QLatin1String("Changed");
            seenSignals.insert(changedSigName);

            cache->appendSignal(changedSigName, flags, effectiveMethodIndex++);
        }
    }

    // Dynamic signals
    for (const QmlIR::Signal *s = obj->firstSignal(); s; s = s->next) {
        const int paramCount = s->parameters->count;

        QList<QByteArray> names;
        QVarLengthArray<int, 10> paramTypes(paramCount?(paramCount + 1):0);

        if (paramCount) {
            paramTypes[0] = paramCount;

            QmlIR::SignalParameter *param = s->parameters->first;
            for (int i = 0; i < paramCount; ++i, param = param->next) {
                names.append(stringAt(param->nameIndex).toUtf8());
                if (param->type < builtinTypeCount) {
                    // built-in type
                    paramTypes[i + 1] = builtinTypes[param->type].metaType;
                } else {
                    // lazily resolved type
                    Q_ASSERT(param->type == QV4::CompiledData::Property::Custom);
                    const QString customTypeName = stringAt(param->customTypeNameIndex);
                    QQmlType *qmltype = 0;
                    if (!imports->resolveType(customTypeName, &qmltype, 0, 0, 0))
                        COMPILE_EXCEPTION(s, tr("Invalid signal parameter type: %1").arg(customTypeName));

                    if (qmltype->isComposite()) {
                        QQmlTypeData *tdata = enginePrivate->typeLoader.getType(qmltype->sourceUrl());
                        Q_ASSERT(tdata);
                        Q_ASSERT(tdata->isComplete());

                        QQmlCompiledData *data = tdata->compiledData();

                        paramTypes[i + 1] = data->metaTypeId;

                        tdata->release();
                    } else {
                        paramTypes[i + 1] = qmltype->typeId();
                    }
                }
            }
        }

        ((QQmlVMEMetaData *)dynamicData.data())->signalCount++;

        quint32 flags = QQmlPropertyData::IsSignal | QQmlPropertyData::IsFunction |
                        QQmlPropertyData::IsVMESignal;
        if (paramCount)
            flags |= QQmlPropertyData::HasArguments;

        QString signalName = stringAt(s->nameIndex);
        if (seenSignals.contains(signalName))
            COMPILE_EXCEPTION(s, tr("Duplicate signal name: invalid override of property change signal or superclass signal"));
        seenSignals.insert(signalName);

        cache->appendSignal(signalName, flags, effectiveMethodIndex++,
                            paramCount?paramTypes.constData():0, names);
    }


    // Dynamic slots
    for (const QmlIR::Function *s = obj->firstFunction(); s; s = s->next) {
        QQmlJS::AST::FunctionDeclaration *astFunction = s->functionDeclaration;

        quint32 flags = QQmlPropertyData::IsFunction | QQmlPropertyData::IsVMEFunction;

        if (astFunction->formals)
            flags |= QQmlPropertyData::HasArguments;

        QString slotName = astFunction->name.toString();
        if (seenSignals.contains(slotName))
            COMPILE_EXCEPTION(s, tr("Duplicate method name: invalid override of property change signal or superclass signal"));
        // Note: we don't append slotName to the seenSignals list, since we don't
        // protect against overriding change signals or methods with properties.

        QList<QByteArray> parameterNames;
        QQmlJS::AST::FormalParameterList *param = astFunction->formals;
        while (param) {
            parameterNames << param->name.toUtf8();
            param = param->next;
        }

        cache->appendMethod(slotName, flags, effectiveMethodIndex++, parameterNames);
    }


    // Dynamic properties (except var and aliases)
    int effectiveSignalIndex = cache->signalHandlerIndexCacheStart;
    int propertyIdx = 0;
    for (const QmlIR::Property *p = obj->firstProperty(); p; p = p->next, ++propertyIdx) {

        if (p->type == QV4::CompiledData::Property::Alias ||
            p->type == QV4::CompiledData::Property::Var)
            continue;

        int propertyType = 0;
        int vmePropertyType = 0;
        quint32 propertyFlags = 0;

        if (p->type < builtinTypeCount) {
            propertyType = builtinTypes[p->type].metaType;
            vmePropertyType = propertyType;

            if (p->type == QV4::CompiledData::Property::Variant)
                propertyFlags |= QQmlPropertyData::IsQVariant;
        } else {
            Q_ASSERT(p->type == QV4::CompiledData::Property::CustomList ||
                     p->type == QV4::CompiledData::Property::Custom);

            QQmlType *qmltype = 0;
            if (!imports->resolveType(stringAt(p->customTypeNameIndex), &qmltype, 0, 0, 0)) {
                COMPILE_EXCEPTION(p, tr("Invalid property type"));
            }

            Q_ASSERT(qmltype);
            if (qmltype->isComposite()) {
                QQmlTypeData *tdata = enginePrivate->typeLoader.getType(qmltype->sourceUrl());
                Q_ASSERT(tdata);
                Q_ASSERT(tdata->isComplete());

                QQmlCompiledData *data = tdata->compiledData();

                if (p->type == QV4::CompiledData::Property::Custom) {
                    propertyType = data->metaTypeId;
                    vmePropertyType = QMetaType::QObjectStar;
                } else {
                    propertyType = data->listMetaTypeId;
                    vmePropertyType = qMetaTypeId<QQmlListProperty<QObject> >();
                }

                tdata->release();
            } else {
                if (p->type == QV4::CompiledData::Property::Custom) {
                    propertyType = qmltype->typeId();
                    vmePropertyType = QMetaType::QObjectStar;
                } else {
                    propertyType = qmltype->qListTypeId();
                    vmePropertyType = qMetaTypeId<QQmlListProperty<QObject> >();
                }
            }

            if (p->type == QV4::CompiledData::Property::Custom)
                propertyFlags |= QQmlPropertyData::IsQObjectDerived;
            else
                propertyFlags |= QQmlPropertyData::IsQList;
        }

        if ((!p->flags & QV4::CompiledData::Property::IsReadOnly) && p->type != QV4::CompiledData::Property::CustomList)
            propertyFlags |= QQmlPropertyData::IsWritable;


        QString propertyName = stringAt(p->nameIndex);
        if (propertyIdx == obj->indexOfDefaultProperty) cache->_defaultPropertyName = propertyName;
        cache->appendProperty(propertyName, propertyFlags, effectivePropertyIndex++,
                              propertyType, effectiveSignalIndex);

        effectiveSignalIndex++;

        VMD *vmd = (QQmlVMEMetaData *)dynamicData.data();
        (vmd->propertyData() + vmd->propertyCount)->propertyType = vmePropertyType;
        vmd->propertyCount++;
    }

    // Now do var properties
    propertyIdx = 0;
    for (const QmlIR::Property *p = obj->firstProperty(); p; p = p->next, ++propertyIdx) {

        if (p->type != QV4::CompiledData::Property::Var)
            continue;

        quint32 propertyFlags = QQmlPropertyData::IsVarProperty;
        if (!p->flags & QV4::CompiledData::Property::IsReadOnly)
            propertyFlags |= QQmlPropertyData::IsWritable;

        VMD *vmd = (QQmlVMEMetaData *)dynamicData.data();
        (vmd->propertyData() + vmd->propertyCount)->propertyType = QMetaType::QVariant;
        vmd->propertyCount++;
        ((QQmlVMEMetaData *)dynamicData.data())->varPropertyCount++;

        QString propertyName = stringAt(p->nameIndex);
        if (propertyIdx == obj->indexOfDefaultProperty) cache->_defaultPropertyName = propertyName;
        cache->appendProperty(propertyName, propertyFlags, effectivePropertyIndex++,
                              QMetaType::QVariant, effectiveSignalIndex);

        effectiveSignalIndex++;
    }

    // Alias property count.  Actual data is setup in buildDynamicMetaAliases
    ((QQmlVMEMetaData *)dynamicData.data())->aliasCount = aliasCount;

    // Dynamic slot data - comes after the property data
    for (const QmlIR::Function *s = obj->firstFunction(); s; s = s->next) {
        QQmlJS::AST::FunctionDeclaration *astFunction = s->functionDeclaration;
        int formalsCount = 0;
        QQmlJS::AST::FormalParameterList *param = astFunction->formals;
        while (param) {
            formalsCount++;
            param = param->next;
        }

        VMD::MethodData methodData = { /* runtimeFunctionIndex*/ 0, // ###
                                       formalsCount,
                                       /* s->location.start.line */0 }; // ###

        VMD *vmd = (QQmlVMEMetaData *)dynamicData.data();
        VMD::MethodData &md = *(vmd->methodData() + vmd->methodCount);
        vmd->methodCount++;
        md = methodData;
    }

    return true;
}

SignalHandlerConverter::SignalHandlerConverter(QQmlTypeCompiler *typeCompiler)
    : QQmlCompilePass(typeCompiler)
    , qmlObjects(*typeCompiler->qmlObjects())
    , customParsers(typeCompiler->customParserCache())
    , resolvedTypes(*typeCompiler->resolvedTypes())
    , illegalNames(QV8Engine::get(QQmlEnginePrivate::get(typeCompiler->enginePrivate()))->illegalNames())
    , propertyCaches(typeCompiler->propertyCaches())
{
}

bool SignalHandlerConverter::convertSignalHandlerExpressionsToFunctionDeclarations()
{
    for (int objectIndex = 0; objectIndex < qmlObjects.count(); ++objectIndex) {
        const QmlIR::Object * const obj = qmlObjects.at(objectIndex);
        QQmlPropertyCache *cache = propertyCaches.at(objectIndex);
        if (!cache)
            continue;
        if (QQmlCustomParser *customParser = customParsers.value(obj->inheritedTypeNameIndex)) {
            if (!(customParser->flags() & QQmlCustomParser::AcceptsSignalHandlers))
                continue;
        }
        const QString elementName = stringAt(obj->inheritedTypeNameIndex);
        if (!convertSignalHandlerExpressionsToFunctionDeclarations(obj, elementName, cache))
            return false;
    }
    return true;
}

bool SignalHandlerConverter::convertSignalHandlerExpressionsToFunctionDeclarations(const QmlIR::Object *obj, const QString &typeName, QQmlPropertyCache *propertyCache)
{
    // map from signal name defined in qml itself to list of parameters
    QHash<QString, QStringList> customSignals;

    for (QmlIR::Binding *binding = obj->firstBinding(); binding; binding = binding->next) {
        QString propertyName = stringAt(binding->propertyNameIndex);
        // Attached property?
        if (binding->type == QV4::CompiledData::Binding::Type_AttachedProperty) {
            const QmlIR::Object *attachedObj = qmlObjects.at(binding->value.objectIndex);
            QQmlCompiledData::TypeReference *typeRef = resolvedTypes.value(binding->propertyNameIndex);
            QQmlType *type = typeRef ? typeRef->type : 0;
            const QMetaObject *attachedType = type ? type->attachedPropertiesType() : 0;
            if (!attachedType)
                COMPILE_EXCEPTION(binding, tr("Non-existent attached object"));
            QQmlPropertyCache *cache = compiler->enginePrivate()->cache(attachedType);
            if (!convertSignalHandlerExpressionsToFunctionDeclarations(attachedObj, propertyName, cache))
                return false;
            continue;
        }

        if (!QmlIR::IRBuilder::isSignalPropertyName(propertyName))
            continue;

        QmlIR::PropertyResolver resolver(propertyCache);

        Q_ASSERT(propertyName.startsWith(QStringLiteral("on")));
        propertyName.remove(0, 2);

        // Note that the property name could start with any alpha or '_' or '$' character,
        // so we need to do the lower-casing of the first alpha character.
        for (int firstAlphaIndex = 0; firstAlphaIndex < propertyName.size(); ++firstAlphaIndex) {
            if (propertyName.at(firstAlphaIndex).isUpper()) {
                propertyName[firstAlphaIndex] = propertyName.at(firstAlphaIndex).toLower();
                break;
            }
        }

        QList<QString> parameters;

        bool notInRevision = false;
        QQmlPropertyData *signal = resolver.signal(propertyName, &notInRevision);
        if (signal) {
            int sigIndex = propertyCache->methodIndexToSignalIndex(signal->coreIndex);
            sigIndex = propertyCache->originalClone(sigIndex);

            bool unnamedParameter = false;

            QList<QByteArray> parameterNames = propertyCache->signalParameterNames(sigIndex);
            for (int i = 0; i < parameterNames.count(); ++i) {
                const QString param = QString::fromUtf8(parameterNames.at(i));
                if (param.isEmpty())
                    unnamedParameter = true;
                else if (unnamedParameter) {
                    COMPILE_EXCEPTION(binding, tr("Signal uses unnamed parameter followed by named parameter."));
                } else if (illegalNames.contains(param)) {
                    COMPILE_EXCEPTION(binding, tr("Signal parameter \"%1\" hides global variable.").arg(param));
                }
                parameters += param;
            }
        } else {
            if (notInRevision) {
                // Try assinging it as a property later
                if (resolver.property(propertyName, /*notInRevision ptr*/0))
                    continue;

                const QString &originalPropertyName = stringAt(binding->propertyNameIndex);

                QQmlCompiledData::TypeReference *typeRef = resolvedTypes.value(obj->inheritedTypeNameIndex);
                const QQmlType *type = typeRef ? typeRef->type : 0;
                if (type) {
                    COMPILE_EXCEPTION(binding, tr("\"%1.%2\" is not available in %3 %4.%5.").arg(typeName).arg(originalPropertyName).arg(type->module()).arg(type->majorVersion()).arg(type->minorVersion()));
                } else {
                    COMPILE_EXCEPTION(binding, tr("\"%1.%2\" is not available due to component versioning.").arg(typeName).arg(originalPropertyName));
                }
            }

            // Try to look up the signal parameter names in the object itself

            // build cache if necessary
            if (customSignals.isEmpty()) {
                for (const QmlIR::Signal *signal = obj->firstSignal(); signal; signal = signal->next) {
                    const QString &signalName = stringAt(signal->nameIndex);
                    customSignals.insert(signalName, signal->parameterStringList(compiler->stringPool()));
                }

                for (const QmlIR::Property *property = obj->firstProperty(); property; property = property->next) {
                    const QString propName = stringAt(property->nameIndex);
                    customSignals.insert(propName, QStringList());
                }
            }

            QHash<QString, QStringList>::ConstIterator entry = customSignals.find(propertyName);
            if (entry == customSignals.constEnd() && propertyName.endsWith(QStringLiteral("Changed"))) {
                QString alternateName = propertyName.mid(0, propertyName.length() - static_cast<int>(strlen("Changed")));
                entry = customSignals.find(alternateName);
            }

            if (entry == customSignals.constEnd()) {
                // Can't find even a custom signal, then just don't do anything and try
                // keeping the binding as a regular property assignment.
                continue;
            }

            parameters = entry.value();
        }

        // Binding object to signal means connect the signal to the object's default method.
        if (binding->type == QV4::CompiledData::Binding::Type_Object) {
            binding->flags |= QV4::CompiledData::Binding::IsSignalHandlerObject;
            continue;
        }

        if (binding->type != QV4::CompiledData::Binding::Type_Script) {
            if (binding->type < QV4::CompiledData::Binding::Type_Script) {
                COMPILE_EXCEPTION(binding, tr("Cannot assign a value to a signal (expecting a script to be run)"));
            } else {
                COMPILE_EXCEPTION(binding, tr("Incorrectly specified signal assignment"));
            }
        }

        QQmlJS::MemoryPool *pool = compiler->memoryPool();

        QQmlJS::AST::FormalParameterList *paramList = 0;
        foreach (const QString &param, parameters) {
            QStringRef paramNameRef = compiler->newStringRef(param);

            if (paramList)
                paramList = new (pool) QQmlJS::AST::FormalParameterList(paramList, paramNameRef);
            else
                paramList = new (pool) QQmlJS::AST::FormalParameterList(paramNameRef);
        }

        if (paramList)
            paramList = paramList->finish();

        QmlIR::CompiledFunctionOrExpression *foe = obj->functionsAndExpressions->slowAt(binding->value.compiledScriptIndex);
        QQmlJS::AST::Statement *statement = static_cast<QQmlJS::AST::Statement*>(foe->node);
        QQmlJS::AST::SourceElement *sourceElement = new (pool) QQmlJS::AST::StatementSourceElement(statement);
        QQmlJS::AST::SourceElements *elements = new (pool) QQmlJS::AST::SourceElements(sourceElement);
        elements = elements->finish();

        QQmlJS::AST::FunctionBody *body = new (pool) QQmlJS::AST::FunctionBody(elements);

        QQmlJS::AST::FunctionDeclaration *functionDeclaration = new (pool) QQmlJS::AST::FunctionDeclaration(compiler->newStringRef(stringAt(binding->propertyNameIndex)), paramList, body);
        functionDeclaration->functionToken = foe->node->firstSourceLocation();

        foe->node = functionDeclaration;
        binding->propertyNameIndex = compiler->registerString(propertyName);
        binding->flags |= QV4::CompiledData::Binding::IsSignalHandlerExpression;
    }
    return true;
}

QQmlEnumTypeResolver::QQmlEnumTypeResolver(QQmlTypeCompiler *typeCompiler)
    : QQmlCompilePass(typeCompiler)
    , qmlObjects(*typeCompiler->qmlObjects())
    , propertyCaches(typeCompiler->propertyCaches())
    , imports(typeCompiler->imports())
    , resolvedTypes(typeCompiler->resolvedTypes())
{
}

bool QQmlEnumTypeResolver::resolveEnumBindings()
{
    for (int i = 0; i < qmlObjects.count(); ++i) {
        QQmlPropertyCache *propertyCache = propertyCaches.at(i);
        if (!propertyCache)
            continue;
        const QmlIR::Object *obj = qmlObjects.at(i);

        QmlIR::PropertyResolver resolver(propertyCache);

        for (QmlIR::Binding *binding = obj->firstBinding(); binding; binding = binding->next) {
            if (binding->flags & QV4::CompiledData::Binding::IsSignalHandlerExpression
                || binding->flags & QV4::CompiledData::Binding::IsSignalHandlerObject)
                continue;

            if (binding->type != QV4::CompiledData::Binding::Type_Script)
                continue;

            const QString propertyName = stringAt(binding->propertyNameIndex);
            bool notInRevision = false;
            QQmlPropertyData *pd = resolver.property(propertyName, &notInRevision);
            if (!pd)
                continue;

            if (!pd->isEnum() && pd->propType != QMetaType::Int)
                continue;

            if (!tryQualifiedEnumAssignment(obj, propertyCache, pd, binding))
                return false;
        }
    }

    return true;
}

struct StaticQtMetaObject : public QObject
{
    static const QMetaObject *get()
        { return &staticQtMetaObject; }
};

bool QQmlEnumTypeResolver::tryQualifiedEnumAssignment(const QmlIR::Object *obj, const QQmlPropertyCache *propertyCache, const QQmlPropertyData *prop, QmlIR::Binding *binding)
{
    bool isIntProp = (prop->propType == QMetaType::Int) && !prop->isEnum();
    if (!prop->isEnum() && !isIntProp)
        return true;

    if (!prop->isWritable() && !(binding->flags & QV4::CompiledData::Binding::InitializerForReadOnlyDeclaration))
        COMPILE_EXCEPTION(binding, tr("Invalid property assignment: \"%1\" is a read-only property").arg(stringAt(binding->propertyNameIndex)));

    Q_ASSERT(binding->type == QV4::CompiledData::Binding::Type_Script);
    const QString string = compiler->bindingAsString(obj, binding->value.compiledScriptIndex);
    if (!string.constData()->isUpper())
        return true;

    int dot = string.indexOf(QLatin1Char('.'));
    if (dot == -1 || dot == string.length()-1)
        return true;

    if (string.indexOf(QLatin1Char('.'), dot+1) != -1)
        return true;

    QHashedStringRef typeName(string.constData(), dot);
    QString enumValue = string.mid(dot+1);

    if (isIntProp) {
        // Allow enum assignment to ints.
        bool ok;
        int enumval = evaluateEnum(typeName.toString(), enumValue.toUtf8(), &ok);
        if (ok) {
            binding->type = QV4::CompiledData::Binding::Type_Number;
            binding->value.d = (double)enumval;
            binding->flags |= QV4::CompiledData::Binding::IsResolvedEnum;
        }
        return true;
    }
    QQmlType *type = 0;
    imports->resolveType(typeName, &type, 0, 0, 0);

    if (!type && typeName != QLatin1String("Qt"))
        return true;
    if (type && type->isComposite()) //No enums on composite (or composite singleton) types
        return true;

    int value = 0;
    bool ok = false;

    QQmlCompiledData::TypeReference *tr = resolvedTypes->value(obj->inheritedTypeNameIndex);
    if (type && tr && tr->type == type) {
        QMetaProperty mprop = propertyCache->firstCppMetaObject()->property(prop->coreIndex);

        // When these two match, we can short cut the search
        if (mprop.isFlagType()) {
            value = mprop.enumerator().keysToValue(enumValue.toUtf8().constData(), &ok);
        } else {
            value = mprop.enumerator().keyToValue(enumValue.toUtf8().constData(), &ok);
        }
    } else {
        // Otherwise we have to search the whole type
        if (type) {
            value = type->enumValue(QHashedStringRef(enumValue), &ok);
        } else {
            QByteArray enumName = enumValue.toUtf8();
            const QMetaObject *metaObject = StaticQtMetaObject::get();
            for (int ii = metaObject->enumeratorCount() - 1; !ok && ii >= 0; --ii) {
                QMetaEnum e = metaObject->enumerator(ii);
                value = e.keyToValue(enumName.constData(), &ok);
            }
        }
    }

    if (!ok)
        return true;

    binding->type = QV4::CompiledData::Binding::Type_Number;
    binding->value.d = (double)value;
    binding->flags |= QV4::CompiledData::Binding::IsResolvedEnum;
    return true;
}

int QQmlEnumTypeResolver::evaluateEnum(const QString &scope, const QByteArray &enumValue, bool *ok) const
{
    Q_ASSERT_X(ok, "QQmlEnumTypeResolver::evaluateEnum", "ok must not be a null pointer");
    *ok = false;

    if (scope != QLatin1String("Qt")) {
        QQmlType *type = 0;
        imports->resolveType(scope, &type, 0, 0, 0);
        return type ? type->enumValue(QHashedCStringRef(enumValue.constData(), enumValue.length()), ok) : -1;
    }

    const QMetaObject *mo = StaticQtMetaObject::get();
    int i = mo->enumeratorCount();
    while (i--) {
        int v = mo->enumerator(i).keyToValue(enumValue.constData(), ok);
        if (*ok)
            return v;
    }
    return -1;
}

QQmlCustomParserScriptIndexer::QQmlCustomParserScriptIndexer(QQmlTypeCompiler *typeCompiler)
    : QQmlCompilePass(typeCompiler)
    , qmlObjects(*typeCompiler->qmlObjects())
    , customParsers(typeCompiler->customParserCache())
{
}

void QQmlCustomParserScriptIndexer::annotateBindingsWithScriptStrings()
{
    scanObjectRecursively(compiler->rootObjectIndex());
}

void QQmlCustomParserScriptIndexer::scanObjectRecursively(int objectIndex, bool annotateScriptBindings)
{
    const QmlIR::Object * const obj = qmlObjects.at(objectIndex);
    if (!annotateScriptBindings)
        annotateScriptBindings = customParsers.contains(obj->inheritedTypeNameIndex);
    for (QmlIR::Binding *binding = obj->firstBinding(); binding; binding = binding->next) {
        if (binding->type >= QV4::CompiledData::Binding::Type_Object) {
            scanObjectRecursively(binding->value.objectIndex, annotateScriptBindings);
            continue;
        } else if (binding->type != QV4::CompiledData::Binding::Type_Script)
            continue;
        if (!annotateScriptBindings)
            continue;
        const QString script = compiler->bindingAsString(obj, binding->value.compiledScriptIndex);
        binding->stringIndex = compiler->registerString(script);
    }
}

QQmlAliasAnnotator::QQmlAliasAnnotator(QQmlTypeCompiler *typeCompiler)
    : QQmlCompilePass(typeCompiler)
    , qmlObjects(*typeCompiler->qmlObjects())
    , propertyCaches(typeCompiler->propertyCaches())
{
}

void QQmlAliasAnnotator::annotateBindingsToAliases()
{
    for (int i = 0; i < qmlObjects.count(); ++i) {
        QQmlPropertyCache *propertyCache = propertyCaches.at(i);
        if (!propertyCache)
            continue;

        const QmlIR::Object *obj = qmlObjects.at(i);

        QmlIR::PropertyResolver resolver(propertyCache);
        QQmlPropertyData *defaultProperty = obj->indexOfDefaultProperty != -1 ? propertyCache->parent()->defaultProperty() : propertyCache->defaultProperty();

        for (QmlIR::Binding *binding = obj->firstBinding(); binding; binding = binding->next) {
            if (!binding->isValueBinding())
                continue;
            bool notInRevision = false;
            QQmlPropertyData *pd = binding->propertyNameIndex != 0 ? resolver.property(stringAt(binding->propertyNameIndex), &notInRevision) : defaultProperty;
            if (pd && pd->isAlias())
                binding->flags |= QV4::CompiledData::Binding::IsBindingToAlias;
        }
    }
}

QQmlScriptStringScanner::QQmlScriptStringScanner(QQmlTypeCompiler *typeCompiler)
    : QQmlCompilePass(typeCompiler)
    , qmlObjects(*typeCompiler->qmlObjects())
    , propertyCaches(typeCompiler->propertyCaches())
{

}

void QQmlScriptStringScanner::scan()
{
    const int scriptStringMetaType = qMetaTypeId<QQmlScriptString>();
    for (int i = 0; i < qmlObjects.count(); ++i) {
        QQmlPropertyCache *propertyCache = propertyCaches.at(i);
        if (!propertyCache)
            continue;

        const QmlIR::Object *obj = qmlObjects.at(i);

        QmlIR::PropertyResolver resolver(propertyCache);
        QQmlPropertyData *defaultProperty = obj->indexOfDefaultProperty != -1 ? propertyCache->parent()->defaultProperty() : propertyCache->defaultProperty();

        for (QmlIR::Binding *binding = obj->firstBinding(); binding; binding = binding->next) {
            if (binding->type != QV4::CompiledData::Binding::Type_Script)
                continue;
            bool notInRevision = false;
            QQmlPropertyData *pd = binding->propertyNameIndex != 0 ? resolver.property(stringAt(binding->propertyNameIndex), &notInRevision) : defaultProperty;
            if (!pd || pd->propType != scriptStringMetaType)
                continue;

            QmlIR::CompiledFunctionOrExpression *foe = obj->functionsAndExpressions->slowAt(binding->value.compiledScriptIndex);
            if (foe)
                foe->disableAcceleratedLookups = true;

            QString script = compiler->bindingAsString(obj, binding->value.compiledScriptIndex);
            binding->stringIndex = compiler->registerString(script);
        }
    }
}

QQmlComponentAndAliasResolver::QQmlComponentAndAliasResolver(QQmlTypeCompiler *typeCompiler)
    : QQmlCompilePass(typeCompiler)
    , enginePrivate(typeCompiler->enginePrivate())
    , pool(typeCompiler->memoryPool())
    , qmlObjects(typeCompiler->qmlObjects())
    , indexOfRootObject(typeCompiler->rootObjectIndex())
    , _componentIndex(-1)
    , _objectIndexToIdInScope(0)
    , resolvedTypes(typeCompiler->resolvedTypes())
    , propertyCaches(typeCompiler->propertyCaches())
    , vmeMetaObjectData(typeCompiler->vmeMetaObjects())
    , objectIndexToIdForRoot(typeCompiler->objectIndexToIdForRoot())
    , objectIndexToIdPerComponent(typeCompiler->objectIndexToIdPerComponent())
{
}

void QQmlComponentAndAliasResolver::findAndRegisterImplicitComponents(const QmlIR::Object *obj, QQmlPropertyCache *propertyCache)
{
    QmlIR::PropertyResolver propertyResolver(propertyCache);

    QQmlPropertyData *defaultProperty = obj->indexOfDefaultProperty != -1 ? propertyCache->parent()->defaultProperty() : propertyCache->defaultProperty();

    for (QmlIR::Binding *binding = obj->firstBinding(); binding; binding = binding->next) {
        if (binding->type != QV4::CompiledData::Binding::Type_Object)
            continue;
        if (binding->flags & QV4::CompiledData::Binding::IsSignalHandlerObject)
            continue;

        const QmlIR::Object *targetObject = qmlObjects->at(binding->value.objectIndex);
        QQmlCompiledData::TypeReference *tr = resolvedTypes->value(targetObject->inheritedTypeNameIndex);
        Q_ASSERT(tr);
        if (QQmlType *targetType = tr->type) {
            if (targetType->metaObject() == &QQmlComponent::staticMetaObject)
                continue;
        } else if (tr->component) {
            if (tr->component->rootPropertyCache->firstCppMetaObject() == &QQmlComponent::staticMetaObject)
                continue;
        }

        QQmlPropertyData *pd = 0;
        if (binding->propertyNameIndex != 0) {
            bool notInRevision = false;
            pd = propertyResolver.property(stringAt(binding->propertyNameIndex), &notInRevision);
        } else {
            pd = defaultProperty;
        }
        if (!pd || !pd->isQObject())
            continue;

        QQmlPropertyCache *pc = enginePrivate->rawPropertyCacheForType(pd->propType);
        const QMetaObject *mo = pc->firstCppMetaObject();
        while (mo) {
            if (mo == &QQmlComponent::staticMetaObject)
                break;
            mo = mo->superClass();
        }

        if (!mo)
            continue;

        QQmlType *componentType = QQmlMetaType::qmlType(&QQmlComponent::staticMetaObject);
        Q_ASSERT(componentType);

        QmlIR::Object *syntheticComponent = pool->New<QmlIR::Object>();
        syntheticComponent->init(pool, compiler->registerString(QString::fromUtf8(componentType->typeName())), compiler->registerString(QString()));
        syntheticComponent->location = binding->valueLocation;

        if (!resolvedTypes->contains(syntheticComponent->inheritedTypeNameIndex)) {
            QQmlCompiledData::TypeReference *typeRef = new QQmlCompiledData::TypeReference;
            typeRef->type = componentType;
            typeRef->majorVersion = componentType->majorVersion();
            typeRef->minorVersion = componentType->minorVersion();
            resolvedTypes->insert(syntheticComponent->inheritedTypeNameIndex, typeRef);
        }

        qmlObjects->append(syntheticComponent);
        const int componentIndex = qmlObjects->count() - 1;
        // Keep property caches symmetric
        QQmlPropertyCache *componentCache = enginePrivate->cache(&QQmlComponent::staticMetaObject);
        componentCache->addref();
        propertyCaches.append(componentCache);

        QmlIR::Binding *syntheticBinding = pool->New<QmlIR::Binding>();
        *syntheticBinding = *binding;
        syntheticBinding->type = QV4::CompiledData::Binding::Type_Object;
        QString error = syntheticComponent->appendBinding(syntheticBinding, /*isListBinding*/false);
        Q_ASSERT(error.isEmpty());
        Q_UNUSED(error);

        binding->value.objectIndex = componentIndex;

        componentRoots.append(componentIndex);
        componentBoundaries.append(syntheticBinding->value.objectIndex);
    }
}

bool QQmlComponentAndAliasResolver::resolve()
{
    // Detect real Component {} objects as well as implicitly defined components, such as
    //     someItemDelegate: Item {}
    // In the implicit case Item is surrounded by a synthetic Component {} because the property
    // on the left hand side is of QQmlComponent type.
    const int objCountWithoutSynthesizedComponents = qmlObjects->count();
    for (int i = 0; i < objCountWithoutSynthesizedComponents; ++i) {
        const QmlIR::Object *obj = qmlObjects->at(i);
        QQmlPropertyCache *cache = propertyCaches.at(i);
        if (obj->inheritedTypeNameIndex == 0 && !cache)
            continue;

        bool isExplicitComponent = false;

        if (obj->inheritedTypeNameIndex) {
            QQmlCompiledData::TypeReference *tref = resolvedTypes->value(obj->inheritedTypeNameIndex);
            Q_ASSERT(tref);
            if (tref->type && tref->type->metaObject() == &QQmlComponent::staticMetaObject)
                isExplicitComponent = true;
        }
        if (!isExplicitComponent) {
            if (cache)
                findAndRegisterImplicitComponents(obj, cache);
            continue;
        }

        componentRoots.append(i);

        if (obj->functionCount() > 0)
            COMPILE_EXCEPTION(obj, tr("Component objects cannot declare new functions."));
        if (obj->propertyCount() > 0)
            COMPILE_EXCEPTION(obj, tr("Component objects cannot declare new properties."));
        if (obj->signalCount() > 0)
            COMPILE_EXCEPTION(obj, tr("Component objects cannot declare new signals."));

        if (obj->bindingCount() == 0)
            COMPILE_EXCEPTION(obj, tr("Cannot create empty component specification"));

        const QmlIR::Binding *rootBinding = obj->firstBinding();

        for (const QmlIR::Binding *b = rootBinding; b; b = b->next) {
            if (b->propertyNameIndex != 0)
                COMPILE_EXCEPTION(rootBinding, tr("Component elements may not contain properties other than id"));
        }

        if (rootBinding->next || rootBinding->type != QV4::CompiledData::Binding::Type_Object)
            COMPILE_EXCEPTION(obj, tr("Invalid component body specification"));

        componentBoundaries.append(rootBinding->value.objectIndex);
    }

    std::sort(componentBoundaries.begin(), componentBoundaries.end());

    for (int i = 0; i < componentRoots.count(); ++i) {
        const QmlIR::Object *component  = qmlObjects->at(componentRoots.at(i));
        const QmlIR::Binding *rootBinding = component->firstBinding();

        _componentIndex = i;
        _idToObjectIndex.clear();

        _objectIndexToIdInScope = &(*objectIndexToIdPerComponent)[componentRoots.at(i)];

        _objectsWithAliases.clear();

        if (!collectIdsAndAliases(rootBinding->value.objectIndex))
            return false;

        if (!resolveAliases())
            return false;
    }

    // Collect ids and aliases for root
    _componentIndex = -1;
    _idToObjectIndex.clear();
    _objectIndexToIdInScope = objectIndexToIdForRoot;
    _objectsWithAliases.clear();

    collectIdsAndAliases(indexOfRootObject);

    resolveAliases();

    // Implicit component insertion may have added objects and thus we also need
    // to extend the symmetric propertyCaches.
    compiler->setPropertyCaches(propertyCaches);

    return true;
}

bool QQmlComponentAndAliasResolver::collectIdsAndAliases(int objectIndex)
{
    const QmlIR::Object *obj = qmlObjects->at(objectIndex);

    if (obj->idIndex != 0) {
        if (_idToObjectIndex.contains(obj->idIndex)) {
            recordError(obj->locationOfIdProperty, tr("id is not unique"));
            return false;
        }
        _idToObjectIndex.insert(obj->idIndex, objectIndex);
        _objectIndexToIdInScope->insert(objectIndex, _objectIndexToIdInScope->count());
    }

    for (const QmlIR::Property *property = obj->firstProperty(); property; property = property->next) {
        if (property->type == QV4::CompiledData::Property::Alias) {
            _objectsWithAliases.append(objectIndex);
            break;
        }
    }

    for (const QmlIR::Binding *binding = obj->firstBinding(); binding; binding = binding->next) {
        if (binding->type != QV4::CompiledData::Binding::Type_Object
            && binding->type != QV4::CompiledData::Binding::Type_AttachedProperty
            && binding->type != QV4::CompiledData::Binding::Type_GroupProperty)
            continue;

        // Stop at Component boundary
        if (std::binary_search(componentBoundaries.constBegin(), componentBoundaries.constEnd(), binding->value.objectIndex))
            continue;

        if (!collectIdsAndAliases(binding->value.objectIndex))
            return false;
    }

    return true;
}

bool QQmlComponentAndAliasResolver::resolveAliases()
{
    foreach (int objectIndex, _objectsWithAliases) {
        const QmlIR::Object *obj = qmlObjects->at(objectIndex);

        QQmlPropertyCache *propertyCache = propertyCaches.at(objectIndex);
        Q_ASSERT(propertyCache);

        int effectiveSignalIndex = propertyCache->signalHandlerIndexCacheStart + propertyCache->propertyIndexCache.count();
        int effectivePropertyIndex = propertyCache->propertyIndexCacheStart + propertyCache->propertyIndexCache.count();
        int effectiveAliasIndex = 0;

        const QmlIR::Property *p = obj->firstProperty();
        for (int propertyIndex = 0; propertyIndex < obj->propertyCount(); ++propertyIndex, p = p->next) {
            if (p->type != QV4::CompiledData::Property::Alias)
                continue;

            const int idIndex = p->aliasIdValueIndex;
            const int targetObjectIndex = _idToObjectIndex.value(idIndex, -1);
            if (targetObjectIndex == -1) {
                recordError(p->aliasLocation, tr("Invalid alias reference. Unable to find id \"%1\"").arg(stringAt(idIndex)));
                return false;
            }
            const int targetId = _objectIndexToIdInScope->value(targetObjectIndex, -1);
            Q_ASSERT(targetId != -1);

            const QString aliasPropertyValue = stringAt(p->aliasPropertyValueIndex);

            QStringRef property;
            QStringRef subProperty;

            const int propertySeparator = aliasPropertyValue.indexOf(QLatin1Char('.'));
            if (propertySeparator != -1) {
                property = aliasPropertyValue.leftRef(propertySeparator);
                subProperty = aliasPropertyValue.midRef(propertySeparator + 1);
            } else
                property = QStringRef(&aliasPropertyValue, 0, aliasPropertyValue.length());

            int propIdx = -1;
            int propType = 0;
            int notifySignal = -1;
            int flags = 0;
            int type = 0;
            bool writable = false;
            bool resettable = false;

            quint32 propertyFlags = QQmlPropertyData::IsAlias;

            if (property.isEmpty()) {
                const QmlIR::Object *targetObject = qmlObjects->at(targetObjectIndex);
                QQmlCompiledData::TypeReference *typeRef = resolvedTypes->value(targetObject->inheritedTypeNameIndex);
                Q_ASSERT(typeRef);

                if (typeRef->type)
                    type = typeRef->type->typeId();
                else
                    type = typeRef->component->metaTypeId;

                flags |= QML_ALIAS_FLAG_PTR;
                propertyFlags |= QQmlPropertyData::IsQObjectDerived;
            } else {
                QQmlPropertyCache *targetCache = propertyCaches.at(targetObjectIndex);
                Q_ASSERT(targetCache);
                QmlIR::PropertyResolver resolver(targetCache);

                QQmlPropertyData *targetProperty = resolver.property(property.toString());
                if (!targetProperty || targetProperty->coreIndex > 0x0000FFFF) {
                    recordError(p->aliasLocation, tr("Invalid alias location"));
                    return false;
                }

                propIdx = targetProperty->coreIndex;
                type = targetProperty->propType;

                writable = targetProperty->isWritable();
                resettable = targetProperty->isResettable();
                notifySignal = targetProperty->notifyIndex;

                if (!subProperty.isEmpty()) {
                    QQmlValueType *valueType = QQmlValueTypeFactory::valueType(type);
                    if (!valueType) {
                        recordError(p->aliasLocation, tr("Invalid alias location"));
                        return false;
                    }

                    propType = type;

                    int valueTypeIndex =
                        valueType->metaObject()->indexOfProperty(subProperty.toString().toUtf8().constData());
                    if (valueTypeIndex == -1) {
                        recordError(p->aliasLocation, tr("Invalid alias location"));
                        return false;
                    }
                    Q_ASSERT(valueTypeIndex <= 0x0000FFFF);

                    propIdx |= (valueTypeIndex << 16);
                    if (valueType->metaObject()->property(valueTypeIndex).isEnumType())
                        type = QVariant::Int;
                    else
                        type = valueType->metaObject()->property(valueTypeIndex).userType();

                } else {
                    if (targetProperty->isEnum()) {
                        type = QVariant::Int;
                    } else {
                        // Copy type flags
                        propertyFlags |= targetProperty->getFlags() & QQmlPropertyData::PropTypeFlagMask;

                        if (targetProperty->isVarProperty())
                            propertyFlags |= QQmlPropertyData::IsQVariant;

                        if (targetProperty->isQObject())
                            flags |= QML_ALIAS_FLAG_PTR;
                    }
                }
            }

            QQmlVMEMetaData::AliasData aliasData = { targetId, propIdx, propType, flags, notifySignal };

            typedef QQmlVMEMetaData VMD;
            QByteArray &dynamicData = (*vmeMetaObjectData)[objectIndex];
            Q_ASSERT(!dynamicData.isEmpty());
            VMD *vmd = (QQmlVMEMetaData *)dynamicData.data();
            *(vmd->aliasData() + effectiveAliasIndex++) = aliasData;

            Q_ASSERT(dynamicData.isDetached());

            if (!(p->flags & QV4::CompiledData::Property::IsReadOnly) && writable)
                propertyFlags |= QQmlPropertyData::IsWritable;
            else
                propertyFlags &= ~QQmlPropertyData::IsWritable;

            if (resettable)
                propertyFlags |= QQmlPropertyData::IsResettable;
            else
                propertyFlags &= ~QQmlPropertyData::IsResettable;

            QString propertyName = stringAt(p->nameIndex);
            if (propertyIndex == obj->indexOfDefaultProperty) propertyCache->_defaultPropertyName = propertyName;
            propertyCache->appendProperty(propertyName, propertyFlags, effectivePropertyIndex++,
                                          type, effectiveSignalIndex++);

        }
    }
    return true;
}

QQmlPropertyValidator::QQmlPropertyValidator(QQmlTypeCompiler *typeCompiler)
    : QQmlCompilePass(typeCompiler)
    , enginePrivate(typeCompiler->enginePrivate())
    , qmlUnit(typeCompiler->qmlUnit())
    , resolvedTypes(*typeCompiler->resolvedTypes())
    , customParsers(typeCompiler->customParserCache())
    , propertyCaches(typeCompiler->propertyCaches())
    , objectIndexToIdPerComponent(*typeCompiler->objectIndexToIdPerComponent())
    , customParserBindingsPerObject(typeCompiler->customParserBindings())
    , _seenObjectWithId(false)
{
}

bool QQmlPropertyValidator::validate()
{
    if (!validateObject(qmlUnit->indexOfRootObject, /*instantiatingBinding*/0))
        return false;
    compiler->setDeferredBindingsPerObject(deferredBindingsPerObject);
    return true;
}

const QQmlImports &QQmlPropertyValidator::imports() const
{
    return *compiler->imports();
}

QString QQmlPropertyValidator::bindingAsString(int objectIndex, const QV4::CompiledData::Binding *binding) const
{
    const QmlIR::Object *object = compiler->qmlObjects()->value(objectIndex);
    if (!object)
        return QString();
    int reverseIndex = object->runtimeFunctionIndices->indexOf(binding->value.compiledScriptIndex);
    if (reverseIndex == -1)
        return QString();
    return compiler->bindingAsString(object, reverseIndex);
}

typedef QVarLengthArray<const QV4::CompiledData::Binding *, 8> GroupPropertyVector;

struct BindingFinder
{
    bool operator()(quint32 name, const QV4::CompiledData::Binding *binding) const
    {
        return name < binding->propertyNameIndex;
    }
    bool operator()(const QV4::CompiledData::Binding *binding, quint32 name) const
    {
        return binding->propertyNameIndex < name;
    }
    bool operator()(const QV4::CompiledData::Binding *lhs, const QV4::CompiledData::Binding *rhs) const
    {
        return lhs->propertyNameIndex < rhs->propertyNameIndex;
    }
};

bool QQmlPropertyValidator::validateObject(int objectIndex, const QV4::CompiledData::Binding *instantiatingBinding, bool populatingValueTypeGroupProperty)
{
    const QV4::CompiledData::Object *obj = qmlUnit->objectAt(objectIndex);
    if (obj->idIndex != 0)
        _seenObjectWithId = true;

    if (isComponent(objectIndex)) {
        Q_ASSERT(obj->nBindings == 1);
        const QV4::CompiledData::Binding *componentBinding = obj->bindingTable();
        Q_ASSERT(componentBinding->type == QV4::CompiledData::Binding::Type_Object);
        return validateObject(componentBinding->value.objectIndex, componentBinding);
    }

    QQmlPropertyCache *propertyCache = propertyCaches.at(objectIndex);
    if (!propertyCache)
        return true;

    QStringList deferredPropertyNames;
    {
        const QMetaObject *mo = propertyCache->firstCppMetaObject();
        const int namesIndex = mo->indexOfClassInfo("DeferredPropertyNames");
        if (namesIndex != -1) {
            QMetaClassInfo classInfo = mo->classInfo(namesIndex);
            deferredPropertyNames = QString::fromUtf8(classInfo.value()).split(QLatin1Char(','));
        }
    }

    QQmlCustomParser *customParser = customParsers.value(obj->inheritedTypeNameIndex);
    QList<const QV4::CompiledData::Binding*> customBindings;

    // Collect group properties first for sanity checking
    // vector values are sorted by property name string index.
    GroupPropertyVector groupProperties;
    const QV4::CompiledData::Binding *binding = obj->bindingTable();
    for (quint32 i = 0; i < obj->nBindings; ++i, ++binding) {
        if (!binding->isGroupProperty())
                continue;

        if (binding->flags & QV4::CompiledData::Binding::IsOnAssignment)
            continue;

        if (populatingValueTypeGroupProperty) {
            recordError(binding->location, tr("Property assignment expected"));
            return false;
        }

        GroupPropertyVector::const_iterator pos = std::lower_bound(groupProperties.constBegin(), groupProperties.constEnd(), binding->propertyNameIndex, BindingFinder());
        groupProperties.insert(pos, binding);
    }

    QBitArray customParserBindings(obj->nBindings);
    QBitArray deferredBindings;

    QmlIR::PropertyResolver propertyResolver(propertyCache);

    QString defaultPropertyName;
    QQmlPropertyData *defaultProperty = 0;
    if (obj->indexOfDefaultProperty != -1) {
        QQmlPropertyCache *cache = propertyCache->parent();
        defaultPropertyName = cache->defaultPropertyName();
        defaultProperty = cache->defaultProperty();
    } else {
        defaultPropertyName = propertyCache->defaultPropertyName();
        defaultProperty = propertyCache->defaultProperty();
    }

    binding = obj->bindingTable();
    for (quint32 i = 0; i < obj->nBindings; ++i, ++binding) {
        QString name = stringAt(binding->propertyNameIndex);

        if (customParser) {
            if (binding->type == QV4::CompiledData::Binding::Type_AttachedProperty) {
                if (customParser->flags() & QQmlCustomParser::AcceptsAttachedProperties) {
                    customBindings << binding;
                    customParserBindings.setBit(i);
                    continue;
                }
            } else if (QmlIR::IRBuilder::isSignalPropertyName(name)
                       && !(customParser->flags() & QQmlCustomParser::AcceptsSignalHandlers)) {
                customBindings << binding;
                customParserBindings.setBit(i);
                continue;
            }
        }

        // Signal handlers were resolved and checked earlier in the signal handler conversion pass.
        if (binding->flags & QV4::CompiledData::Binding::IsSignalHandlerExpression
            || binding->flags & QV4::CompiledData::Binding::IsSignalHandlerObject)
            continue;

        if (name.constData()->isUpper() && !binding->isAttachedProperty()) {
            QQmlType *type = 0;
            QQmlImportNamespace *typeNamespace = 0;
            compiler->imports()->resolveType(stringAt(binding->propertyNameIndex), &type, 0, 0, &typeNamespace);
            if (typeNamespace)
                recordError(binding->location, tr("Invalid use of namespace"));
            else
                recordError(binding->location, tr("Invalid attached object assignment"));
            return false;
        }

        bool bindingToDefaultProperty = false;

        bool notInRevision = false;
        QQmlPropertyData *pd = 0;
        if (!name.isEmpty()) {
            if (binding->flags & QV4::CompiledData::Binding::IsSignalHandlerExpression
                || binding->flags & QV4::CompiledData::Binding::IsSignalHandlerObject)
                pd = propertyResolver.signal(name, &notInRevision);
            else
                pd = propertyResolver.property(name, &notInRevision);

            if (notInRevision) {
                QString typeName = stringAt(obj->inheritedTypeNameIndex);
                QQmlCompiledData::TypeReference *objectType = resolvedTypes.value(obj->inheritedTypeNameIndex);
                if (objectType && objectType->type) {
                    COMPILE_EXCEPTION(binding, tr("\"%1.%2\" is not available in %3 %4.%5.").arg(typeName).arg(name).arg(objectType->type->module()).arg(objectType->majorVersion).arg(objectType->minorVersion));
                } else {
                    COMPILE_EXCEPTION(binding, tr("\"%1.%2\" is not available due to component versioning.").arg(typeName).arg(name));
                }
            }
        } else {
           if (instantiatingBinding && instantiatingBinding->type == QV4::CompiledData::Binding::Type_GroupProperty)
               COMPILE_EXCEPTION(binding, tr("Cannot assign a value directly to a grouped property"));

           pd = defaultProperty;
           name = defaultPropertyName;
           bindingToDefaultProperty = true;
        }

        bool seenSubObjectWithId = false;

        if (binding->type >= QV4::CompiledData::Binding::Type_Object && !customParser) {
            qSwap(_seenObjectWithId, seenSubObjectWithId);
            const bool subObjectValid = validateObject(binding->value.objectIndex, binding, pd && QQmlValueTypeFactory::valueType(pd->propType));
            qSwap(_seenObjectWithId, seenSubObjectWithId);
            if (!subObjectValid)
                return false;
            _seenObjectWithId |= seenSubObjectWithId;
        }

        if (!seenSubObjectWithId
            && !deferredPropertyNames.isEmpty() && deferredPropertyNames.contains(name)) {

            if (deferredBindings.isEmpty())
                deferredBindings.resize(obj->nBindings);

            deferredBindings.setBit(i);
        }

        if (binding->type == QV4::CompiledData::Binding::Type_AttachedProperty) {
            if (instantiatingBinding && (instantiatingBinding->isAttachedProperty() || instantiatingBinding->isGroupProperty())) {
                recordError(binding->location, tr("Attached properties cannot be used here"));
                return false;
            }
            continue;
        }

        if (pd) {
            GroupPropertyVector::const_iterator assignedGroupProperty = std::lower_bound(groupProperties.constBegin(), groupProperties.constEnd(), binding->propertyNameIndex, BindingFinder());
            const bool assigningToGroupProperty = assignedGroupProperty != groupProperties.constEnd() && !(binding->propertyNameIndex < (*assignedGroupProperty)->propertyNameIndex);

            if (!pd->isWritable()
                && !pd->isQList()
                && !binding->isGroupProperty()
                && !(binding->flags & QV4::CompiledData::Binding::InitializerForReadOnlyDeclaration)
                ) {

                if (assigningToGroupProperty && binding->type < QV4::CompiledData::Binding::Type_Object)
                    recordError(binding->valueLocation, tr("Cannot assign a value directly to a grouped property"));
                else
                    recordError(binding->valueLocation, tr("Invalid property assignment: \"%1\" is a read-only property").arg(name));
                return false;
            }

            if (!pd->isQList() && (binding->flags & QV4::CompiledData::Binding::IsListItem)) {
                QString error;
                if (pd->propType == qMetaTypeId<QQmlScriptString>())
                    error = tr( "Cannot assign multiple values to a script property");
                else
                    error = tr( "Cannot assign multiple values to a singular property");
                recordError(binding->valueLocation, error);
                return false;
            }

            if (!bindingToDefaultProperty
                && !binding->isGroupProperty()
                && !(binding->flags & QV4::CompiledData::Binding::IsOnAssignment)
                && assigningToGroupProperty) {
                QV4::CompiledData::Location loc = binding->valueLocation;
                if (loc < (*assignedGroupProperty)->valueLocation)
                    loc = (*assignedGroupProperty)->valueLocation;

                if (pd && QQmlValueTypeFactory::isValueType(pd->propType))
                    recordError(loc, tr("Property has already been assigned a value"));
                else
                    recordError(loc, tr("Cannot assign a value directly to a grouped property"));
                return false;
            }

            if (binding->type < QV4::CompiledData::Binding::Type_Script) {
                if (!validateLiteralBinding(propertyCache, pd, binding))
                    return false;
            } else if (binding->type == QV4::CompiledData::Binding::Type_Object) {
                if (!validateObjectBinding(pd, name, binding))
                    return false;
            } else if (binding->isGroupProperty()) {
                if (QQmlValueTypeFactory::isValueType(pd->propType)) {
                    if (QQmlValueTypeFactory::valueType(pd->propType)) {
                        if (!pd->isWritable()) {
                            recordError(binding->location, tr("Invalid property assignment: \"%1\" is a read-only property").arg(name));
                            return false;
                        }
                    } else {
                        recordError(binding->location, tr("Invalid grouped property access"));
                        return false;
                    }
                } else {
                    if (!enginePrivate->propertyCacheForType(pd->propType)) {
                        recordError(binding->location, tr("Invalid grouped property access"));
                        return false;
                    }
                }
            }
        } else {
            if (customParser) {
                customBindings << binding;
                customParserBindings.setBit(i);
                continue;
            }
            if (bindingToDefaultProperty) {
                COMPILE_EXCEPTION(binding, tr("Cannot assign to non-existent default property"));
            } else {
                COMPILE_EXCEPTION(binding, tr("Cannot assign to non-existent property \"%1\"").arg(name));
            }
        }
    }

    if (customParser && !customBindings.isEmpty()) {
        customParser->clearErrors();
        customParser->compiler = this;
        customParser->imports = compiler->imports();
        customParser->verifyBindings(qmlUnit, customBindings);
        customParser->compiler = 0;
        customParser->imports = (QQmlImports*)0;
        customParserBindingsPerObject->insert(objectIndex, customParserBindings);
        const QList<QQmlError> parserErrors = customParser->errors();
        if (!parserErrors.isEmpty()) {
            foreach (QQmlError error, parserErrors)
                compiler->recordError(error);
            return false;
        }
    }

    if (!deferredBindings.isEmpty())
        deferredBindingsPerObject.insert(objectIndex, deferredBindings);

    return true;
}

bool QQmlPropertyValidator::validateLiteralBinding(QQmlPropertyCache *propertyCache, QQmlPropertyData *property, const QV4::CompiledData::Binding *binding)
{
    if (property->isQList()) {
        recordError(binding->valueLocation, tr("Cannot assign primitives to lists"));
        return false;
    }

    if (property->isEnum()) {
        if (binding->flags & QV4::CompiledData::Binding::IsResolvedEnum)
            return true;

        QString value = binding->valueAsString(qmlUnit);
        QMetaProperty p = propertyCache->firstCppMetaObject()->property(property->coreIndex);
        bool ok;
        if (p.isFlagType()) {
            p.enumerator().keysToValue(value.toUtf8().constData(), &ok);
        } else
            p.enumerator().keyToValue(value.toUtf8().constData(), &ok);

        if (!ok) {
            recordError(binding->valueLocation, tr("Invalid property assignment: unknown enumeration"));
            return false;
        }
        return true;
    }

    switch (property->propType) {
    case QMetaType::QVariant:
    break;
    case QVariant::String: {
        if (!binding->evaluatesToString()) {
            recordError(binding->valueLocation, tr("Invalid property assignment: string expected"));
            return false;
        }
    }
    break;
    case QVariant::StringList: {
        if (!binding->evaluatesToString()) {
            recordError(binding->valueLocation, tr("Invalid property assignment: string or string list expected"));
            return false;
        }
    }
    break;
    case QVariant::ByteArray: {
        if (binding->type != QV4::CompiledData::Binding::Type_String) {
            recordError(binding->valueLocation, tr("Invalid property assignment: byte array expected"));
            return false;
        }
    }
    break;
    case QVariant::Url: {
        if (binding->type != QV4::CompiledData::Binding::Type_String) {
            recordError(binding->valueLocation, tr("Invalid property assignment: url expected"));
            return false;
        }
    }
    break;
    case QVariant::UInt: {
        if (binding->type == QV4::CompiledData::Binding::Type_Number) {
            double d = binding->valueAsNumber();
            if (double(uint(d)) == d)
                return true;
        }
        recordError(binding->valueLocation, tr("Invalid property assignment: unsigned int expected"));
        return false;
    }
    break;
    case QVariant::Int: {
        if (binding->type == QV4::CompiledData::Binding::Type_Number) {
            double d = binding->valueAsNumber();
            if (double(int(d)) == d)
                return true;
        }
        recordError(binding->valueLocation, tr("Invalid property assignment: int expected"));
        return false;
    }
    break;
    case QMetaType::Float: {
        if (binding->type != QV4::CompiledData::Binding::Type_Number) {
            recordError(binding->valueLocation, tr("Invalid property assignment: number expected"));
            return false;
        }
    }
    break;
    case QVariant::Double: {
        if (binding->type != QV4::CompiledData::Binding::Type_Number) {
            recordError(binding->valueLocation, tr("Invalid property assignment: number expected"));
            return false;
        }
    }
    break;
    case QVariant::Color: {
        bool ok = false;
        QQmlStringConverters::rgbaFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            recordError(binding->valueLocation, tr("Invalid property assignment: color expected"));
            return false;
        }
    }
    break;
#ifndef QT_NO_DATESTRING
    case QVariant::Date: {
        bool ok = false;
        QQmlStringConverters::dateFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            recordError(binding->valueLocation, tr("Invalid property assignment: date expected"));
            return false;
        }
    }
    break;
    case QVariant::Time: {
        bool ok = false;
        QQmlStringConverters::timeFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            recordError(binding->valueLocation, tr("Invalid property assignment: time expected"));
            return false;
        }
    }
    break;
    case QVariant::DateTime: {
        bool ok = false;
        QQmlStringConverters::dateTimeFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            recordError(binding->valueLocation, tr("Invalid property assignment: datetime expected"));
            return false;
        }
    }
    break;
#endif // QT_NO_DATESTRING
    case QVariant::Point: {
        bool ok = false;
        QQmlStringConverters::pointFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            recordError(binding->valueLocation, tr("Invalid property assignment: point expected"));
            return false;
        }
    }
    break;
    case QVariant::PointF: {
        bool ok = false;
        QQmlStringConverters::pointFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            recordError(binding->valueLocation, tr("Invalid property assignment: point expected"));
            return false;
        }
    }
    break;
    case QVariant::Size: {
        bool ok = false;
        QQmlStringConverters::sizeFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            recordError(binding->valueLocation, tr("Invalid property assignment: size expected"));
            return false;
        }
    }
    break;
    case QVariant::SizeF: {
        bool ok = false;
        QQmlStringConverters::sizeFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            recordError(binding->valueLocation, tr("Invalid property assignment: size expected"));
            return false;
        }
    }
    break;
    case QVariant::Rect: {
        bool ok = false;
        QQmlStringConverters::rectFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            recordError(binding->valueLocation, tr("Invalid property assignment: rect expected"));
            return false;
        }
    }
    break;
    case QVariant::RectF: {
        bool ok = false;
        QQmlStringConverters::rectFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            recordError(binding->valueLocation, tr("Invalid property assignment: point expected"));
            return false;
        }
    }
    break;
    case QVariant::Bool: {
        if (binding->type != QV4::CompiledData::Binding::Type_Boolean) {
            recordError(binding->valueLocation, tr("Invalid property assignment: boolean expected"));
            return false;
        }
    }
    break;
    case QVariant::Vector3D: {
        struct {
            float xp;
            float yp;
            float zy;
        } vec;
        if (!QQmlStringConverters::createFromString(QMetaType::QVector3D, binding->valueAsString(qmlUnit), &vec, sizeof(vec))) {
            recordError(binding->valueLocation, tr("Invalid property assignment: 3D vector expected"));
            return false;
        }
    }
    break;
    case QVariant::Vector4D: {
        struct {
            float xp;
            float yp;
            float zy;
            float wp;
        } vec;
        if (!QQmlStringConverters::createFromString(QMetaType::QVector4D, binding->valueAsString(qmlUnit), &vec, sizeof(vec))) {
            recordError(binding->valueLocation, tr("Invalid property assignment: 4D vector expected"));
            return false;
        }
    }
    break;
    case QVariant::RegExp:
        recordError(binding->valueLocation, tr("Invalid property assignment: regular expression expected; use /pattern/ syntax"));
        return false;
    default: {
        // generate single literal value assignment to a list property if required
        if (property->propType == qMetaTypeId<QList<qreal> >()) {
            if (binding->type != QV4::CompiledData::Binding::Type_Number) {
                recordError(binding->valueLocation, tr("Invalid property assignment: number or array of numbers expected"));
                return false;
            }
            break;
        } else if (property->propType == qMetaTypeId<QList<int> >()) {
            bool ok = (binding->type == QV4::CompiledData::Binding::Type_Number);
            if (ok) {
                double n = binding->valueAsNumber();
                if (double(int(n)) != n)
                    ok = false;
            }
            if (!ok)
                recordError(binding->valueLocation, tr("Invalid property assignment: int or array of ints expected"));
            break;
        } else if (property->propType == qMetaTypeId<QList<bool> >()) {
            if (binding->type != QV4::CompiledData::Binding::Type_Boolean) {
                recordError(binding->valueLocation, tr("Invalid property assignment: bool or array of bools expected"));
                return false;
            }
            break;
        } else if (property->propType == qMetaTypeId<QList<QUrl> >()) {
            if (binding->type != QV4::CompiledData::Binding::Type_String) {
                recordError(binding->valueLocation, tr("Invalid property assignment: url or array of urls expected"));
                return false;
            }
            break;
        } else if (property->propType == qMetaTypeId<QList<QString> >()) {
            if (!binding->evaluatesToString()) {
                recordError(binding->valueLocation, tr("Invalid property assignment: string or array of strings expected"));
                return false;
            }
            break;
        } else if (property->propType == qMetaTypeId<QJSValue>()) {
            break;
        } else if (property->propType == qMetaTypeId<QQmlScriptString>()) {
            break;
        }

        // otherwise, try a custom type assignment
        QQmlMetaType::StringConverter converter = QQmlMetaType::customStringConverter(property->propType);
        if (!converter) {
            recordError(binding->valueLocation, tr("Invalid property assignment: unsupported type \"%1\"").arg(QString::fromLatin1(QMetaType::typeName(property->propType))));
            return false;
        }
    }
    break;
    }
    return true;
}

/*!
    Returns true if from can be assigned to a (QObject) property of type
    to.
*/
bool QQmlPropertyValidator::canCoerce(int to, QQmlPropertyCache *fromMo)
{
    QQmlPropertyCache *toMo = enginePrivate->rawPropertyCacheForType(to);

    while (fromMo) {
        if (fromMo == toMo)
            return true;
        fromMo = fromMo->parent();
    }
    return false;
}

bool QQmlPropertyValidator::validateObjectBinding(QQmlPropertyData *property, const QString &propertyName, const QV4::CompiledData::Binding *binding)
{
    if (binding->flags & QV4::CompiledData::Binding::IsOnAssignment) {
        Q_ASSERT(binding->type == QV4::CompiledData::Binding::Type_Object);

        bool isValueSource = false;
        bool isPropertyInterceptor = false;

        QQmlType *qmlType = 0;
        const QV4::CompiledData::Object *targetObject = qmlUnit->objectAt(binding->value.objectIndex);
        QQmlCompiledData::TypeReference *typeRef = resolvedTypes.value(targetObject->inheritedTypeNameIndex);
        if (typeRef) {
            QQmlPropertyCache *cache = typeRef->createPropertyCache(QQmlEnginePrivate::get(enginePrivate));
            const QMetaObject *mo = cache->firstCppMetaObject();
            while (mo && !qmlType) {
                qmlType = QQmlMetaType::qmlType(mo);
                mo = mo->superClass();
            }
            Q_ASSERT(qmlType);
        }

        if (qmlType) {
            isValueSource = qmlType->propertyValueSourceCast() != -1;
            isPropertyInterceptor = qmlType->propertyValueInterceptorCast() != -1;
        }

        if (!isValueSource && !isPropertyInterceptor) {
            recordError(binding->valueLocation, tr("\"%1\" cannot operate on \"%2\"").arg(stringAt(targetObject->inheritedTypeNameIndex)).arg(propertyName));
            return false;
        }

        return true;
    }

    if (QQmlMetaType::isInterface(property->propType)) {
        // Can only check at instantiation time if the created sub-object successfully casts to the
        // target interface.
        return true;
    } else if (property->propType == QMetaType::QVariant) {
        // We can convert everything to QVariant :)
        return true;
    } else if (property->isQList()) {
        const int listType = enginePrivate->listType(property->propType);
        if (!QQmlMetaType::isInterface(listType)) {
            QQmlPropertyCache *source = propertyCaches.at(binding->value.objectIndex);
            if (!canCoerce(listType, source)) {
                recordError(binding->valueLocation, tr("Cannot assign object to list"));
                return false;
            }
        }
        return true;
    } else if (isComponent(binding->value.objectIndex)) {
        return true;
    } else if (binding->flags & QV4::CompiledData::Binding::IsSignalHandlerObject && property->isFunction()) {
        return true;
    } else if (QQmlValueTypeFactory::isValueType(property->propType)) {
        recordError(binding->location, tr("Unexpected object assignment"));
        return false;
    } else if (property->propType == qMetaTypeId<QQmlScriptString>()) {
        recordError(binding->valueLocation, tr("Invalid property assignment: script expected"));
        return false;
    } else {
        // We want to raw metaObject here as the raw metaobject is the
        // actual property type before we applied any extensions that might
        // effect the properties on the type, but don't effect assignability
        QQmlPropertyCache *propertyMetaObject = enginePrivate->rawPropertyCacheForType(property->propType);

        // Will be true if the assgned type inherits propertyMetaObject
        bool isAssignable = false;
        // Determine isAssignable value
        if (propertyMetaObject) {
            QQmlPropertyCache *c = propertyCaches.at(binding->value.objectIndex);
            while (c && !isAssignable) {
                isAssignable |= c == propertyMetaObject;
                c = c->parent();
            }
        }

        if (!isAssignable) {
            recordError(binding->valueLocation, tr("Cannot assign object to property"));
            return false;
        }
    }
    return true;
}

QQmlJSCodeGenerator::QQmlJSCodeGenerator(QQmlTypeCompiler *typeCompiler, QmlIR::JSCodeGen *v4CodeGen)
    : QQmlCompilePass(typeCompiler)
    , objectIndexToIdPerComponent(*typeCompiler->objectIndexToIdPerComponent())
    , resolvedTypes(*typeCompiler->resolvedTypes())
    , customParsers(typeCompiler->customParserCache())
    , qmlObjects(*typeCompiler->qmlObjects())
    , propertyCaches(typeCompiler->propertyCaches())
    , v4CodeGen(v4CodeGen)
{
}

bool QQmlJSCodeGenerator::generateCodeForComponents()
{
    const QHash<int, QHash<int, int> > &objectIndexToIdPerComponent = *compiler->objectIndexToIdPerComponent();
    for (QHash<int, QHash<int, int> >::ConstIterator component = objectIndexToIdPerComponent.constBegin(), end = objectIndexToIdPerComponent.constEnd();
         component != end; ++component) {
        if (!compileComponent(component.key(), component.value()))
            return false;
    }

    return compileComponent(compiler->rootObjectIndex(), *compiler->objectIndexToIdForRoot());
}

bool QQmlJSCodeGenerator::compileComponent(int contextObject, const QHash<int, int> &objectIndexToId)
{
    if (isComponent(contextObject)) {
        const QmlIR::Object *component = qmlObjects.at(contextObject);
        Q_ASSERT(component->bindingCount() == 1);
        const QV4::CompiledData::Binding *componentBinding = component->firstBinding();
        Q_ASSERT(componentBinding->type == QV4::CompiledData::Binding::Type_Object);
        contextObject = componentBinding->value.objectIndex;
    }

    QmlIR::JSCodeGen::ObjectIdMapping idMapping;
    if (!objectIndexToId.isEmpty()) {
        idMapping.reserve(objectIndexToId.count());

        for (QHash<int, int>::ConstIterator idIt = objectIndexToId.constBegin(), end = objectIndexToId.constEnd();
             idIt != end; ++idIt) {

            const int objectIndex = idIt.key();
            QmlIR::JSCodeGen::IdMapping m;
            const QmlIR::Object *obj = qmlObjects.at(objectIndex);
            m.name = stringAt(obj->idIndex);
            m.idIndex = idIt.value();
            m.type = propertyCaches.at(objectIndex);

            QQmlCompiledData::TypeReference *tref = resolvedTypes.value(obj->inheritedTypeNameIndex);
            if (tref && tref->isFullyDynamicType)
                m.type = 0;

            idMapping << m;
        }
    }
    v4CodeGen->beginContextScope(idMapping, propertyCaches.at(contextObject));

    if (!compileJavaScriptCodeInObjectsRecursively(contextObject, contextObject))
        return false;

    return true;
}

bool QQmlJSCodeGenerator::compileJavaScriptCodeInObjectsRecursively(int objectIndex, int scopeObjectIndex)
{
    if (isComponent(objectIndex))
        return true;

    QmlIR::Object *object = qmlObjects.at(objectIndex);
    if (object->functionsAndExpressions->count > 0) {
        QQmlPropertyCache *scopeObject = propertyCaches.at(scopeObjectIndex);
        v4CodeGen->beginObjectScope(scopeObject);

        QList<QmlIR::CompiledFunctionOrExpression> functionsToCompile;
        for (QmlIR::CompiledFunctionOrExpression *foe = object->functionsAndExpressions->first; foe; foe = foe->next) {
            const bool haveCustomParser = customParsers.contains(object->inheritedTypeNameIndex);
            if (haveCustomParser)
                foe->disableAcceleratedLookups = true;
            functionsToCompile << *foe;
        }
        const QVector<int> runtimeFunctionIndices = v4CodeGen->generateJSCodeForFunctionsAndBindings(functionsToCompile);
        QList<QQmlError> jsErrors = v4CodeGen->qmlErrors();
        if (!jsErrors.isEmpty()) {
            foreach (const QQmlError &e, jsErrors)
                compiler->recordError(e);
            return false;
        }

        QQmlJS::MemoryPool *pool = compiler->memoryPool();
        object->runtimeFunctionIndices = pool->New<QmlIR::FixedPoolArray<int> >();
        object->runtimeFunctionIndices->init(pool, runtimeFunctionIndices);
    }

    for (const QmlIR::Binding *binding = object->firstBinding(); binding; binding = binding->next) {
        if (binding->type < QV4::CompiledData::Binding::Type_Object)
            continue;

        int target = binding->value.objectIndex;
        int scope = binding->type == QV4::CompiledData::Binding::Type_Object ? target : scopeObjectIndex;

        if (!compileJavaScriptCodeInObjectsRecursively(binding->value.objectIndex, scope))
            return false;
    }

    return true;
}

QQmlDefaultPropertyMerger::QQmlDefaultPropertyMerger(QQmlTypeCompiler *typeCompiler)
    : QQmlCompilePass(typeCompiler)
    , qmlObjects(*typeCompiler->qmlObjects())
    , propertyCaches(typeCompiler->propertyCaches())
{

}

void QQmlDefaultPropertyMerger::mergeDefaultProperties()
{
    for (int i = 0; i < qmlObjects.count(); ++i)
        mergeDefaultProperties(i);
}

void QQmlDefaultPropertyMerger::mergeDefaultProperties(int objectIndex)
{
    QQmlPropertyCache *propertyCache = propertyCaches.at(objectIndex);
    if (!propertyCache)
        return;

    QmlIR::Object *object = qmlObjects.at(objectIndex);

    QString defaultProperty = object->indexOfDefaultProperty != -1 ? propertyCache->parent()->defaultPropertyName() : propertyCache->defaultPropertyName();
    QmlIR::Binding *bindingsToReinsert = 0;
    QmlIR::Binding *tail = 0;

    QmlIR::Binding *previousBinding = 0;
    QmlIR::Binding *binding = object->firstBinding();
    while (binding) {
        if (binding->propertyNameIndex == 0 || stringAt(binding->propertyNameIndex) != defaultProperty) {
            previousBinding = binding;
            binding = binding->next;
            continue;
        }

        QmlIR::Binding *toReinsert = binding;
        binding = object->unlinkBinding(previousBinding, binding);

        if (!tail) {
            bindingsToReinsert = toReinsert;
            tail = toReinsert;
        } else {
            tail->next = toReinsert;
            tail = tail->next;
        }
        tail->next = 0;
    }

    binding = bindingsToReinsert;
    while (binding) {
        QmlIR::Binding *toReinsert = binding;
        binding = binding->next;
        object->insertSorted(toReinsert);
    }
}

QQmlJavaScriptBindingExpressionSimplificationPass::QQmlJavaScriptBindingExpressionSimplificationPass(QQmlTypeCompiler *typeCompiler)
    : QQmlCompilePass(typeCompiler)
    , qmlObjects(*typeCompiler->qmlObjects())
    , jsModule(typeCompiler->jsIRModule())
{

}

void QQmlJavaScriptBindingExpressionSimplificationPass::reduceTranslationBindings()
{
    for (int i = 0; i < qmlObjects.count(); ++i)
        reduceTranslationBindings(i);
    if (!irFunctionsToRemove.isEmpty()) {
        QQmlIRFunctionCleanser cleanser(compiler, irFunctionsToRemove);
        cleanser.clean();
    }
}

void QQmlJavaScriptBindingExpressionSimplificationPass::reduceTranslationBindings(int objectIndex)
{
    const QmlIR::Object *obj = qmlObjects.at(objectIndex);

    for (QmlIR::Binding *binding = obj->firstBinding(); binding; binding = binding->next) {
        if (binding->type != QV4::CompiledData::Binding::Type_Script)
            continue;

        const int irFunctionIndex = obj->runtimeFunctionIndices->at(binding->value.compiledScriptIndex);
        QV4::IR::Function *irFunction = jsModule->functions.at(irFunctionIndex);
        if (simplifyBinding(irFunction, binding)) {
            irFunctionsToRemove.append(irFunctionIndex);
            jsModule->functions[irFunctionIndex] = 0;
            delete irFunction;
        }
    }
}

void QQmlJavaScriptBindingExpressionSimplificationPass::visitMove(QV4::IR::Move *move)
{
    QV4::IR::Temp *target = move->target->asTemp();
    if (!target || target->kind != QV4::IR::Temp::VirtualRegister) {
        discard();
        return;
    }

    if (QV4::IR::Call *call = move->source->asCall()) {
        if (QV4::IR::Name *n = call->base->asName()) {
            if (n->builtin == QV4::IR::Name::builtin_invalid) {
                visitFunctionCall(n->id, call->args, target);
                return;
            }
        }
        discard();
        return;
    }

    if (QV4::IR::Name *n = move->source->asName()) {
        if (n->builtin == QV4::IR::Name::builtin_qml_id_array
            || n->builtin == QV4::IR::Name::builtin_qml_imported_scripts_object
            || n->builtin == QV4::IR::Name::builtin_qml_context_object
            || n->builtin == QV4::IR::Name::builtin_qml_scope_object) {
            // these are free of side-effects
            return;
        }
        discard();
        return;
    }

    if (!move->source->asTemp() && !move->source->asString() && !move->source->asConst()) {
        discard();
        return;
    }

    _temps[target->index] = move->source;
}

void QQmlJavaScriptBindingExpressionSimplificationPass::visitFunctionCall(const QString *name, QV4::IR::ExprList *args, QV4::IR::Temp *target)
{
    // more than one function call?
    if (_nameOfFunctionCalled) {
        discard();
        return;
    }

    _nameOfFunctionCalled = name;

    _functionParameters.clear();
    while (args) {
        int slot;
        if (QV4::IR::Temp *param = args->expr->asTemp()) {
            if (param->kind != QV4::IR::Temp::VirtualRegister) {
                discard();
                return;
            }
            slot = param->index;
            _functionParameters.append(slot);
        } else if (QV4::IR::Const *param = args->expr->asConst()) {
            slot = --_synthesizedConsts;
            Q_ASSERT(!_temps.contains(slot));
            _temps[slot] = param;
            _functionParameters.append(slot);
        }
        args = args->next;
    }

    _functionCallReturnValue = target->index;
}

void QQmlJavaScriptBindingExpressionSimplificationPass::visitRet(QV4::IR::Ret *ret)
{
    // nothing initialized earlier?
    if (_returnValueOfBindingExpression != -1) {
        discard();
        return;
    }
    QV4::IR::Temp *target = ret->expr->asTemp();
    if (!target || target->kind != QV4::IR::Temp::VirtualRegister) {
        discard();
        return;
    }
    _returnValueOfBindingExpression = target->index;
}

bool QQmlJavaScriptBindingExpressionSimplificationPass::simplifyBinding(QV4::IR::Function *function, QmlIR::Binding *binding)
{
    _canSimplify = true;
    _nameOfFunctionCalled = 0;
    _functionParameters.clear();
    _functionCallReturnValue = -1;
    _temps.clear();
    _returnValueOfBindingExpression = -1;
    _synthesizedConsts = 0;

    // It would seem unlikely that function with some many basic blocks (after optimization)
    // consists merely of a qsTr call or a constant value return ;-)
    if (function->basicBlockCount() > 10)
        return false;

    foreach (QV4::IR::BasicBlock *bb, function->basicBlocks()) {
        foreach (QV4::IR::Stmt *s, bb->statements()) {
            s->accept(this);
            if (!_canSimplify)
                return false;
        }
        if (!_canSimplify)
            return false;
    }

    if (_returnValueOfBindingExpression == -1)
        return false;

    if (_canSimplify) {
        if (_nameOfFunctionCalled) {
            if (_functionCallReturnValue != _returnValueOfBindingExpression)
                return false;
            return detectTranslationCallAndConvertBinding(binding);
        }
    }

    return false;
}

bool QQmlJavaScriptBindingExpressionSimplificationPass::detectTranslationCallAndConvertBinding(QmlIR::Binding *binding)
{
    if (*_nameOfFunctionCalled == QStringLiteral("qsTr")) {
        QString translation;
        QV4::CompiledData::TranslationData translationData;
        translationData.number = -1;
        translationData.commentIndex = 0; // empty string

        QVector<int>::ConstIterator param = _functionParameters.constBegin();
        QVector<int>::ConstIterator end = _functionParameters.constEnd();
        if (param == end)
            return false;

        QV4::IR::String *stringParam = _temps[*param]->asString();
        if (!stringParam)
            return false;

        translation = *stringParam->value;

        ++param;
        if (param != end) {
            stringParam = _temps[*param]->asString();
            if (!stringParam)
                return false;
            translationData.commentIndex = compiler->registerString(*stringParam->value);
            ++param;

            if (param != end) {
                QV4::IR::Const *constParam = _temps[*param]->asConst();
                if (!constParam || constParam->type != QV4::IR::SInt32Type)
                    return false;

                translationData.number = int(constParam->value);
                ++param;
            }
        }

        if (param != end)
            return false;

        binding->type = QV4::CompiledData::Binding::Type_Translation;
        binding->stringIndex = compiler->registerString(translation);
        binding->value.translationData = translationData;
        return true;
    } else if (*_nameOfFunctionCalled == QStringLiteral("qsTrId")) {
        QString id;
        QV4::CompiledData::TranslationData translationData;
        translationData.number = -1;
        translationData.commentIndex = 0; // empty string, but unused

        QVector<int>::ConstIterator param = _functionParameters.constBegin();
        QVector<int>::ConstIterator end = _functionParameters.constEnd();
        if (param == end)
            return false;

        QV4::IR::String *stringParam = _temps[*param]->asString();
        if (!stringParam)
            return false;

        id = *stringParam->value;

        ++param;
        if (param != end) {
            QV4::IR::Const *constParam = _temps[*param]->asConst();
            if (!constParam || constParam->type != QV4::IR::SInt32Type)
                return false;

            translationData.number = int(constParam->value);
            ++param;
        }

        if (param != end)
            return false;

        binding->type = QV4::CompiledData::Binding::Type_TranslationById;
        binding->stringIndex = compiler->registerString(id);
        binding->value.translationData = translationData;
        return true;
    } else if (*_nameOfFunctionCalled == QStringLiteral("QT_TR_NOOP") || *_nameOfFunctionCalled == QStringLiteral("QT_TRID_NOOP")) {
        QVector<int>::ConstIterator param = _functionParameters.constBegin();
        QVector<int>::ConstIterator end = _functionParameters.constEnd();
        if (param == end)
            return false;

        QV4::IR::String *stringParam = _temps[*param]->asString();
        if (!stringParam)
            return false;

        ++param;
        if (param != end)
            return false;

        binding->type = QV4::CompiledData::Binding::Type_String;
        binding->stringIndex = compiler->registerString(*stringParam->value);
        return true;
    } else if (*_nameOfFunctionCalled == QStringLiteral("QT_TRANSLATE_NOOP")) {
        QVector<int>::ConstIterator param = _functionParameters.constBegin();
        QVector<int>::ConstIterator end = _functionParameters.constEnd();
        if (param == end)
            return false;

        ++param;
        if (param == end)
            return false;

        QV4::IR::String *stringParam = _temps[*param]->asString();
        if (!stringParam)
            return false;

        ++param;
        if (param != end)
            return false;

        binding->type = QV4::CompiledData::Binding::Type_String;
        binding->stringIndex = compiler->registerString(*stringParam->value);
        return true;
    }
    return false;
}

QQmlIRFunctionCleanser::QQmlIRFunctionCleanser(QQmlTypeCompiler *typeCompiler, const QVector<int> &functionsToRemove)
    : QQmlCompilePass(typeCompiler)
    , module(typeCompiler->jsIRModule())
    , functionsToRemove(functionsToRemove)
{
}

void QQmlIRFunctionCleanser::clean()
{
    QVector<QV4::IR::Function*> newFunctions;
    newFunctions.reserve(module->functions.count() - functionsToRemove.count());

    newFunctionIndices.resize(module->functions.count());

    for (int i = 0; i < module->functions.count(); ++i) {
        QV4::IR::Function *f = module->functions.at(i);
        Q_ASSERT(f || functionsToRemove.contains(i));
        if (f) {
            newFunctionIndices[i] = newFunctions.count();
            newFunctions << f;
        }
    }

    module->functions = newFunctions;

    foreach (QV4::IR::Function *function, module->functions) {
        foreach (QV4::IR::BasicBlock *block, function->basicBlocks()) {
            foreach (QV4::IR::Stmt *s, block->statements()) {
                s->accept(this);
            }
        }
    }

    foreach (QmlIR::Object *obj, *compiler->qmlObjects()) {
        if (!obj->runtimeFunctionIndices)
            continue;
        for (int i = 0; i < obj->runtimeFunctionIndices->count; ++i)
            (*obj->runtimeFunctionIndices)[i] = newFunctionIndices[obj->runtimeFunctionIndices->at(i)];
    }
}

void QQmlIRFunctionCleanser::visitClosure(QV4::IR::Closure *closure)
{
    closure->value = newFunctionIndices.at(closure->value);
}

QT_END_NAMESPACE
