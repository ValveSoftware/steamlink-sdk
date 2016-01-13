/****************************************************************************
**
** Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
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

#include "qmetaobjectpublisher_p.h"
#include "qwebchannel.h"
#include "qwebchannel_p.h"
#include "qwebchannelabstracttransport.h"

#include <QEvent>
#include <QJsonDocument>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
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

/// TODO: what is the proper value here?
const int PROPERTY_UPDATE_INTERVAL = 50;
}

QMetaObjectPublisher::QMetaObjectPublisher(QWebChannel *webChannel)
    : QObject(webChannel)
    , webChannel(webChannel)
    , signalHandler(this)
    , clientIsIdle(false)
    , blockUpdates(false)
    , pendingInit(false)
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
        initializePropertyUpdates(object, classInfoForObject(object));
    }
}

QJsonObject QMetaObjectPublisher::classInfoForObject(const QObject *object)
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
        propertyInfo.append(wrapResult(prop.read(object)));
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

void QMetaObjectPublisher::initializeClients()
{
    if (!webChannel) {
        return;
    }

    QJsonObject objectInfos;
    {
        const QHash<QString, QObject *>::const_iterator end = registeredObjects.constEnd();
        for (QHash<QString, QObject *>::const_iterator it = registeredObjects.constBegin(); it != end; ++it) {
            const QJsonObject &info = classInfoForObject(it.value());
            if (!propertyUpdatesInitialized) {
                initializePropertyUpdates(it.value(), info);
            }
            objectInfos[it.key()] = info;
        }
    }
    QJsonObject message;
    message[KEY_TYPE] = TypeInit;
    message[KEY_DATA] = objectInfos;
    broadcastMessage(message);
    propertyUpdatesInitialized = true;
    pendingInit = false;
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

    // convert pending property updates to JSON data
    const PendingPropertyUpdates::const_iterator end = pendingPropertyUpdates.constEnd();
    for (PendingPropertyUpdates::const_iterator it = pendingPropertyUpdates.constBegin(); it != end; ++it) {
        const QObject *object = it.key();
        const QMetaObject *const metaObject = object->metaObject();
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
                properties[QString::number(propertyIndex)] = wrapResult(property.read(object));
            }
            sigs[QString::number(sigIt.key())] = QJsonArray::fromVariantList(sigIt.value());
        }
        QJsonObject obj;
        obj[KEY_OBJECT] = registeredObjectIds.value(object);
        obj[KEY_SIGNALS] = sigs;
        obj[KEY_PROPERTIES] = properties;
        data.push_back(obj);
    }

    pendingPropertyUpdates.clear();
    QJsonObject message;
    message[KEY_TYPE] = TypePropertyUpdate;
    message[KEY_DATA] = data;
    setClientIsIdle(false);
    broadcastMessage(message);
}

QJsonValue QMetaObjectPublisher::invokeMethod(QObject *const object, const int methodIndex,
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
        QVariant arg = args.at(i).toVariant();
        if (method.parameterType(i) != QMetaType::QVariant && !arg.convert(method.parameterType(i))) {
            qWarning() << "Could not convert argument" << args.at(i) << "to target type" << method.parameterTypes().at(i) << '.';
        }
        arguments[i].value = arg;
    }

    // construct QGenericReturnArgument
    QVariant returnValue;
    if (method.returnType() != qMetaTypeId<QVariant>() && method.returnType() != qMetaTypeId<void>()) {
        // Only init variant with return type if its not a variant itself, which would
        // lead to nested variants which is not what we want.
        // Also, skip void-return types for obvious reasons (and to prevent a runtime warning inside Qt).
        returnValue = QVariant(method.returnType(), 0);
    }
    QGenericReturnArgument returnArgument(method.typeName(), returnValue.data());

    // now we can call the method
    method.invoke(object, returnArgument,
                  arguments[0], arguments[1], arguments[2], arguments[3], arguments[4],
                  arguments[5], arguments[6], arguments[7], arguments[8], arguments[9]);

    return wrapResult(returnValue);
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
            message[KEY_ARGS] = wrapList(arguments);
        }
        message[KEY_TYPE] = TypeSignal;
        broadcastMessage(message);

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
    bool removed = registeredObjects.remove(id);
    Q_ASSERT(removed);
    Q_UNUSED(removed);

    signalHandler.remove(object);
    signalToPropertyMap.remove(object);
    pendingPropertyUpdates.remove(object);
    wrappedObjects.remove(object);
}

QJsonValue QMetaObjectPublisher::wrapResult(const QVariant &result)
{
    if (QObject *object = result.value<QObject *>()) {
        QJsonObject objectInfo;
        objectInfo[KEY_QOBJECT] = true;
        QString id = registeredObjectIds.value(object);
        if (id.isEmpty()) {
            // neither registered, nor wrapped, do so now
            id = QUuid::createUuid().toString();

            registeredObjectIds[object] = id;
            registeredObjects[id] = object;

            QJsonObject info = classInfoForObject(object);
            wrappedObjects[object] = info;
            objectInfo[KEY_DATA] = info;
            if (propertyUpdatesInitialized) {
                // if other objects are initialized already, do the same here
                initializePropertyUpdates(object, info);
            }
        } else if (wrappedObjects.contains(object)) {
            // if this object was wrapped, send the full class info
            // this is required for proper multi-client support
            objectInfo[KEY_DATA] = wrappedObjects.value(object);
        }

        objectInfo[KEY_ID] = id;
        return objectInfo;
    } else if (result.canConvert<QVariantList>()) {
        // recurse and potentially wrap contents of the array
        return wrapList(result.toList());
    }

    // no need to wrap this
    return QJsonValue::fromVariant(result);
}

QJsonArray QMetaObjectPublisher::wrapList(const QVariantList &list)
{
    QJsonArray array;
    foreach (const QVariant &arg, list) {
        array.append(wrapResult(arg));
    }
    return array;
}

void QMetaObjectPublisher::deleteWrappedObject(QObject *object) const
{
    if (!wrappedObjects.contains(object)) {
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
        if (!blockUpdates) {
            initializeClients();
        } else {
            pendingInit = true;
        }
    } else if (type == TypeDebug) {
        static QTextStream out(stdout);
        out << "DEBUG: " << message.value(KEY_DATA).toString() << endl;
    } else if (message.contains(KEY_OBJECT)) {
        const QString &objectName = message.value(KEY_OBJECT).toString();
        QObject *object = registeredObjects.value(objectName);
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
            QJsonObject response;
            response[KEY_TYPE] = TypeResponse;
            response[KEY_ID] = message.value(KEY_ID);
            response[KEY_DATA] = invokeMethod(object, message.value(KEY_METHOD).toInt(-1), message.value(KEY_ARGS).toArray());
            transport->sendMessage(response);
        } else if (type == TypeConnectToSignal) {
            signalHandler.connectTo(object, message.value(KEY_SIGNAL).toInt(-1));
        } else if (type == TypeDisconnectFromSignal) {
            signalHandler.disconnectFrom(object, message.value(KEY_SIGNAL).toInt(-1));
        } else if (type == TypeSetProperty) {
            const int propertyIdx = message.value(KEY_PROPERTY).toInt(-1);
            QMetaProperty property = object->metaObject()->property(propertyIdx);
            if (!property.isValid()) {
                qWarning() << "Cannot set unknown property" << message.value(KEY_PROPERTY) << "of object" << objectName;
            } else if (!object->metaObject()->property(propertyIdx).write(object, message.value(KEY_VALUE).toVariant())) {
                qWarning() << "Could not write value " << message.value(KEY_VALUE)
                           << "to property" << property.name() << "of object" << objectName;
            }
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
        if (pendingInit) {
            initializeClients();
        } else {
            sendPendingPropertyUpdates();
        }
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
