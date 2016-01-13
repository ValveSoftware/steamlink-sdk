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
#include <private/qsimd_p.h>

#include <private/qv4value_inl_p.h>
#include <private/qv4dateobject_p.h>
#include <private/qv4objectiterator_p.h>
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
    QJSValuePrivate *d = QJSValuePrivate::get(*jsv);

    quint32 isNullOrUndefined;
    stream >> isNullOrUndefined;
    if (isNullOrUndefined & 0x1) {
        d->value = QV4::Primitive::nullValue().asReturnedValue();
    } else if (isNullOrUndefined & 0x2) {
        d->value = QV4::Primitive::undefinedValue().asReturnedValue();
    } else {
        d->value = QV4::Primitive::emptyValue().asReturnedValue();
        d->unboundData.load(stream);
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

    QV4::QObjectWrapper::initializeBindings(m_v4Engine);
}

QV8Engine::~QV8Engine()
{
    for (int ii = 0; ii < m_extensionData.count(); ++ii)
        delete m_extensionData[ii];
    m_extensionData.clear();

    qt_rem_qmlxmlhttprequest(this, m_xmlHttpRequestData);
    m_xmlHttpRequestData = 0;
    delete m_listModelData;
    m_listModelData = 0;

    delete m_v4Engine;
}

QVariant QV8Engine::toVariant(const QV4::ValueRef value, int typeHint, bool createJSValueForObjects, V8ObjectSet *visitedObjects)
{
    Q_ASSERT (!value->isEmpty());
    QV4::Scope scope(m_v4Engine);

    if (QV4::VariantObject *v = value->as<QV4::VariantObject>())
        return v->d()->data;

    if (typeHint == QVariant::Bool)
        return QVariant(value->toBoolean());

    if (typeHint == QMetaType::QJsonValue)
        return QVariant::fromValue(QV4::JsonObject::toJsonValue(value));

    if (typeHint == qMetaTypeId<QJSValue>())
        return QVariant::fromValue(QJSValue(new QJSValuePrivate(m_v4Engine, value)));

    if (value->asObject()) {
        QV4::ScopedObject object(scope, value);
        if (typeHint == QMetaType::QJsonObject
                   && !value->asArrayObject() && !value->asFunctionObject()) {
            return QVariant::fromValue(QV4::JsonObject::toJsonObject(object));
        } else if (QV4::QObjectWrapper *wrapper = object->as<QV4::QObjectWrapper>()) {
            return qVariantFromValue<QObject *>(wrapper->object());
        } else if (object->as<QV4::QmlContextWrapper>()) {
            return QVariant();
        } else if (QV4::QmlTypeWrapper *w = object->as<QV4::QmlTypeWrapper>()) {
            return w->toVariant();
        } else if (QV4::QmlValueTypeWrapper *v = object->as<QV4::QmlValueTypeWrapper>()) {
            return v->toVariant();
        } else if (QV4::QmlListWrapper *l = object->as<QV4::QmlListWrapper>()) {
            return l->toVariant();
        } else if (object->isListType())
            return QV4::SequencePrototype::toVariant(object);
    }

    if (value->asArrayObject()) {
        QV4::ScopedArrayObject a(scope, value);
        if (typeHint == qMetaTypeId<QList<QObject *> >()) {
            QList<QObject *> list;
            uint32_t length = a->getLength();
            QV4::Scoped<QV4::QObjectWrapper> qobjectWrapper(scope);
            for (uint32_t ii = 0; ii < length; ++ii) {
                qobjectWrapper = a->getIndexed(ii);
                if (!!qobjectWrapper) {
                    list << qobjectWrapper->object();
                } else {
                    list << 0;
                }
            }

            return qVariantFromValue<QList<QObject*> >(list);
        } else if (typeHint == QMetaType::QJsonArray) {
            return QVariant::fromValue(QV4::JsonObject::toJsonArray(a));
        }

        bool succeeded = false;
        QVariant retn = QV4::SequencePrototype::toVariant(value, typeHint, &succeeded);
        if (succeeded)
            return retn;
    }

    if (value->isUndefined())
        return QVariant();
    if (value->isNull())
        return QVariant(QMetaType::VoidStar, (void *)0);
    if (value->isBoolean())
        return value->booleanValue();
    if (value->isInteger())
        return value->integerValue();
    if (value->isNumber())
        return value->asDouble();
    if (value->isString())
        return value->stringValue()->toQString();
    if (QQmlLocaleData *ld = value->as<QQmlLocaleData>())
        return ld->d()->locale;
    if (QV4::DateObject *d = value->asDateObject())
        return d->toQDateTime();
    // NOTE: since we convert QTime to JS Date, round trip will change the variant type (to QDateTime)!

    QV4::ScopedObject o(scope, value);
    Q_ASSERT(o);

    if (QV4::RegExpObject *re = o->as<QV4::RegExpObject>())
        return re->toQRegExp();

    if (createJSValueForObjects)
        return QVariant::fromValue(QJSValue(new QJSValuePrivate(o->asReturnedValue())));

    return objectToVariant(o, visitedObjects);
}

QVariant QV8Engine::objectToVariant(QV4::Object *o, V8ObjectSet *visitedObjects)
{
    Q_ASSERT(o);

    V8ObjectSet recursionGuardSet;
    if (!visitedObjects) {
        visitedObjects = &recursionGuardSet;
    } else if (visitedObjects->contains(o)) {
        // Avoid recursion.
        // For compatibility with QVariant{List,Map} conversion, we return an
        // empty object (and no error is thrown).
        if (o->asArrayObject())
            return QVariantList();
        return QVariantMap();
    }
    visitedObjects->insert(o);

    QVariant result;

    if (o->asArrayObject()) {
        QV4::Scope scope(m_v4Engine);
        QV4::ScopedArrayObject a(scope, o->asReturnedValue());
        QV4::ScopedValue v(scope);
        QVariantList list;

        int length = a->getLength();
        for (int ii = 0; ii < length; ++ii) {
            v = a->getIndexed(ii);
            list << toVariant(v, -1, /*createJSValueForObjects*/false, visitedObjects);
        }

        result = list;
    } else if (!o->asFunctionObject()) {
        QVariantMap map;
        QV4::Scope scope(m_v4Engine);
        QV4::ObjectIterator it(scope, o, QV4::ObjectIterator::EnumerableOnly);
        QV4::ScopedValue name(scope);
        QV4::ScopedValue val(scope);
        while (1) {
            name = it.nextPropertyNameAsString(val);
            if (name->isNull())
                break;

            QString key = name->toQStringNoThrow();
            map.insert(key, toVariant(val, /*type hint*/-1, /*createJSValueForObjects*/false, visitedObjects));
        }

        result = map;
    }

    visitedObjects->remove(o);
    return result;
}

static QV4::ReturnedValue arrayFromStringList(QV8Engine *engine, const QStringList &list)
{
    QV4::ExecutionEngine *e = QV8Engine::getV4(engine);
    QV4::Scope scope(e);
    QV4::Scoped<QV4::ArrayObject> a(scope, e->newArrayObject());
    int len = list.count();
    a->arrayReserve(len);
    QV4::ScopedValue v(scope);
    for (int ii = 0; ii < len; ++ii)
        a->arrayPut(ii, (v = QV4::Encode(e->newString(list.at(ii)))));

    a->setArrayLengthUnchecked(len);
    return a.asReturnedValue();
}

static QV4::ReturnedValue arrayFromVariantList(QV8Engine *engine, const QVariantList &list)
{
    QV4::ExecutionEngine *e = QV8Engine::getV4(engine);
    QV4::Scope scope(e);
    QV4::Scoped<QV4::ArrayObject> a(scope, e->newArrayObject());
    int len = list.count();
    a->arrayReserve(len);
    QV4::ScopedValue v(scope);
    for (int ii = 0; ii < len; ++ii)
        a->arrayPut(ii, (v = engine->fromVariant(list.at(ii))));

    a->setArrayLengthUnchecked(len);
    return a.asReturnedValue();
}

static QV4::ReturnedValue objectFromVariantMap(QV8Engine *engine, const QVariantMap &map)
{
    QV4::ExecutionEngine *e = QV8Engine::getV4(engine);
    QV4::Scope scope(e);
    QV4::ScopedObject o(scope, e->newObject());
    QV4::ScopedString s(scope);
    QV4::ScopedValue v(scope);
    for (QVariantMap::ConstIterator iter = map.begin(); iter != map.end(); ++iter) {
        s = e->newString(iter.key());
        uint idx = s->asArrayIndex();
        if (idx > 16 && (!o->arrayData() || idx > o->arrayData()->length() * 2))
            o->initSparseArray();
        o->put(s.getPointer(), (v = engine->fromVariant(iter.value())));
    }
    return o.asReturnedValue();
}

Q_CORE_EXPORT QString qt_regexp_toCanonical(const QString &, QRegExp::PatternSyntax);

QV4::ReturnedValue QV8Engine::fromVariant(const QVariant &variant)
{
    int type = variant.userType();
    const void *ptr = variant.constData();

    if (type < QMetaType::User) {
        switch (QMetaType::Type(type)) {
            case QMetaType::UnknownType:
            case QMetaType::Void:
                return QV4::Encode::undefined();
            case QMetaType::VoidStar:
                return QV4::Encode::null();
            case QMetaType::Bool:
                return QV4::Encode(*reinterpret_cast<const bool*>(ptr));
            case QMetaType::Int:
                return QV4::Encode(*reinterpret_cast<const int*>(ptr));
            case QMetaType::UInt:
                return QV4::Encode(*reinterpret_cast<const uint*>(ptr));
            case QMetaType::LongLong:
                return QV4::Encode((double)*reinterpret_cast<const qlonglong*>(ptr));
            case QMetaType::ULongLong:
                return QV4::Encode((double)*reinterpret_cast<const qulonglong*>(ptr));
            case QMetaType::Double:
                return QV4::Encode(*reinterpret_cast<const double*>(ptr));
            case QMetaType::QString:
                return m_v4Engine->currentContext()->d()->engine->newString(*reinterpret_cast<const QString*>(ptr))->asReturnedValue();
            case QMetaType::Float:
                return QV4::Encode(*reinterpret_cast<const float*>(ptr));
            case QMetaType::Short:
                return QV4::Encode((int)*reinterpret_cast<const short*>(ptr));
            case QMetaType::UShort:
                return QV4::Encode((int)*reinterpret_cast<const unsigned short*>(ptr));
            case QMetaType::Char:
                return QV4::Encode((int)*reinterpret_cast<const char*>(ptr));
            case QMetaType::UChar:
                return QV4::Encode((int)*reinterpret_cast<const unsigned char*>(ptr));
            case QMetaType::QChar:
                return QV4::Encode((int)(*reinterpret_cast<const QChar*>(ptr)).unicode());
            case QMetaType::QDateTime:
                return QV4::Encode(m_v4Engine->newDateObject(*reinterpret_cast<const QDateTime *>(ptr)));
            case QMetaType::QDate:
                return QV4::Encode(m_v4Engine->newDateObject(QDateTime(*reinterpret_cast<const QDate *>(ptr))));
            case QMetaType::QTime:
            return QV4::Encode(m_v4Engine->newDateObject(QDateTime(QDate(1970,1,1), *reinterpret_cast<const QTime *>(ptr))));
            case QMetaType::QRegExp:
                return QV4::Encode(m_v4Engine->newRegExpObject(*reinterpret_cast<const QRegExp *>(ptr)));
            case QMetaType::QObjectStar:
                return QV4::QObjectWrapper::wrap(m_v4Engine, *reinterpret_cast<QObject* const *>(ptr));
            case QMetaType::QStringList:
                {
                bool succeeded = false;
                QV4::Scope scope(m_v4Engine);
                QV4::ScopedValue retn(scope, QV4::SequencePrototype::fromVariant(m_v4Engine, variant, &succeeded));
                if (succeeded)
                    return retn.asReturnedValue();
                return arrayFromStringList(this, *reinterpret_cast<const QStringList *>(ptr));
                }
            case QMetaType::QVariantList:
                return arrayFromVariantList(this, *reinterpret_cast<const QVariantList *>(ptr));
            case QMetaType::QVariantMap:
                return objectFromVariantMap(this, *reinterpret_cast<const QVariantMap *>(ptr));
            case QMetaType::QJsonValue:
                return QV4::JsonObject::fromJsonValue(m_v4Engine, *reinterpret_cast<const QJsonValue *>(ptr));
            case QMetaType::QJsonObject:
                return QV4::JsonObject::fromJsonObject(m_v4Engine, *reinterpret_cast<const QJsonObject *>(ptr));
            case QMetaType::QJsonArray:
                return QV4::JsonObject::fromJsonArray(m_v4Engine, *reinterpret_cast<const QJsonArray *>(ptr));
            case QMetaType::QLocale:
                return QQmlLocale::wrap(this, *reinterpret_cast<const QLocale*>(ptr));
            default:
                break;
        }

        if (QQmlValueType *vt = QQmlValueTypeFactory::valueType(type))
            return QV4::QmlValueTypeWrapper::create(this, variant, vt);
    } else {
        QV4::Scope scope(m_v4Engine);
        if (type == qMetaTypeId<QQmlListReference>()) {
            typedef QQmlListReferencePrivate QDLRP;
            QDLRP *p = QDLRP::get((QQmlListReference*)ptr);
            if (p->object) {
                return QV4::QmlListWrapper::create(this, p->property, p->propertyType);
            } else {
                return QV4::Encode::null();
            }
        } else if (type == qMetaTypeId<QJSValue>()) {
            const QJSValue *value = reinterpret_cast<const QJSValue *>(ptr);
            QJSValuePrivate *valuep = QJSValuePrivate::get(*value);
            return valuep->getValue(m_v4Engine);
        } else if (type == qMetaTypeId<QList<QObject *> >()) {
            // XXX Can this be made more by using Array as a prototype and implementing
            // directly against QList<QObject*>?
            const QList<QObject *> &list = *(QList<QObject *>*)ptr;
            QV4::Scoped<QV4::ArrayObject> a(scope, m_v4Engine->newArrayObject());
            a->arrayReserve(list.count());
            QV4::ScopedValue v(scope);
            for (int ii = 0; ii < list.count(); ++ii)
                a->arrayPut(ii, (v = QV4::QObjectWrapper::wrap(m_v4Engine, list.at(ii))));
            a->setArrayLengthUnchecked(list.count());
            return a.asReturnedValue();
        } else if (QMetaType::typeFlags(type) & QMetaType::PointerToQObject) {
            return QV4::QObjectWrapper::wrap(m_v4Engine, *reinterpret_cast<QObject* const *>(ptr));
        }

        bool objOk;
        QObject *obj = QQmlMetaType::toQObject(variant, &objOk);
        if (objOk)
            return QV4::QObjectWrapper::wrap(m_v4Engine, obj);

        bool succeeded = false;
        QV4::ScopedValue retn(scope, QV4::SequencePrototype::fromVariant(m_v4Engine, variant, &succeeded));
        if (succeeded)
            return retn.asReturnedValue();

        if (QQmlValueType *vt = QQmlValueTypeFactory::valueType(type))
            return QV4::QmlValueTypeWrapper::create(this, variant, vt);
    }

    // XXX TODO: To be compatible, we still need to handle:
    //    + QObjectList
    //    + QList<int>

    return QV4::Encode(m_v4Engine->newVariantObject(variant));
}

QNetworkAccessManager *QV8Engine::networkAccessManager()
{
    return QQmlEnginePrivate::get(m_engine)->getNetworkAccessManager();
}

const QSet<QString> &QV8Engine::illegalNames() const
{
    return m_illegalNames;
}

QQmlContextData *QV8Engine::callingContext()
{
    return QV4::QmlContextWrapper::callingContext(m_v4Engine);
}

void QV8Engine::initializeGlobal()
{
    QV4::Scope scope(m_v4Engine);
    QV4::GlobalExtensions::init(m_engine, m_v4Engine->globalObject);

    QQmlLocale::registerStringLocaleCompare(m_v4Engine);
    QQmlDateExtension::registerExtension(m_v4Engine);
    QQmlNumberExtension::registerExtension(m_v4Engine);

    qt_add_domexceptions(m_v4Engine);
    m_xmlHttpRequestData = qt_add_qmlxmlhttprequest(this);

    qt_add_sqlexceptions(m_v4Engine);

    {
        for (uint i = 0; i < m_v4Engine->globalObject->internalClass()->size; ++i) {
            if (m_v4Engine->globalObject->internalClass()->nameMap.at(i))
                m_illegalNames.insert(m_v4Engine->globalObject->internalClass()->nameMap.at(i)->toQString());
        }
    }

    {
#define FREEZE_SOURCE "(function freeze_recur(obj) { "\
                      "    if (Qt.isQtObject(obj)) return;"\
                      "    if (obj != Function.connect && obj != Function.disconnect && "\
                      "        obj instanceof Object) {"\
                      "        var properties = Object.getOwnPropertyNames(obj);"\
                      "        for (var prop in properties) { "\
                      "            if (prop == \"connect\" || prop == \"disconnect\") {"\
                      "                Object.freeze(obj[prop]); "\
                      "                continue;"\
                      "            }"\
                      "            freeze_recur(obj[prop]);"\
                      "        }"\
                      "    }"\
                      "    if (obj instanceof Object) {"\
                      "        Object.freeze(obj);"\
                      "    }"\
                      "})"

        QV4::Scoped<QV4::FunctionObject> result(scope, QV4::Script::evaluate(m_v4Engine, QString::fromUtf8(FREEZE_SOURCE), 0));
        Q_ASSERT(!!result);
        m_freezeObject = result;
#undef FREEZE_SOURCE
    }
}

void QV8Engine::freezeObject(const QV4::ValueRef value)
{
    QV4::Scope scope(m_v4Engine);
    QV4::ScopedFunctionObject f(scope, m_freezeObject.value());
    QV4::ScopedCallData callData(scope, 1);
    callData->args[0] = value;
    callData->thisObject = m_v4Engine->globalObject;
    f->call(callData);
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
    QV4::Scope scope(m_v4Engine);
    QV4::ScopedValue v(scope, m_v4Engine->globalObject);
    freezeObject(v);
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

// Converts a QVariantList to JS.
// The result is a new Array object with length equal to the length
// of the QVariantList, and the elements being the QVariantList's
// elements converted to JS, recursively.
QV4::ReturnedValue QV8Engine::variantListToJS(const QVariantList &lst)
{
    QV4::Scope scope(m_v4Engine);
    QV4::Scoped<QV4::ArrayObject> a(scope, m_v4Engine->newArrayObject());
    a->arrayReserve(lst.size());
    QV4::ScopedValue v(scope);
    for (int i = 0; i < lst.size(); i++)
        a->arrayPut(i, (v = variantToJS(lst.at(i))));
    a->setArrayLengthUnchecked(lst.size());
    return a.asReturnedValue();
}

// Converts a QVariantMap to JS.
// The result is a new Object object with property names being
// the keys of the QVariantMap, and values being the values of
// the QVariantMap converted to JS, recursively.
QV4::ReturnedValue QV8Engine::variantMapToJS(const QVariantMap &vmap)
{
    QV4::Scope scope(m_v4Engine);
    QV4::Scoped<QV4::Object> o(scope, m_v4Engine->newObject());
    QVariantMap::const_iterator it;
    QV4::ScopedString s(scope);
    QV4::ScopedValue v(scope);
    for (it = vmap.constBegin(); it != vmap.constEnd(); ++it) {
        s = m_v4Engine->newIdentifier(it.key());
        v = variantToJS(it.value());
        uint idx = s->asArrayIndex();
        if (idx < UINT_MAX)
            o->arraySet(idx, v);
        else
            o->insertMember(s.getPointer(), v);
    }
    return o.asReturnedValue();
}

// Converts the meta-type defined by the given type and data to JS.
// Returns the value if conversion succeeded, an empty handle otherwise.
QV4::ReturnedValue QV8Engine::metaTypeToJS(int type, const void *data)
{
    Q_ASSERT(data != 0);

    // check if it's one of the types we know
    switch (QMetaType::Type(type)) {
    case QMetaType::UnknownType:
    case QMetaType::Void:
        return QV4::Encode::undefined();
    case QMetaType::VoidStar:
        return QV4::Encode::null();
    case QMetaType::Bool:
        return QV4::Encode(*reinterpret_cast<const bool*>(data));
    case QMetaType::Int:
        return QV4::Encode(*reinterpret_cast<const int*>(data));
    case QMetaType::UInt:
        return QV4::Encode(*reinterpret_cast<const uint*>(data));
    case QMetaType::LongLong:
        return QV4::Encode(double(*reinterpret_cast<const qlonglong*>(data)));
    case QMetaType::ULongLong:
#if defined(Q_OS_WIN) && defined(_MSC_FULL_VER) && _MSC_FULL_VER <= 12008804
#pragma message("** NOTE: You need the Visual Studio Processor Pack to compile support for 64bit unsigned integers.")
        return QV4::Encode(double((qlonglong)*reinterpret_cast<const qulonglong*>(data)));
#elif defined(Q_CC_MSVC) && !defined(Q_CC_MSVC_NET)
        return QV4::Encode(double((qlonglong)*reinterpret_cast<const qulonglong*>(data)));
#else
        return QV4::Encode(double(*reinterpret_cast<const qulonglong*>(data)));
#endif
    case QMetaType::Double:
        return QV4::Encode(*reinterpret_cast<const double*>(data));
    case QMetaType::QString:
        return m_v4Engine->currentContext()->d()->engine->newString(*reinterpret_cast<const QString*>(data))->asReturnedValue();
    case QMetaType::Float:
        return QV4::Encode(*reinterpret_cast<const float*>(data));
    case QMetaType::Short:
        return QV4::Encode((int)*reinterpret_cast<const short*>(data));
    case QMetaType::UShort:
        return QV4::Encode((int)*reinterpret_cast<const unsigned short*>(data));
    case QMetaType::Char:
        return QV4::Encode((int)*reinterpret_cast<const char*>(data));
    case QMetaType::UChar:
        return QV4::Encode((int)*reinterpret_cast<const unsigned char*>(data));
    case QMetaType::QChar:
        return QV4::Encode((int)(*reinterpret_cast<const QChar*>(data)).unicode());
    case QMetaType::QStringList:
        return QV4::Encode(m_v4Engine->newArrayObject(*reinterpret_cast<const QStringList *>(data)));
    case QMetaType::QVariantList:
        return variantListToJS(*reinterpret_cast<const QVariantList *>(data));
    case QMetaType::QVariantMap:
        return variantMapToJS(*reinterpret_cast<const QVariantMap *>(data));
    case QMetaType::QDateTime:
        return QV4::Encode(m_v4Engine->newDateObject(*reinterpret_cast<const QDateTime *>(data)));
    case QMetaType::QDate:
        return QV4::Encode(m_v4Engine->newDateObject(QDateTime(*reinterpret_cast<const QDate *>(data))));
    case QMetaType::QRegExp:
        return QV4::Encode(m_v4Engine->newRegExpObject(*reinterpret_cast<const QRegExp *>(data)));
    case QMetaType::QObjectStar:
        return QV4::QObjectWrapper::wrap(m_v4Engine, *reinterpret_cast<QObject* const *>(data));
    case QMetaType::QVariant:
        return variantToJS(*reinterpret_cast<const QVariant*>(data));
    case QMetaType::QJsonValue:
        return QV4::JsonObject::fromJsonValue(m_v4Engine, *reinterpret_cast<const QJsonValue *>(data));
    case QMetaType::QJsonObject:
        return QV4::JsonObject::fromJsonObject(m_v4Engine, *reinterpret_cast<const QJsonObject *>(data));
    case QMetaType::QJsonArray:
        return QV4::JsonObject::fromJsonArray(m_v4Engine, *reinterpret_cast<const QJsonArray *>(data));
    default:
        if (type == qMetaTypeId<QJSValue>()) {
            return QJSValuePrivate::get(*reinterpret_cast<const QJSValue*>(data))->getValue(m_v4Engine);
        } else {
            QByteArray typeName = QMetaType::typeName(type);
            if (typeName.endsWith('*') && !*reinterpret_cast<void* const *>(data)) {
                return QV4::Encode::null();
            } else {
                // Fall back to wrapping in a QVariant.
                return QV4::Encode(m_v4Engine->newVariantObject(QVariant(type, data)));
            }
        }
    }
    Q_UNREACHABLE();
    return 0;
}

// Converts a JS value to a meta-type.
// data must point to a place that can store a value of the given type.
// Returns true if conversion succeeded, false otherwise.
bool QV8Engine::metaTypeFromJS(const QV4::ValueRef value, int type, void *data)
{
    QV4::Scope scope(QV8Engine::getV4(this));

    // check if it's one of the types we know
    switch (QMetaType::Type(type)) {
    case QMetaType::Bool:
        *reinterpret_cast<bool*>(data) = value->toBoolean();
        return true;
    case QMetaType::Int:
        *reinterpret_cast<int*>(data) = value->toInt32();
        return true;
    case QMetaType::UInt:
        *reinterpret_cast<uint*>(data) = value->toUInt32();
        return true;
    case QMetaType::LongLong:
        *reinterpret_cast<qlonglong*>(data) = qlonglong(value->toInteger());
        return true;
    case QMetaType::ULongLong:
        *reinterpret_cast<qulonglong*>(data) = qulonglong(value->toInteger());
        return true;
    case QMetaType::Double:
        *reinterpret_cast<double*>(data) = value->toNumber();
        return true;
    case QMetaType::QString:
        if (value->isUndefined() || value->isNull())
            *reinterpret_cast<QString*>(data) = QString();
        else
            *reinterpret_cast<QString*>(data) = value->toString(m_v4Engine->currentContext())->toQString();
        return true;
    case QMetaType::Float:
        *reinterpret_cast<float*>(data) = value->toNumber();
        return true;
    case QMetaType::Short:
        *reinterpret_cast<short*>(data) = short(value->toInt32());
        return true;
    case QMetaType::UShort:
        *reinterpret_cast<unsigned short*>(data) = value->toUInt16();
        return true;
    case QMetaType::Char:
        *reinterpret_cast<char*>(data) = char(value->toInt32());
        return true;
    case QMetaType::UChar:
        *reinterpret_cast<unsigned char*>(data) = (unsigned char)(value->toInt32());
        return true;
    case QMetaType::QChar:
        if (value->isString()) {
            QString str = value->stringValue()->toQString();
            *reinterpret_cast<QChar*>(data) = str.isEmpty() ? QChar() : str.at(0);
        } else {
            *reinterpret_cast<QChar*>(data) = QChar(ushort(value->toUInt16()));
        }
        return true;
    case QMetaType::QDateTime:
        if (QV4::DateObject *d = value->asDateObject()) {
            *reinterpret_cast<QDateTime *>(data) = d->toQDateTime();
            return true;
        } break;
    case QMetaType::QDate:
        if (QV4::DateObject *d = value->asDateObject()) {
            *reinterpret_cast<QDate *>(data) = d->toQDateTime().date();
            return true;
        } break;
    case QMetaType::QRegExp:
        if (QV4::RegExpObject *r = value->as<QV4::RegExpObject>()) {
            *reinterpret_cast<QRegExp *>(data) = r->toQRegExp();
            return true;
        } break;
    case QMetaType::QObjectStar: {
        QV4::QObjectWrapper *qobjectWrapper = value->as<QV4::QObjectWrapper>();
        if (qobjectWrapper || value->isNull()) {
            *reinterpret_cast<QObject* *>(data) = qtObjectFromJS(value);
            return true;
        } break;
    }
    case QMetaType::QStringList: {
        QV4::ScopedArrayObject a(scope, value);
        if (a) {
            *reinterpret_cast<QStringList *>(data) = a->toQStringList();
            return true;
        }
        break;
    }
    case QMetaType::QVariantList: {
        QV4::ScopedArrayObject a(scope, value);
        if (a) {
            *reinterpret_cast<QVariantList *>(data) = toVariant(a, /*typeHint*/-1, /*createJSValueForObjects*/false).toList();
            return true;
        }
        break;
    }
    case QMetaType::QVariantMap: {
        QV4::ScopedObject o(scope, value);
        if (o) {
            *reinterpret_cast<QVariantMap *>(data) = variantMapFromJS(o);
            return true;
        }
        break;
    }
    case QMetaType::QVariant:
        *reinterpret_cast<QVariant*>(data) = toVariant(value, /*typeHint*/-1, /*createJSValueForObjects*/false);
        return true;
    case QMetaType::QJsonValue:
        *reinterpret_cast<QJsonValue *>(data) = QV4::JsonObject::toJsonValue(value);
        return true;
    case QMetaType::QJsonObject: {
        QV4::ScopedObject o(scope, value);
        *reinterpret_cast<QJsonObject *>(data) = QV4::JsonObject::toJsonObject(o);
        return true;
    }
    case QMetaType::QJsonArray: {
        QV4::ScopedArrayObject a(scope, value);
        if (a) {
            *reinterpret_cast<QJsonArray *>(data) = QV4::JsonObject::toJsonArray(a);
            return true;
        }
        break;
    }
    default:
    ;
    }

#if 0
    if (isQtVariant(value)) {
        const QVariant &var = variantValue(value);
        // ### Enable once constructInPlace() is in qt master.
        if (var.userType() == type) {
            QMetaType::constructInPlace(type, data, var.constData());
            return true;
        }
        if (var.canConvert(type)) {
            QVariant vv = var;
            vv.convert(type);
            Q_ASSERT(vv.userType() == type);
            QMetaType::constructInPlace(type, data, vv.constData());
            return true;
        }

    }
#endif

    // Try to use magic; for compatibility with qscriptvalue_cast.

    QByteArray name = QMetaType::typeName(type);
    if (convertToNativeQObject(value, name, reinterpret_cast<void* *>(data)))
        return true;
    if (value->as<QV4::VariantObject>() && name.endsWith('*')) {
        int valueType = QMetaType::type(name.left(name.size()-1));
        QVariant &var = value->as<QV4::VariantObject>()->d()->data;
        if (valueType == var.userType()) {
            // We have T t, T* is requested, so return &t.
            *reinterpret_cast<void* *>(data) = var.data();
            return true;
        } else if (value->isObject()) {
            // Look in the prototype chain.
            QV4::ScopedObject proto(scope, value->objectValue()->prototype());
            while (proto) {
                bool canCast = false;
                if (QV4::VariantObject *vo = proto->as<QV4::VariantObject>()) {
                    const QVariant &v = vo->d()->data;
                    canCast = (type == v.userType()) || (valueType && (valueType == v.userType()));
                }
                else if (proto->as<QV4::QObjectWrapper>()) {
                    QByteArray className = name.left(name.size()-1);
                    QV4::ScopedObject p(scope, proto.getPointer());
                    if (QObject *qobject = qtObjectFromJS(p))
                        canCast = qobject->qt_metacast(className) != 0;
                }
                if (canCast) {
                    QByteArray varTypeName = QMetaType::typeName(var.userType());
                    if (varTypeName.endsWith('*'))
                        *reinterpret_cast<void* *>(data) = *reinterpret_cast<void* *>(var.data());
                    else
                        *reinterpret_cast<void* *>(data) = var.data();
                    return true;
                }
                proto = proto->prototype();
            }
        }
    } else if (value->isNull() && name.endsWith('*')) {
        *reinterpret_cast<void* *>(data) = 0;
        return true;
    } else if (type == qMetaTypeId<QJSValue>()) {
        *reinterpret_cast<QJSValue*>(data) = QJSValuePrivate::get(new QJSValuePrivate(m_v4Engine, value));
        return true;
    }

    return false;
}

// Converts a QVariant to JS.
QV4::ReturnedValue QV8Engine::variantToJS(const QVariant &value)
{
    return metaTypeToJS(value.userType(), value.constData());
}

bool QV8Engine::convertToNativeQObject(const QV4::ValueRef value, const QByteArray &targetType, void **result)
{
    if (!targetType.endsWith('*'))
        return false;
    if (QObject *qobject = qtObjectFromJS(value)) {
        int start = targetType.startsWith("const ") ? 6 : 0;
        QByteArray className = targetType.mid(start, targetType.size()-start-1);
        if (void *instance = qobject->qt_metacast(className)) {
            *result = instance;
            return true;
        }
    }
    return false;
}

QObject *QV8Engine::qtObjectFromJS(const QV4::ValueRef value)
{
    if (!value->isObject())
        return 0;

    QV4::Scope scope(m_v4Engine);
    QV4::Scoped<QV4::VariantObject> v(scope, value);

    if (v) {
        QVariant variant = v->d()->data;
        int type = variant.userType();
        if (type == QMetaType::QObjectStar)
            return *reinterpret_cast<QObject* const *>(variant.constData());
    }
    QV4::Scoped<QV4::QObjectWrapper> wrapper(scope, value);
    if (!wrapper)
        return 0;
    return wrapper->object();
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

QV4::ReturnedValue QV8Engine::toString(const QString &string)
{
    return QV4::Encode(m_v4Engine->newString(string));
}

QT_END_NAMESPACE

