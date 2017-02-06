/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
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

#include "qmetaobjectpublisher_p.h"
#include "qwebchannel.h"
#include "qwebchannel_p.h"
#include "qwebchannelabstracttransport.h"

#include <QEvent>
#include <QJsonDocument>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#ifndef QT_NO_JSVALUE
#include <QJSValue>
#endif
#include <QUuid>

QT_BEGIN_NAMESPACE

namespace {

MessageType toType(const QJsonValue &value)
{
    int i = value.toInt(-1);
    if (i >= TYPES_FIRST_VALUE && i <= TYPES_LAST_VALUE) {
        return static_cast<MessageType>(i);
    } else {
        return TypeInvalid;
    }
}

const QString KEY_SIGNALS = QStringLiteral("signals");
const QString KEY_METHODS = QStringLiteral("methods");
const QString KEY_PROPERTIES = QStringLiteral("properties");
const QString KEY_ENUMS = QStringLiteral("enums");
const QString KEY_QOBJECT = QStringLiteral("__QObject*__");
const QString KEY_ID = QStringLiteral("id");
const QString KEY_DATA = QStringLiteral("data");
const QString KEY_OBJECT = QStringLiteral("object");
const QString KEY_DESTROYED = QStringLiteral("destroyed");
const QString KEY_SIGNAL = QStringLiteral("signal");
const QString KEY_TYPE = QStringLiteral("type");
const QString KEY_METHOD = QStringLiteral("method");
const QString KEY_ARGS = QStringLiteral("args");
const QString KEY_PROPERTY = QStringLiteral("property");
const QString KEY_VALUE = QStringLiteral("value");

QJsonObject createResponse(const QJsonValue &id, const QJsonValue &data)
{
    QJsonObject response;
    response[KEY_TYPE] = TypeResponse;
    response[KEY_ID] = id;
    response[KEY_DATA] = data;
    return response;
}

/// TODO: what is the proper value here?
const int PROPERTY_UPDATE_INTERVAL = 50;
}

QMetaObjectPublisher::QMetaObjectPublisher(QWebChannel *webChannel)
    : QObject(webChannel)
    , webChannel(webChannel)
    , signalHandler(this)
    , clientIsIdle(false)
    , blockUpdates(false)
    , propertyUpdatesInitialized(false)
{
}

QMetaObjectPublisher::~QMetaObjectPublisher()
{

}

void QMetaObjectPublisher::registerObject(const QString &id, QObject *object)
{
    registeredObjects[id] = object;
    registeredObjectIds[object] = id;
    if (propertyUpdatesInitialized) {
        if (!webChannel->d_func()->transports.isEmpty()) {
            qWarning("Registered new object after initialization, existing clients won't be notified!");
            // TODO: send a message to clients that an object was added
        }
        initializePropertyUpdates(object, classInfoForObject(object, Q_NULLPTR));
    }
}

QJsonObject QMetaObjectPublisher::classInfoForObject(const QObject *object, QWebChannelAbstractTransport *transport)
{
    QJsonObject data;
    if (!object) {
        qWarning("null object given to MetaObjectPublisher - bad API usage?");
        return data;
    }

    QJsonArray qtSignals;
    QJsonArray qtMethods;
    QJsonArray qtProperties;
    QJsonObject qtEnums;

    const QMetaObject *metaObject = object->metaObject();
    QSet<int> notifySignals;
    QSet<QString> identifiers;
    for (int i = 0; i < metaObject->propertyCount(); ++i) {
        const QMetaProperty &prop = metaObject->property(i);
        QJsonArray propertyInfo;
        const QString &propertyName = QString::fromLatin1(prop.name());
        propertyInfo.append(i);
        propertyInfo.append(propertyName);
        identifiers << propertyName;
        QJsonArray signalInfo;
        if (prop.hasNotifySignal()) {
            notifySignals << prop.notifySignalIndex();
            const int numParams = prop.notifySignal().parameterCount();
            if (numParams > 1) {
                qWarning("Notify signal for property '%s' has %d parameters, expected zero or one.",
                         prop.name(), numParams);
            }
            // optimize: compress the common propertyChanged notification names, just send a 1
            const QByteArray &notifySignal = prop.notifySignal().name();
            static const QByteArray changedSuffix = QByteArrayLiteral("Changed");
            if (notifySignal.length() == changedSuffix.length() + propertyName.length() &&
                notifySignal.endsWith(changedSuffix) && notifySignal.startsWith(prop.name()))
            {
                signalInfo.append(1);
            } else {
                signalInfo.append(QString::fromLatin1(notifySignal));
            }
            signalInfo.append(prop.notifySignalIndex());
        } else if (!prop.isConstant()) {
            qWarning("Property '%s'' of object '%s' has no notify signal and is not constant, "
                     "value updates in HTML will be broken!",
                     prop.name(), object->metaObject()->className());
        }
        propertyInfo.append(signalInfo);
        propertyInfo.append(wrapResult(prop.read(object), transport));
        qtProperties.append(propertyInfo);
    }
    for (int i = 0; i < metaObject->methodCount(); ++i) {
        if (notifySignals.contains(i)) {
            continue;
        }
        const QMetaMethod &method = metaObject->method(i);
        //NOTE: this must be a string, otherwise it will be converted to '{}' in QML
        const QString &name = QString::fromLatin1(method.name());
        // optimize: skip overloaded methods/signals or property getters, on the JS side we can only
        // call one of them anyways
        // TODO: basic support for overloaded signals, methods
        if (identifiers.contains(name)) {
            continue;
        }
        identifiers << name;
        // send data as array to client with format: [name, index]
        QJsonArray data;
        data.append(name);
        data.append(i);
        if (method.methodType() == QMetaMethod::Signal) {
            qtSignals.append(data);
        } else if (method.access() == QMetaMethod::Public) {
            qtMethods.append(data);
        }
    }
    for (int i = 0; i < metaObject->enumeratorCount(); ++i) {
        QMetaEnum enumerator = metaObject->enumerator(i);
        QJsonObject values;
        for (int k = 0; k < enumerator.keyCount(); ++k) {
            values[QString::fromLatin1(enumerator.key(k))] = enumerator.value(k);
        }
        qtEnums[QString::fromLatin1(enumerator.name())] = values;
    }
    data[KEY_SIGNALS] = qtSignals;
    data[KEY_METHODS] = qtMethods;
    data[KEY_PROPERTIES] = qtProperties;
    if (!qtEnums.isEmpty()) {
        data[KEY_ENUMS] = qtEnums;
    }
    return data;
}

void QMetaObjectPublisher::setClientIsIdle(bool isIdle)
{
    if (clientIsIdle == isIdle) {
        return;
    }
    clientIsIdle = isIdle;
    if (!isIdle && timer.isActive()) {
        timer.stop();
    } else if (isIdle && !timer.isActive()) {
        timer.start(PROPERTY_UPDATE_INTERVAL, this);
    }
}

QJsonObject QMetaObjectPublisher::initializeClient(QWebChannelAbstractTransport *transport)
{
    QJsonObject objectInfos;
    {
        const QHash<QString, QObject *>::const_iterator end = registeredObjects.constEnd();
        for (QHash<QString, QObject *>::const_iterator it = registeredObjects.constBegin(); it != end; ++it) {
            const QJsonObject &info = classInfoForObject(it.value(), transport);
            if (!propertyUpdatesInitialized) {
                initializePropertyUpdates(it.value(), info);
            }
            objectInfos[it.key()] = info;
        }
    }
    propertyUpdatesInitialized = true;
    return objectInfos;
}

void QMetaObjectPublisher::initializePropertyUpdates(const QObject *const object, const QJsonObject &objectInfo)
{
    foreach (const QJsonValue &propertyInfoVar, objectInfo[KEY_PROPERTIES].toArray()) {
        const QJsonArray &propertyInfo = propertyInfoVar.toArray();
        if (propertyInfo.size() < 2) {
            qWarning() << "Invalid property info encountered:" << propertyInfoVar;
            continue;
        }
        const int propertyIndex = propertyInfo.at(0).toInt();
        const QJsonArray &signalData = propertyInfo.at(2).toArray();

        if (signalData.isEmpty()) {
            // Property without NOTIFY signal
            continue;
        }

        const int signalIndex = signalData.at(1).toInt();

        QSet<int> &connectedProperties = signalToPropertyMap[object][signalIndex];

        // Only connect for a property update once
        if (connectedProperties.isEmpty()) {
            signalHandler.connectTo(object, signalIndex);
        }

        connectedProperties.insert(propertyIndex);
    }

    // also always connect to destroyed signal
    signalHandler.connectTo(object, s_destroyedSignalIndex);
}

void QMetaObjectPublisher::sendPendingPropertyUpdates()
{
    if (blockUpdates || !clientIsIdle || pendingPropertyUpdates.isEmpty()) {
        return;
    }

    QJsonArray data;
    QHash<QWebChannelAbstractTransport*, QJsonArray> specificUpdates;

    // convert pending property updates to JSON data
    const PendingPropertyUpdates::const_iterator end = pendingPropertyUpdates.constEnd();
    for (PendingPropertyUpdates::const_iterator it = pendingPropertyUpdates.constBegin(); it != end; ++it) {
        const QObject *object = it.key();
        const QMetaObject *const metaObject = object->metaObject();
        const QString objectId = registeredObjectIds.value(object);
        const SignalToPropertyNameMap &objectsSignalToPropertyMap = signalToPropertyMap.value(object);
        // maps property name to current property value
        QJsonObject properties;
        // maps signal index to list of arguments of the last emit
        QJsonObject sigs;
        const SignalToArgumentsMap::const_iterator sigEnd = it.value().constEnd();
        for (SignalToArgumentsMap::const_iterator sigIt = it.value().constBegin(); sigIt != sigEnd; ++sigIt) {
            // TODO: can we get rid of the int <-> string conversions here?
            foreach (const int propertyIndex, objectsSignalToPropertyMap.value(sigIt.key())) {
                const QMetaProperty &property = metaObject->property(propertyIndex);
                Q_ASSERT(property.isValid());
                properties[QString::number(propertyIndex)] = wrapResult(property.read(object), Q_NULLPTR, objectId);
            }
            sigs[QString::number(sigIt.key())] = QJsonArray::fromVariantList(sigIt.value());
        }
        QJsonObject obj;
        obj[KEY_OBJECT] = objectId;
        obj[KEY_SIGNALS] = sigs;
        obj[KEY_PROPERTIES] = properties;

        // if the object is auto registered, just send the update only to clients which know this object
        if (wrappedObjects.contains(objectId)) {
            foreach (QWebChannelAbstractTransport *transport, wrappedObjects.value(objectId).transports) {
                QJsonArray &arr = specificUpdates[transport];
                arr.push_back(obj);
            }
        } else {
            data.push_back(obj);
        }
    }

    pendingPropertyUpdates.clear();
    QJsonObject message;
    message[KEY_TYPE] = TypePropertyUpdate;

    // data does not contain specific updates
    if (!data.isEmpty()) {
        setClientIsIdle(false);

        message[KEY_DATA] = data;
        broadcastMessage(message);
    }

    // send every property update which is not supposed to be broadcasted
    const QHash<QWebChannelAbstractTransport*, QJsonArray>::const_iterator suend = specificUpdates.constEnd();
    for (QHash<QWebChannelAbstractTransport*, QJsonArray>::const_iterator it = specificUpdates.constBegin(); it != suend; ++it) {
        message[KEY_DATA] = it.value();
        it.key()->sendMessage(message);
    }
}

QVariant QMetaObjectPublisher::invokeMethod(QObject *const object, const int methodIndex,
                                              const QJsonArray &args)
{
    const QMetaMethod &method = object->metaObject()->method(methodIndex);

    if (method.name() == QByteArrayLiteral("deleteLater")) {
        // invoke `deleteLater` on wrapped QObject indirectly
        deleteWrappedObject(object);
        return QJsonValue();
    } else if (!method.isValid()) {
        qWarning() << "Cannot invoke unknown method of index" << methodIndex << "on object" << object << '.';
        return QJsonValue();
    } else if (method.access() != QMetaMethod::Public) {
        qWarning() << "Cannot invoke non-public method" << method.name() << "on object" << object << '.';
        return QJsonValue();
    } else if (method.methodType() != QMetaMethod::Method && method.methodType() != QMetaMethod::Slot) {
        qWarning() << "Cannot invoke non-public method" << method.name() << "on object" << object << '.';
        return QJsonValue();
    } else if (args.size() > 10) {
        qWarning() << "Cannot invoke method" << method.name() << "on object" << object << "with more than 10 arguments, as that is not supported by QMetaMethod::invoke.";
        return QJsonValue();
    } else if (args.size() > method.parameterCount()) {
        qWarning() << "Ignoring additional arguments while invoking method" << method.name() << "on object" << object << ':'
                   << args.size() << "arguments given, but method only takes" << method.parameterCount() << '.';
    }

    // construct converter objects of QVariant to QGenericArgument
    VariantArgument arguments[10];
    for (int i = 0; i < qMin(args.size(), method.parameterCount()); ++i) {
        arguments[i].value = toVariant(args.at(i), method.parameterType(i));
    }
    // construct QGenericReturnArgument
    QVariant returnValue;
    if (method.returnType() == QMetaType::Void) {
        // Skip return for void methods (prevents runtime warnings inside Qt), and allows
        // QMetaMethod to invoke void-returning methods on QObjects in a different thread.
        method.invoke(object,
                  arguments[0], arguments[1], arguments[2], arguments[3], arguments[4],
                  arguments[5], arguments[6], arguments[7], arguments[8], arguments[9]);
    } else {
        QGenericReturnArgument returnArgument(method.typeName(), returnValue.data());

        // Only init variant with return type if its not a variant itself, which would
        // lead to nested variants which is not what we want.
        if (method.returnType() != QMetaType::QVariant)
            returnValue = QVariant(method.returnType(), 0);
        method.invoke(object, returnArgument,
                  arguments[0], arguments[1], arguments[2], arguments[3], arguments[4],
                  arguments[5], arguments[6], arguments[7], arguments[8], arguments[9]);
    }
    // now we can call the method
    return returnValue;
}

void QMetaObjectPublisher::setProperty(QObject *object, const int propertyIndex, const QJsonValue &value)
{
    QMetaProperty property = object->metaObject()->property(propertyIndex);
    if (!property.isValid()) {
        qWarning() << "Cannot set unknown property" << propertyIndex << "of object" << object;
    } else if (!property.write(object, toVariant(value, property.userType()))) {
        qWarning() << "Could not write value " << value << "to property" << property.name() << "of object" << object;
    }
}

void QMetaObjectPublisher::signalEmitted(const QObject *object, const int signalIndex, const QVariantList &arguments)
{
    if (!webChannel || webChannel->d_func()->transports.isEmpty()) {
        if (signalIndex == s_destroyedSignalIndex)
            objectDestroyed(object);
        return;
    }
    if (!signalToPropertyMap.value(object).contains(signalIndex)) {
        QJsonObject message;
        const QString &objectName = registeredObjectIds.value(object);
        Q_ASSERT(!objectName.isEmpty());
        message[KEY_OBJECT] = objectName;
        message[KEY_SIGNAL] = signalIndex;
        if (!arguments.isEmpty()) {
            message[KEY_ARGS] = wrapList(arguments, Q_NULLPTR, objectName);
        }
        message[KEY_TYPE] = TypeSignal;

        // if the object is wrapped, just send the response to clients which know this object
        if (wrappedObjects.contains(objectName)) {
            foreach (QWebChannelAbstractTransport *transport, wrappedObjects.value(objectName).transports) {
                transport->sendMessage(message);
            }
        } else {
            broadcastMessage(message);
        }

        if (signalIndex == s_destroyedSignalIndex) {
            objectDestroyed(object);
        }
    } else {
        pendingPropertyUpdates[object][signalIndex] = arguments;
        if (clientIsIdle && !blockUpdates && !timer.isActive()) {
            timer.start(PROPERTY_UPDATE_INTERVAL, this);
        }
    }
}

void QMetaObjectPublisher::objectDestroyed(const QObject *object)
{
    const QString &id = registeredObjectIds.take(object);
    Q_ASSERT(!id.isEmpty());
    bool removed = registeredObjects.remove(id)
            || wrappedObjects.remove(id);
    Q_ASSERT(removed);
    Q_UNUSED(removed);

    signalHandler.remove(object);
    signalToPropertyMap.remove(object);
    pendingPropertyUpdates.remove(object);
}

QObject *QMetaObjectPublisher::unwrapObject(const QString &objectId) const
{
    if (!objectId.isEmpty()) {
        ObjectInfo objectInfo = wrappedObjects.value(objectId);
        if (objectInfo.object && !objectInfo.classinfo.isEmpty())
            return objectInfo.object;
    }

    qWarning() << "No wrapped object" << objectId;
    return Q_NULLPTR;
}

QVariant QMetaObjectPublisher::toVariant(const QJsonValue &value, int targetType) const
{
    if (targetType == QMetaType::QJsonValue) {
        return QVariant::fromValue(value);
    } else if (targetType == QMetaType::QJsonArray) {
        if (!value.isArray())
            qWarning() << "Cannot not convert non-array argument" << value << "to QJsonArray.";
        return QVariant::fromValue(value.toArray());
    } else if (targetType == QMetaType::QJsonObject) {
        if (!value.isObject())
            qWarning() << "Cannot not convert non-object argument" << value << "to QJsonObject.";
        return QVariant::fromValue(value.toObject());
    } else if (QMetaType::typeFlags(targetType) & QMetaType::PointerToQObject) {
        QObject *unwrappedObject = unwrapObject(value.toObject()[KEY_ID].toString());
        if (unwrappedObject == Q_NULLPTR)
            qWarning() << "Cannot not convert non-object argument" << value << "to QObject*.";
        return QVariant::fromValue(unwrappedObject);
    }

    // this converts QJsonObjects to QVariantMaps, which is not desired when
    // we want to get a QJsonObject or QJsonValue (see above)
    QVariant variant = value.toVariant();
    if (targetType != QMetaType::QVariant && !variant.convert(targetType)) {
        qWarning() << "Could not convert argument" << value << "to target type" << QVariant::typeToName(targetType) << '.';
    }
    return variant;
}

void QMetaObjectPublisher::transportRemoved(QWebChannelAbstractTransport *transport)
{
    auto it = transportedWrappedObjects.find(transport);
    // It is not allowed to modify a container while iterating over it. So save
    // objects which should be removed and call objectDestroyed() on them later.
    QVector<QObject*> objectsForDeletion;
    while (it != transportedWrappedObjects.end() && it.key() == transport) {
        if (wrappedObjects.contains(it.value())) {
            QVector<QWebChannelAbstractTransport*> &transports = wrappedObjects[it.value()].transports;
            transports.removeOne(transport);
            if (transports.isEmpty())
                objectsForDeletion.append(wrappedObjects[it.value()].object);
        }

        it++;
    }

    transportedWrappedObjects.remove(transport);

    foreach (QObject *obj, objectsForDeletion)
        objectDestroyed(obj);
}

// NOTE: transport can be a nullptr
//       in such a case, we need to ensure that the property is registered to
//       the target transports of the parentObjectId
QJsonValue QMetaObjectPublisher::wrapResult(const QVariant &result, QWebChannelAbstractTransport *transport,
                                            const QString &parentObjectId)
{
    if (QObject *object = result.value<QObject *>()) {
        QString id = registeredObjectIds.value(object);

        QJsonObject classInfo;
        if (id.isEmpty()) {
            // neither registered, nor wrapped, do so now
            id = QUuid::createUuid().toString();
            // store ID before the call to classInfoForObject()
            // in case of self-contained objects it avoids
            // infinite loops
            registeredObjectIds[object] = id;

            classInfo = classInfoForObject(object, transport);

            ObjectInfo oi(object, classInfo);
            if (transport) {
                oi.transports.append(transport);
            } else {
                // use the transports from the parent object
                oi.transports = wrappedObjects.value(parentObjectId).transports;
                // or fallback to all transports if the parent is not wrapped
                if (oi.transports.isEmpty())
                    oi.transports = webChannel->d_func()->transports;
            }
            wrappedObjects.insert(id, oi);
            transportedWrappedObjects.insert(transport, id);

            initializePropertyUpdates(object, classInfo);
        } else if (wrappedObjects.contains(id)) {
            Q_ASSERT(object == wrappedObjects.value(id).object);
            // check if this transport is already assigned to the object
            if (transport && !wrappedObjects.value(id).transports.contains(transport))
                wrappedObjects[id].transports.append(transport);
            classInfo = wrappedObjects.value(id).classinfo;
        }

        QJsonObject objectInfo;
        objectInfo[KEY_QOBJECT] = true;
        objectInfo[KEY_ID] = id;
        if (!classInfo.isEmpty())
            objectInfo[KEY_DATA] = classInfo;

        return objectInfo;
#ifndef QT_NO_JSVALUE
    } else if (result.canConvert<QJSValue>()) {
        // Workaround for keeping QJSValues from QVariant.
        // Calling QJSValue::toVariant() converts JS-objects/arrays to QVariantMap/List
        // instead of stashing a QJSValue itself into a variant.
        // TODO: Improve QJSValue-QJsonValue conversion in Qt.
        return wrapResult(result.value<QJSValue>().toVariant(), transport, parentObjectId);
#endif
    } else if (result.canConvert<QVariantList>()) {
        // recurse and potentially wrap contents of the array
        return wrapList(result.toList(), transport);
    }

    return QJsonValue::fromVariant(result);
}

QJsonArray QMetaObjectPublisher::wrapList(const QVariantList &list, QWebChannelAbstractTransport *transport, const QString &parentObjectId)
{
    QJsonArray array;
    foreach (const QVariant &arg, list) {
        array.append(wrapResult(arg, transport, parentObjectId));
    }
    return array;
}

void QMetaObjectPublisher::deleteWrappedObject(QObject *object) const
{
    if (!wrappedObjects.contains(registeredObjectIds.value(object))) {
        qWarning() << "Not deleting non-wrapped object" << object;
        return;
    }
    object->deleteLater();
}

void QMetaObjectPublisher::broadcastMessage(const QJsonObject &message) const
{
    if (webChannel->d_func()->transports.isEmpty()) {
        qWarning("QWebChannel is not connected to any transports, cannot send message: %s", QJsonDocument(message).toJson().constData());
        return;
    }

    foreach (QWebChannelAbstractTransport *transport, webChannel->d_func()->transports) {
        transport->sendMessage(message);
    }
}

void QMetaObjectPublisher::handleMessage(const QJsonObject &message, QWebChannelAbstractTransport *transport)
{
    if (!webChannel->d_func()->transports.contains(transport)) {
        qWarning() << "Refusing to handle message of unknown transport:" << transport;
        return;
    }

    if (!message.contains(KEY_TYPE)) {
        qWarning("JSON message object is missing the type property: %s", QJsonDocument(message).toJson().constData());
        return;
    }

    const MessageType type = toType(message.value(KEY_TYPE));
    if (type == TypeIdle) {
        setClientIsIdle(true);
    } else if (type == TypeInit) {
        if (!message.contains(KEY_ID)) {
            qWarning("JSON message object is missing the id property: %s",
                      QJsonDocument(message).toJson().constData());
            return;
        }
        transport->sendMessage(createResponse(message.value(KEY_ID), initializeClient(transport)));
    } else if (type == TypeDebug) {
        static QTextStream out(stdout);
        out << "DEBUG: " << message.value(KEY_DATA).toString() << endl;
    } else if (message.contains(KEY_OBJECT)) {
        const QString &objectName = message.value(KEY_OBJECT).toString();
        QObject *object = registeredObjects.value(objectName);
        if (!object)
            object = wrappedObjects.value(objectName).object;

        if (!object) {
            qWarning() << "Unknown object encountered" << objectName;
            return;
        }

        if (type == TypeInvokeMethod) {
            if (!message.contains(KEY_ID)) {
                qWarning("JSON message object is missing the id property: %s",
                          QJsonDocument(message).toJson().constData());
                return;
            }

            transport->sendMessage(createResponse(message.value(KEY_ID),
                wrapResult(invokeMethod(object, message.value(KEY_METHOD).toInt(-1),
                             message.value(KEY_ARGS).toArray()), transport)));
        } else if (type == TypeConnectToSignal) {
            signalHandler.connectTo(object, message.value(KEY_SIGNAL).toInt(-1));
        } else if (type == TypeDisconnectFromSignal) {
            signalHandler.disconnectFrom(object, message.value(KEY_SIGNAL).toInt(-1));
        } else if (type == TypeSetProperty) {
            setProperty(object, message.value(KEY_PROPERTY).toInt(-1),
                        message.value(KEY_VALUE));
        }
    }
}

void QMetaObjectPublisher::setBlockUpdates(bool block)
{
    if (blockUpdates == block) {
        return;
    }
    blockUpdates = block;

    if (!blockUpdates) {
        sendPendingPropertyUpdates();
    } else if (timer.isActive()) {
        timer.stop();
    }

    emit blockUpdatesChanged(block);
}

void QMetaObjectPublisher::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == timer.timerId()) {
        sendPendingPropertyUpdates();
    } else {
        QObject::timerEvent(event);
    }
}

QT_END_NAMESPACE
