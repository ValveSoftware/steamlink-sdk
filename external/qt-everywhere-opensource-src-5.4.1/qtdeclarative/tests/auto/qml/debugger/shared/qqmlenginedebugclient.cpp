/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlenginedebugclient.h"

struct QmlObjectData {
    QUrl url;
    int lineNumber;
    int columnNumber;
    QString idString;
    QString objectName;
    QString objectType;
    int objectId;
    int contextId;
    int parentId;
};

QDataStream &operator>>(QDataStream &ds, QmlObjectData &data)
{
    ds >> data.url >> data.lineNumber >> data.columnNumber >> data.idString
       >> data.objectName >> data.objectType >> data.objectId >> data.contextId
       >> data.parentId;
    return ds;
}

struct QmlObjectProperty {
    enum Type { Unknown, Basic, Object, List, SignalProperty };
    Type type;
    QString name;
    QVariant value;
    QString valueTypeName;
    QString binding;
    bool hasNotifySignal;
};

QDataStream &operator>>(QDataStream &ds, QmlObjectProperty &data)
{
    int type;
    ds >> type >> data.name >> data.value >> data.valueTypeName
       >> data.binding >> data.hasNotifySignal;
    data.type = (QmlObjectProperty::Type)type;
    return ds;
}

QQmlEngineDebugClient::QQmlEngineDebugClient(
        QQmlDebugConnection *connection)
    : QQmlDebugClient(QLatin1String("QmlDebugger"), connection),
      m_nextId(0),
      m_valid(false),
      m_connection(connection)
{
}

quint32 QQmlEngineDebugClient::addWatch(
        const QmlDebugPropertyReference &property, bool *success)
{
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("WATCH_PROPERTY") << id << property.objectDebugId
           << property.name.toUtf8();
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::addWatch(
        const QmlDebugContextReference &, const QString &, bool *success)
{
    *success = false;
    qWarning("QQmlEngineDebugClient::addWatch(): Not implemented");
    return 0;
}

quint32 QQmlEngineDebugClient::addWatch(
        const QmlDebugObjectReference &object, const QString &expr,
        bool *success)
{
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("WATCH_EXPR_OBJECT") << id << object.debugId << expr;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::addWatch(
        const QmlDebugObjectReference &object, bool *success)
{
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("WATCH_OBJECT") << id << object.debugId;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::addWatch(
        const QmlDebugFileReference &,  bool *success)
{
    *success = false;
    qWarning("QQmlEngineDebugClient::addWatch(): Not implemented");
    return 0;
}

void QQmlEngineDebugClient::removeWatch(quint32 id, bool *success)
{
    *success = false;
    if (state() == QQmlDebugClient::Enabled) {
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("NO_WATCH") << id;
        sendMessage(message);
        *success = true;
    }
}

quint32 QQmlEngineDebugClient::queryAvailableEngines(bool *success)
{
    m_engines.clear();
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("LIST_ENGINES") << id;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::queryRootContexts(
        const QmlDebugEngineReference &engine, bool *success)
{
    m_rootContext = QmlDebugContextReference();
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled && engine.debugId != -1) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("LIST_OBJECTS") << id << engine.debugId;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::queryObject(
        const QmlDebugObjectReference &object, bool *success)
{
    m_object = QmlDebugObjectReference();
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled && object.debugId != -1) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("FETCH_OBJECT") << id << object.debugId << false <<
              true;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::queryObjectsForLocation(
        const QString &file, int lineNumber, int columnNumber, bool *success)
{
    m_objects.clear();
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("FETCH_OBJECTS_FOR_LOCATION") << id << file << lineNumber
           << columnNumber << false << true;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::queryObjectRecursive(
        const QmlDebugObjectReference &object, bool *success)
{
    m_object = QmlDebugObjectReference();
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled && object.debugId != -1) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("FETCH_OBJECT") << id << object.debugId << true <<
              true;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::queryObjectsForLocationRecursive(const QString &file,
        int lineNumber, int columnNumber, bool *success)
{
     m_objects.clear();
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("FETCH_OBJECTS_FOR_LOCATION") << id << file << lineNumber
           << columnNumber << true << true;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::queryExpressionResult(
        int objectDebugId, const QString &expr, bool *success)
{
    m_exprResult = QVariant();
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("EVAL_EXPRESSION") << id << objectDebugId << expr
           << engines()[0].debugId;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::queryExpressionResultBC(
        int objectDebugId, const QString &expr, bool *success)
{
    m_exprResult = QVariant();
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("EVAL_EXPRESSION") << id << objectDebugId << expr;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::setBindingForObject(
        int objectDebugId,
        const QString &propertyName,
        const QVariant &bindingExpression,
        bool isLiteralValue,
        QString source, int line,
        bool *success)
{
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled && objectDebugId != -1) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("SET_BINDING") << id << objectDebugId << propertyName
           << bindingExpression << isLiteralValue << source << line;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::resetBindingForObject(
        int objectDebugId,
        const QString &propertyName,
        bool *success)
{
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled && objectDebugId != -1) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("RESET_BINDING") << id << objectDebugId << propertyName;
        sendMessage(message);
        *success = true;
    }
    return id;
}

quint32 QQmlEngineDebugClient::setMethodBody(
        int objectDebugId, const QString &methodName,
        const QString &methodBody, bool *success)
{
    quint32 id = -1;
    *success = false;
    if (state() == QQmlDebugClient::Enabled && objectDebugId != -1) {
        id = getId();
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << QByteArray("SET_METHOD_BODY") << id << objectDebugId
           << methodName << methodBody;
        sendMessage(message);
        *success = true;
    }
    return id;
}

void QQmlEngineDebugClient::decode(QDataStream &ds,
                                   QmlDebugObjectReference &o,
                                   bool simple)
{
    QmlObjectData data;
    ds >> data;
    o.debugId = data.objectId;
    o.className = data.objectType;
    o.idString = data.idString;
    o.name = data.objectName;
    o.source.url = data.url;
    o.source.lineNumber = data.lineNumber;
    o.source.columnNumber = data.columnNumber;
    o.contextDebugId = data.contextId;

    if (simple)
        return;

    int childCount;
    bool recur;
    ds >> childCount >> recur;

    for (int ii = 0; ii < childCount; ++ii) {
        o.children.append(QmlDebugObjectReference());
        decode(ds, o.children.last(), !recur);
    }

    int propCount;
    ds >> propCount;

    for (int ii = 0; ii < propCount; ++ii) {
        QmlObjectProperty data;
        ds >> data;
        QmlDebugPropertyReference prop;
        prop.objectDebugId = o.debugId;
        prop.name = data.name;
        prop.binding = data.binding;
        prop.hasNotifySignal = data.hasNotifySignal;
        prop.valueTypeName = data.valueTypeName;
        switch (data.type) {
        case QmlObjectProperty::Basic:
        case QmlObjectProperty::List:
        case QmlObjectProperty::SignalProperty:
        {
            prop.value = data.value;
            break;
        }
        case QmlObjectProperty::Object:
        {
            QmlDebugObjectReference obj;
            obj.debugId = prop.value.toInt();
            prop.value = qVariantFromValue(obj);
            break;
        }
        case QmlObjectProperty::Unknown:
            break;
        }
        o.properties << prop;
    }
}

void QQmlEngineDebugClient::decode(QDataStream &ds,
                                   QList<QmlDebugObjectReference> &o,
                                   bool simple)
{
    int count;
    ds >> count;
    for (int i = 0; i < count; i++) {
        QmlDebugObjectReference obj;
        decode(ds, obj, simple);
        o << obj;
    }
}

void QQmlEngineDebugClient::decode(QDataStream &ds,
                                   QmlDebugContextReference &c)
{
    ds >> c.name >> c.debugId;

    int contextCount;
    ds >> contextCount;

    for (int ii = 0; ii < contextCount; ++ii) {
        c.contexts.append(QmlDebugContextReference());
        decode(ds, c.contexts.last());
    }

    int objectCount;
    ds >> objectCount;

    for (int ii = 0; ii < objectCount; ++ii) {
        QmlDebugObjectReference obj;
        decode(ds, obj, true);

        obj.contextDebugId = c.debugId;
        c.objects << obj;
    }
}

void QQmlEngineDebugClient::messageReceived(const QByteArray &data)
{
    m_valid = false;
    QDataStream ds(data);
    ds.setVersion(m_connection->dataStreamVersion());


    int queryId;
    QByteArray type;
    ds >> type >> queryId;

    //qDebug() << "QQmlEngineDebugPrivate::message()" << type;

    if (type == "LIST_ENGINES_R") {
        int count;
        ds >> count;

        m_engines.clear();
        for (int ii = 0; ii < count; ++ii) {
            QmlDebugEngineReference eng;
            ds >> eng.name;
            ds >> eng.debugId;
            m_engines << eng;
        }
    } else if (type == "LIST_OBJECTS_R") {
        if (!ds.atEnd())
            decode(ds, m_rootContext);

    } else if (type == "FETCH_OBJECT_R") {
        if (!ds.atEnd())
            decode(ds, m_object, false);

    } else if (type == "FETCH_OBJECTS_FOR_LOCATION_R") {
        if (!ds.atEnd())
            decode(ds, m_objects, false);

    } else if (type == "EVAL_EXPRESSION_R") {;
        ds >> m_exprResult;

    } else if (type == "WATCH_PROPERTY_R") {
        ds >> m_valid;

    } else if (type == "WATCH_OBJECT_R") {
        ds >> m_valid;

    } else if (type == "WATCH_EXPR_OBJECT_R") {
        ds >> m_valid;

    } else if (type == "UPDATE_WATCH") {
        int debugId;
        QByteArray name;
        QVariant value;
        ds >> debugId >> name >> value;
        emit valueChanged(name, value);
        return;

    } else if (type == "OBJECT_CREATED") {
        emit newObjects();
        return;
    } else if (type == "SET_BINDING_R") {
        ds >> m_valid;
    } else if (type == "RESET_BINDING_R") {
        ds >> m_valid;
    } else if (type == "SET_METHOD_BODY_R") {
        ds >> m_valid;
    } else if (type == "NO_WATCH_R") {
        ds >> m_valid;
    }
    emit result();
}

