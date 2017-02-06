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
#include <QTextDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDir>
#include <QPainter>
#include <QSignalSpy>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <private/qquickborderimage_p.h>
#include <private/qquickimagebase_p.h>
#include <private/qquickscalegrid_p_p.h>
#include <private/qquickloader_p.h>
#include <QtQuick/qquickview.h>
#include <QtQml/qqmlcontext.h>

#include "../../shared/testhttpserver.h"
#include "../../shared/util.h"
#include "../shared/visualtestutil.h"

Q_DECLARE_METATYPE(QQuickImageBase::Status)

class tst_qquickborderimage : public QQmlDataTest

{
    Q_OBJECT
public:
    tst_qquickborderimage();

private slots:
    void cleanup();
    void noSource();
    void imageSource();
    void imageSource_data();
    void clearSource();
    void resized();
    void smooth();
    void mirror();
    void tileModes();
    void sciSource();
    void sciSource_data();
    void invalidSciFile();
    void validSciFiles_data();
    void validSciFiles();
    void pendingRemoteRequest();
    void pendingRemoteRequest_data();
    void statusChanges();
    void statusChanges_data();
    void sourceSizeChanges();
    void progressAndStatusChanges();
#ifndef QT_NO_OPENGL
    void borderImageMesh();
#endif

private:
    QQmlEngine engine;
};

void tst_qquickborderimage::cleanup()
{
    QQuickWindow window;
    window.releaseResources();
    engine.clearComponentCache();
}

tst_qquickborderimage::tst_qquickborderimage()
{
}

void tst_qquickborderimage::noSource()
{
    QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"\" }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->source(), QUrl());
    QCOMPARE(obj->width(), 0.);
    QCOMPARE(obj->height(), 0.);
    QCOMPARE(obj->horizontalTileMode(), QQuickBorderImage::Stretch);
    QCOMPARE(obj->verticalTileMode(), QQuickBorderImage::Stretch);

    delete obj;
}

void tst_qquickborderimage::imageSource_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<bool>("remote");
    QTest::addColumn<QString>("error");

    QTest::newRow("local") << testFileUrl("colors.png").toString() << false << "";
    QTest::newRow("local not found") << testFileUrl("no-such-file.png").toString() << false
        << "<Unknown File>:2:1: QML BorderImage: Cannot open: " + testFileUrl("no-such-file.png").toString();
    QTest::newRow("remote") << "/colors.png" << true << "";
    QTest::newRow("remote not found") << "/no-such-file.png" << true
        << "<Unknown File>:2:1: QML BorderImage: Error transferring {{ServerBaseUrl}}/no-such-file.png - server replied: Not found";
}

void tst_qquickborderimage::imageSource()
{
    QFETCH(QString, source);
    QFETCH(bool, remote);
    QFETCH(QString, error);

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (qstrcmp(QTest::currentDataTag(), "remote") == 0
        || qstrcmp(QTest::currentDataTag(), "remote not found") == 0) {
        QSKIP("Remote tests cause occasional hangs in the CI system -- QTBUG-45655");
    }
#endif

    TestHTTPServer server;
    if (remote) {
        QVERIFY2(server.listen(), qPrintable(server.errorString()));
        server.serveDirectory(dataDirectory());
        source = server.urlString(source);
        error.replace(QStringLiteral("{{ServerBaseUrl}}"), server.baseUrl().toString());
    }

    if (!error.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, error.toUtf8());

    QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"" + source + "\" }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);

    if (remote)
        QTRY_COMPARE(obj->status(), QQuickBorderImage::Loading);

    QCOMPARE(obj->source(), remote ? source : QUrl(source));

    if (error.isEmpty()) {
        QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
        QCOMPARE(obj->width(), 120.);
        QCOMPARE(obj->height(), 120.);
        QCOMPARE(obj->sourceSize().width(), 120);
        QCOMPARE(obj->sourceSize().height(), 120);
        QCOMPARE(obj->horizontalTileMode(), QQuickBorderImage::Stretch);
        QCOMPARE(obj->verticalTileMode(), QQuickBorderImage::Stretch);
    } else {
        QTRY_COMPARE(obj->status(), QQuickBorderImage::Error);
    }

    delete obj;
}

void tst_qquickborderimage::clearSource()
{
    QString componentStr = "import QtQuick 2.0\nBorderImage { source: srcImage }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", testFileUrl("colors.png"));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->status(), QQuickBorderImage::Ready);
    QCOMPARE(obj->width(), 120.);
    QCOMPARE(obj->height(), 120.);

    ctxt->setContextProperty("srcImage", "");
    QVERIFY(obj->source().isEmpty());
    QCOMPARE(obj->status(), QQuickBorderImage::Null);
    QCOMPARE(obj->width(), 0.);
    QCOMPARE(obj->height(), 0.);

    delete obj;
}

void tst_qquickborderimage::resized()
{
    QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"" + testFileUrl("colors.png").toString() + "\"; width: 300; height: 300 }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);
    QCOMPARE(obj->sourceSize().width(), 120);
    QCOMPARE(obj->sourceSize().height(), 120);
    QCOMPARE(obj->horizontalTileMode(), QQuickBorderImage::Stretch);
    QCOMPARE(obj->verticalTileMode(), QQuickBorderImage::Stretch);

    delete obj;
}

void tst_qquickborderimage::smooth()
{
    QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"" + testFile("colors.png") + "\"; smooth: true; width: 300; height: 300 }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);
    QCOMPARE(obj->smooth(), true);
    QCOMPARE(obj->horizontalTileMode(), QQuickBorderImage::Stretch);
    QCOMPARE(obj->verticalTileMode(), QQuickBorderImage::Stretch);

    delete obj;
}

void tst_qquickborderimage::mirror()
{
    QQuickView *window = new QQuickView;
    window->setSource(testFileUrl("mirror.qml"));
    QQuickBorderImage *image = qobject_cast<QQuickBorderImage*>(window->rootObject());
    QVERIFY(image != 0);

    QImage screenshot = window->grabWindow();

    QImage srcPixmap(screenshot);
    QTransform transform;
    transform.translate(image->width(), 0).scale(-1, 1.0);
    srcPixmap = srcPixmap.transformed(transform);

    image->setProperty("mirror", true);
    screenshot = window->grabWindow();

    window->show();
    QTest::qWaitForWindowExposed(window);
    if (window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
        QSKIP("QTBUG-53823");
    QCOMPARE(screenshot, srcPixmap);

    delete window;
}

void tst_qquickborderimage::tileModes()
{
    {
        QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"" + testFile("colors.png") + "\"; width: 100; height: 300; horizontalTileMode: BorderImage.Repeat; verticalTileMode: BorderImage.Repeat }";
        QQmlComponent component(&engine);
        component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
        QVERIFY(obj != 0);
        QCOMPARE(obj->width(), 100.);
        QCOMPARE(obj->height(), 300.);
        QCOMPARE(obj->horizontalTileMode(), QQuickBorderImage::Repeat);
        QCOMPARE(obj->verticalTileMode(), QQuickBorderImage::Repeat);

        delete obj;
    }
    {
        QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"" + testFile("colors.png") + "\"; width: 300; height: 150; horizontalTileMode: BorderImage.Round; verticalTileMode: BorderImage.Round }";
        QQmlComponent component(&engine);
        component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
        QVERIFY(obj != 0);
        QCOMPARE(obj->width(), 300.);
        QCOMPARE(obj->height(), 150.);
        QCOMPARE(obj->horizontalTileMode(), QQuickBorderImage::Round);
        QCOMPARE(obj->verticalTileMode(), QQuickBorderImage::Round);

        delete obj;
    }
}

void tst_qquickborderimage::sciSource()
{
    QFETCH(QString, source);
    QFETCH(bool, valid);
    QFETCH(bool, remote);

    TestHTTPServer server;
    if (remote) {
        QVERIFY2(server.listen(), qPrintable(server.errorString()));
        server.serveDirectory(dataDirectory());
        source = server.urlString(source);
        server.registerFileNameForContentSubstitution(QUrl(source).path());
    }

    QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"" + source + "\"; width: 300; height: 300 }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);

    if (remote)
        QTRY_COMPARE(obj->status(), QQuickBorderImage::Loading);

    QCOMPARE(obj->source(), remote ? source : QUrl(source));
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);

    if (valid) {
        QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
        QCOMPARE(obj->border()->left(), 10);
        QCOMPARE(obj->border()->top(), 20);
        QCOMPARE(obj->border()->right(), 30);
        QCOMPARE(obj->border()->bottom(), 40);
        QCOMPARE(obj->horizontalTileMode(), QQuickBorderImage::Round);
        QCOMPARE(obj->verticalTileMode(), QQuickBorderImage::Repeat);
    } else {
        QTRY_COMPARE(obj->status(), QQuickBorderImage::Error);
    }

    delete obj;
}

void tst_qquickborderimage::sciSource_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<bool>("remote");

    QTest::newRow("local") << testFileUrl("colors-round.sci").toString() << true << /*remote*/false;
    QTest::newRow("local quoted filename") << testFileUrl("colors-round-quotes.sci").toString() << true << /*remote*/false;
    QTest::newRow("local not found") << testFileUrl("no-such-file.sci").toString() << false << /*remote*/false;
    QTest::newRow("remote") << "/colors-round.sci" << true << /*remote*/true;
    QTest::newRow("remote filename quoted") << "/colors-round-quotes.sci" << true << /*remote*/true;
    QTest::newRow("remote image") << "/colors-round-remote.sci" << true << /*remote*/true;
    QTest::newRow("remote not found") << "/no-such-file.sci" << false << /*remote*/true;
}

void tst_qquickborderimage::invalidSciFile()
{
    QTest::ignoreMessage(QtWarningMsg, "QQuickGridScaledImage: Invalid tile rule specified. Using Stretch."); // for "Roun"
    QTest::ignoreMessage(QtWarningMsg, "QQuickGridScaledImage: Invalid tile rule specified. Using Stretch."); // for "Repea"

    QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"" + testFileUrl("invalid.sci").toString() +"\"; width: 300; height: 300 }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);
    QCOMPARE(obj->status(), QQuickImageBase::Error);
    QCOMPARE(obj->horizontalTileMode(), QQuickBorderImage::Stretch);
    QCOMPARE(obj->verticalTileMode(), QQuickBorderImage::Stretch);

    delete obj;
}

void tst_qquickborderimage::validSciFiles_data()
{
    QTest::addColumn<QString>("source");

    QTest::newRow("valid1") << testFileUrl("valid1.sci").toString();
    QTest::newRow("valid2") << testFileUrl("valid2.sci").toString();
    QTest::newRow("valid3") << testFileUrl("valid3.sci").toString();
    QTest::newRow("valid4") << testFileUrl("valid4.sci").toString();
}

void tst_qquickborderimage::validSciFiles()
{
    QFETCH(QString, source);

    QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"" + source +"\"; width: 300; height: 300 }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);
    QCOMPARE(obj->horizontalTileMode(), QQuickBorderImage::Round);
    QCOMPARE(obj->verticalTileMode(), QQuickBorderImage::Repeat);

    delete obj;
}

void tst_qquickborderimage::pendingRemoteRequest()
{
    QFETCH(QString, source);

    QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"" + source + "\" }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->status(), QQuickBorderImage::Loading);

    // verify no crash
    // This will cause a delayed "QThread: Destroyed while thread is still running" warning
    delete obj;
    QTest::qWait(50);
}

void tst_qquickborderimage::pendingRemoteRequest_data()
{
    QTest::addColumn<QString>("source");

    QTest::newRow("png file") << "http://localhost/none.png";
    QTest::newRow("sci file") << "http://localhost/none.sci";
}

//QTBUG-26155
void tst_qquickborderimage::statusChanges_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<int>("emissions");
    QTest::addColumn<bool>("remote");
    QTest::addColumn<QQuickImageBase::Status>("finalStatus");

    QTest::newRow("localfile") << testFileUrl("colors.png").toString() << 1 << false << QQuickImageBase::Ready;
    QTest::newRow("nofile") << "" << 0 << false << QQuickImageBase::Null;
    QTest::newRow("nonexistent") << testFileUrl("thisfiledoesnotexist.png").toString() << 1 << false << QQuickImageBase::Error;
    QTest::newRow("noprotocol") << QString("thisfiledoesnotexisteither.png") << 2 << false << QQuickImageBase::Error;
    QTest::newRow("remote") << "/colors.png" << 2 << true << QQuickImageBase::Ready;
}

void tst_qquickborderimage::statusChanges()
{
    QFETCH(QString, source);
    QFETCH(int, emissions);
    QFETCH(bool, remote);
    QFETCH(QQuickImageBase::Status, finalStatus);

    TestHTTPServer server;
    if (remote) {
        QVERIFY2(server.listen(), qPrintable(server.errorString()));
        server.serveDirectory(dataDirectory());
        source = server.urlString(source);
    }

    QString componentStr = "import QtQuick 2.0\nBorderImage { width: 300; height: 300 }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl(""));

    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    qRegisterMetaType<QQuickImageBase::Status>();
    QSignalSpy spy(obj, SIGNAL(statusChanged(QQuickImageBase::Status)));
    QVERIFY(obj != 0);
    obj->setSource(source);
    if (remote)
        server.sendDelayedItem();
    QTRY_COMPARE(obj->status(), finalStatus);
    QCOMPARE(spy.count(), emissions);

    delete obj;
}

void tst_qquickborderimage::sourceSizeChanges()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nBorderImage { source: srcImage }", QUrl::fromLocalFile(""));
    QTRY_VERIFY(component.isReady());
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", "");
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);

    QSignalSpy sourceSizeSpy(obj, SIGNAL(sourceSizeChanged()));

    // Local
    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Null);
    QTRY_COMPARE(sourceSizeSpy.count(), 0);

    ctxt->setContextProperty("srcImage", testFileUrl("heart200.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("heart200.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("heart200_copy.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("colors.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 2);

    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Null);
    QTRY_COMPARE(sourceSizeSpy.count(), 3);

    // Remote
    ctxt->setContextProperty("srcImage", server.url("/heart200.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", server.url("/heart200.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", server.url("/heart200_copy.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", server.url("/colors.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 5);

    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Null);
    QTRY_COMPARE(sourceSizeSpy.count(), 6);

    delete obj;
}

void tst_qquickborderimage::progressAndStatusChanges()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QString componentStr = "import QtQuick 2.0\nBorderImage { source: srcImage }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", testFileUrl("heart200.png"));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);

    qRegisterMetaType<QQuickBorderImage::Status>();
    QSignalSpy sourceSpy(obj, SIGNAL(sourceChanged(QUrl)));
    QSignalSpy progressSpy(obj, SIGNAL(progressChanged(qreal)));
    QSignalSpy statusSpy(obj, SIGNAL(statusChanged(QQuickImageBase::Status)));

    // Same file
    ctxt->setContextProperty("srcImage", testFileUrl("heart200.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 0);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 0);

    // Loading local file
    ctxt->setContextProperty("srcImage", testFileUrl("colors.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 1);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 1);

    // Loading remote file
    ctxt->setContextProperty("srcImage", server.url("/heart200.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Loading);
    QTRY_COMPARE(obj->progress(), 0.0);
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 2);
    QTRY_VERIFY(progressSpy.count() > 1);
    QTRY_COMPARE(statusSpy.count(), 3);

    ctxt->setContextProperty("srcImage", "");
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Null);
    QTRY_COMPARE(obj->progress(), 0.0);
    QTRY_COMPARE(sourceSpy.count(), 3);
    QTRY_VERIFY(progressSpy.count() > 2);
    QTRY_COMPARE(statusSpy.count(), 4);

    delete obj;
}
#ifndef QT_NO_OPENGL
void tst_qquickborderimage::borderImageMesh()
{
    QQuickView *window = new QQuickView;

    window->setSource(testFileUrl("nonmesh.qml"));
    window->show();
    QTest::qWaitForWindowExposed(window);
    QImage nonmesh = window->grabWindow();

    window->setSource(testFileUrl("mesh.qml"));
    QImage mesh = window->grabWindow();

    QVERIFY(QQuickVisualTestUtil::compareImages(mesh, nonmesh));
}
#endif
QTEST_MAIN(tst_qquickborderimage)

#include "tst_qquickborderimage.moc"
