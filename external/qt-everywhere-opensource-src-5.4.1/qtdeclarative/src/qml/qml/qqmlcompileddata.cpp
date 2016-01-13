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

#include "qqmlcompiler_p.h"
#include "qqmlengine.h"
#include "qqmlcomponent.h"
#include "qqmlcomponent_p.h"
#include "qqmlcontext.h"
#include "qqmlcontext_p.h"
#include "qqmlpropertymap.h"
#ifdef QML_THREADED_VME_INTERPRETER
#include "qqmlvme_p.h"
#endif

#include <QtCore/qdebug.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

QQmlCompiledData::QQmlCompiledData(QQmlEngine *engine)
: engine(engine), importCache(0), metaTypeId(-1), listMetaTypeId(-1), isRegisteredWithEngine(false),
  rootPropertyCache(0), totalBindingsCount(0), totalParserStatusCount(0)
{
    Q_ASSERT(engine);
}

void QQmlCompiledData::destroy()
{
    if (engine && hasEngine())
        QQmlEnginePrivate::deleteInEngineThread(engine, this);
    else
        delete this;
}

QQmlCompiledData::~QQmlCompiledData()
{
    if (isRegisteredWithEngine)
        QQmlEnginePrivate::get(engine)->unregisterInternalCompositeType(this);

    clear();

    for (QHash<int, TypeReference*>::Iterator resolvedType = resolvedTypes.begin(), end = resolvedTypes.end();
         resolvedType != end; ++resolvedType) {
        if ((*resolvedType)->component)
            (*resolvedType)->component->release();
        if ((*resolvedType)->typePropertyCache)
            (*resolvedType)->typePropertyCache->release();
    }
    qDeleteAll(resolvedTypes);
    resolvedTypes.clear();

    for (int ii = 0; ii < propertyCaches.count(); ++ii)
        if (propertyCaches.at(ii))
            propertyCaches.at(ii)->release();

    for (int ii = 0; ii < scripts.count(); ++ii)
        scripts.at(ii)->release();

    if (importCache)
        importCache->release();

    if (rootPropertyCache)
        rootPropertyCache->release();
}

void QQmlCompiledData::clear()
{
}

/*!
Returns the property cache, if one alread exists.  The cache is not referenced.
*/
QQmlPropertyCache *QQmlCompiledData::TypeReference::propertyCache() const
{
    if (type)
        return typePropertyCache;
    else
        return component->rootPropertyCache;
}

/*!
Returns the property cache, creating one if it doesn't already exist.  The cache is not referenced.
*/
QQmlPropertyCache *QQmlCompiledData::TypeReference::createPropertyCache(QQmlEngine *engine)
{
    if (typePropertyCache) {
        return typePropertyCache;
    } else if (type) {
        typePropertyCache = QQmlEnginePrivate::get(engine)->cache(type->metaObject());
        typePropertyCache->addref();
        return typePropertyCache;
    } else {
        return component->rootPropertyCache;
    }
}

template <typename T>
bool qtTypeInherits(const QMetaObject *mo) {
    while (mo) {
        if (mo == &T::staticMetaObject)
            return true;
        mo = mo->superClass();
    }
    return false;
}

void QQmlCompiledData::TypeReference::doDynamicTypeCheck()
{
    const QMetaObject *mo = 0;
    if (typePropertyCache)
        mo = typePropertyCache->firstCppMetaObject();
    else if (type)
        mo = type->metaObject();
    else if (component)
        mo = component->rootPropertyCache->firstCppMetaObject();
    isFullyDynamicType = qtTypeInherits<QQmlPropertyMap>(mo);
}

void QQmlCompiledData::initialize(QQmlEngine *engine)
{
    Q_ASSERT(!hasEngine());
    QQmlCleanup::addToEngine(engine);
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(engine);
    if (compilationUnit && !compilationUnit->engine)
        compilationUnit->linkToEngine(v4);
}

QT_END_NAMESPACE
