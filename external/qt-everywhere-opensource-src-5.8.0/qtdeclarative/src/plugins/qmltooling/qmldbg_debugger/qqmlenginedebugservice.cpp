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

#include "qqmlenginedebugservice.h"
#include "qqmlwatcher.h"
#include "qqmldebugpacket.h"

#include <private/qqmldebugstatesdelegate_p.h>
#include <private/qqmlboundsignal_p.h>
#include <qqmlengine.h>
#include <private/qqmlmetatype_p.h>
#include <qqmlproperty.h>
#include <private/qqmlproperty_p.h>
#include <private/qqmlbinding_p.h>
#include <private/qqmlcontext_p.h>
#include <private/qqmlvaluetype_p.h>
#include <private/qqmlvmemetaobject_p.h>
#include <private/qqmlexpression_p.h>

#include <QtCore/qdebug.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qfileinfo.h>
#include <private/qmetaobject_p.h>

QT_BEGIN_NAMESPACE

QQmlEngineDebugServiceImpl::QQmlEngineDebugServiceImpl(QObject *parent) :
    QQmlEngineDebugService(2, parent), m_watch(new QQmlWatcher(this)), m_statesDelegate(0)
{
    connect(m_watch, &QQmlWatcher::propertyChanged,
            this, &QQmlEngineDebugServiceImpl::propertyChanged);

    // Move the message into the correct thread for processing
    connect(this, &QQmlEngineDebugServiceImpl::scheduleMessage,
            this, &QQmlEngineDebugServiceImpl::processMessage, Qt::QueuedConnection);
}

QQmlEngineDebugServiceImpl::~QQmlEngineDebugServiceImpl()
{
    delete m_statesDelegate;
}

QDataStream &operator<<(QDataStream &ds,
                        const QQmlEngineDebugServiceImpl::QQmlObjectData &data)
{
    ds << data.url << data.lineNumber << data.columnNumber << data.idString
       << data.objectName << data.objectType << data.objectId << data.contextId
       << data.parentId;
    return ds;
}

QDataStream &operator>>(QDataStream &ds,
                        QQmlEngineDebugServiceImpl::QQmlObjectData &data)
{
    ds >> data.url >> data.lineNumber >> data.columnNumber >> data.idString
       >> data.objectName >> data.objectType >> data.objectId >> data.contextId
       >> data.parentId;
    return ds;
}

QDataStream &operator<<(QDataStream &ds,
                        const QQmlEngineDebugServiceImpl::QQmlObjectProperty &data)
{
    ds << (int)data.type << data.name;
    // check first whether the data can be saved
    // (otherwise we assert in QVariant::operator<<)
    QQmlDebugPacket fakeStream;
    if (QMetaType::save(fakeStream, data.value.type(), data.value.constData()))
        ds << data.value;
    else
        ds << QVariant();
    ds << data.valueTypeName << data.binding << data.hasNotifySignal;
    return ds;
}

QDataStream &operator>>(QDataStream &ds,
                        QQmlEngineDebugServiceImpl::QQmlObjectProperty &data)
{
    int type;
    ds >> type >> data.name >> data.value >> data.valueTypeName
       >> data.binding >> data.hasNotifySignal;
    data.type = (QQmlEngineDebugServiceImpl::QQmlObjectProperty::Type)type;
    return ds;
}

static inline bool isSignalPropertyName(const QString &signalName)
{
    // see QmlCompiler::isSignalPropertyName
    return signalName.length() >= 3 && signalName.startsWith(QLatin1String("on")) &&
            signalName.at(2).isLetter() && signalName.at(2).isUpper();
}

static bool hasValidSignal(QObject *object, const QString &propertyName)
{
    if (!isSignalPropertyName(propertyName))
        return false;

    QString signalName = propertyName.mid(2);
    signalName[0] = signalName.at(0).toLower();

    int sigIdx = QQmlPropertyPrivate::findSignalByName(object->metaObject(), signalName.toLatin1()).methodIndex();

    if (sigIdx == -1)
        return false;

    return true;
}

QQmlEngineDebugServiceImpl::QQmlObjectProperty
QQmlEngineDebugServiceImpl::propertyData(QObject *obj, int propIdx)
{
    QQmlObjectProperty rv;

    QMetaProperty prop = obj->metaObject()->property(propIdx);

    rv.type = QQmlObjectProperty::Unknown;
    rv.valueTypeName = QString::fromUtf8(prop.typeName());
    rv.name = QString::fromUtf8(prop.name());
    rv.hasNotifySignal = prop.hasNotifySignal();
    QQmlAbstractBinding *binding =
            QQmlPropertyPrivate::binding(QQmlProperty(obj, rv.name));
    if (binding)
        rv.binding = binding->expression();

    if (QQmlValueTypeFactory::isValueType(prop.userType())) {
        rv.type = QQmlObjectProperty::Basic;
    } else if (QQmlMetaType::isQObject(prop.userType()))  {
        rv.type = QQmlObjectProperty::Object;
    } else if (QQmlMetaType::isList(prop.userType())) {
        rv.type = QQmlObjectProperty::List;
    } else if (prop.userType() == QMetaType::QVariant) {
        rv.type = QQmlObjectProperty::Variant;
    }

    QVariant value;
    if (rv.type != QQmlObjectProperty::Unknown && prop.userType() != 0) {
        value = prop.read(obj);
    }
    rv.value = valueContents(value);

    return rv;
}

QVariant QQmlEngineDebugServiceImpl::valueContents(QVariant value) const
{
    // We can't send JS objects across the wire, so transform them to variant
    // maps for serialization.
    if (value.userType() == qMetaTypeId<QJSValue>())
        value = value.value<QJSValue>().toVariant();
    const int userType = value.userType();

    //QObject * is not streamable.
    //Convert all such instances to a String value

    if (value.type() == QVariant::List) {
        QVariantList contents;
        QVariantList list = value.toList();
        int count = list.size();
        contents.reserve(count);
        for (int i = 0; i < count; i++)
            contents << valueContents(list.at(i));
        return contents;
    }

    if (value.type() == QVariant::Map) {
        QVariantMap contents;
        QMapIterator<QString, QVariant> i(value.toMap());
         while (i.hasNext()) {
             i.next();
             contents.insert(i.key(), valueContents(i.value()));
         }
        return contents;
    }

    if (QQmlValueTypeFactory::isValueType(userType)) {
        const QMetaObject *mo = QQmlValueTypeFactory::metaObjectForMetaType(userType);
        if (mo) {
            int toStringIndex = mo->indexOfMethod("toString");
            if (toStringIndex != -1) {
                QMetaMethod mm = mo->method(toStringIndex);
                QMetaType info(userType);
                QString s;
                if (info.flags() & QMetaType::IsGadget
                        && mm.invokeOnGadget(value.data(), Q_RETURN_ARG(QString, s)))
                    return s;
            }
        }

        return value;
    }

    if (QQmlMetaType::isQObject(userType)) {
        QObject *o = QQmlMetaType::toQObject(value);
        if (o) {
            QString name = o->objectName();
            if (name.isEmpty())
                name = QStringLiteral("<unnamed object>");
            return name;
        }
    }

    return QString(QStringLiteral("<unknown value>"));
}

void QQmlEngineDebugServiceImpl::buildObjectDump(QDataStream &message,
                                                     QObject *object, bool recur, bool dumpProperties)
{
    message << objectData(object);

    QObjectList children = object->children();

    int childrenCount = children.count();
    for (int ii = 0; ii < children.count(); ++ii) {
        if (qobject_cast<QQmlContext*>(children[ii]))
            --childrenCount;
    }

    message << childrenCount << recur;

    QList<QQmlObjectProperty> fakeProperties;

    for (int ii = 0; ii < children.count(); ++ii) {
        QObject *child = children.at(ii);
        if (qobject_cast<QQmlContext*>(child))
            continue;
         if (recur)
             buildObjectDump(message, child, recur, dumpProperties);
         else
             message << objectData(child);
    }

    if (!dumpProperties) {
        message << 0;
        return;
    }

    QList<int> propertyIndexes;
    for (int ii = 0; ii < object->metaObject()->propertyCount(); ++ii) {
        if (object->metaObject()->property(ii).isScriptable())
            propertyIndexes << ii;
    }

    QQmlData *ddata = QQmlData::get(object);
    if (ddata && ddata->signalHandlers) {
        QQmlBoundSignal *signalHandler = ddata->signalHandlers;

        while (signalHandler) {
            QQmlObjectProperty prop;
            prop.type = QQmlObjectProperty::SignalProperty;
            prop.hasNotifySignal = false;
            QQmlBoundSignalExpression *expr = signalHandler->expression();
            if (expr) {
                prop.value = expr->expression();
                QObject *scope = expr->scopeObject();
                if (scope) {
                    QString methodName = QString::fromLatin1(QMetaObjectPrivate::signal(scope->metaObject(), signalHandler->signalIndex()).name());
                    if (!methodName.isEmpty()) {
                        prop.name = QLatin1String("on") + methodName[0].toUpper()
                                + methodName.mid(1);
                    }
                }
            }
            fakeProperties << prop;

            signalHandler = nextSignal(signalHandler);
        }
    }

    message << propertyIndexes.size() + fakeProperties.count();

    for (int ii = 0; ii < propertyIndexes.size(); ++ii)
        message << propertyData(object, propertyIndexes.at(ii));

    for (int ii = 0; ii < fakeProperties.count(); ++ii)
        message << fakeProperties[ii];
}

void QQmlEngineDebugServiceImpl::prepareDeferredObjects(QObject *obj)
{
    qmlExecuteDeferred(obj);

    QObjectList children = obj->children();
    for (int ii = 0; ii < children.count(); ++ii) {
        QObject *child = children.at(ii);
        prepareDeferredObjects(child);
    }

}

void QQmlEngineDebugServiceImpl::storeObjectIds(QObject *co)
{
    QQmlDebugService::idForObject(co);
    QObjectList children = co->children();
    for (int ii = 0; ii < children.count(); ++ii)
        storeObjectIds(children.at(ii));
}

void QQmlEngineDebugServiceImpl::buildObjectList(QDataStream &message,
                                             QQmlContext *ctxt,
                                             const QList<QPointer<QObject> > &instances)
{
    QQmlContextData *p = QQmlContextData::get(ctxt);

    QString ctxtName = ctxt->objectName();
    int ctxtId = QQmlDebugService::idForObject(ctxt);
    if (ctxt->contextObject())
        storeObjectIds(ctxt->contextObject());

    message << ctxtName << ctxtId;

    int count = 0;

    QQmlContextData *child = p->childContexts;
    while (child) {
        ++count;
        child = child->nextChild;
    }

    message << count;

    child = p->childContexts;
    while (child) {
        buildObjectList(message, child->asQQmlContext(), instances);
        child = child->nextChild;
    }

    count = 0;
    for (int ii = 0; ii < instances.count(); ++ii) {
        QQmlData *data = QQmlData::get(instances.at(ii));
        if (data->context == p)
            count ++;
    }
    message << count;

    for (int ii = 0; ii < instances.count(); ++ii) {
        QQmlData *data = QQmlData::get(instances.at(ii));
        if (data->context == p)
            message << objectData(instances.at(ii));
    }
}

void QQmlEngineDebugServiceImpl::buildStatesList(bool cleanList,
                                             const QList<QPointer<QObject> > &instances)
{
    if (m_statesDelegate)
        m_statesDelegate->buildStatesList(cleanList, instances);
}

QQmlEngineDebugServiceImpl::QQmlObjectData
QQmlEngineDebugServiceImpl::objectData(QObject *object)
{
    QQmlData *ddata = QQmlData::get(object);
    QQmlObjectData rv;
    if (ddata && ddata->outerContext) {
        rv.url = ddata->outerContext->url();
        rv.lineNumber = ddata->lineNumber;
        rv.columnNumber = ddata->columnNumber;
    } else {
        rv.lineNumber = -1;
        rv.columnNumber = -1;
    }

    QQmlContext *context = qmlContext(object);
    if (context) {
        QQmlContextData *cdata = QQmlContextData::get(context);
        if (cdata)
            rv.idString = cdata->findObjectId(object);
    }

    rv.objectName = object->objectName();
    rv.objectId = QQmlDebugService::idForObject(object);
    rv.contextId = QQmlDebugService::idForObject(qmlContext(object));
    rv.parentId = QQmlDebugService::idForObject(object->parent());
    QQmlType *type = QQmlMetaType::qmlType(object->metaObject());
    if (type) {
        QString typeName = type->qmlTypeName();
        int lastSlash = typeName.lastIndexOf(QLatin1Char('/'));
        rv.objectType = lastSlash < 0 ? typeName : typeName.mid(lastSlash+1);
    } else {
        rv.objectType = QString::fromUtf8(object->metaObject()->className());
        int marker = rv.objectType.indexOf(QLatin1String("_QMLTYPE_"));
        if (marker != -1)
            rv.objectType = rv.objectType.left(marker);
    }

    return rv;
}

void QQmlEngineDebugServiceImpl::messageReceived(const QByteArray &message)
{
    emit scheduleMessage(message);
}

/*!
    Returns a list of objects matching the given filename, line and column.
*/
QList<QObject*> QQmlEngineDebugServiceImpl::objectForLocationInfo(const QString &filename,
                                                              int lineNumber, int columnNumber)
{
    QList<QObject *> objects;
    const QHash<int, QObject *> &hash = objectsForIds();
    for (QHash<int, QObject *>::ConstIterator i = hash.constBegin(); i != hash.constEnd(); ++i) {
        QQmlData *ddata = QQmlData::get(i.value());
        if (ddata && ddata->outerContext && ddata->outerContext->isValid()) {
            if (QFileInfo(ddata->outerContext->urlString()).fileName() == filename &&
                ddata->lineNumber == lineNumber &&
                ddata->columnNumber >= columnNumber) {
                objects << i.value();
            }
        }
    }
    return objects;
}

void QQmlEngineDebugServiceImpl::processMessage(const QByteArray &message)
{
    QQmlDebugPacket ds(message);

    QByteArray type;
    int queryId;
    ds >> type >> queryId;

    QQmlDebugPacket rs;

    if (type == "LIST_ENGINES") {
        rs << QByteArray("LIST_ENGINES_R");
        rs << queryId << m_engines.count();

        for (int ii = 0; ii < m_engines.count(); ++ii) {
            QJSEngine *engine = m_engines.at(ii);

            QString engineName = engine->objectName();
            int engineId = QQmlDebugService::idForObject(engine);

            rs << engineName << engineId;
        }

    } else if (type == "LIST_OBJECTS") {
        int engineId = -1;
        ds >> engineId;

        QQmlEngine *engine =
                qobject_cast<QQmlEngine *>(QQmlDebugService::objectForId(engineId));

        rs << QByteArray("LIST_OBJECTS_R") << queryId;

        if (engine) {
            QQmlContext *rootContext = engine->rootContext();
            // Clean deleted objects
            QQmlContextPrivate *ctxtPriv = QQmlContextPrivate::get(rootContext);
            for (int ii = 0; ii < ctxtPriv->instances.count(); ++ii) {
                if (!ctxtPriv->instances.at(ii)) {
                    ctxtPriv->instances.removeAt(ii);
                    --ii;
                }
            }
            buildObjectList(rs, rootContext, ctxtPriv->instances);
            buildStatesList(true, ctxtPriv->instances);
        }

    } else if (type == "FETCH_OBJECT") {
        int objectId;
        bool recurse;
        bool dumpProperties = true;

        ds >> objectId >> recurse >> dumpProperties;

        QObject *object = QQmlDebugService::objectForId(objectId);

        rs << QByteArray("FETCH_OBJECT_R") << queryId;

        if (object) {
            if (recurse)
                prepareDeferredObjects(object);
            buildObjectDump(rs, object, recurse, dumpProperties);
        }

    } else if (type == "FETCH_OBJECTS_FOR_LOCATION") {
        QString file;
        int lineNumber;
        int columnNumber;
        bool recurse;
        bool dumpProperties = true;

        ds >> file >> lineNumber >> columnNumber >> recurse >> dumpProperties;

        QList<QObject*> objects = objectForLocationInfo(file, lineNumber, columnNumber);

        rs << QByteArray("FETCH_OBJECTS_FOR_LOCATION_R") << queryId
           << objects.count();

        foreach (QObject *object, objects) {
            if (recurse)
                prepareDeferredObjects(object);
            buildObjectDump(rs, object, recurse, dumpProperties);
        }

    } else if (type == "WATCH_OBJECT") {
        int objectId;

        ds >> objectId;
        bool ok = m_watch->addWatch(queryId, objectId);

        rs << QByteArray("WATCH_OBJECT_R") << queryId << ok;

    } else if (type == "WATCH_PROPERTY") {
        int objectId;
        QByteArray property;

        ds >> objectId >> property;
        bool ok = m_watch->addWatch(queryId, objectId, property);

        rs << QByteArray("WATCH_PROPERTY_R") << queryId << ok;

    } else if (type == "WATCH_EXPR_OBJECT") {
        int debugId;
        QString expr;

        ds >> debugId >> expr;
        bool ok = m_watch->addWatch(queryId, debugId, expr);

        rs << QByteArray("WATCH_EXPR_OBJECT_R") << queryId << ok;

    } else if (type == "NO_WATCH") {
        bool ok = m_watch->removeWatch(queryId);

        rs << QByteArray("NO_WATCH_R") << queryId << ok;

    } else if (type == "EVAL_EXPRESSION") {
        int objectId;
        QString expr;

        ds >> objectId >> expr;
        int engineId = -1;
        if (!ds.atEnd())
            ds >> engineId;

        QObject *object = QQmlDebugService::objectForId(objectId);
        QQmlContext *context = qmlContext(object);
        if (!context) {
            QQmlEngine *engine = qobject_cast<QQmlEngine *>(
                        QQmlDebugService::objectForId(engineId));
            if (engine && m_engines.contains(engine))
                context = engine->rootContext();
        }
        QVariant result;
        if (context) {
            QQmlExpression exprObj(context, object, expr);
            bool undefined = false;
            QVariant value = exprObj.evaluate(&undefined);
            if (undefined)
                result = QString(QStringLiteral("<undefined>"));
            else
                result = valueContents(value);
        } else {
            result = QString(QStringLiteral("<unknown context>"));
        }

        rs << QByteArray("EVAL_EXPRESSION_R") << queryId << result;

    } else if (type == "SET_BINDING") {
        int objectId;
        QString propertyName;
        QVariant expr;
        bool isLiteralValue;
        QString filename;
        int line;
        ds >> objectId >> propertyName >> expr >> isLiteralValue >>
              filename >> line;
        bool ok = setBinding(objectId, propertyName, expr, isLiteralValue,
                             filename, line);

        rs << QByteArray("SET_BINDING_R") << queryId << ok;

    } else if (type == "RESET_BINDING") {
        int objectId;
        QString propertyName;
        ds >> objectId >> propertyName;
        bool ok = resetBinding(objectId, propertyName);

        rs << QByteArray("RESET_BINDING_R") << queryId << ok;

    } else if (type == "SET_METHOD_BODY") {
        int objectId;
        QString methodName;
        QString methodBody;
        ds >> objectId >> methodName >> methodBody;
        bool ok = setMethodBody(objectId, methodName, methodBody);

        rs << QByteArray("SET_METHOD_BODY_R") << queryId << ok;

    }
    emit messageToClient(name(), rs.data());
}

bool QQmlEngineDebugServiceImpl::setBinding(int objectId,
                                                const QString &propertyName,
                                                const QVariant &expression,
                                                bool isLiteralValue,
                                                QString filename,
                                                int line,
                                                int column)
{
    bool ok = true;
    QObject *object = objectForId(objectId);
    QQmlContext *context = qmlContext(object);

    if (object && context) {
        QQmlProperty property(object, propertyName, context);
        if (property.isValid()) {

            bool inBaseState = true;
            if (m_statesDelegate) {
                m_statesDelegate->updateBinding(context, property, expression, isLiteralValue,
                                                filename, line, column, &inBaseState);
            }

            if (inBaseState) {
                if (isLiteralValue) {
                    property.write(expression);
                } else if (hasValidSignal(object, propertyName)) {
                    QQmlBoundSignalExpression *qmlExpression = new QQmlBoundSignalExpression(object, QQmlPropertyPrivate::get(property)->signalIndex(),
                                                                                             QQmlContextData::get(context), object, expression.toString(),
                                                                                             filename, line, column);
                    QQmlPropertyPrivate::takeSignalExpression(property, qmlExpression);
                } else if (property.isProperty()) {
                    QQmlBinding *binding = QQmlBinding::create(&QQmlPropertyPrivate::get(property)->core, expression.toString(), object, QQmlContextData::get(context), filename, line, column);
                    binding->setTarget(property);
                    QQmlPropertyPrivate::setBinding(binding);
                    binding->update();
                } else {
                    ok = false;
                    qWarning() << "QQmlEngineDebugService::setBinding: unable to set property" << propertyName << "on object" << object;
                }
            }

        } else {
            // not a valid property
            if (m_statesDelegate)
                ok = m_statesDelegate->setBindingForInvalidProperty(object, propertyName, expression, isLiteralValue);
            if (!ok)
                qWarning() << "QQmlEngineDebugService::setBinding: unable to set property" << propertyName << "on object" << object;
        }
    }
    return ok;
}

bool QQmlEngineDebugServiceImpl::resetBinding(int objectId, const QString &propertyName)
{
    QObject *object = objectForId(objectId);
    QQmlContext *context = qmlContext(object);

    if (object && context) {
        QStringRef parentPropertyRef(&propertyName);
        const int idx = parentPropertyRef.indexOf(QLatin1Char('.'));
        if (idx != -1)
            parentPropertyRef = parentPropertyRef.left(idx);

        const QByteArray parentProperty = parentPropertyRef.toLatin1();
        if (object->property(parentProperty).isValid()) {
            QQmlProperty property(object, propertyName);
            QQmlPropertyPrivate::removeBinding(property);
            if (property.isResettable()) {
                // Note: this will reset the property in any case, without regard to states
                // Right now almost no QQuickItem has reset methods for its properties (with the
                // notable exception of QQuickAnchors), so this is not a big issue
                // later on, setBinding does take states into account
                property.reset();
            } else {
                // overwrite with default value
                if (QQmlType *objType = QQmlMetaType::qmlType(object->metaObject())) {
                    if (QObject *emptyObject = objType->create()) {
                        if (emptyObject->property(parentProperty).isValid()) {
                            QVariant defaultValue = QQmlProperty(emptyObject, propertyName).read();
                            if (defaultValue.isValid()) {
                                setBinding(objectId, propertyName, defaultValue, true);
                            }
                        }
                        delete emptyObject;
                    }
                }
            }
            return true;
        }

        if (hasValidSignal(object, propertyName)) {
            QQmlProperty property(object, propertyName, context);
            QQmlPropertyPrivate::setSignalExpression(property, 0);
            return true;
        }

        if (m_statesDelegate) {
            m_statesDelegate->resetBindingForInvalidProperty(object, propertyName);
            return true;
        }

        return false;
    }
    // object or context null.
    return false;
}

bool QQmlEngineDebugServiceImpl::setMethodBody(int objectId, const QString &method, const QString &body)
{
    QObject *object = objectForId(objectId);
    QQmlContext *context = qmlContext(object);
    if (!object || !context || !context->engine())
        return false;
    QQmlContextData *contextData = QQmlContextData::get(context);
    if (!contextData)
        return false;

    QQmlPropertyData dummy;
    QQmlPropertyData *prop =
            QQmlPropertyCache::property(context->engine(), object, method, contextData, dummy);

    if (!prop || !prop->isVMEFunction())
        return false;

    QMetaMethod metaMethod = object->metaObject()->method(prop->coreIndex());
    QList<QByteArray> paramNames = metaMethod.parameterNames();

    QString paramStr;
    for (int ii = 0; ii < paramNames.count(); ++ii) {
        if (ii != 0) paramStr.append(QLatin1Char(','));
        paramStr.append(QString::fromUtf8(paramNames.at(ii)));
    }

    const QString jsfunction = QLatin1String("(function ") + method + QLatin1Char('(') + paramStr +
            QLatin1String(") {") + body + QLatin1String("\n})");

    QQmlVMEMetaObject *vmeMetaObject = QQmlVMEMetaObject::get(object);
    Q_ASSERT(vmeMetaObject); // the fact we found the property above should guarentee this

    QV4::ExecutionEngine *v4 = QV8Engine::getV4(qmlEngine(object)->handle());
    QV4::Scope scope(v4);

    int lineNumber = 0;
    QV4::ScopedFunctionObject oldMethod(scope, vmeMetaObject->vmeMethod(prop->coreIndex()));
    if (oldMethod && oldMethod->d()->function) {
        lineNumber = oldMethod->d()->function->compiledFunction->location.line;
    }
    QV4::ScopedValue v(scope, QQmlJavaScriptExpression::evalFunction(contextData, object, jsfunction, contextData->urlString(), lineNumber));
    vmeMetaObject->setVmeMethod(prop->coreIndex(), v);
    return true;
}

void QQmlEngineDebugServiceImpl::propertyChanged(int id, int objectId, const QMetaProperty &property, const QVariant &value)
{
    QQmlDebugPacket rs;
    rs << QByteArray("UPDATE_WATCH") << id << objectId << QByteArray(property.name()) << valueContents(value);
    emit messageToClient(name(), rs.data());
}

void QQmlEngineDebugServiceImpl::engineAboutToBeAdded(QJSEngine *engine)
{
    Q_ASSERT(engine);
    Q_ASSERT(!m_engines.contains(engine));

    m_engines.append(engine);
    emit attachedToEngine(engine);
}

void QQmlEngineDebugServiceImpl::engineAboutToBeRemoved(QJSEngine *engine)
{
    Q_ASSERT(engine);
    Q_ASSERT(m_engines.contains(engine));

    m_engines.removeAll(engine);
    emit detachedFromEngine(engine);
}

void QQmlEngineDebugServiceImpl::objectCreated(QJSEngine *engine, QObject *object)
{
    Q_ASSERT(engine);
    Q_ASSERT(m_engines.contains(engine));

    int engineId = QQmlDebugService::idForObject(engine);
    int objectId = QQmlDebugService::idForObject(object);
    int parentId = QQmlDebugService::idForObject(object->parent());

    QQmlDebugPacket rs;

    //unique queryId -1
    rs << QByteArray("OBJECT_CREATED") << -1 << engineId << objectId << parentId;
    emit messageToClient(name(), rs.data());
}

void QQmlEngineDebugServiceImpl::setStatesDelegate(QQmlDebugStatesDelegate *delegate)
{
    m_statesDelegate = delegate;
}

QT_END_NAMESPACE
