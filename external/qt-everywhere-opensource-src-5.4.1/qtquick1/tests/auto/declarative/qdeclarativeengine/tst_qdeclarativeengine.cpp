/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <qtest.h>
#include <QDeclarativeEngine>
#include <QDeclarativeContext>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QDeclarativeComponent>
#include <QDeclarativeNetworkAccessManagerFactory>

class tst_qdeclarativeengine : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativeengine() {}

private slots:
    void rootContext();
    void networkAccessManager();
    void baseUrl();
    void contextForObject();
    void offlineStoragePath();
    void clearComponentCache();
    void outputWarningsToStandardError();
    void objectOwnership();
    void synchronousNetworkReply();
};

void tst_qdeclarativeengine::rootContext()
{
    QDeclarativeEngine engine;

    QVERIFY(engine.rootContext());

    QCOMPARE(engine.rootContext()->engine(), &engine);
    QVERIFY(engine.rootContext()->parentContext() == 0);
}

class NetworkAccessManagerFactory : public QDeclarativeNetworkAccessManagerFactory
{
public:
    NetworkAccessManagerFactory() : manager(0) {}

    QNetworkAccessManager *create(QObject *parent) {
        manager = new QNetworkAccessManager(parent);
        return manager;
    }

    QNetworkAccessManager *manager;
};

void tst_qdeclarativeengine::networkAccessManager()
{
    QDeclarativeEngine *engine = new QDeclarativeEngine;

    // Test QDeclarativeEngine created manager
    QPointer<QNetworkAccessManager> manager = engine->networkAccessManager();
    QVERIFY(manager != 0);
    delete engine;

    // Test factory created manager
    engine = new QDeclarativeEngine;
    NetworkAccessManagerFactory factory;
    engine->setNetworkAccessManagerFactory(&factory);
    QVERIFY(engine->networkAccessManagerFactory() == &factory);
    QVERIFY(engine->networkAccessManager() == factory.manager);
    delete engine;
}

void tst_qdeclarativeengine::baseUrl()
{
    QDeclarativeEngine engine;

    QUrl cwd = QUrl::fromLocalFile(QDir::currentPath() + QDir::separator());

    QCOMPARE(engine.baseUrl(), cwd);
    QCOMPARE(engine.rootContext()->resolvedUrl(QUrl("main.qml")), cwd.resolved(QUrl("main.qml")));

    QDir dir = QDir::current();
    dir.cdUp();
    QVERIFY(dir != QDir::current());
    QDir::setCurrent(dir.path());
    QVERIFY(QDir::current() == dir);

    QUrl cwd2 = QUrl::fromLocalFile(QDir::currentPath() + QDir::separator());
    QCOMPARE(engine.baseUrl(), cwd2);
    QCOMPARE(engine.rootContext()->resolvedUrl(QUrl("main.qml")), cwd2.resolved(QUrl("main.qml")));

    engine.setBaseUrl(cwd);
    QCOMPARE(engine.baseUrl(), cwd);
    QCOMPARE(engine.rootContext()->resolvedUrl(QUrl("main.qml")), cwd.resolved(QUrl("main.qml")));
}

void tst_qdeclarativeengine::contextForObject()
{
    QDeclarativeEngine *engine = new QDeclarativeEngine;

    // Test null-object
    QVERIFY(QDeclarativeEngine::contextForObject(0) == 0);

    // Test an object with no context
    QObject object;
    QVERIFY(QDeclarativeEngine::contextForObject(&object) == 0);

    // Test setting null-object
    QDeclarativeEngine::setContextForObject(0, engine->rootContext());

    // Test setting null-context
    QDeclarativeEngine::setContextForObject(&object, 0);

    // Test setting context
    QDeclarativeEngine::setContextForObject(&object, engine->rootContext());
    QVERIFY(QDeclarativeEngine::contextForObject(&object) == engine->rootContext());

    QDeclarativeContext context(engine->rootContext());

    // Try changing context
    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeEngine::setContextForObject(): Object already has a QDeclarativeContext");
    QDeclarativeEngine::setContextForObject(&object, &context);
    QVERIFY(QDeclarativeEngine::contextForObject(&object) == engine->rootContext());

    // Delete context
    delete engine; engine = 0;
    QVERIFY(QDeclarativeEngine::contextForObject(&object) == 0);
}

void tst_qdeclarativeengine::offlineStoragePath()
{
    // Without these set, QDesktopServices::storageLocation returns
    // strings with extra "//" at the end. We set them to ignore this problem.
    qApp->setApplicationName("tst_qdeclarativeengine");
    qApp->setOrganizationName("QtProject");
    qApp->setOrganizationDomain("www.qt-project.org");

    QDeclarativeEngine engine;

    QDir dir(QStandardPaths::standardLocations(QStandardPaths::DataLocation).front());
    dir.mkpath("QML");
    dir.cd("QML");
    dir.mkpath("OfflineStorage");
    dir.cd("OfflineStorage");

    QCOMPARE(QDir::fromNativeSeparators(engine.offlineStoragePath()), dir.path());

    engine.setOfflineStoragePath(QDir::homePath());
    QCOMPARE(engine.offlineStoragePath(), QDir::homePath());
}

void tst_qdeclarativeengine::clearComponentCache()
{
    QDeclarativeEngine engine;

    // Create original qml file
    {
        QFile file("temp.qml");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("import QtQuick 1.0\nQtObject {\nproperty int test: 10\n}\n");
        file.close();
    }

    // Test "test" property
    {
        QDeclarativeComponent component(&engine, "temp.qml");
        QObject *obj = component.create();
        QVERIFY(obj != 0);
        QCOMPARE(obj->property("test").toInt(), 10);
        delete obj;
    }

    // Modify qml file
    {
        QFile file("temp.qml");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("import QtQuick 1.0\nQtObject {\nproperty int test: 11\n}\n");
        file.close();
    }

    // Test cache hit
    {
        QDeclarativeComponent component(&engine, "temp.qml");
        QObject *obj = component.create();
        QVERIFY(obj != 0);
        QCOMPARE(obj->property("test").toInt(), 10);
        delete obj;
    }

    // Clear cache
    engine.clearComponentCache();

    // Test cache refresh
    {
        QDeclarativeComponent component(&engine, "temp.qml");
        QObject *obj = component.create();
        QVERIFY(obj != 0);
        QCOMPARE(obj->property("test").toInt(), 11);
        delete obj;
    }
}

static QStringList warnings;
static void msgHandler(QtMsgType, const QMessageLogContext &, const QString &warning)
{
    warnings << warning;
}

void tst_qdeclarativeengine::outputWarningsToStandardError()
{
    QDeclarativeEngine engine;

    QCOMPARE(engine.outputWarningsToStandardError(), true);

    QDeclarativeComponent c(&engine);
    c.setData("import QtQuick 1.0; QtObject { property int a: undefined }", QUrl());

    QVERIFY(c.isReady() == true);

    warnings.clear();
    QtMessageHandler old = qInstallMessageHandler(msgHandler);

    QObject *o = c.create();

    qInstallMessageHandler(old);

    QVERIFY(o != 0);
    delete o;

    QCOMPARE(warnings.count(), 1);
    QCOMPARE(warnings.at(0), QLatin1String("<Unknown File>:1: Unable to assign [undefined] to int a"));
    warnings.clear();


    engine.setOutputWarningsToStandardError(false);
    QCOMPARE(engine.outputWarningsToStandardError(), false);


    old = qInstallMessageHandler(msgHandler);

    o = c.create();

    qInstallMessageHandler(old);

    QVERIFY(o != 0);
    delete o;

    QCOMPARE(warnings.count(), 0);
}

void tst_qdeclarativeengine::objectOwnership()
{
    {
    QCOMPARE(QDeclarativeEngine::objectOwnership(0), QDeclarativeEngine::CppOwnership);
    QDeclarativeEngine::setObjectOwnership(0, QDeclarativeEngine::JavaScriptOwnership);
    QCOMPARE(QDeclarativeEngine::objectOwnership(0), QDeclarativeEngine::CppOwnership);
    }

    {
    QObject o;
    QCOMPARE(QDeclarativeEngine::objectOwnership(&o), QDeclarativeEngine::CppOwnership);
    QDeclarativeEngine::setObjectOwnership(&o, QDeclarativeEngine::CppOwnership);
    QCOMPARE(QDeclarativeEngine::objectOwnership(&o), QDeclarativeEngine::CppOwnership);
    QDeclarativeEngine::setObjectOwnership(&o, QDeclarativeEngine::JavaScriptOwnership);
    QCOMPARE(QDeclarativeEngine::objectOwnership(&o), QDeclarativeEngine::JavaScriptOwnership);
    QDeclarativeEngine::setObjectOwnership(&o, QDeclarativeEngine::CppOwnership);
    QCOMPARE(QDeclarativeEngine::objectOwnership(&o), QDeclarativeEngine::CppOwnership);
    }

    {
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine);
    c.setData("import QtQuick 1.0; QtObject { property QtObject object: QtObject {} }", QUrl());

    QObject *o = c.create();
    QVERIFY(o != 0);

    QCOMPARE(QDeclarativeEngine::objectOwnership(o), QDeclarativeEngine::CppOwnership);

    QObject *o2 = qvariant_cast<QObject *>(o->property("object"));
    QCOMPARE(QDeclarativeEngine::objectOwnership(o2), QDeclarativeEngine::JavaScriptOwnership);

    delete o;
    }

}

class MyReply : public QNetworkReply {

    Q_OBJECT

public:
    MyReply() {
        setFinished(true);
    }
    virtual qint64 readData(char* buffer, qint64 number) {
        Q_UNUSED(buffer)
        Q_UNUSED(number)
        return 0;
    }
    virtual void abort() { }
};

class MyManager : public QNetworkAccessManager {

    Q_OBJECT

public:
    MyManager(QObject *parent = 0) : QNetworkAccessManager(parent) {
    }

    QNetworkReply *createRequest(Operation op, const QNetworkRequest & req, QIODevice * outgoingData = 0) {
        Q_UNUSED(op)
        Q_UNUSED(req)
        Q_UNUSED(outgoingData)
        return new MyReply;
    }
};

class MyFactory : public QDeclarativeNetworkAccessManagerFactory {

public:
    QNetworkAccessManager *create(QObject *parent) {
        return new MyManager(parent);
    }
};

void tst_qdeclarativeengine::synchronousNetworkReply()
{
    MyFactory factory;
    QDeclarativeEngine engine;
    engine.setNetworkAccessManagerFactory(&factory);
    QDeclarativeComponent c(&engine, QUrl("myScheme://test.qml"));
    // we get an error, but we only care about whether we are finished or not in this test
    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeComponent: Component is not ready");
    c.create();
    // reply is finished, so should not be in loading state.
    QVERIFY(!c.isLoading());
}

QTEST_MAIN(tst_qdeclarativeengine)

#include "tst_qdeclarativeengine.moc"
