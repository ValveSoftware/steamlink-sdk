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

#include "qv8engine_p.h"

#include "qv4sequenceobject_p.h"
#include "private/qjsengine_p.h"

#include <private/qqmlbuiltinfunctions_p.h>
#include <private/qqmllist_p.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlxmlhttprequest_p.h>
#include <private/qqmllocale_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qqmlmemoryprofiler_p.h>
#include <private/qqmlplatform_p.h>
#include <private/qjsvalue_p.h>
#include <private/qqmltypewrapper_p.h>
#include <private/qqmlcontextwrapper_p.h>
#include <private/qqmlvaluetypewrapper_p.h>
#include <private/qqmllistwrapper_p.h>
#include <private/qv4scopedvalue_p.h>

#include "qv4domerrors_p.h"
#include "qv4sqlerrors_p.h"

#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdatastream.h>
#include <private/qsimd_p.h>

#include <private/qv4value_p.h>
#include <private/qv4dateobject_p.h>
#include <private/qv4objectiterator_p.h>
#include <private/qv4qobjectwrapper_p.h>
#include <private/qv4mm_p.h>
#include <private/qv4objectproto_p.h>
#include <private/qv4globalobject_p.h>
#include <private/qv4regexpobject_p.h>
#include <private/qv4variantobject_p.h>
#include <private/qv4script_p.h>
#include <private/qv4include_p.h>
#include <private/qv4jsonobject_p.h>

Q_DECLARE_METATYPE(QList<int>)


// XXX TODO: Need to check all the global functions will also work in a worker script where the
// QQmlEngine is not available
QT_BEGIN_NAMESPACE

template <typename ReturnType>
ReturnType convertJSValueToVariantType(const QJSValue &value)
{
    return value.toVariant().value<ReturnType>();
}

static void saveJSValue(QDataStream &stream, const void *data)
{
    const QJSValue *jsv = reinterpret_cast<const QJSValue *>(data);
    quint32 isNullOrUndefined = 0;
    if (jsv->isNull())
        isNullOrUndefined |= 0x1;
    if (jsv->isUndefined())
        isNullOrUndefined |= 0x2;
    stream << isNullOrUndefined;
    if (!isNullOrUndefined)
        reinterpret_cast<const QJSValue*>(data)->toVariant().save(stream);
}

static void restoreJSValue(QDataStream &stream, void *data)
{
    QJSValue *jsv = reinterpret_cast<QJSValue*>(data);

    quint32 isNullOrUndefined;
    stream >> isNullOrUndefined;

    if (isNullOrUndefined & 0x1) {
        *jsv = QJSValue(QJSValue::NullValue);
    } else if (isNullOrUndefined & 0x2) {
        *jsv = QJSValue();
    } else {
        QVariant v;
        v.load(stream);
        QJSValuePrivate::setVariant(jsv, v);
    }
}

QV8Engine::QV8Engine(QJSEngine* qq)
    : q(qq)
    , m_engine(0)
    , m_xmlHttpRequestData(0)
    , m_listModelData(0)
{
#ifdef Q_PROCESSOR_X86_32
    if (!qCpuHasFeature(SSE2)) {
        qFatal("This program requires an X86 processor that supports SSE2 extension, at least a Pentium 4 or newer");
    }
#endif

    QML_MEMORY_SCOPE_STRING("QV8Engine::QV8Engine");
    qMetaTypeId<QJSValue>();
    qMetaTypeId<QList<int> >();

    if (!QMetaType::hasRegisteredConverterFunction<QJSValue, QVariantMap>())
        QMetaType::registerConverter<QJSValue, QVariantMap>(convertJSValueToVariantType<QVariantMap>);
    if (!QMetaType::hasRegisteredConverterFunction<QJSValue, QVariantList>())
        QMetaType::registerConverter<QJSValue, QVariantList>(convertJSValueToVariantType<QVariantList>);
    if (!QMetaType::hasRegisteredConverterFunction<QJSValue, QStringList>())
        QMetaType::registerConverter<QJSValue, QStringList>(convertJSValueToVariantType<QStringList>);
    QMetaType::registerStreamOperators(qMetaTypeId<QJSValue>(), saveJSValue, restoreJSValue);

    m_v4Engine = new QV4::ExecutionEngine;
    m_v4Engine->v8Engine = this;
    m_delayedCallQueue.init(m_v4Engine);

    QV4::QObjectWrapper::initializeBindings(m_v4Engine);
}

QV8Engine::~QV8Engine()
{
    qDeleteAll(m_extensionData);
    m_extensionData.clear();

#if !defined(QT_NO_XMLSTREAMREADER) && QT_CONFIG(qml_network)
    qt_rem_qmlxmlhttprequest(m_v4Engine, m_xmlHttpRequestData);
    m_xmlHttpRequestData = 0;
#endif

    delete m_listModelData;
    m_listModelData = 0;

    delete m_v4Engine;
}

#if QT_CONFIG(qml_network)
QNetworkAccessManager *QV8Engine::networkAccessManager()
{
    return QQmlEnginePrivate::get(m_engine)->getNetworkAccessManager();
}
#endif

const QSet<QString> &QV8Engine::illegalNames() const
{
    return m_illegalNames;
}

void QV8Engine::initializeGlobal()
{
    QV4::Scope scope(m_v4Engine);
    QV4::GlobalExtensions::init(m_v4Engine->globalObject, QJSEngine::AllExtensions);

    QV4::ScopedObject qt(scope, m_v4Engine->memoryManager->allocObject<QV4::QtObject>(m_engine));
    m_v4Engine->globalObject->defineDefaultProperty(QStringLiteral("Qt"), qt);

    QQmlLocale::registerStringLocaleCompare(m_v4Engine);
    QQmlDateExtension::registerExtension(m_v4Engine);
    QQmlNumberExtension::registerExtension(m_v4Engine);

#if !defined(QT_NO_XMLSTREAMREADER) && QT_CONFIG(qml_network)
    qt_add_domexceptions(m_v4Engine);
    m_xmlHttpRequestData = qt_add_qmlxmlhttprequest(m_v4Engine);
#endif

    qt_add_sqlexceptions(m_v4Engine);

    {
        for (uint i = 0; i < m_v4Engine->globalObject->internalClass()->size; ++i) {
            if (m_v4Engine->globalObject->internalClass()->nameMap.at(i))
                m_illegalNames.insert(m_v4Engine->globalObject->internalClass()->nameMap.at(i)->string);
        }
    }
}

static void freeze_recursive(QV4::ExecutionEngine *v4, QV4::Object *object)
{
    if (object->as<QV4::QObjectWrapper>())
        return;

    QV4::Scope scope(v4);

    bool instanceOfObject = false;
    QV4::ScopedObject p(scope, object->prototype());
    while (p) {
        if (p->d() == v4->objectPrototype()->d()) {
            instanceOfObject = true;
            break;
        }
        p = p->prototype();
    }
    if (!instanceOfObject)
        return;

    QV4::InternalClass *frozen = object->internalClass()->propertiesFrozen();
    if (object->internalClass() == frozen)
        return;
    object->setInternalClass(frozen);

    QV4::ScopedObject o(scope);
    for (uint i = 0; i < frozen->size; ++i) {
        if (!frozen->nameMap.at(i))
            continue;
        o = *object->propertyData(i);
        if (o)
            freeze_recursive(v4, o);
    }
}

void QV8Engine::freezeObject(const QV4::Value &value)
{
    QV4::Scope scope(m_v4Engine);
    QV4::ScopedObject o(scope, value);
    freeze_recursive(m_v4Engine, o);
}

struct QV8EngineRegistrationData
{
    QV8EngineRegistrationData() : extensionCount(0) {}

    QMutex mutex;
    int extensionCount;
};
Q_GLOBAL_STATIC(QV8EngineRegistrationData, registrationData);

QMutex *QV8Engine::registrationMutex()
{
    return &registrationData()->mutex;
}

int QV8Engine::registerExtension()
{
    return registrationData()->extensionCount++;
}

void QV8Engine::setExtensionData(int index, Deletable *data)
{
    if (m_extensionData.count() <= index)
        m_extensionData.resize(index + 1);

    if (m_extensionData.at(index))
        delete m_extensionData.at(index);

    m_extensionData[index] = data;
}

void QV8Engine::initQmlGlobalObject()
{
    initializeGlobal();
    freezeObject(*m_v4Engine->globalObject);
}

void QV8Engine::setEngine(QQmlEngine *engine)
{
    m_engine = engine;
    initQmlGlobalObject();
}

QV4::ReturnedValue QV8Engine::global()
{
    return m_v4Engine->globalObject->asReturnedValue();
}

void QV8Engine::startTimer(const QString &timerName)
{
    if (!m_time.isValid())
        m_time.start();
    m_startedTimers[timerName] = m_time.elapsed();
}

qint64 QV8Engine::stopTimer(const QString &timerName, bool *wasRunning)
{
    if (!m_startedTimers.contains(timerName)) {
        *wasRunning = false;
        return 0;
    }
    *wasRunning = true;
    qint64 startedAt = m_startedTimers.take(timerName);
    return m_time.elapsed() - startedAt;
}

int QV8Engine::consoleCountHelper(const QString &file, quint16 line, quint16 column)
{
    const QString key = file + QString::number(line) + QString::number(column);
    int number = m_consoleCount.value(key, 0);
    number++;
    m_consoleCount.insert(key, number);
    return number;
}

QT_END_NAMESPACE

