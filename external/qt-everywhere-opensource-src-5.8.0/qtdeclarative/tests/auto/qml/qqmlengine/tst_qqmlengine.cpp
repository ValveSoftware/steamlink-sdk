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

#include "../../shared/util.h"
#include <QQmlEngine>
#include <QQmlContext>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QDir>
#include <QStandardPaths>
#include <QSignalSpy>
#include <QDebug>
#include <QBuffer>
#include <QQmlComponent>
#include <QQmlNetworkAccessManagerFactory>
#include <QQmlExpression>
#include <QQmlIncubationController>
#include <private/qqmlengine_p.h>
#include <QQmlAbstractUrlInterceptor>

class tst_qqmlengine : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlengine() {}

private slots:
    void rootContext();
    void networkAccessManager();
    void synchronousNetworkAccessManager();
    void baseUrl();
    void contextForObject();
    void offlineStoragePath();
    void clearComponentCache();
    void trimComponentCache();
    void trimComponentCache_data();
    void repeatedCompilation();
    void failedCompilation();
    void failedCompilation_data();
    void outputWarningsToStandardError();
    void objectOwnership();
    void multipleEngines();
    void qtqmlModule_data();
    void qtqmlModule();
    void urlInterceptor_data();
    void urlInterceptor();

    void qmlContextProperties();

public slots:
    QObject *createAQObjectForOwnershipTest ()
    {
        static QObject *ptr = new QObject();
        return ptr;
    }
};

void tst_qqmlengine::rootContext()
{
    QQmlEngine engine;

    QVERIFY(engine.rootContext());

    QCOMPARE(engine.rootContext()->engine(), &engine);
    QVERIFY(!engine.rootContext()->parentContext());
}

class NetworkAccessManagerFactory : public QQmlNetworkAccessManagerFactory
{
public:
    NetworkAccessManagerFactory() : manager(0) {}

    QNetworkAccessManager *create(QObject *parent) {
        manager = new QNetworkAccessManager(parent);
        return manager;
    }

    QNetworkAccessManager *manager;
};

void tst_qqmlengine::networkAccessManager()
{
    QQmlEngine *engine = new QQmlEngine;

    // Test QQmlEngine created manager
    QPointer<QNetworkAccessManager> manager = engine->networkAccessManager();
    QVERIFY(manager != 0);
    delete engine;

    // Test factory created manager
    engine = new QQmlEngine;
    NetworkAccessManagerFactory factory;
    engine->setNetworkAccessManagerFactory(&factory);
    QCOMPARE(engine->networkAccessManagerFactory(), &factory);
    QNetworkAccessManager *engineNam = engine->networkAccessManager(); // calls NetworkAccessManagerFactory::create()
    QCOMPARE(engineNam, factory.manager);
    delete engine;
}

class ImmediateReply : public QNetworkReply {

    Q_OBJECT

public:
    ImmediateReply() {
        setFinished(true);
    }
    virtual qint64 readData(char* , qint64 ) {
        return 0;
    }
    virtual void abort() { }
};

class ImmediateManager : public QNetworkAccessManager {

    Q_OBJECT

public:
    ImmediateManager(QObject *parent = 0) : QNetworkAccessManager(parent) {
    }

    QNetworkReply *createRequest(Operation, const QNetworkRequest & , QIODevice * outgoingData = 0) {
        Q_UNUSED(outgoingData);
        return new ImmediateReply;
    }
};

class ImmediateFactory : public QQmlNetworkAccessManagerFactory {

public:
    QNetworkAccessManager *create(QObject *) {
        return new ImmediateManager;
    }
};

void tst_qqmlengine::synchronousNetworkAccessManager()
{
    ImmediateFactory factory;
    QQmlEngine engine;
    engine.setNetworkAccessManagerFactory(&factory);
    QQmlComponent c(&engine, QUrl("myScheme://test.qml"));
    // reply is finished, so should not be in loading state.
    QVERIFY(!c.isLoading());
}


void tst_qqmlengine::baseUrl()
{
    QQmlEngine engine;

    QUrl cwd = QUrl::fromLocalFile(QDir::currentPath() + QDir::separator());

    QCOMPARE(engine.baseUrl(), cwd);
    QCOMPARE(engine.rootContext()->resolvedUrl(QUrl("main.qml")), cwd.resolved(QUrl("main.qml")));

    QDir dir = QDir::current();
    dir.cdUp();
    QVERIFY(dir != QDir::current());
    QDir::setCurrent(dir.path());
    QCOMPARE(QDir::current(), dir);

    QUrl cwd2 = QUrl::fromLocalFile(QDir::currentPath() + QDir::separator());
    QCOMPARE(engine.baseUrl(), cwd2);
    QCOMPARE(engine.rootContext()->resolvedUrl(QUrl("main.qml")), cwd2.resolved(QUrl("main.qml")));

    engine.setBaseUrl(cwd);
    QCOMPARE(engine.baseUrl(), cwd);
    QCOMPARE(engine.rootContext()->resolvedUrl(QUrl("main.qml")), cwd.resolved(QUrl("main.qml")));
}

void tst_qqmlengine::contextForObject()
{
    QQmlEngine *engine = new QQmlEngine;

    // Test null-object
    QVERIFY(!QQmlEngine::contextForObject(0));

    // Test an object with no context
    QObject object;
    QVERIFY(!QQmlEngine::contextForObject(&object));

    // Test setting null-object
    QQmlEngine::setContextForObject(0, engine->rootContext());

    // Test setting null-context
    QQmlEngine::setContextForObject(&object, 0);

    // Test setting context
    QQmlEngine::setContextForObject(&object, engine->rootContext());
    QCOMPARE(QQmlEngine::contextForObject(&object), engine->rootContext());

    QQmlContext context(engine->rootContext());

    // Try changing context
    QTest::ignoreMessage(QtWarningMsg, "QQmlEngine::setContextForObject(): Object already has a QQmlContext");
    QQmlEngine::setContextForObject(&object, &context);
    QCOMPARE(QQmlEngine::contextForObject(&object), engine->rootContext());

    // Delete context
    delete engine; engine = 0;
    QVERIFY(!QQmlEngine::contextForObject(&object));
}

void tst_qqmlengine::offlineStoragePath()
{
    // Without these set, QDesktopServices::storageLocation returns
    // strings with extra "//" at the end. We set them to ignore this problem.
    qApp->setApplicationName("tst_qqmlengine");
    qApp->setOrganizationName("QtProject");
    qApp->setOrganizationDomain("www.qt-project.org");

    QQmlEngine engine;

    QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    QCOMPARE(dataLocation.isEmpty(), engine.offlineStoragePath().isEmpty());

    QDir dir(dataLocation);
    dir.mkpath("QML");
    dir.cd("QML");
    dir.mkpath("OfflineStorage");
    dir.cd("OfflineStorage");

    QCOMPARE(QDir::fromNativeSeparators(engine.offlineStoragePath()), dir.path());

    engine.setOfflineStoragePath(QDir::homePath());
    QCOMPARE(engine.offlineStoragePath(), QDir::homePath());
}

void tst_qqmlengine::clearComponentCache()
{
    QQmlEngine engine;

    // Create original qml file
    {
        QFile file("temp.qml");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("import QtQuick 2.0\nQtObject {\nproperty int test: 10\n}\n");
        file.close();
    }

    // Test "test" property
    {
        QQmlComponent component(&engine, "temp.qml");
        QObject *obj = component.create();
        QVERIFY(obj != 0);
        QCOMPARE(obj->property("test").toInt(), 10);
        delete obj;
    }

    // Modify qml file
    {
        // On macOS with HFS+ the precision of file times is measured in seconds, so to ensure that
        // the newly written file has a modification date newer than an existing cache file, we must
        // wait.
        // Similar effects of lacking precision have been observed on some Linux systems.
        QThread::sleep(1);

        QFile file("temp.qml");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("import QtQuick 2.0\nQtObject {\nproperty int test: 11\n}\n");
        file.close();
    }

    // Test cache hit
    {
        QQmlComponent component(&engine, "temp.qml");
        QObject *obj = component.create();
        QVERIFY(obj != 0);
        QCOMPARE(obj->property("test").toInt(), 10);
        delete obj;
    }

    // Clear cache
    engine.clearComponentCache();

    // Test cache refresh
    {
        QQmlComponent component(&engine, "temp.qml");
        QObject *obj = component.create();
        QVERIFY(obj != 0);
        QCOMPARE(obj->property("test").toInt(), 11);
        delete obj;
    }
}

struct ComponentCacheFunctions : public QObject, public QQmlIncubationController
{
    Q_OBJECT
public:
    QQmlEngine *engine;

    ComponentCacheFunctions(QQmlEngine &e) : engine(&e) {}

    Q_INVOKABLE void trim()
    {
        // Wait for any pending deletions to occur
        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
        QCoreApplication::processEvents();

        // There might be JS function objects around that hold a last ref to the compilation unit that's
        // keeping the type compilation data (CompilationUnit) around. Let's collect them as well so that
        // trim works well.
        engine->collectGarbage();

        engine->trimComponentCache();
    }

    Q_INVOKABLE bool isTypeLoaded(QString file)
    {
        return QQmlEnginePrivate::get(engine)->isTypeLoaded(tst_qqmlengine::instance()->testFileUrl(file));
    }

    Q_INVOKABLE bool isScriptLoaded(QString file)
    {
        return QQmlEnginePrivate::get(engine)->isScriptLoaded(tst_qqmlengine::instance()->testFileUrl(file));
    }

    Q_INVOKABLE void beginIncubation()
    {
        startTimer(0);
    }

    Q_INVOKABLE void waitForIncubation()
    {
        while (incubatingObjectCount() > 0) {
            QCoreApplication::processEvents();
        }
    }

private:
    virtual void timerEvent(QTimerEvent *)
    {
        incubateFor(1000);
    }
};

void tst_qqmlengine::trimComponentCache()
{
    QFETCH(QString, file);

    QQmlEngine engine;
    ComponentCacheFunctions componentCache(engine);
    engine.rootContext()->setContextProperty("componentCache", &componentCache);
    engine.setIncubationController(&componentCache);

    QQmlComponent component(&engine, testFileUrl(file));
    QVERIFY(component.isReady());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->property("success").toBool(), true);
}

void tst_qqmlengine::trimComponentCache_data()
{
    QTest::addColumn<QString>("file");

    // The various tests here are for two types of components: those that are
    // empty apart from their inherited elements, and those that define new properties.
    // For each there are five types of composition: extension, aggregation,
    // aggregation via component, property and object-created-via-transient-component.
    foreach (const QString &test, (QStringList() << "EmptyComponent"
                                                 << "VMEComponent"
                                                 << "EmptyExtendEmptyComponent"
                                                 << "VMEExtendEmptyComponent"
                                                 << "EmptyExtendVMEComponent"
                                                 << "VMEExtendVMEComponent"
                                                 << "EmptyAggregateEmptyComponent"
                                                 << "VMEAggregateEmptyComponent"
                                                 << "EmptyAggregateVMEComponent"
                                                 << "VMEAggregateVMEComponent"
                                                 << "EmptyPropertyEmptyComponent"
                                                 << "VMEPropertyEmptyComponent"
                                                 << "EmptyPropertyVMEComponent"
                                                 << "VMEPropertyVMEComponent"
                                                 << "VMETransientEmptyComponent"
                                                 << "VMETransientVMEComponent")) {
        // For these cases, we first test that the component instance keeps the components
        // referenced, and then that the instantiated object keeps the components referenced
        for (int i = 1; i <= 2; ++i) {
            QString name(QString("%1-%2").arg(test).arg(i));
            QString file(QString("test%1.%2.qml").arg(test).arg(i));
            QTest::newRow(name.toLatin1().constData()) << file;
        }
    }

    // Test that a transient component is correctly referenced
    QTest::newRow("TransientComponent-1") << "testTransientComponent.1.qml";
    QTest::newRow("TransientComponent-2") << "testTransientComponent.2.qml";

    // Test that components can be reloaded after unloading
    QTest::newRow("ReloadComponent") << "testReloadComponent.qml";

    // Test that components are correctly referenced when dynamically loaded
    QTest::newRow("LoaderComponent") << "testLoaderComponent.qml";

    // Test that components are correctly referenced when incubated
    QTest::newRow("IncubatedComponent") << "testIncubatedComponent.qml";

    // Test that a top-level omponents is correctly referenced
    QTest::newRow("TopLevelComponent") << "testTopLevelComponent.qml";

    // TODO:
    // Test that scripts are unloaded when no longer referenced
    QTest::newRow("ScriptComponent") << "testScriptComponent.qml";
}

void tst_qqmlengine::repeatedCompilation()
{
    QQmlEngine engine;

    for (int i = 0; i < 100; ++i) {
        engine.collectGarbage();
        engine.trimComponentCache();

        QQmlComponent component(&engine, testFileUrl("repeatedCompilation.qml"));
        QVERIFY(component.isReady());
        QScopedPointer<QObject> object(component.create());
        QVERIFY(object != 0);
        QCOMPARE(object->property("success").toBool(), true);
    }
}

void tst_qqmlengine::failedCompilation()
{
    QFETCH(QString, file);

    QQmlEngine engine;

    QQmlComponent component(&engine, testFileUrl(file));
    QVERIFY(!component.isReady());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(object.isNull());

    engine.collectGarbage();
    engine.trimComponentCache();
    engine.clearComponentCache();
}

void tst_qqmlengine::failedCompilation_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("Invalid URL") << "failedCompilation.does.not.exist.qml";
    QTest::newRow("Invalid content") << "failedCompilation.1.qml";
}

void tst_qqmlengine::outputWarningsToStandardError()
{
    QQmlEngine engine;

    QCOMPARE(engine.outputWarningsToStandardError(), true);

    QQmlComponent c(&engine);
    c.setData("import QtQuick 2.0; QtObject { property int a: undefined }", QUrl());

    QVERIFY(c.isReady());

    QQmlTestMessageHandler messageHandler;

    QObject *o = c.create();

    QVERIFY(o != 0);
    delete o;

    QCOMPARE(messageHandler.messages().count(), 1);
    QCOMPARE(messageHandler.messages().at(0), QLatin1String("<Unknown File>:1:48: Unable to assign [undefined] to int"));
    messageHandler.clear();

    engine.setOutputWarningsToStandardError(false);
    QCOMPARE(engine.outputWarningsToStandardError(), false);

    o = c.create();

    QVERIFY(o != 0);
    delete o;

    QVERIFY2(messageHandler.messages().isEmpty(), qPrintable(messageHandler.messageString()));
}

void tst_qqmlengine::objectOwnership()
{
    {
    QCOMPARE(QQmlEngine::objectOwnership(0), QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(0, QQmlEngine::JavaScriptOwnership);
    QCOMPARE(QQmlEngine::objectOwnership(0), QQmlEngine::CppOwnership);
    }

    {
    QObject o;
    QCOMPARE(QQmlEngine::objectOwnership(&o), QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&o, QQmlEngine::CppOwnership);
    QCOMPARE(QQmlEngine::objectOwnership(&o), QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&o, QQmlEngine::JavaScriptOwnership);
    QCOMPARE(QQmlEngine::objectOwnership(&o), QQmlEngine::JavaScriptOwnership);
    QQmlEngine::setObjectOwnership(&o, QQmlEngine::CppOwnership);
    QCOMPARE(QQmlEngine::objectOwnership(&o), QQmlEngine::CppOwnership);
    }

    {
    QQmlEngine engine;
    QQmlComponent c(&engine);
    c.setData("import QtQuick 2.0; QtObject { property QtObject object: QtObject {} }", QUrl());

    QObject *o = c.create();
    QVERIFY(o != 0);

    QCOMPARE(QQmlEngine::objectOwnership(o), QQmlEngine::CppOwnership);

    QObject *o2 = qvariant_cast<QObject *>(o->property("object"));
    QCOMPARE(QQmlEngine::objectOwnership(o2), QQmlEngine::JavaScriptOwnership);

    delete o;
    }
    {
        QObject *ptr = createAQObjectForOwnershipTest();
        QSignalSpy spy(ptr, SIGNAL(destroyed()));
        {
            QQmlEngine engine;
            QQmlComponent c(&engine);
            engine.rootContext()->setContextProperty("test", this);
            QQmlEngine::setObjectOwnership(ptr, QQmlEngine::JavaScriptOwnership);
            c.setData("import QtQuick 2.0; Item { property int data: test.createAQObjectForOwnershipTest() ? 0 : 1 }", QUrl());
            QVERIFY(c.isReady());
            QObject *o = c.create();
            QVERIFY(o != 0);
        }
        QTRY_VERIFY(spy.count());
    }
    {
        QObject *ptr = new QObject();
        QSignalSpy spy(ptr, SIGNAL(destroyed()));
        {
            QQmlEngine engine;
            QQmlComponent c(&engine);
            engine.rootContext()->setContextProperty("test", ptr);
            QQmlEngine::setObjectOwnership(ptr, QQmlEngine::JavaScriptOwnership);
            c.setData("import QtQuick 2.0; QtObject { property var object: { var i = test; test ? 0 : 1 }  }", QUrl());
            QVERIFY(c.isReady());
            QObject *o = c.create();
            QVERIFY(o != 0);
            engine.rootContext()->setContextProperty("test", 0);
        }
        QTRY_VERIFY(spy.count());
    }
}

// Test an object can be accessed by multiple engines
void tst_qqmlengine::multipleEngines()
{
    QObject o;
    o.setObjectName("TestName");

    // Simultaneous engines
    {
        QQmlEngine engine1;
        QQmlEngine engine2;
        engine1.rootContext()->setContextProperty("object", &o);
        engine2.rootContext()->setContextProperty("object", &o);

        QQmlExpression expr1(engine1.rootContext(), 0, QString("object.objectName"));
        QQmlExpression expr2(engine2.rootContext(), 0, QString("object.objectName"));

        QCOMPARE(expr1.evaluate().toString(), QString("TestName"));
        QCOMPARE(expr2.evaluate().toString(), QString("TestName"));
    }

    // Serial engines
    {
        QQmlEngine engine1;
        engine1.rootContext()->setContextProperty("object", &o);
        QQmlExpression expr1(engine1.rootContext(), 0, QString("object.objectName"));
        QCOMPARE(expr1.evaluate().toString(), QString("TestName"));
    }
    {
        QQmlEngine engine1;
        engine1.rootContext()->setContextProperty("object", &o);
        QQmlExpression expr1(engine1.rootContext(), 0, QString("object.objectName"));
        QCOMPARE(expr1.evaluate().toString(), QString("TestName"));
    }
}

void tst_qqmlengine::qtqmlModule_data()
{
    QTest::addColumn<QUrl>("testFile");
    QTest::addColumn<QString>("expectedError");
    QTest::addColumn<QStringList>("expectedWarnings");

    QTest::newRow("import QtQml of correct version (2.0)")
            << testFileUrl("qtqmlModule.1.qml")
            << QString()
            << QStringList();

    QTest::newRow("import QtQml of incorrect version (3.0)")
            << testFileUrl("qtqmlModule.2.qml")
            << QString(testFileUrl("qtqmlModule.2.qml").toString() + QLatin1String(":1 module \"QtQml\" version 3.0 is not installed\n"))
            << QStringList();

    QTest::newRow("import QtQml of incorrect version (1.0)")
            << testFileUrl("qtqmlModule.3.qml")
            << QString(testFileUrl("qtqmlModule.3.qml").toString() + QLatin1String(":1 module \"QtQml\" version 1.0 is not installed\n"))
            << QStringList();

    QTest::newRow("import QtQml of incorrect version (2.50)")
            << testFileUrl("qtqmlModule.4.qml")
            << QString(testFileUrl("qtqmlModule.4.qml").toString() + QLatin1String(":1 module \"QtQml\" version 2.50 is not installed\n"))
            << QStringList();

    QTest::newRow("QtQml 2.0 module provides Component, QtObject, Connections, Binding and Timer")
            << testFileUrl("qtqmlModule.5.qml")
            << QString()
            << QStringList();

    QTest::newRow("can import QtQml then QtQuick")
            << testFileUrl("qtqmlModule.6.qml")
            << QString()
            << QStringList();

    QTest::newRow("can import QtQuick then QtQml")
            << testFileUrl("qtqmlModule.7.qml")
            << QString()
            << QStringList();

    QTest::newRow("no import results in no QtObject availability")
            << testFileUrl("qtqmlModule.8.qml")
            << QString(testFileUrl("qtqmlModule.8.qml").toString() + QLatin1String(":4 QtObject is not a type\n"))
            << QStringList();

    QTest::newRow("importing QtQml only results in no Item availability")
            << testFileUrl("qtqmlModule.9.qml")
            << QString(testFileUrl("qtqmlModule.9.qml").toString() + QLatin1String(":4 Item is not a type\n"))
            << QStringList();
}

// Test that the engine registers the QtQml module
void tst_qqmlengine::qtqmlModule()
{
    QFETCH(QUrl, testFile);
    QFETCH(QString, expectedError);
    QFETCH(QStringList, expectedWarnings);

    foreach (const QString &w, expectedWarnings)
        QTest::ignoreMessage(QtWarningMsg, qPrintable(w));

    QQmlEngine e;
    QQmlComponent c(&e, testFile);
    if (expectedError.isEmpty()) {
        QObject *o = c.create();
        QVERIFY(o);
        delete o;
    } else {
        QCOMPARE(c.errorString(), expectedError);
    }
}

class CustomSelector : public QQmlAbstractUrlInterceptor
{
public:
    CustomSelector(const QUrl &base):m_base(base){}
    virtual QUrl intercept(const QUrl &url, QQmlAbstractUrlInterceptor::DataType d)
    {
        if (url.scheme() != QStringLiteral("file"))
            return url;
        if (!m_interceptionPoints.contains(d))
            return url;

        if (url.path().endsWith("Test.2/qmldir"))//Special case
            return QUrl::fromLocalFile(m_base.path() + "interception/module/intercepted/qmldir");

        QString alteredPath = url.path();
        int a = alteredPath.lastIndexOf('/');
        if (a < 0)
            a = 0;
        alteredPath.insert(a, QStringLiteral("/intercepted"));

        QUrl ret = url;
        ret.setPath(alteredPath);
        return ret;
    }
    QList<QQmlAbstractUrlInterceptor::DataType> m_interceptionPoints;
    QUrl m_base;
};

Q_DECLARE_METATYPE(QList<QQmlAbstractUrlInterceptor::DataType>);
void tst_qqmlengine::urlInterceptor_data()
{
    QTest::addColumn<QUrl>("testFile");
    QTest::addColumn<QList<QQmlAbstractUrlInterceptor::DataType> >("interceptionPoint");
    QTest::addColumn<QString>("expectedFilePath");
    QTest::addColumn<QString>("expectedChildString");
    QTest::addColumn<QString>("expectedScriptString");
    QTest::addColumn<QString>("expectedResolvedUrl");
    QTest::addColumn<QString>("expectedAbsoluteUrl");

    QTest::newRow("InterceptTypes")
        << testFileUrl("interception/types/urlInterceptor.qml")
        << (QList<QQmlAbstractUrlInterceptor::DataType>() << QQmlAbstractUrlInterceptor::QmlFile << QQmlAbstractUrlInterceptor::JavaScriptFile << QQmlAbstractUrlInterceptor::UrlString)
        << testFileUrl("interception/types/intercepted/doesNotExist.file").toString()
        << QStringLiteral("intercepted")
        << QStringLiteral("intercepted")
        << testFileUrl("interception/types/intercepted/doesNotExist.file").toString()
        << QStringLiteral("file:///intercepted/doesNotExist.file");

    QTest::newRow("InterceptQmlDir")
        << testFileUrl("interception/qmldir/urlInterceptor.qml")
        << (QList<QQmlAbstractUrlInterceptor::DataType>() << QQmlAbstractUrlInterceptor::QmldirFile << QQmlAbstractUrlInterceptor::UrlString)
        << testFileUrl("interception/qmldir/intercepted/doesNotExist.file").toString()
        << QStringLiteral("intercepted")
        << QStringLiteral("base file")
        << testFileUrl("interception/qmldir/intercepted/doesNotExist.file").toString()
        << QStringLiteral("file:///intercepted/doesNotExist.file");

    QTest::newRow("InterceptModule")//just a Test{}, needs to intercept the module import for it to work
        << testFileUrl("interception/module/urlInterceptor.qml")
        << (QList<QQmlAbstractUrlInterceptor::DataType>() << QQmlAbstractUrlInterceptor::QmldirFile )
        << testFileUrl("interception/module/intercepted/doesNotExist.file").toString()
        << QStringLiteral("intercepted")
        << QStringLiteral("intercepted")
        << testFileUrl("interception/module/intercepted/doesNotExist.file").toString()
        << QStringLiteral("file:///doesNotExist.file");

    QTest::newRow("InterceptStrings")
        << testFileUrl("interception/strings/urlInterceptor.qml")
        << (QList<QQmlAbstractUrlInterceptor::DataType>() << QQmlAbstractUrlInterceptor::UrlString)
        << testFileUrl("interception/strings/intercepted/doesNotExist.file").toString()
        << QStringLiteral("base file")
        << QStringLiteral("base file")
        << testFileUrl("interception/strings/intercepted/doesNotExist.file").toString()
        << QStringLiteral("file:///intercepted/doesNotExist.file");

    QTest::newRow("InterceptIncludes")
        << testFileUrl("interception/includes/urlInterceptor.qml")
        << (QList<QQmlAbstractUrlInterceptor::DataType>() << QQmlAbstractUrlInterceptor::JavaScriptFile)
        << testFileUrl("interception/includes/doesNotExist.file").toString()
        << QStringLiteral("base file")
        << QStringLiteral("intercepted include file")
        << testFileUrl("interception/includes/doesNotExist.file").toString()
        << QStringLiteral("file:///doesNotExist.file");
}

void tst_qqmlengine::urlInterceptor()
{

    QFETCH(QUrl, testFile);
    QFETCH(QList<QQmlAbstractUrlInterceptor::DataType>, interceptionPoint);
    QFETCH(QString, expectedFilePath);
    QFETCH(QString, expectedChildString);
    QFETCH(QString, expectedScriptString);
    QFETCH(QString, expectedResolvedUrl);
    QFETCH(QString, expectedAbsoluteUrl);

    QQmlEngine e;
    e.setImportPathList(QStringList() << testFileUrl("interception/imports").toLocalFile());
    CustomSelector cs(testFileUrl(""));
    cs.m_interceptionPoints = interceptionPoint;
    e.setUrlInterceptor(&cs);
    QQmlComponent c(&e, testFile); //Note that this can get intercepted too
    QObject *o = c.create();
    if (!o)
        qDebug() << c.errorString();
    QVERIFY(o);
    //Test a URL as a property initialization
    QCOMPARE(o->property("filePath").toString(), expectedFilePath);
    //Test a URL as a Type location
    QCOMPARE(o->property("childString").toString(), expectedChildString);
    //Test a URL as a Script location
    QCOMPARE(o->property("scriptString").toString(), expectedScriptString);
    //Test a URL as a resolveUrl() call
    QCOMPARE(o->property("resolvedUrl").toString(), expectedResolvedUrl);
    QCOMPARE(o->property("absoluteUrl").toString(), expectedAbsoluteUrl);
}

void tst_qqmlengine::qmlContextProperties()
{
    QQmlEngine e;

    QQmlComponent c(&e, testFileUrl("TypeofQmlProperty.qml"));
    QObject *o = c.create();
    if (!o) {
        qDebug() << c.errorString();
    }
    QVERIFY(o);
}

QTEST_MAIN(tst_qqmlengine)

#include "tst_qqmlengine.moc"
