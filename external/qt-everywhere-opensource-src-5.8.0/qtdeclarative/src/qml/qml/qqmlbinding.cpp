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

#include "qqmlbinding_p.h"

#include "qqml.h"
#include "qqmlcontext.h"
#include "qqmlinfo.h"
#include "qqmldata_p.h"
#include <private/qqmlprofiler_p.h>
#include <private/qqmlexpression_p.h>
#include <private/qqmlscriptstring_p.h>
#include <private/qqmlcontextwrapper_p.h>
#include <private/qqmlbuiltinfunctions_p.h>
#include <private/qqmlvmemetaobject_p.h>
#include <private/qqmlvaluetypewrapper_p.h>
#include <private/qv4qobjectwrapper_p.h>
#include <private/qv4variantobject_p.h>

#include <QVariant>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

QQmlBinding *QQmlBinding::create(const QQmlPropertyData *property, const QString &str, QObject *obj, QQmlContext *ctxt)
{
    QQmlBinding *b = newBinding(QQmlEnginePrivate::get(ctxt), property);
    b->setNotifyOnValueChanged(true);
    b->QQmlJavaScriptExpression::setContext(QQmlContextData::get(ctxt));
    b->setScopeObject(obj);

    b->createQmlBinding(b->context(), obj, str, QString(), 0);

    return b;
}

QQmlBinding *QQmlBinding::create(const QQmlPropertyData *property, const QQmlScriptString &script, QObject *obj, QQmlContext *ctxt)
{
    QQmlBinding *b = newBinding(QQmlEnginePrivate::get(ctxt), property);

    if (ctxt && !ctxt->isValid())
        return b;

    const QQmlScriptStringPrivate *scriptPrivate = script.d.data();
    if (!ctxt && (!scriptPrivate->context || !scriptPrivate->context->isValid()))
        return b;

    QString url;
    QV4::Function *runtimeFunction = 0;

    QQmlContextData *ctxtdata = QQmlContextData::get(scriptPrivate->context);
    QQmlEnginePrivate *engine = QQmlEnginePrivate::get(scriptPrivate->context->engine());
    if (engine && ctxtdata && !ctxtdata->urlString().isEmpty() && ctxtdata->typeCompilationUnit) {
        url = ctxtdata->urlString();
        if (scriptPrivate->bindingId != QQmlBinding::Invalid)
            runtimeFunction = ctxtdata->typeCompilationUnit->runtimeFunctions.at(scriptPrivate->bindingId);
    }

    b->setNotifyOnValueChanged(true);
    b->QQmlJavaScriptExpression::setContext(QQmlContextData::get(ctxt ? ctxt : scriptPrivate->context));
    b->setScopeObject(obj ? obj : scriptPrivate->scope);

    QV4::ExecutionEngine *v4 = QQmlEnginePrivate::get(b->context()->engine)->v4engine();
    if (runtimeFunction) {
        b->m_function.set(v4, QV4::FunctionObject::createQmlFunction(ctxtdata, b->scopeObject(), runtimeFunction));
    } else {
        QString code = scriptPrivate->script;
        b->createQmlBinding(b->context(), b->scopeObject(), code, url, scriptPrivate->lineNumber);
    }

    return b;
}

QQmlBinding *QQmlBinding::create(const QQmlPropertyData *property, const QString &str, QObject *obj, QQmlContextData *ctxt)
{
    QQmlBinding *b = newBinding(QQmlEnginePrivate::get(ctxt), property);

    b->setNotifyOnValueChanged(true);
    b->QQmlJavaScriptExpression::setContext(ctxt);
    b->setScopeObject(obj);

    b->createQmlBinding(ctxt, obj, str, QString(), 0);

    return b;
}

QQmlBinding *QQmlBinding::create(const QQmlPropertyData *property, const QString &str, QObject *obj,
                                 QQmlContextData *ctxt, const QString &url, quint16 lineNumber,
                                 quint16 columnNumber)
{
    QQmlBinding *b = newBinding(QQmlEnginePrivate::get(ctxt), property);

    Q_UNUSED(columnNumber);
    b->setNotifyOnValueChanged(true);
    b->QQmlJavaScriptExpression::setContext(ctxt);
    b->setScopeObject(obj);

    b->createQmlBinding(ctxt, obj, str, url, lineNumber);

    return b;
}

QQmlBinding *QQmlBinding::create(const QQmlPropertyData *property, const QV4::Value &functionPtr, QObject *obj, QQmlContextData *ctxt)
{
    QQmlBinding *b = newBinding(QQmlEnginePrivate::get(ctxt), property);

    b->setNotifyOnValueChanged(true);
    b->QQmlJavaScriptExpression::setContext(ctxt);
    b->setScopeObject(obj);

    b->m_function.set(functionPtr.as<QV4::Object>()->engine(), functionPtr);

    return b;
}

QQmlBinding::~QQmlBinding()
{
}

void QQmlBinding::setNotifyOnValueChanged(bool v)
{
    QQmlJavaScriptExpression::setNotifyOnValueChanged(v);
}

void QQmlBinding::update(QQmlPropertyData::WriteFlags flags)
{
    if (!enabledFlag() || !context() || !context()->isValid())
        return;

    // Check that the target has not been deleted
    if (QQmlData::wasDeleted(targetObject()))
        return;

    // Check for a binding update loop
    if (Q_UNLIKELY(updatingFlag())) {
        QQmlPropertyData *d = nullptr;
        QQmlPropertyData vtd;
        getPropertyData(&d, &vtd);
        Q_ASSERT(d);
        QQmlProperty p = QQmlPropertyPrivate::restore(targetObject(), *d, &vtd, 0);
        QQmlAbstractBinding::printBindingLoopError(p);
        return;
    }
    setUpdatingFlag(true);

    DeleteWatcher watcher(this);

    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(context()->engine);
    QV4::Scope scope(ep->v4engine());
    QV4::ScopedFunctionObject f(scope, m_function.value());
    Q_ASSERT(f);

    if (canUseAccessor())
        flags.setFlag(QQmlPropertyData::BypassInterceptor);

    QQmlBindingProfiler prof(ep->profiler, this, f);
    doUpdate(watcher, flags, scope, f);

    if (!watcher.wasDeleted())
        setUpdatingFlag(false);
}

// QQmlBindingBinding is for target properties which are of type "binding" (instead of, say, int or
// double). The reason for being is that GenericBinding::fastWrite needs a compile-time constant
// expression for the switch for the compiler to generate the optimal code, but
// qMetaTypeId<QQmlBinding *>() needs to be used for the ID. So QQmlBinding::newBinding uses that
// to instantiate this class.
class QQmlBindingBinding: public QQmlBinding
{
protected:
    void doUpdate(const DeleteWatcher &,
                  QQmlPropertyData::WriteFlags flags, QV4::Scope &,
                  const QV4::ScopedFunctionObject &) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        Q_ASSERT(!m_targetIndex.hasValueTypeIndex());
        QQmlPropertyData *pd = nullptr;
        getPropertyData(&pd, nullptr);
        QQmlBinding *thisPtr = this;
        pd->writeProperty(*m_target, &thisPtr, flags);
    }
};

// For any target that's not a binding, we have a common doUpdate. However, depending on the type
// of the target property, there is a specialized write method.
class QQmlNonbindingBinding: public QQmlBinding
{
protected:
    void doUpdate(const DeleteWatcher &watcher,
                  QQmlPropertyData::WriteFlags flags, QV4::Scope &scope,
                  const QV4::ScopedFunctionObject &f) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        auto ep = QQmlEnginePrivate::get(scope.engine);
        ep->referenceScarceResources();

        bool isUndefined = false;

        QV4::ScopedCallData callData(scope);
        QQmlJavaScriptExpression::evaluate(callData, &isUndefined, scope);

        bool error = false;
        if (!watcher.wasDeleted() && isAddedToObject() && !hasError())
            error = !write(scope.result, isUndefined, flags);

        if (!watcher.wasDeleted()) {

            if (error) {
                delayedError()->setErrorLocation(f->sourceLocation());
                delayedError()->setErrorObject(m_target.data());
            }

            if (hasError()) {
                if (!delayedError()->addError(ep)) ep->warning(this->error(context()->engine));
            } else {
                clearError();
            }

            cancelPermanentGuards();
        }

        ep->dereferenceScarceResources();
    }

    virtual bool write(const QV4::Value &result, bool isUndefined, QQmlPropertyData::WriteFlags flags) = 0;
};

template<int StaticPropType>
class GenericBinding: public QQmlNonbindingBinding
{
protected:
    // Returns true if successful, false if an error description was set on expression
    Q_ALWAYS_INLINE bool write(const QV4::Value &result, bool isUndefined,
                               QQmlPropertyData::WriteFlags flags) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        Q_ASSERT(targetObject());

        QQmlPropertyData *pd;
        QQmlPropertyData vpd;
        getPropertyData(&pd, &vpd);
        Q_ASSERT(pd);

        int propertyType = StaticPropType; // If the binding is specialized to a type, the if and switch below will be constant-folded.
        if (propertyType == QMetaType::UnknownType)
            propertyType = pd->propType();

        if (Q_LIKELY(!isUndefined && !vpd.isValid())) {
            switch (propertyType) {
            case QMetaType::Bool:
                if (result.isBoolean())
                    return doStore<bool>(result.booleanValue(), pd, flags);
                else
                    return doStore<bool>(result.toBoolean(), pd, flags);
            case QMetaType::Int:
                if (result.isInteger())
                    return doStore<int>(result.integerValue(), pd, flags);
                else if (result.isNumber())
                    return doStore<int>(result.doubleValue(), pd, flags);
                break;
            case QMetaType::Double:
                if (result.isNumber())
                    return doStore<double>(result.asDouble(), pd, flags);
                break;
            case QMetaType::Float:
                if (result.isNumber())
                    return doStore<float>(result.asDouble(), pd, flags);
                break;
            case QMetaType::QString:
                if (result.isString())
                    return doStore<QString>(result.toQStringNoThrow(), pd, flags);
                break;
            default:
                if (const QV4::QQmlValueTypeWrapper *vtw = result.as<const QV4::QQmlValueTypeWrapper>()) {
                    if (vtw->d()->valueType->typeId == pd->propType()) {
                        return vtw->write(m_target.data(), pd->coreIndex());
                    }
                }
                break;
            }
        }

        return slowWrite(*pd, vpd, result, isUndefined, flags);
    }

    template <typename T>
    Q_ALWAYS_INLINE bool doStore(T value, const QQmlPropertyData *pd, QQmlPropertyData::WriteFlags flags) const
    {
        void *o = &value;
        return pd->writeProperty(targetObject(), o, flags);
    }
};

Q_NEVER_INLINE bool QQmlBinding::slowWrite(const QQmlPropertyData &core,
                                           const QQmlPropertyData &valueTypeData,
                                           const QV4::Value &result,
                                           bool isUndefined, QQmlPropertyData::WriteFlags flags)
{
    QQmlEngine *engine = context()->engine;
    QV8Engine *v8engine = QQmlEnginePrivate::getV8Engine(engine);

    int type = valueTypeData.isValid() ? valueTypeData.propType() : core.propType();

    QQmlJavaScriptExpression::DeleteWatcher watcher(this);

    QVariant value;
    bool isVarProperty = core.isVarProperty();

    if (isUndefined) {
    } else if (core.isQList()) {
        value = QV8Engine::getV4(v8engine)->toVariant(result, qMetaTypeId<QList<QObject *> >());
    } else if (result.isNull() && core.isQObject()) {
        value = QVariant::fromValue((QObject *)0);
    } else if (core.propType() == qMetaTypeId<QList<QUrl> >()) {
        value = QQmlPropertyPrivate::resolvedUrlSequence(QV8Engine::getV4(v8engine)->toVariant(result, qMetaTypeId<QList<QUrl> >()), context());
    } else if (!isVarProperty && type != qMetaTypeId<QJSValue>()) {
        value = QV8Engine::getV4(v8engine)->toVariant(result, type);
    }

    if (hasError()) {
        return false;
    } else if (isVarProperty) {
        const QV4::FunctionObject *f = result.as<QV4::FunctionObject>();
        if (f && f->isBinding()) {
            // we explicitly disallow this case to avoid confusion.  Users can still store one
            // in an array in a var property if they need to, but the common case is user error.
            delayedError()->setErrorDescription(QLatin1String("Invalid use of Qt.binding() in a binding declaration."));
            return false;
        }

        QQmlVMEMetaObject *vmemo = QQmlVMEMetaObject::get(m_target.data());
        Q_ASSERT(vmemo);
        vmemo->setVMEProperty(core.coreIndex(), result);
    } else if (isUndefined && core.isResettable()) {
        void *args[] = { 0 };
        QMetaObject::metacall(m_target.data(), QMetaObject::ResetProperty, core.coreIndex(), args);
    } else if (isUndefined && type == qMetaTypeId<QVariant>()) {
        QQmlPropertyPrivate::writeValueProperty(m_target.data(), core, valueTypeData, QVariant(), context(), flags);
    } else if (type == qMetaTypeId<QJSValue>()) {
        const QV4::FunctionObject *f = result.as<QV4::FunctionObject>();
        if (f && f->isBinding()) {
            delayedError()->setErrorDescription(QLatin1String("Invalid use of Qt.binding() in a binding declaration."));
            return false;
        }
        QQmlPropertyPrivate::writeValueProperty(m_target.data(), core, valueTypeData, QVariant::fromValue(
                               QJSValue(QV8Engine::getV4(v8engine), result.asReturnedValue())),
                           context(), flags);
    } else if (isUndefined) {
        const QLatin1String typeName(QMetaType::typeName(type)
                                     ? QMetaType::typeName(type)
                                     : "[unknown property type]");
        delayedError()->setErrorDescription(QLatin1String("Unable to assign [undefined] to ")
                                            + typeName);
        return false;
    } else if (const QV4::FunctionObject *f = result.as<QV4::FunctionObject>()) {
        if (f->isBinding())
            delayedError()->setErrorDescription(QLatin1String("Invalid use of Qt.binding() in a binding declaration."));
        else
            delayedError()->setErrorDescription(QLatin1String("Unable to assign a function to a property of any type other than var."));
        return false;
    } else if (!QQmlPropertyPrivate::writeValueProperty(m_target.data(), core, valueTypeData, value, context(), flags)) {

        if (watcher.wasDeleted())
            return true;

        const char *valueType = 0;
        const char *propertyType = 0;

        const int userType = value.userType();
        if (userType == QMetaType::QObjectStar) {
            if (QObject *o = *(QObject *const *)value.constData()) {
                valueType = o->metaObject()->className();

                QQmlMetaObject propertyMetaObject = QQmlPropertyPrivate::rawMetaObjectForType(QQmlEnginePrivate::get(engine), type);
                if (!propertyMetaObject.isNull())
                    propertyType = propertyMetaObject.className();
            }
        } else if (userType != QVariant::Invalid) {
            if (userType == QMetaType::Nullptr || userType == QMetaType::VoidStar)
                valueType = "null";
            else
                valueType = QMetaType::typeName(userType);
        }

        if (!valueType)
            valueType = "undefined";
        if (!propertyType)
            propertyType = QMetaType::typeName(type);
        if (!propertyType)
            propertyType = "[unknown property type]";

        delayedError()->setErrorDescription(QLatin1String("Unable to assign ") +
                                                        QLatin1String(valueType) +
                                                        QLatin1String(" to ") +
                                                        QLatin1String(propertyType));
        return false;
    }

    return true;
}

QVariant QQmlBinding::evaluate()
{
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(context()->engine);
    ep->referenceScarceResources();

    bool isUndefined = false;

    QV4::Scope scope(ep->v4engine());
    QV4::ScopedCallData callData(scope);
    QQmlJavaScriptExpression::evaluate(callData, &isUndefined, scope);

    ep->dereferenceScarceResources();

    return scope.engine->toVariant(scope.result, qMetaTypeId<QList<QObject*> >());
}

QString QQmlBinding::expressionIdentifier()
{
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(context()->engine);
    QV4::Scope scope(ep->v4engine());
    QV4::ScopedValue f(scope, m_function.value());
    QV4::Function *function = f->as<QV4::FunctionObject>()->function();

    QString url = function->sourceFile();
    quint16 lineNumber = function->compiledFunction->location.line;
    quint16 columnNumber = function->compiledFunction->location.column;
    return url + QString::asprintf(":%u:%u", uint(lineNumber), uint(columnNumber));
}

void QQmlBinding::expressionChanged()
{
    update();
}

void QQmlBinding::refresh()
{
    update();
}

void QQmlBinding::setEnabled(bool e, QQmlPropertyData::WriteFlags flags)
{
    setEnabledFlag(e);
    setNotifyOnValueChanged(e);

    m_nextBinding.setFlag2(); // Always use accessors, only not when:
    if (auto interceptorMetaObject = QQmlInterceptorMetaObject::get(targetObject())) {
        if (!m_targetIndex.isValid() || interceptorMetaObject->intercepts(m_targetIndex))
            m_nextBinding.clearFlag2();
    }

    if (e)
        update(flags);
}

QString QQmlBinding::expression() const
{
    QV4::Scope scope(QQmlEnginePrivate::get(context()->engine)->v4engine());
    QV4::ScopedValue v(scope, m_function.value());
    return v->toQStringNoThrow();
}

void QQmlBinding::setTarget(const QQmlProperty &prop)
{
    auto pd = QQmlPropertyPrivate::get(prop);
    setTarget(prop.object(), pd->core, &pd->valueTypeData);
}

void QQmlBinding::setTarget(QObject *object, const QQmlPropertyData &core, const QQmlPropertyData *valueType)
{
    m_target = object;

    if (!object) {
        m_targetIndex = QQmlPropertyIndex();
        return;
    }

    int coreIndex = core.coreIndex();
    int valueTypeIndex = valueType ? valueType->coreIndex() : -1;
    for (bool isAlias = core.isAlias(); isAlias; ) {
        QQmlVMEMetaObject *vme = QQmlVMEMetaObject::getForProperty(object, coreIndex);

        int aValueTypeIndex;
        if (!vme->aliasTarget(coreIndex, &object, &coreIndex, &aValueTypeIndex)) {
            m_target = 0;
            m_targetIndex = QQmlPropertyIndex();
            return;
        }
        if (valueTypeIndex == -1)
            valueTypeIndex = aValueTypeIndex;

        QQmlData *data = QQmlData::get(object, false);
        if (!data || !data->propertyCache) {
            m_target = 0;
            m_targetIndex = QQmlPropertyIndex();
            return;
        }
        QQmlPropertyData *propertyData = data->propertyCache->property(coreIndex);
        Q_ASSERT(propertyData);

        m_target = object;
        isAlias = propertyData->isAlias();
        coreIndex = propertyData->coreIndex();
    }
    m_targetIndex = QQmlPropertyIndex(coreIndex, valueTypeIndex);

    QQmlData *data = QQmlData::get(*m_target, true);
    if (!data->propertyCache) {
        data->propertyCache = QQmlEnginePrivate::get(context()->engine)->cache(m_target->metaObject());
        data->propertyCache->addref();
    }
}

void QQmlBinding::getPropertyData(QQmlPropertyData **propertyData, QQmlPropertyData *valueTypeData) const
{
    Q_ASSERT(propertyData);

    QQmlData *data = QQmlData::get(*m_target, false);
    Q_ASSERT(data && data->propertyCache);

    *propertyData = data->propertyCache->property(m_targetIndex.coreIndex());
    Q_ASSERT(*propertyData);

    if (Q_UNLIKELY(m_targetIndex.hasValueTypeIndex() && valueTypeData)) {
        const QMetaObject *valueTypeMetaObject = QQmlValueTypeFactory::metaObjectForMetaType((*propertyData)->propType());
        Q_ASSERT(valueTypeMetaObject);
        QMetaProperty vtProp = valueTypeMetaObject->property(m_targetIndex.valueTypeIndex());
        valueTypeData->setFlags(QQmlPropertyData::flagsForProperty(vtProp));
        valueTypeData->setPropType(vtProp.userType());
        valueTypeData->setCoreIndex(m_targetIndex.valueTypeIndex());
    }
}

class QObjectPointerBinding: public QQmlNonbindingBinding
{
    QQmlMetaObject targetMetaObject;

public:
    QObjectPointerBinding(QQmlEnginePrivate *engine, int propertyType)
        : targetMetaObject(QQmlPropertyPrivate::rawMetaObjectForType(engine, propertyType))
    {}

protected:
    Q_ALWAYS_INLINE bool write(const QV4::Value &result, bool isUndefined,
                               QQmlPropertyData::WriteFlags flags) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        QQmlPropertyData *pd;
        QQmlPropertyData vtpd;
        getPropertyData(&pd, &vtpd);
        if (Q_UNLIKELY(isUndefined || vtpd.isValid()))
            return slowWrite(*pd, vtpd, result, isUndefined, flags);

        // Check if the result is a QObject:
        QObject *resultObject = nullptr;
        QQmlMetaObject resultMo;
        if (result.isNull()) {
            // Special case: we can always write a nullptr. Don't bother checking anything else.
            return pd->writeProperty(targetObject(), &resultObject, flags);
        } else if (auto wrapper = result.as<QV4::QObjectWrapper>()) {
            resultObject = wrapper->object();
            if (!resultObject)
                return pd->writeProperty(targetObject(), &resultObject, flags);
            if (QQmlData *ddata = QQmlData::get(resultObject, false))
                resultMo = ddata->propertyCache;
            if (resultMo.isNull()) {
                resultMo = resultObject->metaObject();
            }
        } else if (auto variant = result.as<QV4::VariantObject>()) {
            QVariant value = variant->d()->data();
            QQmlEnginePrivate *ep = QQmlEnginePrivate::get(context());
            resultMo = QQmlPropertyPrivate::rawMetaObjectForType(ep, value.userType());
            if (resultMo.isNull())
                return slowWrite(*pd, vtpd, result, isUndefined, flags);
            resultObject = *static_cast<QObject *const *>(value.constData());
        } else {
            return slowWrite(*pd, vtpd, result, isUndefined, flags);
        }

        // Compare & set:
        if (QQmlMetaObject::canConvert(resultMo, targetMetaObject)) {
            return pd->writeProperty(targetObject(), &resultObject, flags);
        } else if (!resultObject && QQmlMetaObject::canConvert(targetMetaObject, resultMo)) {
            // In the case of a null QObject, we assign the null if there is
            // any change that the null variant type could be up or down cast to
            // the property type.
            return pd->writeProperty(targetObject(), &resultObject, flags);
        } else {
            return slowWrite(*pd, vtpd, result, isUndefined, flags);
        }
    }
};

QQmlBinding *QQmlBinding::newBinding(QQmlEnginePrivate *engine, const QQmlPropertyData *property)
{
    if (property && property->isQObject())
        return new QObjectPointerBinding(engine, property->propType());

    const int type = (property && property->isFullyResolved()) ? property->propType() : QMetaType::UnknownType;

    if (type == qMetaTypeId<QQmlBinding *>()) {
        return new QQmlBindingBinding;
    }

    switch (type) {
    case QMetaType::Bool:
        return new GenericBinding<QMetaType::Bool>;
    case QMetaType::Int:
        return new GenericBinding<QMetaType::Int>;
    case QMetaType::Double:
        return new GenericBinding<QMetaType::Double>;
    case QMetaType::Float:
        return new GenericBinding<QMetaType::Float>;
    case QMetaType::QString:
        return new GenericBinding<QMetaType::QString>;
    default:
        return new GenericBinding<QMetaType::UnknownType>;
    }
}

QT_END_NAMESPACE
