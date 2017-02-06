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

#include <QtQml/qqmlprivate.h>
#include "qqmlmetatype_p.h"

#include <private/qqmlproxymetaobject_p.h>
#include <private/qqmlcustomparser_p.h>
#include <private/qhashedstring_p.h>
#include <private/qqmlimport_p.h>

#include <QtCore/qdebug.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qbitarray.h>
#include <QtCore/qreadwritelock.h>
#include <QtCore/private/qmetaobject_p.h>

#include <qmetatype.h>
#include <qobjectdefs.h>
#include <qbytearray.h>
#include <qreadwritelock.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qvector.h>

#include <ctype.h>
#include "qqmlcomponent.h"

QT_BEGIN_NAMESPACE

struct QQmlMetaTypeData
{
    QQmlMetaTypeData();
    ~QQmlMetaTypeData();
    QList<QQmlType *> types;
    typedef QHash<int, QQmlType *> Ids;
    Ids idToType;
    typedef QHash<QHashedStringRef,QQmlType *> Names;
    Names nameToType;
    typedef QHash<QUrl, QQmlType *> Files; //For file imported composite types only
    Files urlToType;
    Files urlToNonFileImportType; // For non-file imported composite and composite
                                  // singleton types. This way we can locate any
                                  // of them by url, even if it was registered as
                                  // a module via qmlRegisterCompositeType.
    typedef QHash<const QMetaObject *, QQmlType *> MetaObjects;
    MetaObjects metaObjectToType;
    typedef QHash<int, QQmlMetaType::StringConverter> StringConverters;
    StringConverters stringConverters;

    struct VersionedUri {
        VersionedUri()
        : majorVersion(0) {}
        VersionedUri(const QHashedString &uri, int majorVersion)
        : uri(uri), majorVersion(majorVersion) {}
        bool operator==(const VersionedUri &other) const {
            return other.majorVersion == majorVersion && other.uri == uri;
        }
        QHashedString uri;
        int majorVersion;
    };
    typedef QHash<VersionedUri, QQmlTypeModule *> TypeModules;
    TypeModules uriToModule;

    QBitArray objects;
    QBitArray interfaces;
    QBitArray lists;

    QList<QQmlPrivate::AutoParentFunction> parentFunctions;
    QVector<QQmlPrivate::QmlUnitCacheLookupFunction> lookupCachedQmlUnit;

    QSet<QString> protectedNamespaces;

    QString typeRegistrationNamespace;
    QStringList typeRegistrationFailures;
};

class QQmlTypeModulePrivate
{
public:
    QQmlTypeModulePrivate()
    : minMinorVersion(INT_MAX), maxMinorVersion(0), locked(false) {}

    static QQmlTypeModulePrivate* get(QQmlTypeModule* q) { return q->d; }

    QQmlMetaTypeData::VersionedUri uri;

    int minMinorVersion;
    int maxMinorVersion;
    bool locked;

    void add(QQmlType *);

    QStringHash<QList<QQmlType *> > typeHash;
    QList<QQmlType *> types;
};

Q_GLOBAL_STATIC(QQmlMetaTypeData, metaTypeData)
Q_GLOBAL_STATIC_WITH_ARGS(QMutex, metaTypeDataLock, (QMutex::Recursive))

static uint qHash(const QQmlMetaTypeData::VersionedUri &v)
{
    return v.uri.hash() ^ qHash(v.majorVersion);
}

QQmlMetaTypeData::QQmlMetaTypeData()
{
}

QQmlMetaTypeData::~QQmlMetaTypeData()
{
    for (int i = 0; i < types.count(); ++i)
        delete types.at(i);

    for (TypeModules::const_iterator i = uriToModule.constBegin(), cend = uriToModule.constEnd(); i != cend; ++i)
        delete *i;
}

class QQmlTypePrivate
{
public:
    QQmlTypePrivate(QQmlType::RegistrationType type);
    ~QQmlTypePrivate();

    void init() const;
    void initEnums() const;
    void insertEnums(const QMetaObject *metaObject) const;

    QQmlType::RegistrationType regType;

    struct QQmlCppTypeData
    {
        int allocationSize;
        void (*newFunc)(void *);
        QString noCreationReason;
        int parserStatusCast;
        QObject *(*extFunc)(QObject *);
        const QMetaObject *extMetaObject;
        QQmlCustomParser *customParser;
        QQmlAttachedPropertiesFunc attachedPropertiesFunc;
        const QMetaObject *attachedPropertiesType;
        int attachedPropertiesId;
        int propertyValueSourceCast;
        int propertyValueInterceptorCast;
    };

    struct QQmlSingletonTypeData
    {
        QQmlType::SingletonInstanceInfo *singletonInstanceInfo;
    };

    struct QQmlCompositeTypeData
    {
        QUrl url;
    };

    union extraData {
        QQmlCppTypeData* cd;
        QQmlSingletonTypeData* sd;
        QQmlCompositeTypeData* fd;
    } extraData;

    const char *iid;
    QHashedString module;
    QString name;
    QString elementName;
    int version_maj;
    int version_min;
    int typeId;
    int listId;
    int revision;
    mutable bool containsRevisionedAttributes;
    mutable QQmlType *superType;
    const QMetaObject *baseMetaObject;

    int index;
    mutable volatile bool isSetup:1;
    mutable volatile bool isEnumSetup:1;
    mutable bool haveSuperType:1;
    mutable QList<QQmlProxyMetaObject::ProxyData> metaObjects;
    mutable QStringHash<int> enums;

    static QHash<const QMetaObject *, int> attachedPropertyIds;
};

void QQmlType::SingletonInstanceInfo::init(QQmlEngine *e)
{
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(e->handle());
    v4->pushGlobalContext();
    if (scriptCallback && scriptApi(e).isUndefined()) {
        setScriptApi(e, scriptCallback(e, e));
    } else if (qobjectCallback && !qobjectApi(e)) {
        QObject *o = qobjectCallback(e, e);
        setQObjectApi(e, o);
        if (!o) {
            qFatal("qmlRegisterSingletonType(): \"%s\" is not available because the callback function returns a null pointer.", qPrintable(typeName));
        }
        // if this object can use a property cache, create it now
        QQmlData::ensurePropertyCache(e, o);
    } else if (!url.isEmpty() && !qobjectApi(e)) {
        QQmlComponent component(e, url, QQmlComponent::PreferSynchronous);
        QObject *o = component.create();
        setQObjectApi(e, o);
    }
    v4->popContext();
}

void QQmlType::SingletonInstanceInfo::destroy(QQmlEngine *e)
{
    // cleans up the engine-specific singleton instances if they exist.
    scriptApis.remove(e);
    QObject *o = qobjectApis.take(e);
    if (o) {
        QQmlData *ddata = QQmlData::get(o, false);
        if (url.isEmpty() && ddata && ddata->indestructible && ddata->explicitIndestructibleSet)
            return;
        delete o;
    }
}

void QQmlType::SingletonInstanceInfo::setQObjectApi(QQmlEngine *e, QObject *o)
{
    qobjectApis.insert(e, o);
}

QObject *QQmlType::SingletonInstanceInfo::qobjectApi(QQmlEngine *e) const
{
    return qobjectApis.value(e);
}

void QQmlType::SingletonInstanceInfo::setScriptApi(QQmlEngine *e, const QJSValue &v)
{
    scriptApis.insert(e, v);
}

QJSValue QQmlType::SingletonInstanceInfo::scriptApi(QQmlEngine *e) const
{
    return scriptApis.value(e);
}

QHash<const QMetaObject *, int> QQmlTypePrivate::attachedPropertyIds;

QQmlTypePrivate::QQmlTypePrivate(QQmlType::RegistrationType type)
: regType(type), iid(0), typeId(0), listId(0), revision(0),
    containsRevisionedAttributes(false), superType(0), baseMetaObject(0),
    index(-1), isSetup(false), isEnumSetup(false), haveSuperType(false)
{
    switch (type) {
    case QQmlType::CppType:
        extraData.cd = new QQmlCppTypeData;
        extraData.cd->allocationSize = 0;
        extraData.cd->newFunc = 0;
        extraData.cd->parserStatusCast = -1;
        extraData.cd->extFunc = 0;
        extraData.cd->extMetaObject = 0;
        extraData.cd->customParser = 0;
        extraData.cd->attachedPropertiesFunc = 0;
        extraData.cd->attachedPropertiesType = 0;
        extraData.cd->propertyValueSourceCast = -1;
        extraData.cd->propertyValueInterceptorCast = -1;
        break;
    case QQmlType::SingletonType:
    case QQmlType::CompositeSingletonType:
        extraData.sd = new QQmlSingletonTypeData;
        extraData.sd->singletonInstanceInfo = 0;
        break;
    case QQmlType::InterfaceType:
        extraData.cd = 0;
        break;
    case QQmlType::CompositeType:
        extraData.fd = new QQmlCompositeTypeData;
        break;
    default: qFatal("QQmlTypePrivate Internal Error.");
    }
}

QQmlTypePrivate::~QQmlTypePrivate()
{
    switch (regType) {
    case QQmlType::CppType:
        delete extraData.cd->customParser;
        delete extraData.cd;
        break;
    case QQmlType::SingletonType:
    case QQmlType::CompositeSingletonType:
        delete extraData.sd->singletonInstanceInfo;
        delete extraData.sd;
        break;
    case QQmlType::CompositeType:
        delete extraData.fd;
        break;
    default: //Also InterfaceType, because it has no extra data
        break;
    }
}

QQmlType::QQmlType(int index, const QQmlPrivate::RegisterInterface &interface)
: d(new QQmlTypePrivate(InterfaceType))
{
    d->iid = interface.iid;
    d->typeId = interface.typeId;
    d->listId = interface.listId;
    d->index = index;
    d->isSetup = true;
    d->version_maj = 0;
    d->version_min = 0;
}

QQmlType::QQmlType(int index, const QString &elementName, const QQmlPrivate::RegisterSingletonType &type)
: d(new QQmlTypePrivate(SingletonType))
{
    d->elementName = elementName;
    d->module = QString::fromUtf8(type.uri);

    d->version_maj = type.versionMajor;
    d->version_min = type.versionMinor;

    if (type.qobjectApi) {
        if (type.version >= 1) // static metaobject added in version 1
            d->baseMetaObject = type.instanceMetaObject;
        if (type.version >= 2) // typeId added in version 2
            d->typeId = type.typeId;
        if (type.version >= 2) // revisions added in version 2
            d->revision = type.revision;
    }

    d->index = index;

    d->extraData.sd->singletonInstanceInfo = new SingletonInstanceInfo;
    d->extraData.sd->singletonInstanceInfo->scriptCallback = type.scriptApi;
    d->extraData.sd->singletonInstanceInfo->qobjectCallback = type.qobjectApi;
    d->extraData.sd->singletonInstanceInfo->typeName = QString::fromUtf8(type.typeName);
    d->extraData.sd->singletonInstanceInfo->instanceMetaObject
        = (type.qobjectApi && type.version >= 1) ? type.instanceMetaObject : 0;
}

QQmlType::QQmlType(int index, const QString &elementName, const QQmlPrivate::RegisterCompositeSingletonType &type)
  : d(new QQmlTypePrivate(CompositeSingletonType))
{
    d->elementName = elementName;
    d->module = QString::fromUtf8(type.uri);

    d->version_maj = type.versionMajor;
    d->version_min = type.versionMinor;

    d->index = index;

    d->extraData.sd->singletonInstanceInfo = new SingletonInstanceInfo;
    d->extraData.sd->singletonInstanceInfo->url = type.url;
    d->extraData.sd->singletonInstanceInfo->typeName = QString::fromUtf8(type.typeName);
}

QQmlType::QQmlType(int index, const QString &elementName, const QQmlPrivate::RegisterType &type)
: d(new QQmlTypePrivate(CppType))
{
    d->elementName = elementName;
    d->module = QString::fromUtf8(type.uri);

    d->version_maj = type.versionMajor;
    d->version_min = type.versionMinor;
    if (type.version >= 1) // revisions added in version 1
        d->revision = type.revision;
    d->typeId = type.typeId;
    d->listId = type.listId;
    d->extraData.cd->allocationSize = type.objectSize;
    d->extraData.cd->newFunc = type.create;
    d->extraData.cd->noCreationReason = type.noCreationReason;
    d->baseMetaObject = type.metaObject;
    d->extraData.cd->attachedPropertiesFunc = type.attachedPropertiesFunction;
    d->extraData.cd->attachedPropertiesType = type.attachedPropertiesMetaObject;
    if (d->extraData.cd->attachedPropertiesType) {
        QHash<const QMetaObject *, int>::Iterator iter = d->attachedPropertyIds.find(d->baseMetaObject);
        if (iter == d->attachedPropertyIds.end())
            iter = d->attachedPropertyIds.insert(d->baseMetaObject, index);
        d->extraData.cd->attachedPropertiesId = *iter;
    } else {
        d->extraData.cd->attachedPropertiesId = -1;
    }
    d->extraData.cd->parserStatusCast = type.parserStatusCast;
    d->extraData.cd->propertyValueSourceCast = type.valueSourceCast;
    d->extraData.cd->propertyValueInterceptorCast = type.valueInterceptorCast;
    d->extraData.cd->extFunc = type.extensionObjectCreate;
    d->extraData.cd->customParser = type.customParser;
    d->index = index;

    if (type.extensionMetaObject)
        d->extraData.cd->extMetaObject = type.extensionMetaObject;
}

QQmlType::QQmlType(int index, const QString &elementName, const QQmlPrivate::RegisterCompositeType &type)
: d(new QQmlTypePrivate(CompositeType))
{
    d->index = index;
    d->elementName = elementName;

    d->module = QString::fromUtf8(type.uri);
    d->version_maj = type.versionMajor;
    d->version_min = type.versionMinor;

    d->extraData.fd->url = type.url;
}

QQmlType::~QQmlType()
{
    delete d;
}

const QHashedString &QQmlType::module() const
{
    return d->module;
}

int QQmlType::majorVersion() const
{
    return d->version_maj;
}

int QQmlType::minorVersion() const
{
    return d->version_min;
}

bool QQmlType::availableInVersion(int vmajor, int vminor) const
{
    Q_ASSERT(vmajor >= 0 && vminor >= 0);
    return vmajor == d->version_maj && vminor >= d->version_min;
}

bool QQmlType::availableInVersion(const QHashedStringRef &module, int vmajor, int vminor) const
{
    Q_ASSERT(vmajor >= 0 && vminor >= 0);
    return module == d->module && vmajor == d->version_maj && vminor >= d->version_min;
}

// returns the nearest _registered_ super class
QQmlType *QQmlType::superType() const
{
    if (!d->haveSuperType && d->baseMetaObject) {
        const QMetaObject *mo = d->baseMetaObject->superClass();
        while (mo && !d->superType) {
            d->superType = QQmlMetaType::qmlType(mo, d->module, d->version_maj, d->version_min);
            mo = mo->superClass();
        }
        d->haveSuperType = true;
    }

    return d->superType;
}

QQmlType *QQmlType::resolveCompositeBaseType(QQmlEnginePrivate *engine) const
{
    Q_ASSERT(isComposite());
    if (!engine)
        return 0;
    QQmlRefPointer<QQmlTypeData> td(engine->typeLoader.getType(sourceUrl()), QQmlRefPointer<QQmlTypeData>::Adopt);
    if (td.isNull() || !td->isComplete())
        return 0;
    QV4::CompiledData::CompilationUnit *compilationUnit = td->compilationUnit();
    const QMetaObject *mo = compilationUnit->rootPropertyCache()->firstCppMetaObject();
    return QQmlMetaType::qmlType(mo);
}

int QQmlType::resolveCompositeEnumValue(QQmlEnginePrivate *engine, const QString &name, bool *ok) const
{
    Q_ASSERT(isComposite());
    *ok = false;
    QQmlType *type = resolveCompositeBaseType(engine);
    if (!type)
        return -1;
    return type->enumValue(engine, name, ok);
}

static void clone(QMetaObjectBuilder &builder, const QMetaObject *mo,
                  const QMetaObject *ignoreStart, const QMetaObject *ignoreEnd)
{
    // Set classname
    builder.setClassName(ignoreEnd->className());

    // Clone Q_CLASSINFO
    for (int ii = mo->classInfoOffset(); ii < mo->classInfoCount(); ++ii) {
        QMetaClassInfo info = mo->classInfo(ii);

        int otherIndex = ignoreEnd->indexOfClassInfo(info.name());
        if (otherIndex >= ignoreStart->classInfoOffset() + ignoreStart->classInfoCount()) {
            // Skip
        } else {
            builder.addClassInfo(info.name(), info.value());
        }
    }

    // Clone Q_PROPERTY
    for (int ii = mo->propertyOffset(); ii < mo->propertyCount(); ++ii) {
        QMetaProperty property = mo->property(ii);

        int otherIndex = ignoreEnd->indexOfProperty(property.name());
        if (otherIndex >= ignoreStart->propertyOffset() + ignoreStart->propertyCount()) {
            builder.addProperty(QByteArray("__qml_ignore__") + property.name(), QByteArray("void"));
            // Skip
        } else {
            builder.addProperty(property);
        }
    }

    // Clone Q_METHODS
    for (int ii = mo->methodOffset(); ii < mo->methodCount(); ++ii) {
        QMetaMethod method = mo->method(ii);

        // More complex - need to search name
        QByteArray name = method.name();


        bool found = false;

        for (int ii = ignoreStart->methodOffset() + ignoreStart->methodCount();
             !found && ii < ignoreEnd->methodOffset() + ignoreEnd->methodCount();
             ++ii) {

            QMetaMethod other = ignoreEnd->method(ii);

            found = name == other.name();
        }

        QMetaMethodBuilder m = builder.addMethod(method);
        if (found) // SKIP
            m.setAccess(QMetaMethod::Private);
    }

    // Clone Q_ENUMS
    for (int ii = mo->enumeratorOffset(); ii < mo->enumeratorCount(); ++ii) {
        QMetaEnum enumerator = mo->enumerator(ii);

        int otherIndex = ignoreEnd->indexOfEnumerator(enumerator.name());
        if (otherIndex >= ignoreStart->enumeratorOffset() + ignoreStart->enumeratorCount()) {
            // Skip
        } else {
            builder.addEnumerator(enumerator);
        }
    }
}

static bool isPropertyRevisioned(const QMetaObject *mo, int index)
{
    int i = index;
    i -= mo->propertyOffset();
    if (i < 0 && mo->d.superdata)
        return isPropertyRevisioned(mo->d.superdata, index);

    const QMetaObjectPrivate *mop = reinterpret_cast<const QMetaObjectPrivate*>(mo->d.data);
    if (i >= 0 && i < mop->propertyCount) {
        int handle = mop->propertyData + 3*i;
        int flags = mo->d.data[handle + 2];

        return (flags & Revisioned);
    }

    return false;
}

void QQmlTypePrivate::init() const
{
    if (isSetup)
        return;

    QMutexLocker lock(metaTypeDataLock());
    if (isSetup)
        return;

    const QMetaObject *mo = baseMetaObject;
    if (!mo) {
        // version 0 singleton type without metaobject information
        return;
    }

    if (regType == QQmlType::CppType) {
        // Setup extended meta object
        // XXX - very inefficient
        if (extraData.cd->extFunc) {
            QMetaObjectBuilder builder;
            clone(builder, extraData.cd->extMetaObject, extraData.cd->extMetaObject, extraData.cd->extMetaObject);
            builder.setFlags(QMetaObjectBuilder::DynamicMetaObject);
            QMetaObject *mmo = builder.toMetaObject();
            mmo->d.superdata = mo;
            QQmlProxyMetaObject::ProxyData data = { mmo, extraData.cd->extFunc, 0, 0 };
            metaObjects << data;
        }
    }

    mo = mo->d.superdata;
    while(mo) {
        QQmlType *t = metaTypeData()->metaObjectToType.value(mo);
        if (t) {
            if (t->d->regType == QQmlType::CppType) {
                if (t->d->extraData.cd->extFunc) {
                    QMetaObjectBuilder builder;
                    clone(builder, t->d->extraData.cd->extMetaObject, t->d->baseMetaObject, baseMetaObject);
                    builder.setFlags(QMetaObjectBuilder::DynamicMetaObject);
                    QMetaObject *mmo = builder.toMetaObject();
                    mmo->d.superdata = baseMetaObject;
                    if (!metaObjects.isEmpty())
                        metaObjects.constLast().metaObject->d.superdata = mmo;
                    QQmlProxyMetaObject::ProxyData data = { mmo, t->d->extraData.cd->extFunc, 0, 0 };
                    metaObjects << data;
                }
            }
        }
        mo = mo->d.superdata;
    }

    for (int ii = 0; ii < metaObjects.count(); ++ii) {
        metaObjects[ii].propertyOffset =
            metaObjects.at(ii).metaObject->propertyOffset();
        metaObjects[ii].methodOffset =
            metaObjects.at(ii).metaObject->methodOffset();
    }

    // Check for revisioned details
    {
        const QMetaObject *mo = 0;
        if (metaObjects.isEmpty())
            mo = baseMetaObject;
        else
            mo = metaObjects.constFirst().metaObject;

        for (int ii = 0; !containsRevisionedAttributes && ii < mo->propertyCount(); ++ii) {
            if (isPropertyRevisioned(mo, ii))
                containsRevisionedAttributes = true;
        }

        for (int ii = 0; !containsRevisionedAttributes && ii < mo->methodCount(); ++ii) {
            if (mo->method(ii).revision() != 0)
                containsRevisionedAttributes = true;
        }
    }

    isSetup = true;
    lock.unlock();
}

void QQmlTypePrivate::initEnums() const
{
    if (isEnumSetup) return;

    init();

    QMutexLocker lock(metaTypeDataLock());
    if (isEnumSetup) return;

    if (baseMetaObject) // could be singleton type without metaobject
        insertEnums(baseMetaObject);

    isEnumSetup = true;
}

void QQmlTypePrivate::insertEnums(const QMetaObject *metaObject) const
{
    // Add any enum values defined by 'related' classes
    if (metaObject->d.relatedMetaObjects) {
        const QMetaObject * const *related = metaObject->d.relatedMetaObjects;
        if (related) {
            while (*related)
                insertEnums(*related++);
        }
    }

    // Add any enum values defined by this class, overwriting any inherited values
    for (int ii = 0; ii < metaObject->enumeratorCount(); ++ii) {
        QMetaEnum e = metaObject->enumerator(ii);
        for (int jj = 0; jj < e.keyCount(); ++jj)
            enums.insert(QString::fromUtf8(e.key(jj)), e.value(jj));
    }
}

QByteArray QQmlType::typeName() const
{
    if (d->regType == SingletonType || d->regType == CompositeSingletonType)
        return d->extraData.sd->singletonInstanceInfo->typeName.toUtf8();
    else if (d->baseMetaObject)
        return d->baseMetaObject->className();
    else
        return QByteArray();
}

const QString &QQmlType::elementName() const
{
    return d->elementName;
}

const QString &QQmlType::qmlTypeName() const
{
    if (d->name.isEmpty()) {
        if (!d->module.isEmpty())
            d->name = static_cast<QString>(d->module) + QLatin1Char('/') + d->elementName;
        else
            d->name = d->elementName;
    }

    return d->name;
}

QObject *QQmlType::create() const
{
    if (!isCreatable())
        return 0;

    d->init();

    QObject *rv = (QObject *)operator new(d->extraData.cd->allocationSize);
    d->extraData.cd->newFunc(rv);

    if (rv && !d->metaObjects.isEmpty())
        (void)new QQmlProxyMetaObject(rv, &d->metaObjects);

    return rv;
}

void QQmlType::create(QObject **out, void **memory, size_t additionalMemory) const
{
    if (!isCreatable())
        return;

    d->init();

    QObject *rv = (QObject *)operator new(d->extraData.cd->allocationSize + additionalMemory);
    d->extraData.cd->newFunc(rv);

    if (rv && !d->metaObjects.isEmpty())
        (void)new QQmlProxyMetaObject(rv, &d->metaObjects);

    *out = rv;
    *memory = ((char *)rv) + d->extraData.cd->allocationSize;
}

QQmlType::SingletonInstanceInfo *QQmlType::singletonInstanceInfo() const
{
    if (d->regType != SingletonType && d->regType != CompositeSingletonType)
        return 0;
    return d->extraData.sd->singletonInstanceInfo;
}

QQmlCustomParser *QQmlType::customParser() const
{
    if (d->regType != CppType)
        return 0;
    return d->extraData.cd->customParser;
}

QQmlType::CreateFunc QQmlType::createFunction() const
{
    if (d->regType != CppType)
        return 0;
    return d->extraData.cd->newFunc;
}

QString QQmlType::noCreationReason() const
{
    if (d->regType != CppType)
        return QString();
    return d->extraData.cd->noCreationReason;
}

int QQmlType::createSize() const
{
    if (d->regType != CppType)
        return 0;
    return d->extraData.cd->allocationSize;
}

bool QQmlType::isCreatable() const
{
    return d->regType == CppType && d->extraData.cd->newFunc;
}

bool QQmlType::isExtendedType() const
{
    d->init();

    return !d->metaObjects.isEmpty();
}

bool QQmlType::isSingleton() const
{
    return d->regType == SingletonType || d->regType == CompositeSingletonType;
}

bool QQmlType::isInterface() const
{
    return d->regType == InterfaceType;
}

bool QQmlType::isComposite() const
{
    return d->regType == CompositeType || d->regType == CompositeSingletonType;
}

bool QQmlType::isCompositeSingleton() const
{
    return d->regType == CompositeSingletonType;
}

int QQmlType::typeId() const
{
    return d->typeId;
}

int QQmlType::qListTypeId() const
{
    return d->listId;
}

const QMetaObject *QQmlType::metaObject() const
{
    d->init();

    if (d->metaObjects.isEmpty())
        return d->baseMetaObject;
    else
        return d->metaObjects.constFirst().metaObject;

}

const QMetaObject *QQmlType::baseMetaObject() const
{
    return d->baseMetaObject;
}

bool QQmlType::containsRevisionedAttributes() const
{
    d->init();

    return d->containsRevisionedAttributes;
}

int QQmlType::metaObjectRevision() const
{
    return d->revision;
}

QQmlAttachedPropertiesFunc QQmlType::attachedPropertiesFunction(QQmlEnginePrivate *engine) const
{
    if (d->regType == CppType)
        return d->extraData.cd->attachedPropertiesFunc;

    QQmlType *base = 0;
    if (d->regType == CompositeType)
        base = resolveCompositeBaseType(engine);
    return base ? base->attachedPropertiesFunction(engine) : 0;
}

const QMetaObject *QQmlType::attachedPropertiesType(QQmlEnginePrivate *engine) const
{
    if (d->regType == CppType)
        return d->extraData.cd->attachedPropertiesType;

    QQmlType *base = 0;
    if (d->regType == CompositeType)
        base = resolveCompositeBaseType(engine);
    return base ? base->attachedPropertiesType(engine) : 0;
}

/*
This is the id passed to qmlAttachedPropertiesById().  This is different from the index
for the case that a single class is registered under two or more names (eg. Item in
Qt 4.7 and QtQuick 1.0).
*/
int QQmlType::attachedPropertiesId(QQmlEnginePrivate *engine) const
{
    if (d->regType == CppType)
        return d->extraData.cd->attachedPropertiesId;

    QQmlType *base = 0;
    if (d->regType == CompositeType)
        base = resolveCompositeBaseType(engine);
    return base ? base->attachedPropertiesId(engine) : 0;
}

int QQmlType::parserStatusCast() const
{
    if (d->regType != CppType)
        return -1;
    return d->extraData.cd->parserStatusCast;
}

int QQmlType::propertyValueSourceCast() const
{
    if (d->regType != CppType)
        return -1;
    return d->extraData.cd->propertyValueSourceCast;
}

int QQmlType::propertyValueInterceptorCast() const
{
    if (d->regType != CppType)
        return -1;
    return d->extraData.cd->propertyValueInterceptorCast;
}

const char *QQmlType::interfaceIId() const
{
    if (d->regType != InterfaceType)
        return 0;
    return d->iid;
}

int QQmlType::index() const
{
    return d->index;
}

QUrl QQmlType::sourceUrl() const
{
    if (d->regType == CompositeType)
        return d->extraData.fd->url;
    else if (d->regType == CompositeSingletonType)
        return d->extraData.sd->singletonInstanceInfo->url;
    else
        return QUrl();
}

int QQmlType::enumValue(QQmlEnginePrivate *engine, const QHashedStringRef &name, bool *ok) const
{
    Q_ASSERT(ok);
    if (isComposite())
        return resolveCompositeEnumValue(engine, name.toString(), ok);
    *ok = true;

    d->initEnums();

    int *rv = d->enums.value(name);
    if (rv)
        return *rv;

    *ok = false;
    return -1;
}

int QQmlType::enumValue(QQmlEnginePrivate *engine, const QHashedCStringRef &name, bool *ok) const
{
    Q_ASSERT(ok);
    if (isComposite())
        return resolveCompositeEnumValue(engine, name.toUtf16(), ok);
    *ok = true;

    d->initEnums();

    int *rv = d->enums.value(name);
    if (rv)
        return *rv;

    *ok = false;
    return -1;
}

int QQmlType::enumValue(QQmlEnginePrivate *engine, const QV4::String *name, bool *ok) const
{
    Q_ASSERT(ok);
    if (isComposite())
        return resolveCompositeEnumValue(engine, name->toQString(), ok);
    *ok = true;

    d->initEnums();

    int *rv = d->enums.value(name);
    if (rv)
        return *rv;

    *ok = false;
    return -1;
}

QQmlTypeModule::QQmlTypeModule()
: d(new QQmlTypeModulePrivate)
{
}

QQmlTypeModule::~QQmlTypeModule()
{
    delete d; d = 0;
}

QString QQmlTypeModule::module() const
{
    return d->uri.uri;
}

int QQmlTypeModule::majorVersion() const
{
    return d->uri.majorVersion;
}

int QQmlTypeModule::minimumMinorVersion() const
{
    return d->minMinorVersion;
}

int QQmlTypeModule::maximumMinorVersion() const
{
    return d->maxMinorVersion;
}

void QQmlTypeModulePrivate::add(QQmlType *type)
{
    minMinorVersion = qMin(minMinorVersion, type->minorVersion());
    maxMinorVersion = qMax(maxMinorVersion, type->minorVersion());

    QList<QQmlType *> &list = typeHash[type->elementName()];
    for (int ii = 0; ii < list.count(); ++ii) {
        if (list.at(ii)->minorVersion() < type->minorVersion()) {
            list.insert(ii, type);
            return;
        }
    }
    list.append(type);
}

QQmlType *QQmlTypeModule::type(const QHashedStringRef &name, int minor)
{
    QMutexLocker lock(metaTypeDataLock());

    QList<QQmlType *> *types = d->typeHash.value(name);
    if (!types) return 0;

    for (int ii = 0; ii < types->count(); ++ii)
        if (types->at(ii)->minorVersion() <= minor)
            return types->at(ii);

    return 0;
}

QQmlType *QQmlTypeModule::type(const QV4::String *name, int minor)
{
    QMutexLocker lock(metaTypeDataLock());

    QList<QQmlType *> *types = d->typeHash.value(name);
    if (!types) return 0;

    for (int ii = 0; ii < types->count(); ++ii)
        if (types->at(ii)->minorVersion() <= minor)
            return types->at(ii);

    return 0;
}

QList<QQmlType*> QQmlTypeModule::singletonTypes(int minor) const
{
    QMutexLocker lock(metaTypeDataLock());

    QList<QQmlType *> retn;
    for (int ii = 0; ii < d->types.count(); ++ii) {
        QQmlType *curr = d->types.at(ii);
        if (curr->isSingleton() && curr->minorVersion() <= minor)
            retn.append(curr);
    }

    return retn;
}

QQmlTypeModuleVersion::QQmlTypeModuleVersion()
: m_module(0), m_minor(0)
{
}

QQmlTypeModuleVersion::QQmlTypeModuleVersion(QQmlTypeModule *module, int minor)
: m_module(module), m_minor(minor)
{
    Q_ASSERT(m_module);
    Q_ASSERT(m_minor >= 0);
}

QQmlTypeModuleVersion::QQmlTypeModuleVersion(const QQmlTypeModuleVersion &o)
: m_module(o.m_module), m_minor(o.m_minor)
{
}

QQmlTypeModuleVersion &QQmlTypeModuleVersion::operator=(const QQmlTypeModuleVersion &o)
{
    m_module = o.m_module;
    m_minor = o.m_minor;
    return *this;
}

QQmlTypeModule *QQmlTypeModuleVersion::module() const
{
    return m_module;
}

int QQmlTypeModuleVersion::minorVersion() const
{
    return m_minor;
}

QQmlType *QQmlTypeModuleVersion::type(const QHashedStringRef &name) const
{
    if (m_module) return m_module->type(name, m_minor);
    else return 0;
}

QQmlType *QQmlTypeModuleVersion::type(const QV4::String *name) const
{
    if (m_module) return m_module->type(name, m_minor);
    else return 0;
}

void qmlClearTypeRegistrations() // Declared in qqml.h
{
    //Only cleans global static, assumed no running engine
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    for (int i = 0; i < data->types.count(); ++i)
        delete data->types.at(i);

    for (QQmlMetaTypeData::TypeModules::const_iterator i = data->uriToModule.constBegin(), cend = data->uriToModule.constEnd(); i != cend; ++i)
        delete *i;

    data->types.clear();
    data->idToType.clear();
    data->nameToType.clear();
    data->urlToType.clear();
    data->urlToNonFileImportType.clear();
    data->metaObjectToType.clear();
    data->uriToModule.clear();

    QQmlEnginePrivate::baseModulesUninitialized = true; //So the engine re-registers its types
#ifndef QT_NO_LIBRARY
    qmlClearEnginePlugins();
#endif
}

int registerAutoParentFunction(QQmlPrivate::RegisterAutoParent &autoparent)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    data->parentFunctions.append(autoparent.function);

    return data->parentFunctions.count() - 1;
}

int registerInterface(const QQmlPrivate::RegisterInterface &interface)
{
    if (interface.version > 0)
        qFatal("qmlRegisterType(): Cannot mix incompatible QML versions.");

    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    int index = data->types.count();

    QQmlType *type = new QQmlType(index, interface);

    data->types.append(type);
    data->idToType.insert(type->typeId(), type);
    data->idToType.insert(type->qListTypeId(), type);
    // XXX No insertMulti, so no multi-version interfaces?
    if (!type->elementName().isEmpty())
        data->nameToType.insert(type->elementName(), type);

    if (data->interfaces.size() <= interface.typeId)
        data->interfaces.resize(interface.typeId + 16);
    if (data->lists.size() <= interface.listId)
        data->lists.resize(interface.listId + 16);
    data->interfaces.setBit(interface.typeId, true);
    data->lists.setBit(interface.listId, true);

    return index;
}

QString registrationTypeString(QQmlType::RegistrationType typeType)
{
    QString typeStr;
    if (typeType == QQmlType::CppType)
        typeStr = QStringLiteral("element");
    else if (typeType == QQmlType::SingletonType)
        typeStr = QStringLiteral("singleton type");
    else if (typeType == QQmlType::CompositeSingletonType)
        typeStr = QStringLiteral("composite singleton type");
    else
        typeStr = QStringLiteral("type");
    return typeStr;
}

// NOTE: caller must hold a QMutexLocker on "data"
bool checkRegistration(QQmlType::RegistrationType typeType, QQmlMetaTypeData *data, const char *uri, const QString &typeName, int majorVersion = -1)
{
    if (!typeName.isEmpty()) {
        int typeNameLen = typeName.length();
        for (int ii = 0; ii < typeNameLen; ++ii) {
            if (!(typeName.at(ii).isLetterOrNumber() || typeName.at(ii) == '_')) {
                QString failure(QCoreApplication::translate("qmlRegisterType", "Invalid QML %1 name \"%2\""));
                data->typeRegistrationFailures.append(failure.arg(registrationTypeString(typeType)).arg(typeName));
                return false;
            }
        }
    }

    if (uri && !typeName.isEmpty()) {
        QString nameSpace = QString::fromUtf8(uri);

        if (!data->typeRegistrationNamespace.isEmpty()) {
            // We can only install types into the registered namespace
            if (nameSpace != data->typeRegistrationNamespace) {
                QString failure(QCoreApplication::translate("qmlRegisterType",
                                                            "Cannot install %1 '%2' into unregistered namespace '%3'"));
                data->typeRegistrationFailures.append(failure.arg(registrationTypeString(typeType)).arg(typeName).arg(nameSpace));
                return false;
            }
        } else if (data->typeRegistrationNamespace != nameSpace) {
            // Is the target namespace protected against further registrations?
            if (data->protectedNamespaces.contains(nameSpace)) {
                QString failure(QCoreApplication::translate("qmlRegisterType",
                                                            "Cannot install %1 '%2' into protected namespace '%3'"));
                data->typeRegistrationFailures.append(failure.arg(registrationTypeString(typeType)).arg(typeName).arg(nameSpace));
                return false;
            }
        } else if (majorVersion >= 0) {
            QQmlMetaTypeData::VersionedUri versionedUri;
            versionedUri.uri = nameSpace;
            versionedUri.majorVersion = majorVersion;
            if (QQmlTypeModule* qqtm = data->uriToModule.value(versionedUri, 0)){
                if (QQmlTypeModulePrivate::get(qqtm)->locked){
                    QString failure(QCoreApplication::translate("qmlRegisterType",
                                                                "Cannot install %1 '%2' into protected module '%3' version '%4'"));
                    data->typeRegistrationFailures.append(failure.arg(registrationTypeString(typeType)).arg(typeName).arg(nameSpace).arg(majorVersion));
                    return false;
                }
            }
        }
    }

    return true;
}

// NOTE: caller must hold a QMutexLocker on "data"
void addTypeToData(QQmlType* type, QQmlMetaTypeData *data)
{
    if (!type->elementName().isEmpty())
        data->nameToType.insertMulti(type->elementName(), type);

    if (type->baseMetaObject())
        data->metaObjectToType.insertMulti(type->baseMetaObject(), type);

    if (type->typeId()) {
        data->idToType.insert(type->typeId(), type);
        if (data->objects.size() <= type->typeId())
            data->objects.resize(type->typeId() + 16);
        data->objects.setBit(type->typeId(), true);
    }

    if (type->qListTypeId()) {
        if (data->lists.size() <= type->qListTypeId())
            data->lists.resize(type->qListTypeId() + 16);
        data->lists.setBit(type->qListTypeId(), true);
        data->idToType.insert(type->qListTypeId(), type);
    }

    if (!type->module().isEmpty()) {
        const QHashedString &mod = type->module();

        QQmlMetaTypeData::VersionedUri versionedUri(mod, type->majorVersion());
        QQmlTypeModule *module = data->uriToModule.value(versionedUri);
        if (!module) {
            module = new QQmlTypeModule;
            module->d->uri = versionedUri;
            data->uriToModule.insert(versionedUri, module);
        }
        module->d->add(type);
    }
}

int registerType(const QQmlPrivate::RegisterType &type)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    QString elementName = QString::fromUtf8(type.elementName);
    if (!checkRegistration(QQmlType::CppType, data, type.uri, elementName, type.versionMajor))
        return -1;

    int index = data->types.count();

    QQmlType *dtype = new QQmlType(index, elementName, type);

    data->types.append(dtype);
    addTypeToData(dtype, data);
    if (!type.typeId)
        data->idToType.insert(dtype->typeId(), dtype);

    return index;
}

int registerSingletonType(const QQmlPrivate::RegisterSingletonType &type)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    QString typeName = QString::fromUtf8(type.typeName);
    if (!checkRegistration(QQmlType::SingletonType, data, type.uri, typeName, type.versionMajor))
        return -1;

    int index = data->types.count();

    QQmlType *dtype = new QQmlType(index, typeName, type);

    data->types.append(dtype);
    addTypeToData(dtype, data);

    return index;
}

int registerCompositeSingletonType(const QQmlPrivate::RegisterCompositeSingletonType &type)
{
    // Assumes URL is absolute and valid. Checking of user input should happen before the URL enters type.
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    QString typeName = QString::fromUtf8(type.typeName);
    bool fileImport = false;
    if (*(type.uri) == '\0')
        fileImport = true;
    if (!checkRegistration(QQmlType::CompositeSingletonType, data, fileImport ? 0 : type.uri, typeName))
        return -1;

    int index = data->types.count();

    QQmlType *dtype = new QQmlType(index, typeName, type);

    data->types.append(dtype);
    addTypeToData(dtype, data);

    QQmlMetaTypeData::Files *files = fileImport ? &(data->urlToType) : &(data->urlToNonFileImportType);
    files->insertMulti(type.url, dtype);

    return index;
}

int registerCompositeType(const QQmlPrivate::RegisterCompositeType &type)
{
    // Assumes URL is absolute and valid. Checking of user input should happen before the URL enters type.
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    QString typeName = QString::fromUtf8(type.typeName);
    bool fileImport = false;
    if (*(type.uri) == '\0')
        fileImport = true;
    if (!checkRegistration(QQmlType::CompositeType, data, fileImport?0:type.uri, typeName, type.versionMajor))
        return -1;

    int index = data->types.count();

    QQmlType *dtype = new QQmlType(index, typeName, type);
    data->types.append(dtype);
    addTypeToData(dtype, data);

    QQmlMetaTypeData::Files *files = fileImport ? &(data->urlToType) : &(data->urlToNonFileImportType);
    files->insertMulti(type.url, dtype);

    return index;
}

int registerQmlUnitCacheHook(const QQmlPrivate::RegisterQmlUnitCacheHook &hookRegistration)
{
    if (hookRegistration.version > 0)
        qFatal("qmlRegisterType(): Cannot mix incompatible QML versions.");
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    data->lookupCachedQmlUnit << hookRegistration.lookupCachedQmlUnit;
    return 0;
}

/*
This method is "over generalized" to allow us to (potentially) register more types of things in
the future without adding exported symbols.
*/
int QQmlPrivate::qmlregister(RegistrationType type, void *data)
{
    if (type == TypeRegistration) {
        return registerType(*reinterpret_cast<RegisterType *>(data));
    } else if (type == InterfaceRegistration) {
        return registerInterface(*reinterpret_cast<RegisterInterface *>(data));
    } else if (type == AutoParentRegistration) {
        return registerAutoParentFunction(*reinterpret_cast<RegisterAutoParent *>(data));
    } else if (type == SingletonRegistration) {
        return registerSingletonType(*reinterpret_cast<RegisterSingletonType *>(data));
    } else if (type == CompositeRegistration) {
        return registerCompositeType(*reinterpret_cast<RegisterCompositeType *>(data));
    } else if (type == CompositeSingletonRegistration) {
        return registerCompositeSingletonType(*reinterpret_cast<RegisterCompositeSingletonType *>(data));
    } else if (type == QmlUnitCacheHookRegistration) {
        return registerQmlUnitCacheHook(*reinterpret_cast<RegisterQmlUnitCacheHook *>(data));
    }
    return -1;
}

//From qqml.h
bool qmlProtectModule(const char *uri, int majVersion)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    QQmlMetaTypeData::VersionedUri versionedUri;
    versionedUri.uri = QString::fromUtf8(uri);
    versionedUri.majorVersion = majVersion;

    if (QQmlTypeModule* qqtm = data->uriToModule.value(versionedUri, 0)) {
        QQmlTypeModulePrivate::get(qqtm)->locked = true;
        return true;
    }
    return false;
}

bool QQmlMetaType::namespaceContainsRegistrations(const QString &uri, int majorVersion)
{
    QQmlMetaTypeData *data = metaTypeData();

    // Has any type previously been installed to this namespace?
    QHashedString nameSpace(uri);
    foreach (const QQmlType *type, data->types)
        if (type->module() == nameSpace && type->majorVersion() == majorVersion)
            return true;

    return false;
}

void QQmlMetaType::protectNamespace(const QString &uri)
{
    QQmlMetaTypeData *data = metaTypeData();

    data->protectedNamespaces.insert(uri);
}

void QQmlMetaType::setTypeRegistrationNamespace(const QString &uri)
{
    QQmlMetaTypeData *data = metaTypeData();

    data->typeRegistrationNamespace = uri;
    data->typeRegistrationFailures.clear();
}

QStringList QQmlMetaType::typeRegistrationFailures()
{
    QQmlMetaTypeData *data = metaTypeData();

    return data->typeRegistrationFailures;
}

QMutex *QQmlMetaType::typeRegistrationLock()
{
    return metaTypeDataLock();
}

/*
    Returns true if a module \a uri of any version is installed.
*/
bool QQmlMetaType::isAnyModule(const QString &uri)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    for (QQmlMetaTypeData::TypeModules::ConstIterator iter = data->uriToModule.cbegin();
         iter != data->uriToModule.cend(); ++iter) {
        if ((*iter)->module() == uri)
            return true;
    }

    return false;
}

/*
    Returns true if a module \a uri of this version is installed and locked;
*/
bool QQmlMetaType::isLockedModule(const QString &uri, int majVersion)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    QQmlMetaTypeData::VersionedUri versionedUri;
    versionedUri.uri = uri;
    versionedUri.majorVersion = majVersion;
    if (QQmlTypeModule* qqtm = data->uriToModule.value(versionedUri, 0))
        return QQmlTypeModulePrivate::get(qqtm)->locked;
    return false;
}

/*
    Returns true if any type or API has been registered for the given \a module with at least
    versionMajor.versionMinor, or if types have been registered for \a module with at most
    versionMajor.versionMinor.

    So if only 4.7 and 4.9 have been registered, 4.7,4.8, and 4.9 are valid, but not 4.6 nor 4.10.
*/
bool QQmlMetaType::isModule(const QString &module, int versionMajor, int versionMinor)
{
    Q_ASSERT(versionMajor >= 0 && versionMinor >= 0);
    QMutexLocker lock(metaTypeDataLock());

    QQmlMetaTypeData *data = metaTypeData();

    // first, check Types
    QQmlTypeModule *tm =
        data->uriToModule.value(QQmlMetaTypeData::VersionedUri(module, versionMajor));
    if (tm && tm->minimumMinorVersion() <= versionMinor && tm->maximumMinorVersion() >= versionMinor)
        return true;

    return false;
}

QQmlTypeModule *QQmlMetaType::typeModule(const QString &uri, int majorVersion)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    return data->uriToModule.value(QQmlMetaTypeData::VersionedUri(uri, majorVersion));
}

QList<QQmlPrivate::AutoParentFunction> QQmlMetaType::parentFunctions()
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    return data->parentFunctions;
}

QObject *QQmlMetaType::toQObject(const QVariant &v, bool *ok)
{
    if (!isQObject(v.userType())) {
        if (ok) *ok = false;
        return 0;
    }

    if (ok) *ok = true;

    return *(QObject *const *)v.constData();
}

bool QQmlMetaType::isQObject(int userType)
{
    if (userType == QMetaType::QObjectStar)
        return true;

    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    return userType >= 0 && userType < data->objects.size() && data->objects.testBit(userType);
}

/*
    Returns the item type for a list of type \a id.
 */
int QQmlMetaType::listType(int id)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    QQmlType *type = data->idToType.value(id);
    if (type && type->qListTypeId() == id)
        return type->typeId();
    else
        return 0;
}

int QQmlMetaType::attachedPropertiesFuncId(QQmlEnginePrivate *engine, const QMetaObject *mo)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    QQmlType *type = data->metaObjectToType.value(mo);
    if (type && type->attachedPropertiesFunction(engine))
        return type->attachedPropertiesId(engine);
    else
        return -1;
}

QQmlAttachedPropertiesFunc QQmlMetaType::attachedPropertiesFuncById(QQmlEnginePrivate *engine, int id)
{
    if (id < 0)
        return 0;
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    return data->types.at(id)->attachedPropertiesFunction(engine);
}

QMetaProperty QQmlMetaType::defaultProperty(const QMetaObject *metaObject)
{
    int idx = metaObject->indexOfClassInfo("DefaultProperty");
    if (-1 == idx)
        return QMetaProperty();

    QMetaClassInfo info = metaObject->classInfo(idx);
    if (!info.value())
        return QMetaProperty();

    idx = metaObject->indexOfProperty(info.value());
    if (-1 == idx)
        return QMetaProperty();

    return metaObject->property(idx);
}

QMetaProperty QQmlMetaType::defaultProperty(QObject *obj)
{
    if (!obj)
        return QMetaProperty();

    const QMetaObject *metaObject = obj->metaObject();
    return defaultProperty(metaObject);
}

QMetaMethod QQmlMetaType::defaultMethod(const QMetaObject *metaObject)
{
    int idx = metaObject->indexOfClassInfo("DefaultMethod");
    if (-1 == idx)
        return QMetaMethod();

    QMetaClassInfo info = metaObject->classInfo(idx);
    if (!info.value())
        return QMetaMethod();

    idx = metaObject->indexOfMethod(info.value());
    if (-1 == idx)
        return QMetaMethod();

    return metaObject->method(idx);
}

QMetaMethod QQmlMetaType::defaultMethod(QObject *obj)
{
    if (!obj)
        return QMetaMethod();

    const QMetaObject *metaObject = obj->metaObject();
    return defaultMethod(metaObject);
}

QQmlMetaType::TypeCategory QQmlMetaType::typeCategory(int userType)
{
    if (userType < 0)
        return Unknown;
    if (userType == QMetaType::QObjectStar)
        return Object;

    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    if (userType < data->objects.size() && data->objects.testBit(userType))
        return Object;
    else if (userType < data->lists.size() && data->lists.testBit(userType))
        return List;
    else
        return Unknown;
}

bool QQmlMetaType::isInterface(int userType)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    return userType >= 0 && userType < data->interfaces.size() && data->interfaces.testBit(userType);
}

const char *QQmlMetaType::interfaceIId(int userType)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    QQmlType *type = data->idToType.value(userType);
    lock.unlock();
    if (type && type->isInterface() && type->typeId() == userType)
        return type->interfaceIId();
    else
        return 0;
}

bool QQmlMetaType::isList(int userType)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();
    return userType >= 0 && userType < data->lists.size() && data->lists.testBit(userType);
}

/*!
    A custom string convertor allows you to specify a function pointer that
    returns a variant of \a type. For example, if you have written your own icon
    class that you want to support as an object property assignable in QML:

    \code
    int type = qRegisterMetaType<SuperIcon>("SuperIcon");
    QML::addCustomStringConvertor(type, &SuperIcon::pixmapFromString);
    \endcode

    The function pointer must be of the form:
    \code
    QVariant (*StringConverter)(const QString &);
    \endcode
 */
void QQmlMetaType::registerCustomStringConverter(int type, StringConverter converter)
{
    QMutexLocker lock(metaTypeDataLock());

    QQmlMetaTypeData *data = metaTypeData();
    if (data->stringConverters.contains(type))
        return;
    data->stringConverters.insert(type, converter);
}

/*!
    Return the custom string converter for \a type, previously installed through
    registerCustomStringConverter()
 */
QQmlMetaType::StringConverter QQmlMetaType::customStringConverter(int type)
{
    QMutexLocker lock(metaTypeDataLock());

    QQmlMetaTypeData *data = metaTypeData();
    return data->stringConverters.value(type);
}

/*!
    Returns the type (if any) of URI-qualified named \a qualifiedName and version specified
    by \a version_major and \a version_minor.
*/
QQmlType *QQmlMetaType::qmlType(const QString &qualifiedName, int version_major, int version_minor)
{
    int slash = qualifiedName.indexOf(QLatin1Char('/'));
    if (slash <= 0)
        return 0;

    QHashedStringRef module(qualifiedName.constData(), slash);
    QHashedStringRef name(qualifiedName.constData() + slash + 1, qualifiedName.length() - slash - 1);

    return qmlType(name, module, version_major, version_minor);
}

/*!
    Returns the type (if any) of \a name in \a module and version specified
    by \a version_major and \a version_minor.
*/
QQmlType *QQmlMetaType::qmlType(const QHashedStringRef &name, const QHashedStringRef &module, int version_major, int version_minor)
{
    Q_ASSERT(version_major >= 0 && version_minor >= 0);
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    QQmlMetaTypeData::Names::ConstIterator it = data->nameToType.constFind(name);
    while (it != data->nameToType.cend() && it.key() == name) {
        // XXX version_major<0 just a kludge for QQmlPropertyPrivate::initProperty
        if (version_major < 0 || module.isEmpty() || (*it)->availableInVersion(module, version_major,version_minor))
            return (*it);
        ++it;
    }

    return 0;
}

/*!
    Returns the type (if any) that corresponds to the \a metaObject.  Returns null if no
    type is registered.
*/
QQmlType *QQmlMetaType::qmlType(const QMetaObject *metaObject)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    return data->metaObjectToType.value(metaObject);
}

/*!
    Returns the type (if any) that corresponds to the \a metaObject in version specified
    by \a version_major and \a version_minor in module specified by \a uri.  Returns null if no
    type is registered.
*/
QQmlType *QQmlMetaType::qmlType(const QMetaObject *metaObject, const QHashedStringRef &module, int version_major, int version_minor)
{
    Q_ASSERT(version_major >= 0 && version_minor >= 0);
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    QQmlMetaTypeData::MetaObjects::const_iterator it = data->metaObjectToType.constFind(metaObject);
    while (it != data->metaObjectToType.cend() && it.key() == metaObject) {
        QQmlType *t = *it;
        if (version_major < 0 || module.isEmpty() || t->availableInVersion(module, version_major,version_minor))
            return t;
        ++it;
    }

    return 0;
}

/*!
    Returns the type (if any) that corresponds to the QVariant::Type \a userType.
    Returns null if no type is registered.
*/
QQmlType *QQmlMetaType::qmlType(int userType)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    QQmlType *type = data->idToType.value(userType);
    if (type && type->typeId() == userType)
        return type;
    else
        return 0;
}

/*!
    Returns the type (if any) that corresponds to the given \a url in the set of
    composite types added through file imports.

    Returns null if no such type is registered.
*/
QQmlType *QQmlMetaType::qmlType(const QUrl &url, bool includeNonFileImports /* = false */)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    QQmlType *type = data->urlToType.value(url);
    if (!type && includeNonFileImports)
        type = data->urlToNonFileImportType.value(url);

    if (type && type->sourceUrl() == url)
        return type;
    else
        return 0;
}

/*!
    Returns the type (if any) with the given \a index in the global type store.
    This is for use when you just got the index back from a qmlRegister function.
    Returns null if the index is out of bounds.
*/
QQmlType *QQmlMetaType::qmlTypeFromIndex(int idx)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    if (idx < 0 || idx >= data->types.count())
            return 0;
    return data->types.at(idx);
}

/*!
    Returns the list of registered QML type names.
*/
QList<QString> QQmlMetaType::qmlTypeNames()
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    QList<QString> names;
    names.reserve(data->nameToType.count());
    QQmlMetaTypeData::Names::ConstIterator it = data->nameToType.cbegin();
    while (it != data->nameToType.cend()) {
        names += (*it)->qmlTypeName();
        ++it;
    }

    return names;
}

/*!
    Returns the list of registered QML types.
*/
QList<QQmlType*> QQmlMetaType::qmlTypes()
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    return data->nameToType.values();
}

/*!
    Returns the list of all registered types.
*/
QList<QQmlType*> QQmlMetaType::qmlAllTypes()
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    return data->types;
}

/*!
    Returns the list of registered QML singleton types.
*/
QList<QQmlType*> QQmlMetaType::qmlSingletonTypes()
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    QList<QQmlType*> retn;
    for (const auto type : qAsConst(data->nameToType)) {
        if (type->isSingleton())
            retn.append(type);
    }
    return retn;
}

const QQmlPrivate::CachedQmlUnit *QQmlMetaType::findCachedCompilationUnit(const QUrl &uri)
{
    QMutexLocker lock(metaTypeDataLock());
    QQmlMetaTypeData *data = metaTypeData();

    for (const auto lookup : qAsConst(data->lookupCachedQmlUnit)) {
        if (const QQmlPrivate::CachedQmlUnit *unit = lookup(uri))
            return unit;
    }
    return 0;
}

/*!
    Returns the pretty QML type name (e.g. 'Item' instead of 'QtQuickItem') for the given object.
 */
QString QQmlMetaType::prettyTypeName(const QObject *object)
{
    QString typeName;

    if (!object)
        return typeName;

    const QQmlType *type = QQmlMetaType::qmlType(object->metaObject());
    if (type) {
        typeName = type->qmlTypeName();
        const int lastSlash = typeName.lastIndexOf(QLatin1Char('/'));
        if (lastSlash != -1)
            typeName = typeName.mid(lastSlash + 1);
    } else {
        typeName = QString::fromUtf8(object->metaObject()->className());
        int marker = typeName.indexOf(QLatin1String("_QMLTYPE_"));
        if (marker != -1)
            typeName = typeName.left(marker);

        marker = typeName.indexOf(QLatin1String("_QML_"));
        if (marker != -1) {
            typeName = typeName.left(marker) + QLatin1Char('*');
            type = QQmlMetaType::qmlType(QMetaType::type(typeName.toLatin1()));
            if (type) {
                typeName = type->qmlTypeName();
                const int lastSlash = typeName.lastIndexOf(QLatin1Char('/'));
                if (lastSlash != -1)
                    typeName = typeName.mid(lastSlash + 1);
            }
        }
    }

    return typeName;
}

QT_END_NAMESPACE
