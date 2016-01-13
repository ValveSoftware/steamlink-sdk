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

#define SERVER_PORT 14446
#define SERVER_ADDR "http://127.0.0.1:14446"

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
    QTest::newRow("remote") << SERVER_ADDR "/colors.png" << true << "";
    QTest::newRow("remote not found") << SERVER_ADDR "/no-such-file.png" << true
        << "<Unknown File>:2:1: QML BorderImage: Error downloading " SERVER_ADDR "/no-such-file.png - server replied: Not found";
}

void tst_qquickborderimage::imageSource()
{
    QFETCH(QString, source);
    QFETCH(bool, remote);
    QFETCH(QString, error);

    TestHTTPServer server;
    if (remote) {
        QVERIFY2(server.listen(SERVER_PORT), qPrintable(server.errorString()));
        server.serveDirectory(dataDirectory());
    }

    if (!error.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, error.toUtf8());

    QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"" + source + "\" }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);

    if (remote)
        QTRY_VERIFY(obj->status() == QQuickBorderImage::Loading);

    QCOMPARE(obj->source(), remote ? source : QUrl(source));

    if (error.isEmpty()) {
        QTRY_VERIFY(obj->status() == QQuickBorderImage::Ready);
        QCOMPARE(obj->width(), 120.);
        QCOMPARE(obj->height(), 120.);
        QCOMPARE(obj->sourceSize().width(), 120);
        QCOMPARE(obj->sourceSize().height(), 120);
        QCOMPARE(obj->horizontalTileMode(), QQuickBorderImage::Stretch);
        QCOMPARE(obj->verticalTileMode(), QQuickBorderImage::Stretch);
    } else {
        QTRY_VERIFY(obj->status() == QQuickBorderImage::Error);
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
    QVERIFY(obj->status() == QQuickBorderImage::Ready);
    QCOMPARE(obj->width(), 120.);
    QCOMPARE(obj->height(), 120.);

    ctxt->setContextProperty("srcImage", "");
    QVERIFY(obj->source().isEmpty());
    QVERIFY(obj->status() == QQuickBorderImage::Null);
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

    bool remote = source.startsWith("http");

    TestHTTPServer server;
    if (remote) {
        QVERIFY2(server.listen(SERVER_PORT), qPrintable(server.errorString()));
        server.serveDirectory(dataDirectory());
    }

    QString componentStr = "import QtQuick 2.0\nBorderImage { source: \"" + source + "\"; width: 300; height: 300 }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);

    if (remote)
        QTRY_VERIFY(obj->status() == QQuickBorderImage::Loading);

    QCOMPARE(obj->source(), remote ? source : QUrl(source));
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);

    if (valid) {
        QTRY_VERIFY(obj->status() == QQuickBorderImage::Ready);
        QCOMPARE(obj->border()->left(), 10);
        QCOMPARE(obj->border()->top(), 20);
        QCOMPARE(obj->border()->right(), 30);
        QCOMPARE(obj->border()->bottom(), 40);
        QCOMPARE(obj->horizontalTileMode(), QQuickBorderImage::Round);
        QCOMPARE(obj->verticalTileMode(), QQuickBorderImage::Repeat);
    } else {
        QTRY_VERIFY(obj->status() == QQuickBorderImage::Error);
    }

    delete obj;
}

void tst_qquickborderimage::sciSource_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<bool>("valid");

    QTest::newRow("local") << testFileUrl("colors-round.sci").toString() << true;
    QTest::newRow("local quoted filename") << testFileUrl("colors-round-quotes.sci").toString() << true;
    QTest::newRow("local not found") << testFileUrl("no-such-file.sci").toString() << false;
    QTest::newRow("remote") << SERVER_ADDR "/colors-round.sci" << true;
    QTest::newRow("remote filename quoted") << SERVER_ADDR "/colors-round-quotes.sci" << true;
    QTest::newRow("remote image") << SERVER_ADDR "/colors-round-remote.sci" << true;
    QTest::newRow("remote not found") << SERVER_ADDR "/no-such-file.sci" << false;
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
    QTest::newRow("remote") << "http://localhost:14446/colors.png" << 2 << true << QQuickImageBase::Ready;
}

void tst_qquickborderimage::statusChanges()
{
    QFETCH(QString, source);
    QFETCH(int, emissions);
    QFETCH(bool, remote);
    QFETCH(QQuickImageBase::Status, finalStatus);

    TestHTTPServer server;
    if (remote) {
        QVERIFY2(server.listen(SERVER_PORT), qPrintable(server.errorString()));
        server.serveDirectory(dataDirectory());
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
    QTRY_VERIFY(obj->status() == finalStatus);
    QCOMPARE(spy.count(), emissions);

    delete obj;
}

void tst_qquickborderimage::sourceSizeChanges()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(14449), qPrintable(server.errorString()));
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
    ctxt->setContextProperty("srcImage", QUrl("http://127.0.0.1:14449/heart200.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", QUrl("http://127.0.0.1:14449/heart200.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", QUrl("http://127.0.0.1:14449/heart200_copy.png"));
    QTRY_COMPARE(obj->status(), QQuickBorderImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", QUrl("http://127.0.0.1:14449/colors.png"));
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
    QVERIFY2(server.listen(14449), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QString componentStr = "import QtQuick 2.0\nBorderImage { source: srcImage }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", testFileUrl("heart200.png"));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickBorderImage *obj = qobject_cast<QQuickBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QVERIFY(obj->status() == QQuickBorderImage::Ready);
    QTRY_VERIFY(obj->progress() == 1.0);

    qRegisterMetaType<QQuickBorderImage::Status>();
    QSignalSpy sourceSpy(obj, SIGNAL(sourceChanged(QUrl)));
    QSignalSpy progressSpy(obj, SIGNAL(progressChanged(qreal)));
    QSignalSpy statusSpy(obj, SIGNAL(statusChanged(QQuickImageBase::Status)));

    // Same file
    ctxt->setContextProperty("srcImage", testFileUrl("heart200.png"));
    QTRY_VERIFY(obj->status() == QQuickBorderImage::Ready);
    QTRY_VERIFY(obj->progress() == 1.0);
    QTRY_COMPARE(sourceSpy.count(), 0);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 0);

    // Loading local file
    ctxt->setContextProperty("srcImage", testFileUrl("colors.png"));
    QTRY_VERIFY(obj->status() == QQuickBorderImage::Ready);
    QTRY_VERIFY(obj->progress() == 1.0);
    QTRY_COMPARE(sourceSpy.count(), 1);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 1);

    // Loading remote file
    ctxt->setContextProperty("srcImage", "http://127.0.0.1:14449/heart200.png");
    QTRY_VERIFY(obj->status() == QQuickBorderImage::Loading);
    QTRY_VERIFY(obj->progress() == 0.0);
    QTRY_VERIFY(obj->status() == QQuickBorderImage::Ready);
    QTRY_VERIFY(obj->progress() == 1.0);
    QTRY_COMPARE(sourceSpy.count(), 2);
    QTRY_VERIFY(progressSpy.count() > 1);
    QTRY_COMPARE(statusSpy.count(), 3);

    ctxt->setContextProperty("srcImage", "");
    QTRY_VERIFY(obj->status() == QQuickBorderImage::Null);
    QTRY_VERIFY(obj->progress() == 0.0);
    QTRY_COMPARE(sourceSpy.count(), 3);
    QTRY_VERIFY(progressSpy.count() > 2);
    QTRY_COMPARE(statusSpy.count(), 4);

    delete obj;
}

QTEST_MAIN(tst_qquickborderimage)

#include "tst_qquickborderimage.moc"
