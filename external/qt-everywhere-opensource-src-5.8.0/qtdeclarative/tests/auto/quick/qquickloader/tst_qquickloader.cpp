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
#include <qtest.h>

#include <QSignalSpy>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlincubator.h>
#include <private/qquickloader_p.h>
#include "testhttpserver.h"
#include "../../shared/util.h"

class SlowComponent : public QQmlComponent
{
    Q_OBJECT
public:
    SlowComponent() {
        QTest::qSleep(500);
    }
};

class PeriodicIncubationController : public QObject,
    public QQmlIncubationController
{
public:
    PeriodicIncubationController() : incubated(false) {}

    void start() { startTimer(20); }

    bool incubated;

protected:
    virtual void timerEvent(QTimerEvent *) {
        incubateFor(15);
    }

    virtual void incubatingObjectCountChanged(int count) {
        if (count)
            incubated = true;
    }
};

class tst_QQuickLoader : public QQmlDataTest

{
    Q_OBJECT
public:
    tst_QQuickLoader();

private slots:
    void cleanup();

    void sourceOrComponent();
    void sourceOrComponent_data();
    void clear();
    void urlToComponent();
    void componentToUrl();
    void anchoredLoader();
    void sizeLoaderToItem();
    void sizeItemToLoader();
    void noResize();
    void networkRequestUrl();
    void failNetworkRequest();
    void networkComponent();
    void active();
    void initialPropertyValues_data();
    void initialPropertyValues();
    void initialPropertyValuesBinding();
    void initialPropertyValuesError_data();
    void initialPropertyValuesError();

    void deleteComponentCrash();
    void nonItem();
    void vmeErrors();
    void creationContext();
    void QTBUG_16928();
    void implicitSize();
    void QTBUG_17114();
    void asynchronous_data();
    void asynchronous();
    void asynchronous_clear();
    void simultaneousSyncAsync();
    void asyncToSync1();
    void asyncToSync2();
    void loadedSignal();

    void parented();
    void sizeBound();
    void QTBUG_30183();

    void sourceComponentGarbageCollection();

private:
    QQmlEngine engine;
};

void tst_QQuickLoader::cleanup()
{
    // clear components. otherwise we even bypass the test server by using the cache.
    engine.clearComponentCache();
}

tst_QQuickLoader::tst_QQuickLoader()
{
    qmlRegisterType<SlowComponent>("LoaderTest", 1, 0, "SlowComponent");
}

void tst_QQuickLoader::sourceOrComponent()
{
    QFETCH(QString, sourceOrComponent);
    QFETCH(QString, sourceDefinition);
    QFETCH(QUrl, sourceUrl);
    QFETCH(QString, errorString);

    bool error = !errorString.isEmpty();
    if (error)
        QTest::ignoreMessage(QtWarningMsg, errorString.toUtf8().constData());

    QQmlComponent component(&engine);
    component.setData(QByteArray(
            "import QtQuick 2.0\n"
            "Loader {\n"
            "   property int onItemChangedCount: 0\n"
            "   property int onSourceChangedCount: 0\n"
            "   property int onSourceComponentChangedCount: 0\n"
            "   property int onStatusChangedCount: 0\n"
            "   property int onProgressChangedCount: 0\n"
            "   property int onLoadedCount: 0\n")
            + sourceDefinition.toUtf8()
            + QByteArray(
            "   onItemChanged: onItemChangedCount += 1\n"
            "   onSourceChanged: onSourceChangedCount += 1\n"
            "   onSourceComponentChanged: onSourceComponentChangedCount += 1\n"
            "   onStatusChanged: onStatusChangedCount += 1\n"
            "   onProgressChanged: onProgressChangedCount += 1\n"
            "   onLoaded: onLoadedCount += 1\n"
            "}")
        , dataDirectoryUrl());

    QQuickLoader *loader = qobject_cast<QQuickLoader*>(component.create());
    QVERIFY(loader != 0);
    QCOMPARE(loader->item() == 0, error);
    QCOMPARE(loader->source(), sourceUrl);
    QCOMPARE(loader->progress(), 1.0);

    QCOMPARE(loader->status(), error ? QQuickLoader::Error : QQuickLoader::Ready);
    QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), error ? 0: 1);

    if (!error) {
        bool sourceComponentIsChildOfLoader = false;
        for (int ii = 0; ii < loader->children().size(); ++ii) {
            QQmlComponent *c = qobject_cast<QQmlComponent*>(loader->children().at(ii));
            if (c && c == loader->sourceComponent()) {
                sourceComponentIsChildOfLoader = true;
            }
        }
        QVERIFY(sourceComponentIsChildOfLoader);
    }

    if (sourceOrComponent == "component") {
        QCOMPARE(loader->property("onSourceComponentChangedCount").toInt(), 1);
        QCOMPARE(loader->property("onSourceChangedCount").toInt(), 0);
    } else {
        QCOMPARE(loader->property("onSourceComponentChangedCount").toInt(), 0);
        QCOMPARE(loader->property("onSourceChangedCount").toInt(), 1);
    }
    QCOMPARE(loader->property("onStatusChangedCount").toInt(), 1);
    QCOMPARE(loader->property("onProgressChangedCount").toInt(), 1);

    QCOMPARE(loader->property("onItemChangedCount").toInt(), 1);
    QCOMPARE(loader->property("onLoadedCount").toInt(), error ? 0 : 1);

    delete loader;
}

void tst_QQuickLoader::sourceOrComponent_data()
{
    QTest::addColumn<QString>("sourceOrComponent");
    QTest::addColumn<QString>("sourceDefinition");
    QTest::addColumn<QUrl>("sourceUrl");
    QTest::addColumn<QString>("errorString");

    QTest::newRow("source") << "source" << "source: 'Rect120x60.qml'\n" << testFileUrl("Rect120x60.qml") << "";
    QTest::newRow("source with subdir") << "source" << "source: 'subdir/Test.qml'\n" << testFileUrl("subdir/Test.qml") << "";
    QTest::newRow("source with encoded subdir literal") << "source" << "source: 'subdir%2fTest.qml'\n" << testFileUrl("subdir/Test.qml") << "";
    QTest::newRow("source with encoded subdir optimized binding") << "source" << "source: 'subdir' + '%2fTest.qml'\n" << testFileUrl("subdir/Test.qml") << "";
    QTest::newRow("source with encoded subdir binding") << "source" << "source: encodeURIComponent('subdir/Test.qml')\n" << testFileUrl("subdir/Test.qml") << "";
    QTest::newRow("sourceComponent") << "component" << "Component { id: comp; Rectangle { width: 100; height: 50 } }\n sourceComponent: comp\n" << QUrl() << "";
    QTest::newRow("invalid source") << "source" << "source: 'IDontExist.qml'\n" << testFileUrl("IDontExist.qml")
            << QString(testFileUrl("IDontExist.qml").toString() + ": No such file or directory");
}

void tst_QQuickLoader::clear()
{
    {
        QQmlComponent component(&engine);
        component.setData(QByteArray(
                    "import QtQuick 2.0\n"
                    " Loader { id: loader\n"
                    "  source: 'Rect120x60.qml'\n"
                    "  Timer { interval: 200; running: true; onTriggered: loader.source = '' }\n"
                    " }")
                , dataDirectoryUrl());
        QQuickLoader *loader = qobject_cast<QQuickLoader*>(component.create());
        QVERIFY(loader != 0);
        QVERIFY(loader->item());
        QCOMPARE(loader->progress(), 1.0);
        QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 1);

        QTRY_VERIFY(!loader->item());
        QCOMPARE(loader->progress(), 0.0);
        QCOMPARE(loader->status(), QQuickLoader::Null);
        QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 0);

        delete loader;
    }
    {
        QQmlComponent component(&engine, testFileUrl("/SetSourceComponent.qml"));
        QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
        QVERIFY(item);

        QQuickLoader *loader = qobject_cast<QQuickLoader*>(item->QQuickItem::childItems().at(0));
        QVERIFY(loader);
        QVERIFY(loader->item());
        QCOMPARE(loader->progress(), 1.0);
        QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 1);

        loader->setSourceComponent(0);

        QVERIFY(!loader->item());
        QCOMPARE(loader->progress(), 0.0);
        QCOMPARE(loader->status(), QQuickLoader::Null);
        QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 0);

        delete item;
    }
    {
        QQmlComponent component(&engine, testFileUrl("/SetSourceComponent.qml"));
        QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
        QVERIFY(item);

        QQuickLoader *loader = qobject_cast<QQuickLoader*>(item->QQuickItem::childItems().at(0));
        QVERIFY(loader);
        QVERIFY(loader->item());
        QCOMPARE(loader->progress(), 1.0);
        QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 1);

        QMetaObject::invokeMethod(item, "clear");

        QVERIFY(!loader->item());
        QCOMPARE(loader->progress(), 0.0);
        QCOMPARE(loader->status(), QQuickLoader::Null);
        QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 0);

        delete item;
    }
}

void tst_QQuickLoader::urlToComponent()
{
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQuick 2.0\n"
                "Loader {\n"
                " id: loader\n"
                " Component { id: myComp; Rectangle { width: 10; height: 10 } }\n"
                " source: \"Rect120x60.qml\"\n"
                " Timer { interval: 100; running: true; onTriggered: loader.sourceComponent = myComp }\n"
                "}" )
            , dataDirectoryUrl());
    QQuickLoader *loader = qobject_cast<QQuickLoader*>(component.create());
    QTest::qWait(200);
    QTRY_VERIFY(loader != 0);
    QVERIFY(loader->item());
    QCOMPARE(loader->progress(), 1.0);
    QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 1);
    QCOMPARE(loader->width(), 10.0);
    QCOMPARE(loader->height(), 10.0);

    delete loader;
}

void tst_QQuickLoader::componentToUrl()
{
    QQmlComponent component(&engine, testFileUrl("/SetSourceComponent.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);

    QQuickLoader *loader = qobject_cast<QQuickLoader*>(item->QQuickItem::childItems().at(0));
    QVERIFY(loader);
    QVERIFY(loader->item());
    QCOMPARE(loader->progress(), 1.0);
    QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 1);

    loader->setSource(testFileUrl("/Rect120x60.qml"));
    QVERIFY(loader->item());
    QCOMPARE(loader->progress(), 1.0);
    QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 1);
    QCOMPARE(loader->width(), 120.0);
    QCOMPARE(loader->height(), 60.0);

    delete item;
}

void tst_QQuickLoader::anchoredLoader()
{
    QQmlComponent component(&engine, testFileUrl("/AnchoredLoader.qml"));
    QQuickItem *rootItem = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(rootItem != 0);
    QQuickItem *loader = rootItem->findChild<QQuickItem*>("loader");
    QQuickItem *sourceElement = rootItem->findChild<QQuickItem*>("sourceElement");

    QVERIFY(loader != 0);
    QVERIFY(sourceElement != 0);

    QCOMPARE(rootItem->width(), 300.0);
    QCOMPARE(rootItem->height(), 200.0);

    QCOMPARE(loader->width(), 300.0);
    QCOMPARE(loader->height(), 200.0);

    QCOMPARE(sourceElement->width(), 300.0);
    QCOMPARE(sourceElement->height(), 200.0);
}

void tst_QQuickLoader::sizeLoaderToItem()
{
    QQmlComponent component(&engine, testFileUrl("/SizeToItem.qml"));
    QQuickLoader *loader = qobject_cast<QQuickLoader*>(component.create());
    QVERIFY(loader != 0);
    QCOMPARE(loader->width(), 120.0);
    QCOMPARE(loader->height(), 60.0);

    // Check resize
    QQuickItem *rect = qobject_cast<QQuickItem*>(loader->item());
    QVERIFY(rect);
    rect->setWidth(150);
    rect->setHeight(45);
    QCOMPARE(loader->width(), 150.0);
    QCOMPARE(loader->height(), 45.0);

    // Check explicit width
    loader->setWidth(200.0);
    QCOMPARE(loader->width(), 200.0);
    QCOMPARE(rect->width(), 200.0);
    rect->setWidth(100.0); // when rect changes ...
    QCOMPARE(rect->width(), 100.0); // ... it changes
    QCOMPARE(loader->width(), 200.0); // ... but loader stays the same

    // Check explicit height
    loader->setHeight(200.0);
    QCOMPARE(loader->height(), 200.0);
    QCOMPARE(rect->height(), 200.0);
    rect->setHeight(100.0); // when rect changes ...
    QCOMPARE(rect->height(), 100.0); // ... it changes
    QCOMPARE(loader->height(), 200.0); // ... but loader stays the same

    // Switch mode
    loader->setWidth(180);
    loader->setHeight(30);
    QCOMPARE(rect->width(), 180.0);
    QCOMPARE(rect->height(), 30.0);

    delete loader;
}

void tst_QQuickLoader::sizeItemToLoader()
{
    QQmlComponent component(&engine, testFileUrl("/SizeToLoader.qml"));
    QQuickLoader *loader = qobject_cast<QQuickLoader*>(component.create());
    QVERIFY(loader != 0);
    QCOMPARE(loader->width(), 200.0);
    QCOMPARE(loader->height(), 80.0);

    QQuickItem *rect = qobject_cast<QQuickItem*>(loader->item());
    QVERIFY(rect);
    QCOMPARE(rect->width(), 200.0);
    QCOMPARE(rect->height(), 80.0);

    // Check resize
    loader->setWidth(180);
    loader->setHeight(30);
    QCOMPARE(rect->width(), 180.0);
    QCOMPARE(rect->height(), 30.0);

    // Switch mode
    loader->resetWidth(); // reset explicit size
    loader->resetHeight();
    rect->setWidth(160);
    rect->setHeight(45);
    QCOMPARE(loader->width(), 160.0);
    QCOMPARE(loader->height(), 45.0);

    delete loader;
}

void tst_QQuickLoader::noResize()
{
    QQmlComponent component(&engine, testFileUrl("/NoResize.qml"));
    QQuickItem* item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item != 0);
    QCOMPARE(item->width(), 200.0);
    QCOMPARE(item->height(), 80.0);

    delete item;
}

void tst_QQuickLoader::networkRequestUrl()
{
    ThreadedTestHTTPServer server(dataDirectory());

    QQmlComponent component(&engine);
    const QString qml = "import QtQuick 2.0\nLoader { property int signalCount : 0; source: \"" + server.baseUrl().toString() + "/Rect120x60.qml\"; onLoaded: signalCount += 1 }";
    component.setData(qml.toUtf8(), testFileUrl("../dummy.qml"));
    if (component.isError())
        qDebug() << component.errors();
    QQuickLoader *loader = qobject_cast<QQuickLoader*>(component.create());
    QVERIFY(loader != 0);

    QTRY_COMPARE(loader->status(), QQuickLoader::Ready);

    QVERIFY(loader->item());
    QCOMPARE(loader->progress(), 1.0);
    QCOMPARE(loader->property("signalCount").toInt(), 1);
    QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 1);

    delete loader;
}

/* XXX Component waits until all dependencies are loaded.  Is this actually possible? */
void tst_QQuickLoader::networkComponent()
{
    ThreadedTestHTTPServer server(dataDirectory(), TestHTTPServer::Delay);

    QQmlComponent component(&engine);
    const QString qml = "import QtQuick 2.0\n"
                        "import \"" + server.baseUrl().toString() + "/\" as NW\n"
                        "Item {\n"
                        " Component { id: comp; NW.Rect120x60 {} }\n"
                        " Loader { sourceComponent: comp } }";
    component.setData(qml.toUtf8(), dataDirectory());
    // The component may be loaded synchronously or asynchronously, so we cannot test for
    // status == Loading here. Also, it makes no sense to instruct the server to send here
    // because in the synchronous case we're already done loading.
    QTRY_COMPARE(component.status(), QQmlComponent::Ready);

    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);

    QQuickLoader *loader = qobject_cast<QQuickLoader*>(item->children().at(1));
    QVERIFY(loader);
    QTRY_COMPARE(loader->status(), QQuickLoader::Ready);

    QVERIFY(loader->item());
    QCOMPARE(loader->progress(), 1.0);
    QCOMPARE(loader->status(), QQuickLoader::Ready);
    QCOMPARE(static_cast<QQuickItem*>(loader)->children().count(), 1);

    delete loader;
}

void tst_QQuickLoader::failNetworkRequest()
{
    ThreadedTestHTTPServer server(dataDirectory());

    QTest::ignoreMessage(QtWarningMsg, QString(server.baseUrl().toString() + "/IDontExist.qml: File not found").toUtf8());

    QQmlComponent component(&engine);
    const QString qml = "import QtQuick 2.0\nLoader { property int did_load: 123; source: \"" + server.baseUrl().toString() + "/IDontExist.qml\"; onLoaded: did_load=456 }";
    component.setData(qml.toUtf8(), server.url("/dummy.qml"));
    QTRY_COMPARE(component.status(), QQmlComponent::Ready);
    QQuickLoader *loader = qobject_cast<QQuickLoader*>(component.create());
    QVERIFY(loader != 0);

    QTRY_COMPARE(loader->status(), QQuickLoader::Error);

    QVERIFY(!loader->item());
    QCOMPARE(loader->progress(), 1.0);
    QCOMPARE(loader->property("did_load").toInt(), 123);
    QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 0);

    delete loader;
}

void tst_QQuickLoader::active()
{
    // check that the item isn't instantiated until active is set to true
    {
        QQmlComponent component(&engine, testFileUrl("active.1.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QQuickLoader *loader = object->findChild<QQuickLoader*>("loader");

        QVERIFY(loader->active() == false); // set manually to false
        QVERIFY(!loader->item());
        QMetaObject::invokeMethod(object, "doSetSourceComponent");
        QVERIFY(!loader->item());
        QMetaObject::invokeMethod(object, "doSetSource");
        QVERIFY(!loader->item());
        QMetaObject::invokeMethod(object, "doSetActive");
        QVERIFY(loader->item() != 0);

        delete object;
    }

    // check that the status is Null if active is set to false
    {
        QQmlComponent component(&engine, testFileUrl("active.2.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QQuickLoader *loader = object->findChild<QQuickLoader*>("loader");

        QVERIFY(loader->active() == true); // active is true by default
        QCOMPARE(loader->status(), QQuickLoader::Ready);
        int currStatusChangedCount = loader->property("statusChangedCount").toInt();
        QMetaObject::invokeMethod(object, "doSetInactive");
        QCOMPARE(loader->status(), QQuickLoader::Null);
        QCOMPARE(loader->property("statusChangedCount").toInt(), (currStatusChangedCount+1));

        delete object;
    }

    // check that the source is not cleared if active is set to false
    {
        QQmlComponent component(&engine, testFileUrl("active.3.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QQuickLoader *loader = object->findChild<QQuickLoader*>("loader");

        QVERIFY(loader->active() == true); // active is true by default
        QVERIFY(!loader->source().isEmpty());
        int currSourceChangedCount = loader->property("sourceChangedCount").toInt();
        QMetaObject::invokeMethod(object, "doSetInactive");
        QVERIFY(!loader->source().isEmpty());
        QCOMPARE(loader->property("sourceChangedCount").toInt(), currSourceChangedCount);

        delete object;
    }

    // check that the sourceComponent is not cleared if active is set to false
    {
        QQmlComponent component(&engine, testFileUrl("active.4.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QQuickLoader *loader = object->findChild<QQuickLoader*>("loader");

        QVERIFY(loader->active() == true); // active is true by default
        QVERIFY(loader->sourceComponent() != 0);
        int currSourceComponentChangedCount = loader->property("sourceComponentChangedCount").toInt();
        QMetaObject::invokeMethod(object, "doSetInactive");
        QVERIFY(loader->sourceComponent() != 0);
        QCOMPARE(loader->property("sourceComponentChangedCount").toInt(), currSourceComponentChangedCount);

        delete object;
    }

    // check that the item is released if active is set to false
    {
        QQmlComponent component(&engine, testFileUrl("active.5.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QQuickLoader *loader = object->findChild<QQuickLoader*>("loader");

        QVERIFY(loader->active() == true); // active is true by default
        QVERIFY(loader->item() != 0);
        int currItemChangedCount = loader->property("itemChangedCount").toInt();
        QMetaObject::invokeMethod(object, "doSetInactive");
        QVERIFY(!loader->item());
        QCOMPARE(loader->property("itemChangedCount").toInt(), (currItemChangedCount+1));

        delete object;
    }

    // check that the activeChanged signal is emitted correctly
    {
        QQmlComponent component(&engine, testFileUrl("active.6.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QQuickLoader *loader = object->findChild<QQuickLoader*>("loader");

        QVERIFY(loader->active() == true); // active is true by default
        loader->setActive(true);           // no effect
        QCOMPARE(loader->property("activeChangedCount").toInt(), 0);
        loader->setActive(false);          // change signal should be emitted
        QCOMPARE(loader->property("activeChangedCount").toInt(), 1);
        loader->setActive(false);          // no effect
        QCOMPARE(loader->property("activeChangedCount").toInt(), 1);
        loader->setActive(true);           // change signal should be emitted
        QCOMPARE(loader->property("activeChangedCount").toInt(), 2);
        loader->setActive(false);          // change signal should be emitted
        QCOMPARE(loader->property("activeChangedCount").toInt(), 3);
        QMetaObject::invokeMethod(object, "doSetActive");
        QCOMPARE(loader->property("activeChangedCount").toInt(), 4);
        QMetaObject::invokeMethod(object, "doSetActive");
        QCOMPARE(loader->property("activeChangedCount").toInt(), 4);
        QMetaObject::invokeMethod(object, "doSetInactive");
        QCOMPARE(loader->property("activeChangedCount").toInt(), 5);
        loader->setActive(true);           // change signal should be emitted
        QCOMPARE(loader->property("activeChangedCount").toInt(), 6);

        delete object;
    }

    // check that the component isn't loaded until active is set to true
    {
        QQmlComponent component(&engine, testFileUrl("active.7.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QCOMPARE(object->property("success").toBool(), true);
        delete object;
    }

    // check that the component is loaded if active is not set (true by default)
    {
        QQmlComponent component(&engine, testFileUrl("active.8.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QCOMPARE(object->property("success").toBool(), true);
        delete object;
    }
}

void tst_QQuickLoader::initialPropertyValues_data()
{
    QTest::addColumn<QUrl>("qmlFile");
    QTest::addColumn<QStringList>("expectedWarnings");
    QTest::addColumn<QStringList>("propertyNames");
    QTest::addColumn<QVariantList>("propertyValues");

    QTest::newRow("source url with value set in onLoaded, initially active = true") << testFileUrl("initialPropertyValues.1.qml")
            << QStringList()
            << (QStringList() << "initialValue" << "behaviorCount")
            << (QVariantList() << 1 << 1);

    QTest::newRow("set source with initial property values specified, active = true") << testFileUrl("initialPropertyValues.2.qml")
            << QStringList()
            << (QStringList() << "initialValue" << "behaviorCount")
            << (QVariantList() << 2 << 0);

    QTest::newRow("set source with initial property values specified, active = false") << testFileUrl("initialPropertyValues.3.qml")
            << (QStringList() << QString(testFileUrl("initialPropertyValues.3.qml").toString() + QLatin1String(":16: TypeError: Cannot read property 'canary' of null")))
            << (QStringList())
            << (QVariantList());

    QTest::newRow("set source with initial property values specified, active = false, with active set true later") << testFileUrl("initialPropertyValues.4.qml")
            << QStringList()
            << (QStringList() << "initialValue" << "behaviorCount")
            << (QVariantList() << 4 << 0);

    QTest::newRow("set source without initial property values specified, active = true") << testFileUrl("initialPropertyValues.5.qml")
            << QStringList()
            << (QStringList() << "initialValue" << "behaviorCount")
            << (QVariantList() << 0 << 0);

    QTest::newRow("set source with initial property values specified with binding, active = true") << testFileUrl("initialPropertyValues.6.qml")
            << QStringList()
            << (QStringList() << "initialValue" << "behaviorCount")
            << (QVariantList() << 6 << 0);

    QTest::newRow("ensure initial property value semantics mimic createObject") << testFileUrl("initialPropertyValues.7.qml")
            << QStringList()
            << (QStringList() << "loaderValue" << "createObjectValue")
            << (QVariantList() << 1 << 1);

    QTest::newRow("ensure initial property values aren't disposed prior to component completion") << testFileUrl("initialPropertyValues.8.qml")
            << QStringList()
            << (QStringList() << "initialValue")
            << (QVariantList() << 6);
}

void tst_QQuickLoader::initialPropertyValues()
{
    QFETCH(QUrl, qmlFile);
    QFETCH(QStringList, expectedWarnings);
    QFETCH(QStringList, propertyNames);
    QFETCH(QVariantList, propertyValues);

    ThreadedTestHTTPServer server(dataDirectory());

    foreach (const QString &warning, expectedWarnings)
        QTest::ignoreMessage(QtWarningMsg, warning.toLatin1().constData());

    QQmlComponent component(&engine, qmlFile);
    QObject *object = component.beginCreate(engine.rootContext());
    QVERIFY(object != 0);

    const int serverBaseUrlPropertyIndex = object->metaObject()->indexOfProperty("serverBaseUrl");
    if (serverBaseUrlPropertyIndex != -1) {
        QMetaProperty prop = object->metaObject()->property(serverBaseUrlPropertyIndex);
        QVERIFY(prop.write(object, server.baseUrl().toString()));
    }

    component.completeCreate();
    if (expectedWarnings.isEmpty()) {
        QQuickLoader *loader = object->findChild<QQuickLoader*>("loader");
        QTRY_VERIFY(loader->item());
    }

    for (int i = 0; i < propertyNames.size(); ++i)
        QCOMPARE(object->property(propertyNames.at(i).toLatin1().constData()), propertyValues.at(i));

    delete object;
}

void tst_QQuickLoader::initialPropertyValuesBinding()
{
    QQmlComponent component(&engine, testFileUrl("initialPropertyValues.binding.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QVERIFY(object->setProperty("bindable", QVariant(8)));
    QCOMPARE(object->property("canaryValue").toInt(), 8);

    delete object;
}

void tst_QQuickLoader::initialPropertyValuesError_data()
{
    QTest::addColumn<QUrl>("qmlFile");
    QTest::addColumn<QStringList>("expectedWarnings");

    QTest::newRow("invalid initial property values object") << testFileUrl("initialPropertyValues.error.1.qml")
            << (QStringList() << QString(testFileUrl("initialPropertyValues.error.1.qml").toString() + ":6:5: QML Loader: setSource: value is not an object"));

    QTest::newRow("nonexistent source url") << testFileUrl("initialPropertyValues.error.2.qml")
            << (QStringList() << QString(testFileUrl("NonexistentSourceComponent.qml").toString() + ": No such file or directory"));

    QTest::newRow("invalid source url") << testFileUrl("initialPropertyValues.error.3.qml")
            << (QStringList() << QString(testFileUrl("InvalidSourceComponent.qml").toString() + ":5:1: Syntax error"));

    QTest::newRow("invalid initial property values object with invalid property access") << testFileUrl("initialPropertyValues.error.4.qml")
            << (QStringList() << QString(testFileUrl("initialPropertyValues.error.4.qml").toString() + ":7:5: QML Loader: setSource: value is not an object")
                              << QString(testFileUrl("initialPropertyValues.error.4.qml").toString() + ":5: TypeError: Cannot read property 'canary' of null"));
}

void tst_QQuickLoader::initialPropertyValuesError()
{
    QFETCH(QUrl, qmlFile);
    QFETCH(QStringList, expectedWarnings);

    foreach (const QString &warning, expectedWarnings)
        QTest::ignoreMessage(QtWarningMsg, warning.toUtf8().constData());

    QQmlComponent component(&engine, qmlFile);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QQuickLoader *loader = object->findChild<QQuickLoader*>("loader");
    QVERIFY(loader != 0);
    QVERIFY(!loader->item());
    delete object;
}

// QTBUG-9241
void tst_QQuickLoader::deleteComponentCrash()
{
    QQmlComponent component(&engine, testFileUrl("crash.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);

    item->metaObject()->invokeMethod(item, "setLoaderSource");

    QQuickLoader *loader = qobject_cast<QQuickLoader*>(item->QQuickItem::childItems().at(0));
    QVERIFY(loader);
    QVERIFY(loader->item());
    QCOMPARE(loader->item()->objectName(), QLatin1String("blue"));
    QCOMPARE(loader->progress(), 1.0);
    QCOMPARE(loader->status(), QQuickLoader::Ready);
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    QTRY_COMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 1);
    QCOMPARE(loader->source(), testFileUrl("BlueRect.qml"));

    delete item;
}

void tst_QQuickLoader::nonItem()
{
    QQmlComponent component(&engine, testFileUrl("nonItem.qml"));

    QQuickLoader *loader = qobject_cast<QQuickLoader*>(component.create());
    QVERIFY(loader);
    QVERIFY(loader->item());

    QCOMPARE(loader, loader->item()->parent());

    QPointer<QObject> item = loader->item();
    loader->setActive(false);
    QVERIFY(!loader->item());
    QTRY_VERIFY(!item);

    delete loader;
}

void tst_QQuickLoader::vmeErrors()
{
    QQmlComponent component(&engine, testFileUrl("vmeErrors.qml"));
    QString err = testFileUrl("VmeError.qml").toString() + ":6:26: Cannot assign object type QObject with no default method";
    QTest::ignoreMessage(QtWarningMsg, err.toLatin1().constData());
    QQuickLoader *loader = qobject_cast<QQuickLoader*>(component.create());
    QVERIFY(loader);
    QVERIFY(!loader->item());

    delete loader;
}

// QTBUG-13481
void tst_QQuickLoader::creationContext()
{
    QQmlComponent component(&engine, testFileUrl("creationContext.qml"));

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test").toBool(), true);

    delete o;
}

void tst_QQuickLoader::QTBUG_16928()
{
    QQmlComponent component(&engine, testFileUrl("QTBUG_16928.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);

    QCOMPARE(item->width(), 250.);
    QCOMPARE(item->height(), 250.);

    delete item;
}

void tst_QQuickLoader::implicitSize()
{
    QQmlComponent component(&engine, testFileUrl("implicitSize.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);

    QCOMPARE(item->width(), 150.);
    QCOMPARE(item->height(), 150.);

    QCOMPARE(item->property("implHeight").toReal(), 100.);
    QCOMPARE(item->property("implWidth").toReal(), 100.);

    QQuickLoader *loader = item->findChild<QQuickLoader*>("loader");
    QSignalSpy implWidthSpy(loader, SIGNAL(implicitWidthChanged()));
    QSignalSpy implHeightSpy(loader, SIGNAL(implicitHeightChanged()));

    QMetaObject::invokeMethod(item, "changeImplicitSize");

    QCOMPARE(loader->property("implicitWidth").toReal(), 200.);
    QCOMPARE(loader->property("implicitHeight").toReal(), 300.);

    QCOMPARE(implWidthSpy.count(), 1);
    QCOMPARE(implHeightSpy.count(), 1);

    delete item;
}

void tst_QQuickLoader::QTBUG_17114()
{
    QQmlComponent component(&engine, testFileUrl("QTBUG_17114.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);

    QCOMPARE(item->property("loaderWidth").toReal(), 32.);
    QCOMPARE(item->property("loaderHeight").toReal(), 32.);

    delete item;
}

void tst_QQuickLoader::asynchronous_data()
{
    QTest::addColumn<QUrl>("qmlFile");
    QTest::addColumn<QStringList>("expectedWarnings");

    QTest::newRow("Valid component") << testFileUrl("BigComponent.qml")
            << QStringList();

    QTest::newRow("Non-existent component") << testFileUrl("IDoNotExist.qml")
            << (QStringList() << QString(testFileUrl("IDoNotExist.qml").toString() + ": No such file or directory"));

    QTest::newRow("Invalid component") << testFileUrl("InvalidSourceComponent.qml")
            << (QStringList() << QString(testFileUrl("InvalidSourceComponent.qml").toString() + ":5:1: Syntax error"));
}

void tst_QQuickLoader::asynchronous()
{
    QFETCH(QUrl, qmlFile);
    QFETCH(QStringList, expectedWarnings);

    PeriodicIncubationController *controller = new PeriodicIncubationController;
    QQmlIncubationController *previous = engine.incubationController();
    engine.setIncubationController(controller);
    delete previous;

    QQmlComponent component(&engine, testFileUrl("asynchronous.qml"));
    QQuickItem *root = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(root);

    QQuickLoader *loader = root->findChild<QQuickLoader*>("loader");
    QVERIFY(loader);

    foreach (const QString &warning, expectedWarnings)
        QTest::ignoreMessage(QtWarningMsg, warning.toUtf8().constData());

    QVERIFY(!loader->item());
    QCOMPARE(loader->progress(), 0.0);
    root->setProperty("comp", qmlFile.toString());
    QMetaObject::invokeMethod(root, "loadComponent");
    QVERIFY(!loader->item());

    if (expectedWarnings.isEmpty()) {
        QCOMPARE(loader->status(), QQuickLoader::Loading);

        controller->start();
        QVERIFY(!controller->incubated); // asynchronous compilation means not immediately compiled/incubating.
        QTRY_VERIFY(controller->incubated); // but should start incubating once compilation is complete.
        QTRY_VERIFY(loader->item());
        QCOMPARE(loader->progress(), 1.0);
        QCOMPARE(loader->status(), QQuickLoader::Ready);
    } else {
        QTRY_COMPARE(loader->progress(), 1.0);
        QTRY_COMPARE(loader->status(), QQuickLoader::Error);
    }

    delete root;
}

void tst_QQuickLoader::asynchronous_clear()
{
    PeriodicIncubationController *controller = new PeriodicIncubationController;
    QQmlIncubationController *previous = engine.incubationController();
    engine.setIncubationController(controller);
    delete previous;

    QQmlComponent component(&engine, testFileUrl("asynchronous.qml"));
    QQuickItem *root = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(root);

    QQuickLoader *loader = root->findChild<QQuickLoader*>("loader");
    QVERIFY(loader);

    QVERIFY(!loader->item());
    root->setProperty("comp", "BigComponent.qml");
    QMetaObject::invokeMethod(root, "loadComponent");
    QVERIFY(!loader->item());

    controller->start();
    QCOMPARE(loader->status(), QQuickLoader::Loading);
    QTRY_COMPARE(engine.incubationController()->incubatingObjectCount(), 1);

    // clear before component created
    root->setProperty("comp", "");
    QMetaObject::invokeMethod(root, "loadComponent");
    QVERIFY(!loader->item());
    QCOMPARE(engine.incubationController()->incubatingObjectCount(), 0);

    QCOMPARE(loader->progress(), 0.0);
    QCOMPARE(loader->status(), QQuickLoader::Null);
    QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 0);

    // check loading component
    root->setProperty("comp", "BigComponent.qml");
    QMetaObject::invokeMethod(root, "loadComponent");
    QVERIFY(!loader->item());

    QCOMPARE(loader->status(), QQuickLoader::Loading);
    QCOMPARE(engine.incubationController()->incubatingObjectCount(), 1);

    QTRY_VERIFY(loader->item());
    QCOMPARE(loader->progress(), 1.0);
    QCOMPARE(loader->status(), QQuickLoader::Ready);
    QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 1);

    delete root;
}

void tst_QQuickLoader::simultaneousSyncAsync()
{
    PeriodicIncubationController *controller = new PeriodicIncubationController;
    QQmlIncubationController *previous = engine.incubationController();
    engine.setIncubationController(controller);
    delete previous;

    QQmlComponent component(&engine, testFileUrl("simultaneous.qml"));
    QQuickItem *root = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(root);

    QQuickLoader *asyncLoader = root->findChild<QQuickLoader*>("asyncLoader");
    QQuickLoader *syncLoader = root->findChild<QQuickLoader*>("syncLoader");
    QVERIFY(asyncLoader);
    QVERIFY(syncLoader);

    QVERIFY(!asyncLoader->item());
    QVERIFY(!syncLoader->item());
    QMetaObject::invokeMethod(root, "loadComponents");
    QVERIFY(!asyncLoader->item());
    QVERIFY(syncLoader->item());

    controller->start();
    QCOMPARE(asyncLoader->status(), QQuickLoader::Loading);
    QVERIFY(!controller->incubated); // asynchronous compilation means not immediately compiled/incubating.
    QTRY_VERIFY(controller->incubated); // but should start incubating once compilation is complete.
    QTRY_VERIFY(asyncLoader->item());
    QCOMPARE(asyncLoader->progress(), 1.0);
    QCOMPARE(asyncLoader->status(), QQuickLoader::Ready);

    delete root;
}

void tst_QQuickLoader::asyncToSync1()
{
    QQmlEngine engine;
    PeriodicIncubationController *controller = new PeriodicIncubationController;
    QQmlIncubationController *previous = engine.incubationController();
    engine.setIncubationController(controller);
    delete previous;

    QQmlComponent component(&engine, testFileUrl("asynchronous.qml"));
    QQuickItem *root = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(root);

    QQuickLoader *loader = root->findChild<QQuickLoader*>("loader");
    QVERIFY(loader);

    QVERIFY(!loader->item());
    root->setProperty("comp", "BigComponent.qml");
    QMetaObject::invokeMethod(root, "loadComponent");
    QVERIFY(!loader->item());

    controller->start();
    QCOMPARE(loader->status(), QQuickLoader::Loading);
    QCOMPARE(engine.incubationController()->incubatingObjectCount(), 0);

    // force completion before component created
    loader->setAsynchronous(false);
    QVERIFY(loader->item());
    QCOMPARE(loader->progress(), 1.0);
    QCOMPARE(loader->status(), QQuickLoader::Ready);
    QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 1);

    delete root;
}

void tst_QQuickLoader::asyncToSync2()
{
    PeriodicIncubationController *controller = new PeriodicIncubationController;
    QQmlIncubationController *previous = engine.incubationController();
    engine.setIncubationController(controller);
    delete previous;

    QQmlComponent component(&engine, testFileUrl("asynchronous.qml"));
    QQuickItem *root = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(root);

    QQuickLoader *loader = root->findChild<QQuickLoader*>("loader");
    QVERIFY(loader);

    QVERIFY(!loader->item());
    root->setProperty("comp", "BigComponent.qml");
    QMetaObject::invokeMethod(root, "loadComponent");
    QVERIFY(!loader->item());

    controller->start();
    QCOMPARE(loader->status(), QQuickLoader::Loading);
    QTRY_COMPARE(engine.incubationController()->incubatingObjectCount(), 1);

    // force completion after component created but before incubation complete
    loader->setAsynchronous(false);
    QVERIFY(loader->item());
    QCOMPARE(loader->progress(), 1.0);
    QCOMPARE(loader->status(), QQuickLoader::Ready);
    QCOMPARE(static_cast<QQuickItem*>(loader)->childItems().count(), 1);

    delete root;
}

void tst_QQuickLoader::loadedSignal()
{
    PeriodicIncubationController *controller = new PeriodicIncubationController;
    QQmlIncubationController *previous = engine.incubationController();
    engine.setIncubationController(controller);
    delete previous;

    {
        // ensure that triggering loading (by setting active = true)
        // and then immediately setting active to false, causes the
        // loader to be deactivated, including disabling the incubator.
        QQmlComponent component(&engine, testFileUrl("loadedSignal.qml"));
        QObject *obj = component.create();

        QMetaObject::invokeMethod(obj, "triggerLoading");
        QTest::qWait(100); // ensure that loading would have finished if it wasn't deactivated
        QCOMPARE(obj->property("loadCount").toInt(), 0);
        QVERIFY(obj->property("success").toBool());

        QMetaObject::invokeMethod(obj, "triggerLoading");
        QTest::qWait(100);
        QCOMPARE(obj->property("loadCount").toInt(), 0);
        QVERIFY(obj->property("success").toBool());

        QMetaObject::invokeMethod(obj, "triggerMultipleLoad");
        controller->start();
        QTest::qWait(100);
        QTRY_COMPARE(obj->property("loadCount").toInt(), 1); // only one loaded signal should be emitted.
        QVERIFY(obj->property("success").toBool());

        delete obj;
    }

    {
        // ensure that an error doesn't result in the onLoaded signal being emitted.
        QQmlComponent component(&engine, testFileUrl("loadedSignal.2.qml"));
        QObject *obj = component.create();

        QMetaObject::invokeMethod(obj, "triggerLoading");
        QTest::qWait(100);
        QCOMPARE(obj->property("loadCount").toInt(), 0);
        QVERIFY(obj->property("success").toBool());

        delete obj;
    }
}

void tst_QQuickLoader::parented()
{
    QQmlComponent component(&engine, testFileUrl("parented.qml"));
    QQuickItem *root = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(root);

    QQuickItem *item = root->findChild<QQuickItem*>("comp");
    QVERIFY(item);

    QCOMPARE(item->parentItem(), root);

    QCOMPARE(item->width(), 300.);
    QCOMPARE(item->height(), 300.);

    delete root;
}

void tst_QQuickLoader::sizeBound()
{
    QQmlComponent component(&engine, testFileUrl("sizebound.qml"));
    QQuickItem *root = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(root);
    QQuickLoader *loader = root->findChild<QQuickLoader*>("loader");
    QVERIFY(loader != 0);

    QVERIFY(loader->item());

    QCOMPARE(loader->width(), 50.0);
    QCOMPARE(loader->height(), 60.0);

    QMetaObject::invokeMethod(root, "switchComponent");

    QCOMPARE(loader->width(), 80.0);
    QCOMPARE(loader->height(), 90.0);

    delete root;
}

void tst_QQuickLoader::QTBUG_30183()
{
    QQmlComponent component(&engine, testFileUrl("/QTBUG_30183.qml"));
    QQuickLoader *loader = qobject_cast<QQuickLoader*>(component.create());
    QVERIFY(loader != 0);
    QCOMPARE(loader->width(), 240.0);
    QCOMPARE(loader->height(), 120.0);

    // the loaded item must follow the size
    QQuickItem *rect = qobject_cast<QQuickItem*>(loader->item());
    QVERIFY(rect);
    QCOMPARE(rect->width(), 240.0);
    QCOMPARE(rect->height(), 120.0);

    delete loader;
}

void tst_QQuickLoader::sourceComponentGarbageCollection()
{
    QQmlComponent component(&engine, testFileUrl("sourceComponentGarbageCollection.qml"));
    QScopedPointer<QObject> obj(component.create());
    QVERIFY(!obj.isNull());

    QMetaObject::invokeMethod(obj.data(), "setSourceComponent");
    engine.collectGarbage();
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);

    QSignalSpy spy(obj.data(), SIGNAL(loaded()));

    obj->setProperty("active", true);

    if (spy.isEmpty())
        QVERIFY(spy.wait());

    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(tst_QQuickLoader)

#include "tst_qquickloader.moc"
