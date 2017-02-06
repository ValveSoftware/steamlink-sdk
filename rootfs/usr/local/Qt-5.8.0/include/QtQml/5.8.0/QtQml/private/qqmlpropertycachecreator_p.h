/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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
#ifndef QQMLPROPERTYCACHECREATOR_P_H
#define QQMLPROPERTYCACHECREATOR_P_H

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

#include "qqmltypecompiler_p.h"
#include <private/qqmlvaluetype_p.h>
#include <private/qqmlengine_p.h>

QT_BEGIN_NAMESPACE

struct QQmlBindingInstantiationContext {
    QQmlBindingInstantiationContext();
    QQmlBindingInstantiationContext(int referencingObjectIndex, const QV4::CompiledData::Binding *instantiatingBinding, const QString &instantiatingPropertyName, const QQmlPropertyCache *referencingObjectPropertyCache);
    int referencingObjectIndex;
    const QV4::CompiledData::Binding *instantiatingBinding;
    QQmlPropertyData *instantiatingProperty;
};

struct QQmlPropertyCacheCreatorBase
{
    Q_DECLARE_TR_FUNCTIONS(QQmlPropertyCacheCreatorBase)
public:
    static QAtomicInt classIndexCounter;
};

template <typename ObjectContainer>
class QQmlPropertyCacheCreator : public QQmlPropertyCacheCreatorBase
{
public:
    typedef typename ObjectContainer::CompiledObject CompiledObject;

    QQmlPropertyCacheCreator(QQmlPropertyCacheVector *propertyCaches, QQmlEnginePrivate *enginePrivate, const ObjectContainer *objectContainer, const QQmlImports *imports);

    QQmlCompileError buildMetaObjects();

protected:
    QQmlCompileError buildMetaObjectRecursively(int objectIndex, const QQmlBindingInstantiationContext &context);
    QQmlPropertyCache *propertyCacheForObject(const CompiledObject *obj, const QQmlBindingInstantiationContext &context, QQmlCompileError *error) const;
    QQmlCompileError createMetaObject(int objectIndex, const CompiledObject *obj, QQmlPropertyCache *baseTypeCache);

    QString stringAt(int index) const { return objectContainer->stringAt(index); }

    QQmlEnginePrivate * const enginePrivate;
    const ObjectContainer * const objectContainer;
    const QQmlImports * const imports;
    QQmlPropertyCacheVector *propertyCaches;
};

template <typename ObjectContainer>
inline QQmlPropertyCacheCreator<ObjectContainer>::QQmlPropertyCacheCreator(QQmlPropertyCacheVector *propertyCaches, QQmlEnginePrivate *enginePrivate, const ObjectContainer *objectContainer, const QQmlImports *imports)
    : enginePrivate(enginePrivate)
    , objectContainer(objectContainer)
    , imports(imports)
    , propertyCaches(propertyCaches)
{
    propertyCaches->resize(objectContainer->objectCount());
}

template <typename ObjectContainer>
inline QQmlCompileError QQmlPropertyCacheCreator<ObjectContainer>::buildMetaObjects()
{
    QQmlBindingInstantiationContext context;
    return buildMetaObjectRecursively(objectContainer->rootObjectIndex(), context);
}

template <typename ObjectContainer>
inline QQmlCompileError QQmlPropertyCacheCreator<ObjectContainer>::buildMetaObjectRecursively(int objectIndex, const QQmlBindingInstantiationContext &context)
{
    const CompiledObject *obj = objectContainer->objectAt(objectIndex);

    bool needVMEMetaObject = obj->propertyCount() != 0 || obj->aliasCount() != 0 || obj->signalCount() != 0 || obj->functionCount() != 0;
    if (!needVMEMetaObject) {
        auto binding = obj->bindingsBegin();
        auto end = obj->bindingsEnd();
        for ( ; binding != end; ++binding) {
            if (binding->type == QV4::CompiledData::Binding::Type_Object && (binding->flags & QV4::CompiledData::Binding::IsOnAssignment)) {
                // If the on assignment is inside a group property, we need to distinguish between QObject based
                // group properties and value type group properties. For the former the base type is derived from
                // the property that references us, for the latter we only need a meta-object on the referencing object
                // because interceptors can't go to the shared value type instances.
                if (context.instantiatingProperty && QQmlValueTypeFactory::isValueType(context.instantiatingProperty->propType())) {
                    if (!propertyCaches->needsVMEMetaObject(context.referencingObjectIndex)) {
                        const CompiledObject *obj = objectContainer->objectAt(context.referencingObjectIndex);
                        auto *typeRef = objectContainer->resolvedTypes.value(obj->inheritedTypeNameIndex);
                        Q_ASSERT(typeRef);
                        QQmlPropertyCache *baseTypeCache = typeRef->createPropertyCache(QQmlEnginePrivate::get(enginePrivate));
                        QQmlCompileError error = createMetaObject(context.referencingObjectIndex, obj, baseTypeCache);
                        if (error.isSet())
                            return error;
                    }
                } else {
                    // On assignments are implemented using value interceptors, which require a VME meta object.
                    needVMEMetaObject = true;
                }
                break;
            }
        }
    }

    QQmlPropertyCache *baseTypeCache;
    {
        QQmlCompileError error;
        baseTypeCache = propertyCacheForObject(obj, context, &error);
        if (error.isSet())
            return error;
    }

    if (baseTypeCache) {
        if (needVMEMetaObject) {
            QQmlCompileError error = createMetaObject(objectIndex, obj, baseTypeCache);
            if (error.isSet())
                return error;
        } else {
            propertyCaches->set(objectIndex, baseTypeCache);
        }
    }

    if (QQmlPropertyCache *thisCache = propertyCaches->at(objectIndex)) {
        auto binding = obj->bindingsBegin();
        auto end = obj->bindingsEnd();
        for ( ; binding != end; ++binding)
            if (binding->type >= QV4::CompiledData::Binding::Type_Object) {
                QQmlBindingInstantiationContext context(objectIndex, &(*binding), stringAt(binding->propertyNameIndex), thisCache);
                QQmlCompileError error = buildMetaObjectRecursively(binding->value.objectIndex, context);
                if (error.isSet())
                    return error;
            }
    }

    QQmlCompileError noError;
    return noError;
}

template <typename ObjectContainer>
inline QQmlPropertyCache *QQmlPropertyCacheCreator<ObjectContainer>::propertyCacheForObject(const CompiledObject *obj, const QQmlBindingInstantiationContext &context, QQmlCompileError *error) const
{
    if (context.instantiatingProperty) {
        if (context.instantiatingProperty->isQObject()) {
            return enginePrivate->rawPropertyCacheForType(context.instantiatingProperty->propType());
        } else if (const QMetaObject *vtmo = QQmlValueTypeFactory::metaObjectForMetaType(context.instantiatingProperty->propType())) {
            return enginePrivate->cache(vtmo);
        }
    } else if (obj->inheritedTypeNameIndex != 0) {
        auto *typeRef = objectContainer->resolvedTypes.value(obj->inheritedTypeNameIndex);
        Q_ASSERT(typeRef);

        if (typeRef->isFullyDynamicType) {
            if (obj->propertyCount() > 0 || obj->aliasCount() > 0) {
                *error = QQmlCompileError(obj->location, QQmlPropertyCacheCreatorBase::tr("Fully dynamic types cannot declare new properties."));
                return nullptr;
            }
            if (obj->signalCount() > 0) {
                *error = QQmlCompileError(obj->location, QQmlPropertyCacheCreatorBase::tr("Fully dynamic types cannot declare new signals."));
                return nullptr;
            }
            if (obj->functionCount() > 0) {
                *error = QQmlCompileError(obj->location, QQmlPropertyCacheCreatorBase::tr("Fully Dynamic types cannot declare new functions."));
                return nullptr;
            }
        }

        return typeRef->createPropertyCache(QQmlEnginePrivate::get(enginePrivate));
    } else if (context.instantiatingBinding && context.instantiatingBinding->isAttachedProperty()) {
        auto *typeRef = objectContainer->resolvedTypes.value(context.instantiatingBinding->propertyNameIndex);
        Q_ASSERT(typeRef);
        QQmlType *qmltype = typeRef->type;
        if (!qmltype) {
            QString propertyName = stringAt(context.instantiatingBinding->propertyNameIndex);
            if (imports->resolveType(propertyName, &qmltype, 0, 0, 0)) {
                if (qmltype->isComposite()) {
                    QQmlTypeData *tdata = enginePrivate->typeLoader.getType(qmltype->sourceUrl());
                    Q_ASSERT(tdata);
                    Q_ASSERT(tdata->isComplete());

                    auto compilationUnit = tdata->compilationUnit();
                    qmltype = QQmlMetaType::qmlType(compilationUnit->metaTypeId);

                    tdata->release();
                }
            }
        }

        const QMetaObject *attachedMo = qmltype ? qmltype->attachedPropertiesType(enginePrivate) : 0;
        if (!attachedMo) {
            *error = QQmlCompileError(context.instantiatingBinding->location, QQmlPropertyCacheCreatorBase::tr("Non-existent attached object"));
            return nullptr;
        }
        return enginePrivate->cache(attachedMo);
    }
    return nullptr;
}

template <typename ObjectContainer>
inline QQmlCompileError QQmlPropertyCacheCreator<ObjectContainer>::createMetaObject(int objectIndex, const CompiledObject *obj, QQmlPropertyCache *baseTypeCache)
{
    QQmlRefPointer<QQmlPropertyCache> cache;
    cache.adopt(baseTypeCache->copyAndReserve(obj->propertyCount() + obj->aliasCount(),
                                              obj->functionCount() + obj->propertyCount() + obj->aliasCount() + obj->signalCount(),
                                              obj->signalCount() + obj->propertyCount() + obj->aliasCount()));

    propertyCaches->set(objectIndex, cache);
    propertyCaches->setNeedsVMEMetaObject(objectIndex);

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

    if (objectIndex == objectContainer->rootObjectIndex()) {
        const QString path = objectContainer->url().path();
        int lastSlash = path.lastIndexOf(QLatin1Char('/'));
        if (lastSlash > -1) {
            const QStringRef nameBase = path.midRef(lastSlash + 1, path.length() - lastSlash - 5);
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

    int varPropCount = 0;

    QmlIR::PropertyResolver resolver(baseTypeCache);

    auto p = obj->propertiesBegin();
    auto pend = obj->propertiesEnd();
    for ( ; p != pend; ++p) {
        if (p->type == QV4::CompiledData::Property::Var)
            varPropCount++;

        bool notInRevision = false;
        QQmlPropertyData *d = resolver.property(stringAt(p->nameIndex), &notInRevision);
        if (d && d->isFinal())
            return QQmlCompileError(p->location, QQmlPropertyCacheCreatorBase::tr("Cannot override FINAL property"));
    }

    auto a = obj->aliasesBegin();
    auto aend = obj->aliasesEnd();
    for ( ; a != aend; ++a) {
        bool notInRevision = false;
        QQmlPropertyData *d = resolver.property(stringAt(a->nameIndex), &notInRevision);
        if (d && d->isFinal())
            return QQmlCompileError(a->location, QQmlPropertyCacheCreatorBase::tr("Cannot override FINAL property"));
    }

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

    // Set up notify signals for properties - first normal, then alias
    p = obj->propertiesBegin();
    pend = obj->propertiesEnd();
    for (  ; p != pend; ++p) {
        auto flags = QQmlPropertyData::defaultSignalFlags();

        QString changedSigName = stringAt(p->nameIndex) + QLatin1String("Changed");
        seenSignals.insert(changedSigName);

        cache->appendSignal(changedSigName, flags, effectiveMethodIndex++);
    }

    a = obj->aliasesBegin();
    aend = obj->aliasesEnd();
    for ( ; a != aend; ++a) {
        auto flags = QQmlPropertyData::defaultSignalFlags();

        QString changedSigName = stringAt(a->nameIndex) + QLatin1String("Changed");
        seenSignals.insert(changedSigName);

        cache->appendSignal(changedSigName, flags, effectiveMethodIndex++);
    }

    // Dynamic signals
    auto s = obj->signalsBegin();
    auto send = obj->signalsEnd();
    for ( ; s != send; ++s) {
        const int paramCount = s->parameterCount();

        QList<QByteArray> names;
        names.reserve(paramCount);
        QVarLengthArray<int, 10> paramTypes(paramCount?(paramCount + 1):0);

        if (paramCount) {
            paramTypes[0] = paramCount;

            int i = 0;
            auto param = s->parametersBegin();
            auto end = s->parametersEnd();
            for ( ; param != end; ++param, ++i) {
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
                        return QQmlCompileError(s->location, QQmlPropertyCacheCreatorBase::tr("Invalid signal parameter type: %1").arg(customTypeName));

                    if (qmltype->isComposite()) {
                        QQmlTypeData *tdata = enginePrivate->typeLoader.getType(qmltype->sourceUrl());
                        Q_ASSERT(tdata);
                        Q_ASSERT(tdata->isComplete());

                        auto compilationUnit = tdata->compilationUnit();

                        paramTypes[i + 1] = compilationUnit->metaTypeId;

                        tdata->release();
                    } else {
                        paramTypes[i + 1] = qmltype->typeId();
                    }
                }
            }
        }

        auto flags = QQmlPropertyData::defaultSignalFlags();
        if (paramCount)
            flags.hasArguments = true;

        QString signalName = stringAt(s->nameIndex);
        if (seenSignals.contains(signalName))
            return QQmlCompileError(s->location, QQmlPropertyCacheCreatorBase::tr("Duplicate signal name: invalid override of property change signal or superclass signal"));
        seenSignals.insert(signalName);

        cache->appendSignal(signalName, flags, effectiveMethodIndex++,
                            paramCount?paramTypes.constData():0, names);
    }


    // Dynamic slots
    auto function = objectContainer->objectFunctionsBegin(obj);
    auto fend = objectContainer->objectFunctionsEnd(obj);
    for ( ; function != fend; ++function) {
        auto flags = QQmlPropertyData::defaultSlotFlags();

        const QString slotName = stringAt(function->nameIndex);
        if (seenSignals.contains(slotName))
            return QQmlCompileError(function->location, QQmlPropertyCacheCreatorBase::tr("Duplicate method name: invalid override of property change signal or superclass signal"));
        // Note: we don't append slotName to the seenSignals list, since we don't
        // protect against overriding change signals or methods with properties.

        QList<QByteArray> parameterNames;
        auto formal = function->formalsBegin();
        auto end = function->formalsEnd();
        for ( ; formal != end; ++formal) {
            flags.hasArguments = true;
            parameterNames << stringAt(*formal).toUtf8();
        }

        cache->appendMethod(slotName, flags, effectiveMethodIndex++, parameterNames);
    }


    // Dynamic properties
    int effectiveSignalIndex = cache->signalHandlerIndexCacheStart;
    int propertyIdx = 0;
    p = obj->propertiesBegin();
    pend = obj->propertiesEnd();
    for ( ; p != pend; ++p, ++propertyIdx) {
        int propertyType = 0;
        QQmlPropertyData::Flags propertyFlags;

        if (p->type == QV4::CompiledData::Property::Var) {
            propertyType = QMetaType::QVariant;
            propertyFlags.type = QQmlPropertyData::Flags::VarPropertyType;
        } else if (p->type < builtinTypeCount) {
            propertyType = builtinTypes[p->type].metaType;

            if (p->type == QV4::CompiledData::Property::Variant)
                propertyFlags.type = QQmlPropertyData::Flags::QVariantType;
        } else {
            Q_ASSERT(p->type == QV4::CompiledData::Property::CustomList ||
                     p->type == QV4::CompiledData::Property::Custom);

            QQmlType *qmltype = 0;
            if (!imports->resolveType(stringAt(p->customTypeNameIndex), &qmltype, 0, 0, 0)) {
                return QQmlCompileError(p->location, QQmlPropertyCacheCreatorBase::tr("Invalid property type"));
            }

            Q_ASSERT(qmltype);
            if (qmltype->isComposite()) {
                QQmlTypeData *tdata = enginePrivate->typeLoader.getType(qmltype->sourceUrl());
                Q_ASSERT(tdata);
                Q_ASSERT(tdata->isComplete());

                auto compilationUnit = tdata->compilationUnit();

                if (p->type == QV4::CompiledData::Property::Custom) {
                    propertyType = compilationUnit->metaTypeId;
                } else {
                    propertyType = compilationUnit->listMetaTypeId;
                }

                tdata->release();
            } else {
                if (p->type == QV4::CompiledData::Property::Custom) {
                    propertyType = qmltype->typeId();
                } else {
                    propertyType = qmltype->qListTypeId();
                }
            }

            if (p->type == QV4::CompiledData::Property::Custom)
                propertyFlags.type = QQmlPropertyData::Flags::QObjectDerivedType;
            else
                propertyFlags.type = QQmlPropertyData::Flags::QListType;
        }

        if (!(p->flags & QV4::CompiledData::Property::IsReadOnly) && p->type != QV4::CompiledData::Property::CustomList)
            propertyFlags.isWritable = true;


        QString propertyName = stringAt(p->nameIndex);
        if (!obj->defaultPropertyIsAlias && propertyIdx == obj->indexOfDefaultPropertyOrAlias)
            cache->_defaultPropertyName = propertyName;
        cache->appendProperty(propertyName, propertyFlags, effectivePropertyIndex++,
                              propertyType, effectiveSignalIndex);

        effectiveSignalIndex++;
    }

    QQmlCompileError noError;
    return noError;
}

template <typename ObjectContainer>
class QQmlPropertyCacheAliasCreator
{
public:
    typedef typename ObjectContainer::CompiledObject CompiledObject;

    QQmlPropertyCacheAliasCreator(QQmlPropertyCacheVector *propertyCaches, const ObjectContainer *objectContainer);

    void appendAliasPropertiesToMetaObjects();

    void appendAliasesToPropertyCache(const CompiledObject &component, int objectIndex);

private:
    void appendAliasPropertiesInMetaObjectsWithinComponent(const CompiledObject &component, int firstObjectIndex);
    void propertyDataForAlias(const CompiledObject &component, const QV4::CompiledData::Alias &alias, int *type, QQmlPropertyRawData::Flags *propertyFlags);

    void collectObjectsWithAliasesRecursively(int objectIndex, QVector<int> *objectsWithAliases) const;

    int objectForId(const CompiledObject &component, int id) const;

    QQmlPropertyCacheVector *propertyCaches;
    const ObjectContainer *objectContainer;
};

template <typename ObjectContainer>
inline QQmlPropertyCacheAliasCreator<ObjectContainer>::QQmlPropertyCacheAliasCreator(QQmlPropertyCacheVector *propertyCaches, const ObjectContainer *objectContainer)
    : propertyCaches(propertyCaches)
    , objectContainer(objectContainer)
{

}

template <typename ObjectContainer>
inline void QQmlPropertyCacheAliasCreator<ObjectContainer>::appendAliasPropertiesToMetaObjects()
{
    for (int i = 0; i < objectContainer->objectCount(); ++i) {
        const CompiledObject &component = *objectContainer->objectAt(i);
        if (!(component.flags & QV4::CompiledData::Object::IsComponent))
            continue;

        const auto rootBinding = component.bindingsBegin();
        appendAliasPropertiesInMetaObjectsWithinComponent(component, rootBinding->value.objectIndex);
    }

    const int rootObjectIndex = objectContainer->rootObjectIndex();
    appendAliasPropertiesInMetaObjectsWithinComponent(*objectContainer->objectAt(rootObjectIndex), rootObjectIndex);
}

template <typename ObjectContainer>
inline void QQmlPropertyCacheAliasCreator<ObjectContainer>::appendAliasPropertiesInMetaObjectsWithinComponent(const CompiledObject &component, int firstObjectIndex)
{
    QVector<int> objectsWithAliases;
    collectObjectsWithAliasesRecursively(firstObjectIndex, &objectsWithAliases);
    if (objectsWithAliases.isEmpty())
        return;

    const auto allAliasTargetsExist = [this, &component](const CompiledObject &object) {
        auto alias = object.aliasesBegin();
        auto end = object.aliasesEnd();
        for ( ; alias != end; ++alias) {
            Q_ASSERT(alias->flags & QV4::CompiledData::Alias::Resolved);

            const int targetObjectIndex = objectForId(component, alias->targetObjectId);
            Q_ASSERT(targetObjectIndex >= 0);

            if (alias->aliasToLocalAlias)
                continue;

            if (alias->encodedMetaPropertyIndex == -1)
                continue;

            const QQmlPropertyCache *targetCache = propertyCaches->at(targetObjectIndex);
            Q_ASSERT(targetCache);

            int coreIndex = QQmlPropertyIndex::fromEncoded(alias->encodedMetaPropertyIndex).coreIndex();
            QQmlPropertyData *targetProperty = targetCache->property(coreIndex);
            if (!targetProperty)
                return false;
       }
       return true;
    };

    do {
        QVector<int> pendingObjects;

        for (int objectIndex: qAsConst(objectsWithAliases)) {
            const CompiledObject &object = *objectContainer->objectAt(objectIndex);

            if (allAliasTargetsExist(object)) {
                appendAliasesToPropertyCache(component, objectIndex);
            } else {
                pendingObjects.append(objectIndex);
            }

        }
        qSwap(objectsWithAliases, pendingObjects);
    } while (!objectsWithAliases.isEmpty());
}

template <typename ObjectContainer>
inline void QQmlPropertyCacheAliasCreator<ObjectContainer>::collectObjectsWithAliasesRecursively(int objectIndex, QVector<int> *objectsWithAliases) const
{
    const CompiledObject &object = *objectContainer->objectAt(objectIndex);
    if (object.aliasCount() > 0)
        objectsWithAliases->append(objectIndex);

    // Stop at Component boundary
    if (object.flags & QV4::CompiledData::Object::IsComponent && objectIndex != objectContainer->rootObjectIndex())
        return;

    auto binding = object.bindingsBegin();
    auto end = object.bindingsEnd();
    for (; binding != end; ++binding) {
        if (binding->type != QV4::CompiledData::Binding::Type_Object
            && binding->type != QV4::CompiledData::Binding::Type_AttachedProperty
            && binding->type != QV4::CompiledData::Binding::Type_GroupProperty)
            continue;

        collectObjectsWithAliasesRecursively(binding->value.objectIndex, objectsWithAliases);
    }
}

template <typename ObjectContainer>
inline void QQmlPropertyCacheAliasCreator<ObjectContainer>::propertyDataForAlias(
        const CompiledObject &component, const QV4::CompiledData::Alias &alias, int *type,
        QQmlPropertyData::Flags *propertyFlags)
{
    const int targetObjectIndex = objectForId(component, alias.targetObjectId);
    Q_ASSERT(targetObjectIndex >= 0);

    const CompiledObject &targetObject = *objectContainer->objectAt(targetObjectIndex);

    *type = 0;
    bool writable = false;
    bool resettable = false;

    propertyFlags->isAlias = true;

    if (alias.aliasToLocalAlias) {
        auto targetAlias = targetObject.aliasesBegin();
        for (uint i = 0; i < alias.localAliasIndex; ++i)
            ++targetAlias;
        propertyDataForAlias(component, *targetAlias, type, propertyFlags);
        return;
    } else if (alias.encodedMetaPropertyIndex == -1) {
        Q_ASSERT(alias.flags & QV4::CompiledData::Alias::AliasPointsToPointerObject);
        auto *typeRef = objectContainer->resolvedTypes.value(targetObject.inheritedTypeNameIndex);
        Q_ASSERT(typeRef);

        if (typeRef->type)
            *type = typeRef->type->typeId();
        else
            *type = typeRef->compilationUnit->metaTypeId;

        propertyFlags->type = QQmlPropertyData::Flags::QObjectDerivedType;
    } else {
        int coreIndex = QQmlPropertyIndex::fromEncoded(alias.encodedMetaPropertyIndex).coreIndex();
        int valueTypeIndex = QQmlPropertyIndex::fromEncoded(alias.encodedMetaPropertyIndex).valueTypeIndex();

        QQmlPropertyCache *targetCache = propertyCaches->at(targetObjectIndex);
        Q_ASSERT(targetCache);
        QQmlPropertyData *targetProperty = targetCache->property(coreIndex);
        Q_ASSERT(targetProperty);

        *type = targetProperty->propType();

        writable = targetProperty->isWritable();
        resettable = targetProperty->isResettable();

        if (valueTypeIndex != -1) {
            const QMetaObject *valueTypeMetaObject = QQmlValueTypeFactory::metaObjectForMetaType(*type);
            if (valueTypeMetaObject->property(valueTypeIndex).isEnumType())
                *type = QVariant::Int;
            else
                *type = valueTypeMetaObject->property(valueTypeIndex).userType();
        } else {
            if (targetProperty->isEnum()) {
                *type = QVariant::Int;
            } else {
                // Copy type flags
                propertyFlags->copyPropertyTypeFlags(targetProperty->flags());

                if (targetProperty->isVarProperty())
                    propertyFlags->type = QQmlPropertyData::Flags::QVariantType;
            }
        }
    }

    propertyFlags->isWritable = !(alias.flags & QV4::CompiledData::Property::IsReadOnly) && writable;
    propertyFlags->isResettable = resettable;
}

template <typename ObjectContainer>
inline void QQmlPropertyCacheAliasCreator<ObjectContainer>::appendAliasesToPropertyCache(
        const CompiledObject &component, int objectIndex)
{
    const CompiledObject &object = *objectContainer->objectAt(objectIndex);
    if (!object.aliasCount())
        return;

    QQmlPropertyCache *propertyCache = propertyCaches->at(objectIndex);
    Q_ASSERT(propertyCache);

    int effectiveSignalIndex = propertyCache->signalHandlerIndexCacheStart + propertyCache->propertyIndexCache.count();
    int effectivePropertyIndex = propertyCache->propertyIndexCacheStart + propertyCache->propertyIndexCache.count();

    int aliasIndex = 0;
    auto alias = object.aliasesBegin();
    auto end = object.aliasesEnd();
    for ( ; alias != end; ++alias, ++aliasIndex) {
        Q_ASSERT(alias->flags & QV4::CompiledData::Alias::Resolved);

        int type = 0;
        QQmlPropertyData::Flags propertyFlags;
        propertyDataForAlias(component, *alias, &type, &propertyFlags);

        const QString propertyName = objectContainer->stringAt(alias->nameIndex);

        if (object.defaultPropertyIsAlias && aliasIndex == object.indexOfDefaultPropertyOrAlias)
            propertyCache->_defaultPropertyName = propertyName;

        propertyCache->appendProperty(propertyName, propertyFlags, effectivePropertyIndex++,
                                      type, effectiveSignalIndex++);
    }
}

template <typename ObjectContainer>
inline int QQmlPropertyCacheAliasCreator<ObjectContainer>::objectForId(const CompiledObject &component, int id) const
{
    for (quint32 i = 0, count = component.namedObjectsInComponentCount(); i < count; ++i) {
        const int candidateIndex = component.namedObjectsInComponentTable()[i];
        const CompiledObject &candidate = *objectContainer->objectAt(candidateIndex);
        if (candidate.id == id)
            return candidateIndex;
    }
    return -1;
}

QT_END_NAMESPACE

#endif // QQMLPROPERTYCACHECREATOR_P_H
