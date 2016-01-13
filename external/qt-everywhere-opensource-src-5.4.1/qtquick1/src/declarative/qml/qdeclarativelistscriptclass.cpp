/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "private/qdeclarativelistscriptclass_p.h"

#include "private/qdeclarativeengine_p.h"
#include "private/qdeclarativeguard_p.h"
#include "private/qdeclarativelist_p.h"

QT_BEGIN_NAMESPACE

struct ListData : public QScriptDeclarativeClass::Object {
    QDeclarativeGuard<QObject> object;
    QDeclarativeListProperty<QObject> property;
    int propertyType;
};

QDeclarativeListScriptClass::QDeclarativeListScriptClass(QDeclarativeEngine *e)
: QScriptDeclarativeClass(QDeclarativeEnginePrivate::getScriptEngine(e)), engine(e)
{
    QScriptEngine *scriptEngine = QDeclarativeEnginePrivate::getScriptEngine(engine);
    Q_UNUSED(scriptEngine);

    m_lengthId = createPersistentIdentifier(QLatin1String("length"));
}

QDeclarativeListScriptClass::~QDeclarativeListScriptClass()
{
}

QScriptValue QDeclarativeListScriptClass::newList(QObject *object, int propId, int propType)
{
    QScriptEngine *scriptEngine = QDeclarativeEnginePrivate::getScriptEngine(engine);

    if (!object || propId == -1)
        return scriptEngine->nullValue();

    ListData *data = new ListData;
    data->object = object;
    data->propertyType = propType;
    void *args[] = { &data->property, 0 };
    QMetaObject::metacall(object, QMetaObject::ReadProperty, propId, args);

    return newObject(scriptEngine, this, data);
}

QScriptValue QDeclarativeListScriptClass::newList(const QDeclarativeListProperty<QObject> &prop, int propType)
{
    QScriptEngine *scriptEngine = QDeclarativeEnginePrivate::getScriptEngine(engine);

    ListData *data = new ListData;
    data->object = prop.object;
    data->property = prop;
    data->propertyType = propType;

    return newObject(scriptEngine, this, data);
}

QScriptClass::QueryFlags
QDeclarativeListScriptClass::queryProperty(Object *object, const Identifier &name,
                                  QScriptClass::QueryFlags flags)
{
    Q_UNUSED(object);
    Q_UNUSED(flags);
    if (name == m_lengthId.identifier)
        return QScriptClass::HandlesReadAccess;

    bool ok = false;
    quint32 idx = toArrayIndex(name, &ok);

    if (ok) {
        lastIndex = idx;
        return QScriptClass::HandlesReadAccess;
    } else {
        return 0;
    }
}

QDeclarativeListScriptClass::Value QDeclarativeListScriptClass::property(Object *obj, const Identifier &name)
{
    QScriptEngine *scriptEngine = QDeclarativeEnginePrivate::getScriptEngine(engine);
    QDeclarativeEnginePrivate *enginePriv = QDeclarativeEnginePrivate::get(engine);

    ListData *data = (ListData *)obj;
    if (!data->object)
        return Value();

    quint32 count = data->property.count?data->property.count(&data->property):0;

    if (name == m_lengthId.identifier)
        return Value(scriptEngine, count);
    else if (lastIndex < count && data->property.at)
        return Value(scriptEngine, enginePriv->objectClass->newQObject(data->property.at(&data->property, lastIndex)));
    else
        return Value();
}

QVariant QDeclarativeListScriptClass::toVariant(Object *obj, bool *ok)
{
    ListData *data = (ListData *)obj;

    if (!data->object) {
        if (ok) *ok = false;
        return QVariant();
    }

    return QVariant::fromValue(QDeclarativeListReferencePrivate::init(data->property, data->propertyType, engine));
}

QT_END_NAMESPACE

