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

#include "qqmlpropertyvalidator_p.h"

#include <private/qqmlcustomparser_p.h>
#include <private/qqmlstringconverters_p.h>
#include <QtCore/qdatetime.h>

QT_BEGIN_NAMESPACE

QQmlPropertyValidator::QQmlPropertyValidator(QQmlEnginePrivate *enginePrivate, const QQmlImports &imports, QV4::CompiledData::CompilationUnit *compilationUnit)
    : enginePrivate(enginePrivate)
    , imports(imports)
    , qmlUnit(compilationUnit->data)
    , resolvedTypes(compilationUnit->resolvedTypes)
    , propertyCaches(compilationUnit->propertyCaches)
    , bindingPropertyDataPerObject(&compilationUnit->bindingPropertyDataPerObject)
{
    bindingPropertyDataPerObject->resize(qmlUnit->nObjects);
}

QVector<QQmlCompileError> QQmlPropertyValidator::validate()
{
    return validateObject(qmlUnit->indexOfRootObject, /*instantiatingBinding*/0);
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

QVector<QQmlCompileError> QQmlPropertyValidator::validateObject(int objectIndex, const QV4::CompiledData::Binding *instantiatingBinding, bool populatingValueTypeGroupProperty) const
{
    const QV4::CompiledData::Object *obj = qmlUnit->objectAt(objectIndex);

    if (obj->flags & QV4::CompiledData::Object::IsComponent) {
        Q_ASSERT(obj->nBindings == 1);
        const QV4::CompiledData::Binding *componentBinding = obj->bindingTable();
        Q_ASSERT(componentBinding->type == QV4::CompiledData::Binding::Type_Object);
        return validateObject(componentBinding->value.objectIndex, componentBinding);
    }

    QQmlPropertyCache *propertyCache = propertyCaches.at(objectIndex);
    if (!propertyCache)
        return QVector<QQmlCompileError>();

    QStringList deferredPropertyNames;
    {
        const QMetaObject *mo = propertyCache->firstCppMetaObject();
        const int namesIndex = mo->indexOfClassInfo("DeferredPropertyNames");
        if (namesIndex != -1) {
            QMetaClassInfo classInfo = mo->classInfo(namesIndex);
            deferredPropertyNames = QString::fromUtf8(classInfo.value()).split(QLatin1Char(','));
        }
    }

    QQmlCustomParser *customParser = 0;
    if (auto typeRef = resolvedTypes.value(obj->inheritedTypeNameIndex)) {
        if (typeRef->type)
            customParser = typeRef->type->customParser();
    }

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
            return recordError(binding->location, tr("Property assignment expected"));
        }

        GroupPropertyVector::const_iterator pos = std::lower_bound(groupProperties.constBegin(), groupProperties.constEnd(), binding->propertyNameIndex, BindingFinder());
        groupProperties.insert(pos, binding);
    }

    QmlIR::PropertyResolver propertyResolver(propertyCache);

    QString defaultPropertyName;
    QQmlPropertyData *defaultProperty = 0;
    if (obj->indexOfDefaultPropertyOrAlias != -1) {
        QQmlPropertyCache *cache = propertyCache->parent();
        defaultPropertyName = cache->defaultPropertyName();
        defaultProperty = cache->defaultProperty();
    } else {
        defaultPropertyName = propertyCache->defaultPropertyName();
        defaultProperty = propertyCache->defaultProperty();
    }

    QV4::CompiledData::BindingPropertyData collectedBindingPropertyData(obj->nBindings);

    binding = obj->bindingTable();
    for (quint32 i = 0; i < obj->nBindings; ++i, ++binding) {
        QString name = stringAt(binding->propertyNameIndex);

        if (customParser) {
            if (binding->type == QV4::CompiledData::Binding::Type_AttachedProperty) {
                if (customParser->flags() & QQmlCustomParser::AcceptsAttachedProperties) {
                    customBindings << binding;
                    continue;
                }
            } else if (QmlIR::IRBuilder::isSignalPropertyName(name)
                       && !(customParser->flags() & QQmlCustomParser::AcceptsSignalHandlers)) {
                customBindings << binding;
                continue;
            }
        }

        bool bindingToDefaultProperty = false;
        bool isGroupProperty = instantiatingBinding && instantiatingBinding->type == QV4::CompiledData::Binding::Type_GroupProperty;

        bool notInRevision = false;
        QQmlPropertyData *pd = 0;
        if (!name.isEmpty()) {
            if (binding->flags & QV4::CompiledData::Binding::IsSignalHandlerExpression
                || binding->flags & QV4::CompiledData::Binding::IsSignalHandlerObject)
                pd = propertyResolver.signal(name, &notInRevision);
            else
                pd = propertyResolver.property(name, &notInRevision, isGroupProperty ? QmlIR::PropertyResolver::IgnoreRevision : QmlIR::PropertyResolver::CheckRevision);

            if (notInRevision) {
                QString typeName = stringAt(obj->inheritedTypeNameIndex);
                auto *objectType = resolvedTypes.value(obj->inheritedTypeNameIndex);
                if (objectType && objectType->type) {
                    return recordError(binding->location, tr("\"%1.%2\" is not available in %3 %4.%5.").arg(typeName).arg(name).arg(objectType->type->module()).arg(objectType->majorVersion).arg(objectType->minorVersion));
                } else {
                    return recordError(binding->location, tr("\"%1.%2\" is not available due to component versioning.").arg(typeName).arg(name));
                }
            }
        } else {
           if (isGroupProperty)
               return recordError(binding->location, tr("Cannot assign a value directly to a grouped property"));

           pd = defaultProperty;
           name = defaultPropertyName;
           bindingToDefaultProperty = true;
        }

        if (pd)
            collectedBindingPropertyData[i] = pd;

        if (name.constData()->isUpper() && !binding->isAttachedProperty()) {
            QQmlType *type = 0;
            QQmlImportNamespace *typeNamespace = 0;
            imports.resolveType(stringAt(binding->propertyNameIndex), &type, 0, 0, &typeNamespace);
            if (typeNamespace)
                return recordError(binding->location, tr("Invalid use of namespace"));
            return recordError(binding->location, tr("Invalid attached object assignment"));
        }

        if (binding->type >= QV4::CompiledData::Binding::Type_Object && (pd || binding->isAttachedProperty())) {
            const QVector<QQmlCompileError> subObjectValidatorErrors = validateObject(binding->value.objectIndex, binding, pd && QQmlValueTypeFactory::metaObjectForMetaType(pd->propType()));
            if (!subObjectValidatorErrors.isEmpty())
                return subObjectValidatorErrors;
        }

        // Signal handlers were resolved and checked earlier in the signal handler conversion pass.
        if (binding->flags & QV4::CompiledData::Binding::IsSignalHandlerExpression
            || binding->flags & QV4::CompiledData::Binding::IsSignalHandlerObject)
            continue;

        if (binding->type == QV4::CompiledData::Binding::Type_AttachedProperty) {
            if (instantiatingBinding && (instantiatingBinding->isAttachedProperty() || instantiatingBinding->isGroupProperty())) {
                return recordError(binding->location, tr("Attached properties cannot be used here"));
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
                    return recordError(binding->valueLocation, tr("Cannot assign a value directly to a grouped property"));
                return recordError(binding->valueLocation, tr("Invalid property assignment: \"%1\" is a read-only property").arg(name));
            }

            if (!pd->isQList() && (binding->flags & QV4::CompiledData::Binding::IsListItem)) {
                QString error;
                if (pd->propType() == qMetaTypeId<QQmlScriptString>())
                    error = tr( "Cannot assign multiple values to a script property");
                else
                    error = tr( "Cannot assign multiple values to a singular property");
                return recordError(binding->valueLocation, error);
            }

            if (!bindingToDefaultProperty
                && !binding->isGroupProperty()
                && !(binding->flags & QV4::CompiledData::Binding::IsOnAssignment)
                && assigningToGroupProperty) {
                QV4::CompiledData::Location loc = binding->valueLocation;
                if (loc < (*assignedGroupProperty)->valueLocation)
                    loc = (*assignedGroupProperty)->valueLocation;

                if (pd && QQmlValueTypeFactory::isValueType(pd->propType()))
                    return recordError(loc, tr("Property has already been assigned a value"));
                return recordError(loc, tr("Cannot assign a value directly to a grouped property"));
            }

            if (binding->type < QV4::CompiledData::Binding::Type_Script) {
                QQmlCompileError bindingError = validateLiteralBinding(propertyCache, pd, binding);
                if (bindingError.isSet())
                    return recordError(bindingError);
            } else if (binding->type == QV4::CompiledData::Binding::Type_Object) {
                QQmlCompileError bindingError = validateObjectBinding(pd, name, binding);
                if (bindingError.isSet())
                    return recordError(bindingError);
            } else if (binding->isGroupProperty()) {
                if (QQmlValueTypeFactory::isValueType(pd->propType())) {
                    if (QQmlValueTypeFactory::metaObjectForMetaType(pd->propType())) {
                        if (!pd->isWritable()) {
                            return recordError(binding->location, tr("Invalid property assignment: \"%1\" is a read-only property").arg(name));
                        }
                    } else {
                        return recordError(binding->location, tr("Invalid grouped property access"));
                    }
                } else {
                    if (!enginePrivate->propertyCacheForType(pd->propType())) {
                        return recordError(binding->location, tr("Invalid grouped property access"));
                    }
                }
            }
        } else {
            if (customParser) {
                customBindings << binding;
                continue;
            }
            if (bindingToDefaultProperty) {
                return recordError(binding->location, tr("Cannot assign to non-existent default property"));
            } else {
                return recordError(binding->location, tr("Cannot assign to non-existent property \"%1\"").arg(name));
            }
        }
    }

    if (obj->idNameIndex) {
        bool notInRevision = false;
        collectedBindingPropertyData << propertyResolver.property(QStringLiteral("id"), &notInRevision);
    }

    if (customParser && !customBindings.isEmpty()) {
        customParser->clearErrors();
        customParser->validator = this;
        customParser->engine = enginePrivate;
        customParser->imports = &imports;
        customParser->verifyBindings(qmlUnit, customBindings);
        customParser->validator = 0;
        customParser->engine = 0;
        customParser->imports = (QQmlImports*)0;
        QVector<QQmlCompileError> parserErrors = customParser->errors();
        if (!parserErrors.isEmpty())
            return parserErrors;
    }

    (*bindingPropertyDataPerObject)[objectIndex] = collectedBindingPropertyData;

    QVector<QQmlCompileError> noError;
    return noError;
}

QQmlCompileError QQmlPropertyValidator::validateLiteralBinding(QQmlPropertyCache *propertyCache, QQmlPropertyData *property, const QV4::CompiledData::Binding *binding) const
{
    if (property->isQList()) {
        return QQmlCompileError(binding->valueLocation, tr("Cannot assign primitives to lists"));
    }

    QQmlCompileError noError;

    if (property->isEnum()) {
        if (binding->flags & QV4::CompiledData::Binding::IsResolvedEnum)
            return noError;

        QString value = binding->valueAsString(qmlUnit);
        QMetaProperty p = propertyCache->firstCppMetaObject()->property(property->coreIndex());
        bool ok;
        if (p.isFlagType()) {
            p.enumerator().keysToValue(value.toUtf8().constData(), &ok);
        } else
            p.enumerator().keyToValue(value.toUtf8().constData(), &ok);

        if (!ok) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: unknown enumeration"));
        }
        return noError;
    }

    switch (property->propType()) {
    case QMetaType::QVariant:
    break;
    case QVariant::String: {
        if (!binding->evaluatesToString()) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: string expected"));
        }
    }
    break;
    case QVariant::StringList: {
        if (!binding->evaluatesToString()) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: string or string list expected"));
        }
    }
    break;
    case QVariant::ByteArray: {
        if (binding->type != QV4::CompiledData::Binding::Type_String) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: byte array expected"));
        }
    }
    break;
    case QVariant::Url: {
        if (binding->type != QV4::CompiledData::Binding::Type_String) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: url expected"));
        }
    }
    break;
    case QVariant::UInt: {
        if (binding->type == QV4::CompiledData::Binding::Type_Number) {
            double d = binding->valueAsNumber();
            if (double(uint(d)) == d)
                return noError;
        }
        return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: unsigned int expected"));
    }
    break;
    case QVariant::Int: {
        if (binding->type == QV4::CompiledData::Binding::Type_Number) {
            double d = binding->valueAsNumber();
            if (double(int(d)) == d)
                return noError;
        }
        return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: int expected"));
    }
    break;
    case QMetaType::Float: {
        if (binding->type != QV4::CompiledData::Binding::Type_Number) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: number expected"));
        }
    }
    break;
    case QVariant::Double: {
        if (binding->type != QV4::CompiledData::Binding::Type_Number) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: number expected"));
        }
    }
    break;
    case QVariant::Color: {
        bool ok = false;
        QQmlStringConverters::rgbaFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: color expected"));
        }
    }
    break;
#ifndef QT_NO_DATESTRING
    case QVariant::Date: {
        bool ok = false;
        QQmlStringConverters::dateFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: date expected"));
        }
    }
    break;
    case QVariant::Time: {
        bool ok = false;
        QQmlStringConverters::timeFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: time expected"));
        }
    }
    break;
    case QVariant::DateTime: {
        bool ok = false;
        QQmlStringConverters::dateTimeFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: datetime expected"));
        }
    }
    break;
#endif // QT_NO_DATESTRING
    case QVariant::Point: {
        bool ok = false;
        QQmlStringConverters::pointFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: point expected"));
        }
    }
    break;
    case QVariant::PointF: {
        bool ok = false;
        QQmlStringConverters::pointFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: point expected"));
        }
    }
    break;
    case QVariant::Size: {
        bool ok = false;
        QQmlStringConverters::sizeFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: size expected"));
        }
    }
    break;
    case QVariant::SizeF: {
        bool ok = false;
        QQmlStringConverters::sizeFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: size expected"));
        }
    }
    break;
    case QVariant::Rect: {
        bool ok = false;
        QQmlStringConverters::rectFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: rect expected"));
        }
    }
    break;
    case QVariant::RectF: {
        bool ok = false;
        QQmlStringConverters::rectFFromString(binding->valueAsString(qmlUnit), &ok);
        if (!ok) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: point expected"));
        }
    }
    break;
    case QVariant::Bool: {
        if (binding->type != QV4::CompiledData::Binding::Type_Boolean) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: boolean expected"));
        }
    }
    break;
    case QVariant::Vector2D: {
        struct {
            float xp;
            float yp;
        } vec;
        if (!QQmlStringConverters::createFromString(QMetaType::QVector2D, binding->valueAsString(qmlUnit), &vec, sizeof(vec))) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: 2D vector expected"));
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
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: 3D vector expected"));
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
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: 4D vector expected"));
        }
    }
    break;
    case QVariant::Quaternion: {
        struct {
            float wp;
            float xp;
            float yp;
            float zp;
        } vec;
        if (!QQmlStringConverters::createFromString(QMetaType::QQuaternion, binding->valueAsString(qmlUnit), &vec, sizeof(vec))) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: quaternion expected"));
        }
    }
    break;
    case QVariant::RegExp:
        return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: regular expression expected; use /pattern/ syntax"));
    default: {
        // generate single literal value assignment to a list property if required
        if (property->propType() == qMetaTypeId<QList<qreal> >()) {
            if (binding->type != QV4::CompiledData::Binding::Type_Number) {
                return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: number or array of numbers expected"));
            }
            break;
        } else if (property->propType() == qMetaTypeId<QList<int> >()) {
            bool ok = (binding->type == QV4::CompiledData::Binding::Type_Number);
            if (ok) {
                double n = binding->valueAsNumber();
                if (double(int(n)) != n)
                    ok = false;
            }
            if (!ok)
                return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: int or array of ints expected"));
            break;
        } else if (property->propType() == qMetaTypeId<QList<bool> >()) {
            if (binding->type != QV4::CompiledData::Binding::Type_Boolean) {
                return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: bool or array of bools expected"));
            }
            break;
        } else if (property->propType() == qMetaTypeId<QList<QUrl> >()) {
            if (binding->type != QV4::CompiledData::Binding::Type_String) {
                return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: url or array of urls expected"));
            }
            break;
        } else if (property->propType() == qMetaTypeId<QList<QString> >()) {
            if (!binding->evaluatesToString()) {
                return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: string or array of strings expected"));
            }
            break;
        } else if (property->propType() == qMetaTypeId<QJSValue>()) {
            break;
        } else if (property->propType() == qMetaTypeId<QQmlScriptString>()) {
            break;
        }

        // otherwise, try a custom type assignment
        QQmlMetaType::StringConverter converter = QQmlMetaType::customStringConverter(property->propType());
        if (!converter) {
            return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: unsupported type \"%1\"").arg(QString::fromLatin1(QMetaType::typeName(property->propType()))));
        }
    }
    break;
    }
    return noError;
}

/*!
    Returns true if from can be assigned to a (QObject) property of type
    to.
*/
bool QQmlPropertyValidator::canCoerce(int to, QQmlPropertyCache *fromMo) const
{
    QQmlPropertyCache *toMo = enginePrivate->rawPropertyCacheForType(to);

    while (fromMo) {
        if (fromMo == toMo)
            return true;
        fromMo = fromMo->parent();
    }
    return false;
}

QVector<QQmlCompileError> QQmlPropertyValidator::recordError(const QV4::CompiledData::Location &location, const QString &description) const
{
    QVector<QQmlCompileError> errors;
    errors.append(QQmlCompileError(location, description));
    return errors;
}

QVector<QQmlCompileError> QQmlPropertyValidator::recordError(const QQmlCompileError &error) const
{
    QVector<QQmlCompileError> errors;
    errors.append(error);
    return errors;
}

QQmlCompileError QQmlPropertyValidator::validateObjectBinding(QQmlPropertyData *property, const QString &propertyName, const QV4::CompiledData::Binding *binding) const
{
    QQmlCompileError noError;

    if (binding->flags & QV4::CompiledData::Binding::IsOnAssignment) {
        Q_ASSERT(binding->type == QV4::CompiledData::Binding::Type_Object);

        bool isValueSource = false;
        bool isPropertyInterceptor = false;

        QQmlType *qmlType = 0;
        const QV4::CompiledData::Object *targetObject = qmlUnit->objectAt(binding->value.objectIndex);
        if (auto *typeRef = resolvedTypes.value(targetObject->inheritedTypeNameIndex)) {
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
            return QQmlCompileError(binding->valueLocation, tr("\"%1\" cannot operate on \"%2\"").arg(stringAt(targetObject->inheritedTypeNameIndex)).arg(propertyName));
        }

        return noError;
    }

    if (QQmlMetaType::isInterface(property->propType())) {
        // Can only check at instantiation time if the created sub-object successfully casts to the
        // target interface.
        return noError;
    } else if (property->propType() == QMetaType::QVariant) {
        // We can convert everything to QVariant :)
        return noError;
    } else if (property->isQList()) {
        const int listType = enginePrivate->listType(property->propType());
        if (!QQmlMetaType::isInterface(listType)) {
            QQmlPropertyCache *source = propertyCaches.at(binding->value.objectIndex);
            if (!canCoerce(listType, source)) {
                return QQmlCompileError(binding->valueLocation, tr("Cannot assign object to list property \"%1\"").arg(propertyName));
            }
        }
        return noError;
    } else if (qmlUnit->objectAt(binding->value.objectIndex)->flags & QV4::CompiledData::Object::IsComponent) {
        return noError;
    } else if (binding->flags & QV4::CompiledData::Binding::IsSignalHandlerObject && property->isFunction()) {
        return noError;
    } else if (QQmlValueTypeFactory::isValueType(property->propType())) {
        return QQmlCompileError(binding->location, tr("Unexpected object assignment"));
    } else if (property->propType() == qMetaTypeId<QQmlScriptString>()) {
        return QQmlCompileError(binding->valueLocation, tr("Invalid property assignment: script expected"));
    } else {
        // We want to raw metaObject here as the raw metaobject is the
        // actual property type before we applied any extensions that might
        // effect the properties on the type, but don't effect assignability
        QQmlPropertyCache *propertyMetaObject = enginePrivate->rawPropertyCacheForType(property->propType());

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
            return QQmlCompileError(binding->valueLocation, tr("Cannot assign object to property"));
        }
    }
    return noError;
}

QT_END_NAMESPACE
