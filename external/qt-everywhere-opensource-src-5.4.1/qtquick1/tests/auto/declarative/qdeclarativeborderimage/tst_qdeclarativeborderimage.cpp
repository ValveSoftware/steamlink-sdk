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
#include <QGraphicsScene>
#include <QPainter>

#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <private/qdeclarativeborderimage_p.h>
#include <private/qdeclarativeimagebase_p.h>
#include <private/qdeclarativescalegrid_p_p.h>
#include <private/qdeclarativeloader_p.h>
#include <QtDeclarative/qdeclarativecontext.h>

#include "../shared/testhttpserver.h"

#define SERVER_PORT 14446
#define SERVER_ADDR "http://127.0.0.1:14446"

class tst_qdeclarativeborderimage : public QObject

{
    Q_OBJECT
public:
    tst_qdeclarativeborderimage();

private slots:
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
    void pendingRemoteRequest();
    void pendingRemoteRequest_data();
    void testQtQuick11Attributes();
    void testQtQuick11Attributes_data();

private:
    QDeclarativeEngine engine;
};

tst_qdeclarativeborderimage::tst_qdeclarativeborderimage()
{
}

void tst_qdeclarativeborderimage::noSource()
{
    QString componentStr = "import QtQuick 1.0\nBorderImage { source: \"\" }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeBorderImage *obj = qobject_cast<QDeclarativeBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->source(), QUrl());
    QCOMPARE(obj->width(), 0.);
    QCOMPARE(obj->height(), 0.);
    QCOMPARE(obj->horizontalTileMode(), QDeclarativeBorderImage::Stretch);
    QCOMPARE(obj->verticalTileMode(), QDeclarativeBorderImage::Stretch);

    delete obj;
}

void tst_qdeclarativeborderimage::imageSource_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<bool>("remote");
    QTest::addColumn<QString>("error");

    QTest::newRow("local") << QUrl::fromLocalFile(SRCDIR "/data/colors.png").toString() << false << "";
    QTest::newRow("local not found") << QUrl::fromLocalFile(SRCDIR "/data/no-such-file.png").toString() << false
        << "<Unknown File>:2:1: QML BorderImage: Cannot open: " + QUrl::fromLocalFile(SRCDIR "/data/no-such-file.png").toString();
    QTest::newRow("remote") << SERVER_ADDR "/colors.png" << true << "";
    QTest::newRow("remote not found") << SERVER_ADDR "/no-such-file.png" << true
        << "<Unknown File>:2:1: QML BorderImage: Error downloading " SERVER_ADDR "/no-such-file.png - server replied: Not found";
}

void tst_qdeclarativeborderimage::imageSource()
{
    QFETCH(QString, source);
    QFETCH(bool, remote);
    QFETCH(QString, error);

    TestHTTPServer *server = 0;
    if (remote) {
        server = new TestHTTPServer(SERVER_PORT);
        QVERIFY(server->isValid());
        server->serveDirectory(SRCDIR "/data");
    }

    if (!error.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, error.toUtf8());

    QString componentStr = "import QtQuick 1.0\nBorderImage { source: \"" + source + "\" }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeBorderImage *obj = qobject_cast<QDeclarativeBorderImage*>(component.create());
    QVERIFY(obj != 0);

    if (remote)
        QTRY_VERIFY(obj->status() == QDeclarativeBorderImage::Loading);

    QCOMPARE(obj->source(), remote ? source : QUrl(source));

    if (error.isEmpty()) {
        QTRY_VERIFY(obj->status() == QDeclarativeBorderImage::Ready);
        QCOMPARE(obj->width(), 120.);
        QCOMPARE(obj->height(), 120.);
        QCOMPARE(obj->sourceSize().width(), 120);
        QCOMPARE(obj->sourceSize().height(), 120);
        QCOMPARE(obj->horizontalTileMode(), QDeclarativeBorderImage::Stretch);
        QCOMPARE(obj->verticalTileMode(), QDeclarativeBorderImage::Stretch);
    } else {
        QTRY_VERIFY(obj->status() == QDeclarativeBorderImage::Error);
    }

    delete obj;
    delete server;
}

void tst_qdeclarativeborderimage::clearSource()
{
    QString componentStr = "import QtQuick 1.0\nBorderImage { source: srcImage }";
    QDeclarativeContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", QUrl::fromLocalFile(SRCDIR "/data/colors.png"));
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeBorderImage *obj = qobject_cast<QDeclarativeBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QVERIFY(obj->status() == QDeclarativeBorderImage::Ready);
    QCOMPARE(obj->width(), 120.);
    QCOMPARE(obj->height(), 120.);

    ctxt->setContextProperty("srcImage", "");
    QVERIFY(obj->source().isEmpty());
    QVERIFY(obj->status() == QDeclarativeBorderImage::Null);
    QCOMPARE(obj->width(), 0.);
    QCOMPARE(obj->height(), 0.);
}

void tst_qdeclarativeborderimage::resized()
{
    QString componentStr = "import QtQuick 1.0\nBorderImage { source: \"" + QUrl::fromLocalFile(SRCDIR "/data/colors.png").toString() + "\"; width: 300; height: 300 }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeBorderImage *obj = qobject_cast<QDeclarativeBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);
    QCOMPARE(obj->sourceSize().width(), 120);
    QCOMPARE(obj->sourceSize().height(), 120);
    QCOMPARE(obj->horizontalTileMode(), QDeclarativeBorderImage::Stretch);
    QCOMPARE(obj->verticalTileMode(), QDeclarativeBorderImage::Stretch);

    delete obj;
}

void tst_qdeclarativeborderimage::smooth()
{
    QString componentStr = "import QtQuick 1.0\nBorderImage { source: \"" SRCDIR "/data/colors.png\"; smooth: true; width: 300; height: 300 }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeBorderImage *obj = qobject_cast<QDeclarativeBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);
    QCOMPARE(obj->smooth(), true);
    QCOMPARE(obj->horizontalTileMode(), QDeclarativeBorderImage::Stretch);
    QCOMPARE(obj->verticalTileMode(), QDeclarativeBorderImage::Stretch);

    delete obj;
}

void tst_qdeclarativeborderimage::mirror()
{
    const QString imagePath = QLatin1String(SRCDIR) + QStringLiteral("/data/heart200.png");
    QVERIFY(QFileInfo(imagePath).exists());
    const QString componentStr = QStringLiteral("import QtQuick 1.0\nBorderImage { source: \"")
            + QUrl::fromLocalFile(imagePath).toString()
            + QStringLiteral("\"; smooth: true; width: 300; height: 300; border { top: 50; right: 50; bottom: 50; left: 50 } }");
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeBorderImage *obj = qobject_cast<QDeclarativeBorderImage*>(component.create());
    QVERIFY(obj != 0);

    int width = obj->property("width").toInt();
    int height = obj->property("height").toInt();

    QGraphicsScene scene;
    scene.addItem(qobject_cast<QGraphicsObject *>(obj));
    QPixmap screenshot(width, height);
    screenshot.fill();
    QPainter p_screenshot(&screenshot);
    scene.render(&p_screenshot, QRect(0, 0, width, height), QRect(0, 0, width, height));

    QTransform transform;
    transform.translate(width, 0).scale(-1, 1.0);
    QPixmap expected = screenshot.transformed(transform);

    obj->setProperty("mirror", true);
    p_screenshot.fillRect(QRect(0, 0, width, height), Qt::white);
    scene.render(&p_screenshot, QRect(0, 0, width, height), QRect(0, 0, width, height));

    QEXPECT_FAIL("", "QTBUG-19538", Continue);
    QCOMPARE(screenshot, expected);

    delete obj;
}

void tst_qdeclarativeborderimage::tileModes()
{
    {
        QString componentStr = "import QtQuick 1.0\nBorderImage { source: \"" SRCDIR "/data/colors.png\"; width: 100; height: 300; horizontalTileMode: BorderImage.Repeat; verticalTileMode: BorderImage.Repeat }";
        QDeclarativeComponent component(&engine);
        component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeBorderImage *obj = qobject_cast<QDeclarativeBorderImage*>(component.create());
        QVERIFY(obj != 0);
        QCOMPARE(obj->width(), 100.);
        QCOMPARE(obj->height(), 300.);
        QCOMPARE(obj->horizontalTileMode(), QDeclarativeBorderImage::Repeat);
        QCOMPARE(obj->verticalTileMode(), QDeclarativeBorderImage::Repeat);

        delete obj;
    }
    {
        QString componentStr = "import QtQuick 1.0\nBorderImage { source: \"" SRCDIR "/data/colors.png\"; width: 300; height: 150; horizontalTileMode: BorderImage.Round; verticalTileMode: BorderImage.Round }";
        QDeclarativeComponent component(&engine);
        component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeBorderImage *obj = qobject_cast<QDeclarativeBorderImage*>(component.create());
        QVERIFY(obj != 0);
        QCOMPARE(obj->width(), 300.);
        QCOMPARE(obj->height(), 150.);
        QCOMPARE(obj->horizontalTileMode(), QDeclarativeBorderImage::Round);
        QCOMPARE(obj->verticalTileMode(), QDeclarativeBorderImage::Round);

        delete obj;
    }
}

void tst_qdeclarativeborderimage::sciSource()
{
    QFETCH(QString, source);
    QFETCH(bool, valid);

    bool remote = source.startsWith("http");
    TestHTTPServer *server = 0;
    if (remote) {
        server = new TestHTTPServer(SERVER_PORT);
        QVERIFY(server->isValid());
        server->serveDirectory(SRCDIR "/data");
    }

    QString componentStr = "import QtQuick 1.0\nBorderImage { source: \"" + source + "\"; width: 300; height: 300 }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeBorderImage *obj = qobject_cast<QDeclarativeBorderImage*>(component.create());
    QVERIFY(obj != 0);

    if (remote)
        QTRY_VERIFY(obj->status() == QDeclarativeBorderImage::Loading);

    QCOMPARE(obj->source(), remote ? source : QUrl(source));
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);

    if (valid) {
        QTRY_VERIFY(obj->status() == QDeclarativeBorderImage::Ready);
        QCOMPARE(obj->border()->left(), 10);
        QCOMPARE(obj->border()->top(), 20);
        QCOMPARE(obj->border()->right(), 30);
        QCOMPARE(obj->border()->bottom(), 40);
        QCOMPARE(obj->horizontalTileMode(), QDeclarativeBorderImage::Round);
        QCOMPARE(obj->verticalTileMode(), QDeclarativeBorderImage::Repeat);
    } else {
        QTRY_VERIFY(obj->status() == QDeclarativeBorderImage::Error);
    }

    delete obj;
    delete server;
}

void tst_qdeclarativeborderimage::sciSource_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<bool>("valid");

    QTest::newRow("local") << QUrl::fromLocalFile(SRCDIR "/data/colors-round.sci").toString() << true;
    QTest::newRow("local not found") << QUrl::fromLocalFile(SRCDIR "/data/no-such-file.sci").toString() << false;
    QTest::newRow("remote") << SERVER_ADDR "/colors-round.sci" << true;
    QTest::newRow("remote image") << SERVER_ADDR "/colors-round-remote.sci" << true;
    QTest::newRow("remote not found") << SERVER_ADDR "/no-such-file.sci" << false;
}

void tst_qdeclarativeborderimage::invalidSciFile()
{
    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeGridScaledImage: Invalid tile rule specified. Using Stretch."); // for "Roun"
    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeGridScaledImage: Invalid tile rule specified. Using Stretch."); // for "Repea"

    QString componentStr = "import QtQuick 1.0\nBorderImage { source: \"" + QUrl::fromLocalFile(SRCDIR "/data/invalid.sci").toString() +"\"; width: 300; height: 300 }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeBorderImage *obj = qobject_cast<QDeclarativeBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);
    QCOMPARE(obj->status(), QDeclarativeImageBase::Error);
    QCOMPARE(obj->horizontalTileMode(), QDeclarativeBorderImage::Stretch);
    QCOMPARE(obj->verticalTileMode(), QDeclarativeBorderImage::Stretch);

    delete obj;
}

void tst_qdeclarativeborderimage::pendingRemoteRequest()
{
    QFETCH(QString, source);

    QString componentStr = "import QtQuick 1.0\nBorderImage { source: \"" + source + "\" }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeBorderImage *obj = qobject_cast<QDeclarativeBorderImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->status(), QDeclarativeBorderImage::Loading);

    // verify no crash
    // This will cause a delayed "QThread: Destroyed while thread is still running" warning
    delete obj;
    QTest::qWait(50);
}

void tst_qdeclarativeborderimage::pendingRemoteRequest_data()
{
    QTest::addColumn<QString>("source");

    QTest::newRow("png file") << "http://localhost/none.png";
    QTest::newRow("sci file") << "http://localhost/none.sci";
}

void tst_qdeclarativeborderimage::testQtQuick11Attributes()
{
    QFETCH(QString, code);
    QFETCH(QString, warning);
    QFETCH(QString, error);

    QDeclarativeEngine engine;
    QObject *obj;

    QDeclarativeComponent valid(&engine);
    valid.setData("import QtQuick 1.1; BorderImage { " + code.toUtf8() + " }", QUrl(""));
    obj = valid.create();
    QVERIFY(obj);
    QVERIFY(valid.errorString().isEmpty());
    delete obj;

    QDeclarativeComponent invalid(&engine);
    invalid.setData("import QtQuick 1.0; BorderImage { " + code.toUtf8() + " }", QUrl(""));
    QTest::ignoreMessage(QtWarningMsg, warning.toUtf8());
    obj = invalid.create();
    QCOMPARE(invalid.errorString(), error);
    delete obj;
}

void tst_qdeclarativeborderimage::testQtQuick11Attributes_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("warning");
    QTest::addColumn<QString>("error");

    QTest::newRow("mirror") << "mirror: true"
        << "QDeclarativeComponent: Component is not ready"
        << ":1 \"BorderImage.mirror\" is not available in QtQuick 1.0.\n";

    QTest::newRow("cache") << "cache: true"
        << "QDeclarativeComponent: Component is not ready"
        << ":1 \"BorderImage.cache\" is not available in QtQuick 1.0.\n";
}

QTEST_MAIN(tst_qdeclarativeborderimage)

#include "tst_qdeclarativeborderimage.moc"
