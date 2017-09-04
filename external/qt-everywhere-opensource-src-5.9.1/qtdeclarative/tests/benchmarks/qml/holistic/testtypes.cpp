/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "testtypes.h"
#include <QQmlEngine>

static QJSValue script_api(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    static int testProperty = 13;
    QJSValue v = scriptEngine->newObject();
    v.setProperty("scriptTestProperty", testProperty++);
    return v;
}

static QObject *qobject_api(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    testQObjectApi *o = new testQObjectApi;
    o->setQObjectTestProperty(20);
    return o;
}

static QObject *qobject_api_engine_parent(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(scriptEngine)

    static int testProperty = 26;
    testQObjectApi *o = new testQObjectApi(engine);
    o->setQObjectTestProperty(testProperty++);
    return o;
}

void registerTypes()
{
    qmlRegisterType<MyQmlObject>("Qt.test", 1,0, "MyQmlObjectAlias");
    qmlRegisterType<MyQmlObject>("Qt.test", 1,0, "MyQmlObject");

    qRegisterMetaType<MyQmlObject::MyType>("MyQmlObject::MyType");

    qmlRegisterType<ScarceResourceProvider>("Qt.test", 1,0, "MyScarceResourceProvider");
    qmlRegisterType<ArbitraryVariantProvider>("Qt.test", 1,0, "MyArbitraryVariantProvider");

    qmlRegisterSingletonType("Qt.test",1,0,"Script",script_api);             // register (script) singleton Type for an existing uri which contains elements
    qmlRegisterSingletonType<testQObjectApi>("Qt.test",1,0,"QObject",qobject_api);            // register (qobject) for an existing uri for which another singleton Type was previously regd.  Should replace!
    qmlRegisterSingletonType("Qt.test.scriptApi",1,0,"Script",script_api);   // register (script) singleton Type for a uri which doesn't contain elements
    qmlRegisterSingletonType<testQObjectApi>("Qt.test.qobjectApi",1,0,"QObject",qobject_api); // register (qobject) singleton Type for a uri which doesn't contain elements
    qmlRegisterSingletonType<testQObjectApi>("Qt.test.qobjectApi",1,3,"QObject",qobject_api); // register (qobject) singleton Type for a uri which doesn't contain elements, minor version set
    qmlRegisterSingletonType<testQObjectApi>("Qt.test.qobjectApi",2,0,"QObject",qobject_api); // register (qobject) singleton Type for a uri which doesn't contain elements, major version set
    qmlRegisterSingletonType<testQObjectApi>("Qt.test.qobjectApiParented",1,0,"QObject",qobject_api_engine_parent); // register (parented qobject) singleton Type for a uri which doesn't contain elements
}

//#include "testtypes.moc"
