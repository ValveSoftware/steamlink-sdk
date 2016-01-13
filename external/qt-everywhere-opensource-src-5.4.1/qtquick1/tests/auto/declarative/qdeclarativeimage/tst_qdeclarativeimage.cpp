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

#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/qdeclarativeview.h>
#include <private/qdeclarativeimage_p.h>
#include <private/qdeclarativeimagebase_p.h>
#include <private/qdeclarativeloader_p.h>
#include <QtDeclarative/qdeclarativecontext.h>
#include <QtDeclarative/qdeclarativeexpression.h>
#include <QtTest/QSignalSpy>
#include <QtGui/qimagereader.h>

#include "../shared/testhttpserver.h"

#define SERVER_PORT 14451
#define SERVER_ADDR "http://127.0.0.1:14451"

class tst_qdeclarativeimage : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativeimage();

private slots:
    void initTestCase() {
        qRegisterMetaType<QDeclarativeImageBase::Status>("QDeclarativeImageBase::Status");
    }

    void noSource();
    void imageSource();
    void imageSource_data();
    void clearSource();
    void resized();
    void preserveAspectRatio();
    void smooth();
    void mirror();
    void mirror_data();
    void svg();
    void geometry();
    void geometry_data();
    void big();
    void tiling_QTBUG_6716();
    void noLoading();
    void paintedWidthHeight();
    void sourceSize_QTBUG_14303();
    void sourceSize_QTBUG_16389();
    void nullPixmapPaint();
    void resetSourceSize();
    void testQtQuick11Attributes();
    void testQtQuick11Attributes_data();
    void readerCrash_QTBUG_22125();

private:
    template<typename T>
    T *findItem(QGraphicsObject *parent, const QString &id, int index=-1);

    QDeclarativeEngine engine;
};

tst_qdeclarativeimage::tst_qdeclarativeimage()
{
}

void tst_qdeclarativeimage::noSource()
{
    QString componentStr = "import QtQuick 1.0\nImage { source: \"\" }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->source(), QUrl());
    QVERIFY(obj->status() == QDeclarativeImage::Null);
    QCOMPARE(obj->width(), 0.);
    QCOMPARE(obj->height(), 0.);
    QCOMPARE(obj->fillMode(), QDeclarativeImage::Stretch);
    QCOMPARE(obj->progress(), 0.0);

    delete obj;
}

void tst_qdeclarativeimage::imageSource_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<double>("width");
    QTest::addColumn<double>("height");
    QTest::addColumn<bool>("remote");
    QTest::addColumn<bool>("async");
    QTest::addColumn<bool>("cache");
    QTest::addColumn<QString>("error");

    QTest::newRow("local") << QUrl::fromLocalFile(SRCDIR "/data/colors.png").toString() << 120.0 << 120.0 << false << false << true << "";
    QTest::newRow("local no cache") << QUrl::fromLocalFile(SRCDIR "/data/colors.png").toString() << 120.0 << 120.0 << false << false << false << "";
    QTest::newRow("local async") << QUrl::fromLocalFile(SRCDIR "/data/colors1.png").toString() << 120.0 << 120.0 << false << true << true << "";
    QTest::newRow("local not found") << QUrl::fromLocalFile(SRCDIR "/data/no-such-file.png").toString() << 0.0 << 0.0 << false
        << false << true << "<Unknown File>:2:1: QML Image: Cannot open: " + QUrl::fromLocalFile(SRCDIR "/data/no-such-file.png").toString();
    QTest::newRow("local async not found") << QUrl::fromLocalFile(SRCDIR "/data/no-such-file-1.png").toString() << 0.0 << 0.0 << false
        << true << true << "<Unknown File>:2:1: QML Image: Cannot open: " + QUrl::fromLocalFile(SRCDIR "/data/no-such-file-1.png").toString();
    QTest::newRow("remote") << SERVER_ADDR "/colors.png" << 120.0 << 120.0 << true << false << true << "";
    QTest::newRow("remote redirected") << SERVER_ADDR "/oldcolors.png" << 120.0 << 120.0 << true << false << false << "";
    if (QImageReader::supportedImageFormats().contains("SVG"))
        QTest::newRow("remote svg") << SERVER_ADDR "/heart.svg" << 550.0 << 500.0 << true << false << false << "";
    QTest::newRow("remote not found") << SERVER_ADDR "/no-such-file.png" << 0.0 << 0.0 << true
        << false << true << "<Unknown File>:2:1: QML Image: Error downloading " SERVER_ADDR "/no-such-file.png - server replied: Not found";

}

void tst_qdeclarativeimage::imageSource()
{
    QFETCH(QString, source);
    QFETCH(double, width);
    QFETCH(double, height);
    QFETCH(bool, remote);
    QFETCH(bool, async);
    QFETCH(bool, cache);
    QFETCH(QString, error);

    TestHTTPServer server(SERVER_PORT);
    if (remote) {
        QVERIFY(server.isValid());
        server.serveDirectory(SRCDIR "/data");
        server.addRedirect("oldcolors.png", SERVER_ADDR "/colors.png");
    }

    if (!error.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, error.toUtf8());

    QString componentStr = "import QtQuick 1.1\nImage { source: \"" + source + "\"; asynchronous: "
        + (async ? QLatin1String("true") : QLatin1String("false")) + "; cache: "
        + (cache ? QLatin1String("true") : QLatin1String("false")) + " }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);

    if (async)
        QVERIFY(obj->asynchronous() == true);
    else
        QVERIFY(obj->asynchronous() == false);

    if (cache)
        QVERIFY(obj->cache() == true);
    else
        QVERIFY(obj->cache() == false);

    if (remote || async)
        QTRY_VERIFY(obj->status() == QDeclarativeImage::Loading);

    QCOMPARE(obj->source(), remote ? source : QUrl(source));

    if (error.isEmpty()) {
        QTRY_VERIFY(obj->status() == QDeclarativeImage::Ready);
        QCOMPARE(obj->width(), qreal(width));
        QCOMPARE(obj->height(), qreal(height));
        QCOMPARE(obj->fillMode(), QDeclarativeImage::Stretch);
        QCOMPARE(obj->progress(), 1.0);
    } else {
        QTRY_VERIFY(obj->status() == QDeclarativeImage::Error);
    }

    delete obj;
}

void tst_qdeclarativeimage::clearSource()
{
    QString componentStr = "import QtQuick 1.0\nImage { source: srcImage }";
    QDeclarativeContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", QUrl::fromLocalFile(SRCDIR "/data/colors.png"));
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);
    QVERIFY(obj->status() == QDeclarativeImage::Ready);
    QCOMPARE(obj->width(), 120.);
    QCOMPARE(obj->height(), 120.);
    QCOMPARE(obj->progress(), 1.0);

    ctxt->setContextProperty("srcImage", "");
    QVERIFY(obj->source().isEmpty());
    QVERIFY(obj->status() == QDeclarativeImage::Null);
    QCOMPARE(obj->width(), 0.);
    QCOMPARE(obj->height(), 0.);
    QCOMPARE(obj->progress(), 0.0);
}

void tst_qdeclarativeimage::resized()
{
    QString componentStr = "import QtQuick 1.0\nImage { source: \"" SRCDIR "/data/colors.png\"; width: 300; height: 300 }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);
    QCOMPARE(obj->fillMode(), QDeclarativeImage::Stretch);
    delete obj;
}


void tst_qdeclarativeimage::preserveAspectRatio()
{
    QDeclarativeView *canvas = new QDeclarativeView(0);
    canvas->show();

    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/aspectratio.qml"));
    QDeclarativeImage *image = qobject_cast<QDeclarativeImage*>(canvas->rootObject());
    QVERIFY(image != 0);
    QCOMPARE(image->property("widthChange").toInt(), 1);
    QCOMPARE(image->property("heightChange").toInt(), 1);
    image->setWidth(80.0);
    QCOMPARE(image->property("widthChange").toInt(), 2);
    QCOMPARE(image->property("heightChange").toInt(), 2);
    QCOMPARE(image->width(), 80.);
    QCOMPARE(image->height(), 80.);

    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/aspectratio.qml"));
    image = qobject_cast<QDeclarativeImage*>(canvas->rootObject());
    QVERIFY(image != 0);
    QCOMPARE(image->property("widthChange").toInt(), 1);
    QCOMPARE(image->property("heightChange").toInt(), 1);
    image->setHeight(60.0);
    QCOMPARE(image->property("widthChange").toInt(), 2);
    QCOMPARE(image->property("heightChange").toInt(), 2);
    QCOMPARE(image->height(), 60.);
    QCOMPARE(image->width(), 60.);
    delete canvas;
}

void tst_qdeclarativeimage::smooth()
{
    QString componentStr = "import QtQuick 1.0\nImage { source: \"" SRCDIR "/data/colors.png\"; smooth: true; width: 300; height: 300 }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);
    QCOMPARE(obj->smooth(), true);
    QCOMPARE(obj->fillMode(), QDeclarativeImage::Stretch);

    delete obj;
}

void tst_qdeclarativeimage::mirror()
{
    QFETCH(int, fillMode);

    qreal width = 300;
    qreal height = 250;

    QString src = QUrl::fromLocalFile(SRCDIR "/data/heart200.png").toString();
    QString componentStr = "import QtQuick 1.1\nImage { source: \"" + src + "\"; }";

    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);

    obj->setProperty("width", width);
    obj->setProperty("height", height);
    obj->setFillMode((QDeclarativeImage::FillMode)fillMode);
    obj->setProperty("mirror", true);

    QGraphicsScene scene;
    scene.addItem(qobject_cast<QGraphicsObject *>(obj));
    QPixmap screenshot(width, height);
    screenshot.fill();
    QPainter p_screenshot(&screenshot);
    scene.render(&p_screenshot, QRect(0, 0, width, height), QRect(0, 0, width, height));

    QPixmap srcPixmap;
    QVERIFY(srcPixmap.load(SRCDIR "/data/heart200.png"));

    QPixmap expected(width, height);
    expected.fill();
    QPainter p_e(&expected);
    QTransform transform;
    transform.translate(width, 0).scale(-1, 1.0);
    p_e.setTransform(transform);

    switch (fillMode) {
        case QDeclarativeImage::Stretch:
            p_e.drawPixmap(QRect(0, 0, width, height), srcPixmap, QRect(0, 0, srcPixmap.width(), srcPixmap.height()));
            break;
        case QDeclarativeImage::PreserveAspectFit:
            p_e.drawPixmap(QRect(25, 0, height, height), srcPixmap, QRect(0, 0, srcPixmap.width(), srcPixmap.height()));
            break;
        case QDeclarativeImage::PreserveAspectCrop:
        {
            qreal ratio = width/srcPixmap.width(); // width is the longer side
            QRect rect(0, 0, srcPixmap.width()*ratio, srcPixmap.height()*ratio);
            rect.moveCenter(QRect(0, 0, width, height).center());
            p_e.drawPixmap(rect, srcPixmap, QRect(0, 0, srcPixmap.width(), srcPixmap.height()));
            break;
        }
        case QDeclarativeImage::Tile:
            p_e.drawTiledPixmap(QRect(0, 0, width, height), srcPixmap);
            break;
        case QDeclarativeImage::TileVertically:
            transform.scale(width / srcPixmap.width(), 1.0);
            p_e.setTransform(transform);
            p_e.drawTiledPixmap(QRect(0, 0, width, height), srcPixmap);
            break;
        case QDeclarativeImage::TileHorizontally:
            transform.scale(1.0, height / srcPixmap.height());
            p_e.setTransform(transform);
            p_e.drawTiledPixmap(QRect(0, 0, width, height), srcPixmap);
            break;
    }

    QCOMPARE(screenshot, expected);

    delete obj;
}

void tst_qdeclarativeimage::mirror_data()
{
    QTest::addColumn<int>("fillMode");

    QTest::newRow("Stretch") << int(QDeclarativeImage::Stretch);
    QTest::newRow("PreserveAspectFit") << int(QDeclarativeImage::PreserveAspectFit);
    QTest::newRow("PreserveAspectCrop") << int(QDeclarativeImage::PreserveAspectCrop);
    QTest::newRow("Tile") << int(QDeclarativeImage::Tile);
    QTest::newRow("TileVertically") << int(QDeclarativeImage::TileVertically);
    QTest::newRow("TileHorizontally") << int(QDeclarativeImage::TileHorizontally);
}

void tst_qdeclarativeimage::svg()
{
    if (!QImageReader::supportedImageFormats().contains("SVG"))
        QSKIP("No image reader for SVG");

    QString src = QUrl::fromLocalFile(SRCDIR "/data/heart.svg").toString();
    QString componentStr = "import QtQuick 1.0\nImage { source: \"" + src + "\"; sourceSize.width: 300; sourceSize.height: 300 }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->pixmap().width(), 300);
    QCOMPARE(obj->pixmap().height(), 300);
    QCOMPARE(obj->width(), 300.0);
    QCOMPARE(obj->height(), 300.0);
#if defined(Q_OS_LINUX)
    QCOMPARE(obj->pixmap(), QPixmap(SRCDIR "/data/heart.png"));
#elif defined(Q_OS_WIN32)
    QCOMPARE(obj->pixmap(), QPixmap(SRCDIR "/data/heart-win32.png"));
#endif

    obj->setSourceSize(QSize(200,200));

    QCOMPARE(obj->pixmap().width(), 200);
    QCOMPARE(obj->pixmap().height(), 200);
    QCOMPARE(obj->width(), 200.0);
    QCOMPARE(obj->height(), 200.0);
#if defined(Q_OS_LINUX)
    QCOMPARE(obj->pixmap(), QPixmap(SRCDIR "/data/heart200.png"));
#elif defined(Q_OS_WIN32)
    QCOMPARE(obj->pixmap(), QPixmap(SRCDIR "/data/heart200-win32.png"));
#endif
    delete obj;
}

void tst_qdeclarativeimage::geometry_data()
{
    QTest::addColumn<QString>("fillMode");
    QTest::addColumn<bool>("explicitWidth");
    QTest::addColumn<bool>("explicitHeight");
    QTest::addColumn<double>("itemWidth");
    QTest::addColumn<double>("paintedWidth");
    QTest::addColumn<double>("boundingWidth");
    QTest::addColumn<double>("itemHeight");
    QTest::addColumn<double>("paintedHeight");
    QTest::addColumn<double>("boundingHeight");

    // tested image has width 200, height 100

    // bounding rect and item rect are equal with fillMode PreserveAspectFit, painted rect may be smaller if the aspect ratio doesn't match
    QTest::newRow("PreserveAspectFit") << "PreserveAspectFit" << false << false << 200.0 << 200.0 << 200.0 << 100.0 << 100.0 << 100.0;
    QTest::newRow("PreserveAspectFit explicit width 300") << "PreserveAspectFit" << true << false << 300.0 << 200.0 << 300.0 << 100.0 << 100.0 << 100.0;
    QTest::newRow("PreserveAspectFit explicit height 400") << "PreserveAspectFit" << false << true << 200.0 << 200.0 << 200.0 << 400.0 << 100.0 << 400.0;
    QTest::newRow("PreserveAspectFit explicit width 300, height 400") << "PreserveAspectFit" << true << true << 300.0 << 300.0 << 300.0 << 400.0 << 150.0 << 400.0;

    // bounding rect and painted rect are equal with fillMode PreserveAspectCrop, item rect may be smaller if the aspect ratio doesn't match
    QTest::newRow("PreserveAspectCrop") << "PreserveAspectCrop" << false << false << 200.0 << 200.0 << 200.0 << 100.0 << 100.0 << 100.0;
    QTest::newRow("PreserveAspectCrop explicit width 300") << "PreserveAspectCrop" << true << false << 300.0 << 300.0 << 300.0 << 100.0 << 150.0 << 150.0;
    QTest::newRow("PreserveAspectCrop explicit height 400") << "PreserveAspectCrop" << false << true << 200.0 << 800.0 << 800.0 << 400.0 << 400.0 << 400.0;
    QTest::newRow("PreserveAspectCrop explicit width 300, height 400") << "PreserveAspectCrop" << true << true << 300.0 << 800.0 << 800.0 << 400.0 << 400.0 << 400.0;

    // bounding rect, painted rect and item rect are equal in stretching and tiling images
    QStringList fillModes;
    fillModes << "Stretch" << "Tile" << "TileVertically" << "TileHorizontally";
    foreach (QString fillMode, fillModes) {
        QTest::newRow(fillMode.toLatin1()) << fillMode << false << false << 200.0 << 200.0 << 200.0 << 100.0 << 100.0 << 100.0;
        QTest::newRow(QString(fillMode + " explicit width 300").toLatin1()) << fillMode << true << false << 300.0 << 300.0 << 300.0 << 100.0 << 100.0 << 100.0;
        QTest::newRow(QString(fillMode + " explicit height 400").toLatin1()) << fillMode << false << true << 200.0 << 200.0 << 200.0 << 400.0 << 400.0 << 400.0;
        QTest::newRow(QString(fillMode + " explicit width 300, height 400").toLatin1()) << fillMode << true << true << 300.0 << 300.0 << 300.0 << 400.0 << 400.0 << 400.0;
    }
}

void tst_qdeclarativeimage::geometry()
{
    QFETCH(QString, fillMode);
    QFETCH(bool, explicitWidth);
    QFETCH(bool, explicitHeight);
    QFETCH(double, itemWidth);
    QFETCH(double, itemHeight);
    QFETCH(double, paintedWidth);
    QFETCH(double, paintedHeight);
    QFETCH(double, boundingWidth);
    QFETCH(double, boundingHeight);

    QString src = QUrl::fromLocalFile(SRCDIR "/data/rect.png").toString();
    QString componentStr = "import QtQuick 1.0\nImage { source: \"" + src + "\"; fillMode: Image." + fillMode + "; ";

    if (explicitWidth)
        componentStr.append("width: 300; ");
    if (explicitHeight)
        componentStr.append("height: 400; ");
    componentStr.append("}");
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->width(), itemWidth);
    QCOMPARE(obj->paintedWidth(), paintedWidth);
    QCOMPARE(obj->boundingRect().width(), boundingWidth);

    QCOMPARE(obj->height(), itemHeight);
    QCOMPARE(obj->paintedHeight(), paintedHeight);
    QCOMPARE(obj->boundingRect().height(), boundingHeight);
    delete obj;
}

void tst_qdeclarativeimage::big()
{
    // If the JPEG loader does not implement scaling efficiently, it would
    // have to build a 400 MB image. That would be a bug in the JPEG loader.

    QString src = QUrl::fromLocalFile(SRCDIR "/data/big.jpeg").toString();
    QString componentStr = "import QtQuick 1.0\nImage { source: \"" + src + "\"; width: 100; sourceSize.height: 256 }";

    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->pixmap().width(), 256);
    QCOMPARE(obj->pixmap().height(), 256);
    QCOMPARE(obj->width(), 100.0);
    QCOMPARE(obj->height(), 256.0);
    QCOMPARE(obj->pixmap(), QPixmap(SRCDIR "/data/big256.png"));

    delete obj;
}

void tst_qdeclarativeimage::tiling_QTBUG_6716()
{
    QDeclarativeView *canvas = new QDeclarativeView(0);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/tiling.qml"));
    canvas->show();
    qApp->processEvents();

    QDeclarativeImage *vTiling = findItem<QDeclarativeImage>(canvas->rootObject(), "vTiling");
    QDeclarativeImage *hTiling = findItem<QDeclarativeImage>(canvas->rootObject(), "hTiling");

    QVERIFY(vTiling != 0);
    QVERIFY(hTiling != 0);

    {
        QPixmap pm(vTiling->width(), vTiling->height());
        QPainter p(&pm);
        vTiling->paint(&p, 0, 0);

        QImage img = pm.toImage();
        for (int x = 0; x < vTiling->width(); ++x) {
            for (int y = 0; y < vTiling->height(); ++y) {
                QVERIFY(img.pixel(x, y) == qRgb(0, 255, 0));
            }
        }
    }

    {
        QPixmap pm(hTiling->width(), hTiling->height());
        QPainter p(&pm);
        hTiling->paint(&p, 0, 0);

        QImage img = pm.toImage();
        for (int x = 0; x < hTiling->width(); ++x) {
            for (int y = 0; y < hTiling->height(); ++y) {
                QVERIFY(img.pixel(x, y) == qRgb(0, 255, 0));
            }
        }
    }
}

void tst_qdeclarativeimage::noLoading()
{
    TestHTTPServer server(SERVER_PORT);
    QVERIFY(server.isValid());
    server.serveDirectory(SRCDIR "/data");
    server.addRedirect("oldcolors.png", SERVER_ADDR "/colors.png");

    QString componentStr = "import QtQuick 1.0\nImage { source: srcImage }";
    QDeclarativeContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", QUrl::fromLocalFile(SRCDIR "/data/heart.png"));
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);
    QVERIFY(obj->status() == QDeclarativeImage::Ready);

    QSignalSpy sourceSpy(obj, SIGNAL(sourceChanged(const QUrl &)));
    QSignalSpy progressSpy(obj, SIGNAL(progressChanged(qreal)));
    QSignalSpy statusSpy(obj, SIGNAL(statusChanged(QDeclarativeImageBase::Status)));

    // Loading local file
    ctxt->setContextProperty("srcImage", QUrl::fromLocalFile(SRCDIR "/data/colors.png"));
    QTRY_VERIFY(obj->status() == QDeclarativeImage::Ready);
    QTRY_VERIFY(obj->progress() == 1.0);
    QTRY_COMPARE(sourceSpy.count(), 1);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 0);

    // Loading remote file
    ctxt->setContextProperty("srcImage", QString(SERVER_ADDR) + "/heart200.png");
    QTRY_VERIFY(obj->status() == QDeclarativeImage::Loading);
    QTRY_VERIFY(obj->progress() == 0.0);
    QTRY_VERIFY(obj->status() == QDeclarativeImage::Ready);
    QTRY_VERIFY(obj->progress() == 1.0);
    QTRY_COMPARE(sourceSpy.count(), 2);
    QTRY_VERIFY(progressSpy.count() > 1);
    QTRY_COMPARE(statusSpy.count(), 2);

    // Loading remote file again - should not go through 'Loading' state.
    ctxt->setContextProperty("srcImage", QUrl::fromLocalFile(SRCDIR "/data/colors.png"));
    ctxt->setContextProperty("srcImage", QString(SERVER_ADDR) + "/heart200.png");
    QTRY_VERIFY(obj->status() == QDeclarativeImage::Ready);
    QTRY_VERIFY(obj->progress() == 1.0);
    QTRY_COMPARE(sourceSpy.count(), 4);
    QTRY_VERIFY(progressSpy.count() > 1);
    QTRY_COMPARE(statusSpy.count(), 2);
}

void tst_qdeclarativeimage::paintedWidthHeight()
{
    {
        QString src = QUrl::fromLocalFile(SRCDIR "/data/heart.png").toString();
        QString componentStr = "import QtQuick 1.0\nImage { source: \"" + src + "\"; width: 200; height: 25; fillMode: Image.PreserveAspectFit }";

        QDeclarativeComponent component(&engine);
        component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
        QVERIFY(obj != 0);
        QCOMPARE(obj->pixmap().width(), 300);
        QCOMPARE(obj->pixmap().height(), 300);
        QCOMPARE(obj->width(), 200.0);
        QCOMPARE(obj->height(), 25.0);
        QCOMPARE(obj->paintedWidth(), 25.0);
        QCOMPARE(obj->paintedHeight(), 25.0);
        QCOMPARE(obj->pixmap(), QPixmap(SRCDIR "/data/heart.png"));

        delete obj;
    }

    {
        QString src = QUrl::fromLocalFile(SRCDIR "/data/heart.png").toString();
        QString componentStr = "import QtQuick 1.0\nImage { source: \"" + src + "\"; width: 26; height: 175; fillMode: Image.PreserveAspectFit }";
        QDeclarativeComponent component(&engine);
        component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
        QVERIFY(obj != 0);
        QCOMPARE(obj->pixmap().width(), 300);
        QCOMPARE(obj->pixmap().height(), 300);
        QCOMPARE(obj->width(), 26.0);
        QCOMPARE(obj->height(), 175.0);
        QCOMPARE(obj->paintedWidth(), 26.0);
        QCOMPARE(obj->paintedHeight(), 26.0);
        QCOMPARE(obj->pixmap(), QPixmap(SRCDIR "/data/heart.png"));

        delete obj;
    }
}

void tst_qdeclarativeimage::sourceSize_QTBUG_14303()
{
    QString componentStr = "import QtQuick 1.0\nImage { source: srcImage }";
    QDeclarativeContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", QUrl::fromLocalFile(SRCDIR "/data/heart200.png"));
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());

    QSignalSpy sourceSizeSpy(obj, SIGNAL(sourceSizeChanged()));

    QTRY_VERIFY(obj != 0);
    QTRY_VERIFY(obj->status() == QDeclarativeImage::Ready);

    QTRY_COMPARE(obj->sourceSize().width(), 200);
    QTRY_COMPARE(obj->sourceSize().height(), 200);
    QTRY_COMPARE(sourceSizeSpy.count(), 0);

    ctxt->setContextProperty("srcImage", QUrl::fromLocalFile(SRCDIR "/data/colors.png"));
    QTRY_COMPARE(obj->sourceSize().width(), 120);
    QTRY_COMPARE(obj->sourceSize().height(), 120);
    QTRY_COMPARE(sourceSizeSpy.count(), 1);

    ctxt->setContextProperty("srcImage", QUrl::fromLocalFile(SRCDIR "/data/heart200.png"));
    QTRY_COMPARE(obj->sourceSize().width(), 200);
    QTRY_COMPARE(obj->sourceSize().height(), 200);
    QTRY_COMPARE(sourceSizeSpy.count(), 2);
}

void tst_qdeclarativeimage::sourceSize_QTBUG_16389()
{
    QDeclarativeView *canvas = new QDeclarativeView(0);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/qtbug_16389.qml"));
    canvas->show();
    qApp->processEvents();

    QDeclarativeImage *image = findItem<QDeclarativeImage>(canvas->rootObject(), "iconImage");
    QDeclarativeItem *handle = findItem<QDeclarativeItem>(canvas->rootObject(), "blueHandle");

    QCOMPARE(image->sourceSize().width(), 200);
    QCOMPARE(image->sourceSize().height(), 200);
    QCOMPARE(image->paintedWidth(), 0.0);
    QCOMPARE(image->paintedHeight(), 0.0);

    handle->setY(20);

    QCOMPARE(image->sourceSize().width(), 200);
    QCOMPARE(image->sourceSize().height(), 200);
    QCOMPARE(image->paintedWidth(), 20.0);
    QCOMPARE(image->paintedHeight(), 20.0);
}

static int numberOfWarnings = 0;
static void checkWarnings(QtMsgType, const QMessageLogContext &, const QString &)
{
    numberOfWarnings++;
}

// QTBUG-15690
void tst_qdeclarativeimage::nullPixmapPaint()
{
    QString componentStr = QString("import QtQuick 1.0\nImage { width: 10; height:10; fillMode: Image.PreserveAspectFit; source: \"")
            + SERVER_ADDR + QString("/no-such-file.png\" }");
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *image = qobject_cast<QDeclarativeImage*>(component.create());

    QTRY_VERIFY(image != 0);

    QtMessageHandler previousMsgHandler = qInstallMessageHandler(checkWarnings);

    QPixmap pm(100, 100);
    QPainter p(&pm);

    // used to print "QTransform::translate with NaN called"
    image->paint(&p, 0, 0);
    qInstallMessageHandler(previousMsgHandler);
    QVERIFY(numberOfWarnings == 0);
    delete image;
}

void tst_qdeclarativeimage::resetSourceSize()
{
    QString src = QUrl::fromLocalFile(SRCDIR "/data/heart200.png").toString();
    QString componentStr = "import QtQuick 1.1\nImage { function reset() { sourceSize = undefined }\nsource: \"" + src + "\"; sourceSize: Qt.size(100,100) }";

    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->pixmap().width(), 100);
    QCOMPARE(obj->pixmap().height(), 100);
    QCOMPARE(obj->sourceSize().height(), 100);
    QCOMPARE(obj->sourceSize().width(), 100);

    QMetaObject::invokeMethod(obj, "reset");
    QCOMPARE(obj->pixmap().width(), 200);
    QCOMPARE(obj->pixmap().height(), 200);
    QCOMPARE(obj->sourceSize().height(), 200);
    QCOMPARE(obj->sourceSize().width(), 200);
}

void tst_qdeclarativeimage::testQtQuick11Attributes()
{
    QFETCH(QString, code);
    QFETCH(QString, warning);
    QFETCH(QString, error);

    QDeclarativeEngine engine;
    QObject *obj;

    QDeclarativeComponent valid(&engine);
    valid.setData("import QtQuick 1.1; Image { " + code.toUtf8() + " }", QUrl(""));
    obj = valid.create();
    QVERIFY(obj);
    QVERIFY(valid.errorString().isEmpty());
    delete obj;

    QDeclarativeComponent invalid(&engine);
    invalid.setData("import QtQuick 1.0; Image { " + code.toUtf8() + " }", QUrl(""));
    QTest::ignoreMessage(QtWarningMsg, warning.toUtf8());
    obj = invalid.create();
    QCOMPARE(invalid.errorString(), error);
    delete obj;
}

void tst_qdeclarativeimage::testQtQuick11Attributes_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("warning");
    QTest::addColumn<QString>("error");

    QTest::newRow("mirror") << "mirror: true"
        << "QDeclarativeComponent: Component is not ready"
        << ":1 \"Image.mirror\" is not available in QtQuick 1.0.\n";

    QTest::newRow("cache") << "cache: true"
        << "QDeclarativeComponent: Component is not ready"
        << ":1 \"Image.cache\" is not available in QtQuick 1.0.\n";
}

void tst_qdeclarativeimage::readerCrash_QTBUG_22125()
{
    {
        TestHTTPServer server(SERVER_PORT);
        QVERIFY(server.isValid());
        server.serveDirectory(SRCDIR "/data/", TestHTTPServer::Delay);

        {
            QDeclarativeView view(QUrl::fromLocalFile(SRCDIR "/data/qtbug_22125.qml"));
            view.show();
            qApp->processEvents();
            qApp->processEvents();
            // shouldn't crash when the view drops out of scope due to
            // QDeclarativePixmapData attempting to dereference a pointer to
            // the destroyed reader.
        }

        // shouldn't crash when deleting cancelled QDeclarativePixmapReplys.
        QTest::qWait(1000);
        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    }

    {
        TestHTTPServer server(SERVER_PORT);
        QVERIFY(server.isValid());
        server.serveDirectory(SRCDIR "/data/");

        {
            QDeclarativeView view(QUrl::fromLocalFile(SRCDIR "/data/qtbug_22125.qml"));
            view.show();
            qApp->processEvents();
            QTest::qWait(1000);
            qApp->processEvents();
            // shouldn't crash when the view drops out of scope due to
            // the reader thread accessing self-deleted QDeclarativePixmapReplys.
        }
        qApp->processEvents();
    }
}

/*
   Find an item with the specified objectName.  If index is supplied then the
   item must also evaluate the {index} expression equal to index
*/
template<typename T>
T *tst_qdeclarativeimage::findItem(QGraphicsObject *parent, const QString &objectName, int index)
{
    const QMetaObject &mo = T::staticMetaObject;
    //qDebug() << parent->childItems().count() << "children";
    for (int i = 0; i < parent->childItems().count(); ++i) {
        QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(parent->childItems().at(i));
        if(!item)
            continue;
        //qDebug() << "try" << item;
        if (mo.cast(item) && (objectName.isEmpty() || item->objectName() == objectName)) {
            if (index != -1) {
                QDeclarativeExpression e(qmlContext(item), item, "index");
                if (e.evaluate().toInt() == index)
                    return static_cast<T*>(item);
            } else {
                return static_cast<T*>(item);
            }
        }
        item = findItem<T>(item, objectName, index);
        if (item)
            return static_cast<T*>(item);
    }

    return 0;
}

QTEST_MAIN(tst_qdeclarativeimage)

#include "tst_qdeclarativeimage.moc"
