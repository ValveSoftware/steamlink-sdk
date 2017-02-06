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

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/qquickview.h>
#include <private/qquickimage_p.h>
#include <private/qquickimagebase_p.h>
#include <private/qquickloader_p.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlexpression.h>
#include <QtTest/QSignalSpy>
#include <QtGui/QPainter>
#include <QtGui/QImageReader>
#include <QQuickWindow>
#include <QQuickView>
#include <QQuickImageProvider>

#include "../../shared/util.h"
#include "../../shared/testhttpserver.h"
#include "../shared/visualtestutil.h"


using namespace QQuickVisualTestUtil;

Q_DECLARE_METATYPE(QQuickImageBase::Status)

class tst_qquickimage : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickimage();

private slots:
    void cleanup();
    void noSource();
    void imageSource();
    void imageSource_data();
    void clearSource();
    void resized();
    void preserveAspectRatio();
    void smooth();
    void mirror();
    void svg();
    void svg_data();
    void geometry();
    void geometry_data();
    void big();
    void tiling_QTBUG_6716();
    void tiling_QTBUG_6716_data();
    void noLoading();
    void paintedWidthHeight();
    void sourceSize_QTBUG_14303();
    void sourceSize_QTBUG_16389();
    void nullPixmapPaint();
    void imageCrash_QTBUG_22125();
    void imageCrash_QTBUG_32513();
    void sourceSize_data();
    void sourceSize();
    void progressAndStatusChanges();
    void sourceSizeChanges();
    void correctStatus();
    void highdpi();
    void hugeImages();

private:
    QQmlEngine engine;
};

tst_qquickimage::tst_qquickimage()
{
}

void tst_qquickimage::cleanup()
{
    QQuickWindow window;
    window.releaseResources();
    engine.clearComponentCache();
}

void tst_qquickimage::noSource()
{
    QString componentStr = "import QtQuick 2.0\nImage { source: \"\" }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->source(), QUrl());
    QCOMPARE(obj->status(), QQuickImage::Null);
    QCOMPARE(obj->width(), 0.);
    QCOMPARE(obj->height(), 0.);
    QCOMPARE(obj->fillMode(), QQuickImage::Stretch);
    QCOMPARE(obj->progress(), 0.0);

    delete obj;
}

void tst_qquickimage::imageSource_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<double>("width");
    QTest::addColumn<double>("height");
    QTest::addColumn<bool>("remote");
    QTest::addColumn<bool>("async");
    QTest::addColumn<bool>("cache");
    QTest::addColumn<QString>("error");

    QTest::newRow("local") << testFileUrl("colors.png").toString() << 120.0 << 120.0 << false << false << true << "";
    QTest::newRow("local no cache") << testFileUrl("colors.png").toString() << 120.0 << 120.0 << false << false << false << "";
    QTest::newRow("local async") << testFileUrl("colors1.png").toString() << 120.0 << 120.0 << false << true << true << "";
    QTest::newRow("local not found") << testFileUrl("no-such-file.png").toString() << 0.0 << 0.0 << false
        << false << true << "<Unknown File>:2:1: QML Image: Cannot open: " + testFileUrl("no-such-file.png").toString();
    QTest::newRow("local async not found") << testFileUrl("no-such-file-1.png").toString() << 0.0 << 0.0 << false
        << true << true << "<Unknown File>:2:1: QML Image: Cannot open: " + testFileUrl("no-such-file-1.png").toString();
    QTest::newRow("remote") << "/colors.png" << 120.0 << 120.0 << true << false << true << "";
    QTest::newRow("remote redirected") << "/oldcolors.png" << 120.0 << 120.0 << true << false << false << "";
    if (QImageReader::supportedImageFormats().contains("svg"))
        QTest::newRow("remote svg") << "/heart.svg" << 550.0 << 500.0 << true << false << false << "";
    if (QImageReader::supportedImageFormats().contains("svgz"))
        QTest::newRow("remote svgz") << "/heart.svgz" << 550.0 << 500.0 << true << false << false << "";
    QTest::newRow("remote not found") << "/no-such-file.png" << 0.0 << 0.0 << true
        << false << true << "<Unknown File>:2:1: QML Image: Error transferring {{ServerBaseUrl}}/no-such-file.png - server replied: Not found";

}

void tst_qquickimage::imageSource()
{
    QFETCH(QString, source);
    QFETCH(double, width);
    QFETCH(double, height);
    QFETCH(bool, remote);
    QFETCH(bool, async);
    QFETCH(bool, cache);
    QFETCH(QString, error);


#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (qstrcmp(QTest::currentDataTag(), "remote") == 0
        || qstrcmp(QTest::currentDataTag(), "remote redirected") == 0
        || qstrcmp(QTest::currentDataTag(), "remote svg") == 0
        || qstrcmp(QTest::currentDataTag(), "remote svgz") == 0
        || qstrcmp(QTest::currentDataTag(), "remote not found") == 0
       ) {
        QSKIP("Remote tests cause occasional hangs in the CI system -- QTBUG-45655");
    }
#endif

    TestHTTPServer server;
    if (remote) {
        QVERIFY2(server.listen(), qPrintable(server.errorString()));
        server.serveDirectory(dataDirectory());
        server.addRedirect("oldcolors.png", server.urlString("/colors.png"));
        source = server.urlString(source);
        error.replace(QStringLiteral("{{ServerBaseUrl}}"), server.baseUrl().toString());
    }

    if (!error.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, error.toUtf8());

    QString componentStr = "import QtQuick 2.0\nImage { source: \"" + source + "\"; asynchronous: "
        + (async ? QLatin1String("true") : QLatin1String("false")) + "; cache: "
        + (cache ? QLatin1String("true") : QLatin1String("false")) + " }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);

    if (async)
        QVERIFY(obj->asynchronous());
    else
        QVERIFY(!obj->asynchronous());

    if (cache)
        QVERIFY(obj->cache());
    else
        QVERIFY(!obj->cache());

    if (remote || async)
        QTRY_COMPARE(obj->status(), QQuickImage::Loading);

    QCOMPARE(obj->source(), remote ? source : QUrl(source));

    if (error.isEmpty()) {
        QTRY_COMPARE(obj->status(), QQuickImage::Ready);
        QCOMPARE(obj->width(), qreal(width));
        QCOMPARE(obj->height(), qreal(height));
        QCOMPARE(obj->fillMode(), QQuickImage::Stretch);
        QCOMPARE(obj->progress(), 1.0);
    } else {
        QTRY_COMPARE(obj->status(), QQuickImage::Error);
    }

    delete obj;
}

void tst_qquickimage::clearSource()
{
    QString componentStr = "import QtQuick 2.0\nImage { source: srcImage }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", testFileUrl("colors.png"));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->status(), QQuickImage::Ready);
    QCOMPARE(obj->width(), 120.);
    QCOMPARE(obj->height(), 120.);
    QCOMPARE(obj->progress(), 1.0);

    ctxt->setContextProperty("srcImage", "");
    QVERIFY(obj->source().isEmpty());
    QCOMPARE(obj->status(), QQuickImage::Null);
    QCOMPARE(obj->width(), 0.);
    QCOMPARE(obj->height(), 0.);
    QCOMPARE(obj->progress(), 0.0);

    delete obj;
}

void tst_qquickimage::resized()
{
    QString componentStr = "import QtQuick 2.0\nImage { source: \"" + testFile("colors.png") + "\"; width: 300; height: 300 }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);
    QCOMPARE(obj->fillMode(), QQuickImage::Stretch);
    delete obj;
}


void tst_qquickimage::preserveAspectRatio()
{
    QScopedPointer<QQuickView> window(new QQuickView(0));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    window->setSource(testFileUrl("aspectratio.qml"));
    QQuickImage *image = qobject_cast<QQuickImage*>(window->rootObject());
    QVERIFY(image != 0);
    image->setWidth(80.0);
    QCOMPARE(image->width(), 80.);
    QCOMPARE(image->height(), 80.);

    window->setSource(testFileUrl("aspectratio.qml"));
    image = qobject_cast<QQuickImage*>(window->rootObject());
    image->setHeight(60.0);
    QVERIFY(image != 0);
    QCOMPARE(image->height(), 60.);
    QCOMPARE(image->width(), 60.);
}

void tst_qquickimage::smooth()
{
    QString componentStr = "import QtQuick 2.0\nImage { source: \"" + testFile("colors.png") + "\"; smooth: true; width: 300; height: 300 }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.);
    QCOMPARE(obj->height(), 300.);
    QCOMPARE(obj->smooth(), true);
    QCOMPARE(obj->fillMode(), QQuickImage::Stretch);

    delete obj;
}

void tst_qquickimage::mirror()
{
    QMap<QQuickImage::FillMode, QImage> screenshots;
    QList<QQuickImage::FillMode> fillModes;
    fillModes << QQuickImage::Stretch << QQuickImage::PreserveAspectFit << QQuickImage::PreserveAspectCrop
              << QQuickImage::Tile << QQuickImage::TileVertically << QQuickImage::TileHorizontally << QQuickImage::Pad;

    qreal width = 300;
    qreal height = 250;
    qreal devicePixelRatio = 1.0;

    foreach (QQuickImage::FillMode fillMode, fillModes) {
#if defined(Q_OS_BLACKBERRY)
        QWindow dummy;          // On BlackBerry first window is always full screen,
        dummy.showFullScreen(); // so make test window a second window.
#endif
        QScopedPointer<QQuickView> window(new QQuickView);
        window->setSource(testFileUrl("mirror.qml"));

        QQuickImage *obj = window->rootObject()->findChild<QQuickImage*>("image");
        QVERIFY(obj != 0);

        obj->setFillMode(fillMode);
        obj->setProperty("mirror", true);
        window->showNormal();
        QVERIFY(QTest::qWaitForWindowExposed(window.data()));

        QImage screenshot = window->grabWindow();
        screenshots[fillMode] = screenshot;
        devicePixelRatio = window->devicePixelRatio();
    }

    foreach (QQuickImage::FillMode fillMode, fillModes) {
        QPixmap srcPixmap;
        QVERIFY(srcPixmap.load(testFile("pattern.png")));

        QPixmap expected(width * (int)devicePixelRatio, height * (int)devicePixelRatio);
        expected.setDevicePixelRatio(devicePixelRatio);
        expected.fill();
        QPainter p_e(&expected);
        QTransform transform;
        transform.translate(width, 0).scale(-1, 1.0);
        p_e.setTransform(transform);

        QPoint offset(width / 2 - srcPixmap.width() / 2, height / 2 - srcPixmap.height() / 2);

        switch (fillMode) {
        case QQuickImage::Stretch:
            p_e.drawPixmap(QRect(0, 0, width, height), srcPixmap, QRect(0, 0, srcPixmap.width(), srcPixmap.height()));
            break;
        case QQuickImage::PreserveAspectFit:
            p_e.drawPixmap(QRect(25, 0, height, height), srcPixmap, QRect(0, 0, srcPixmap.width(), srcPixmap.height()));
            break;
        case QQuickImage::PreserveAspectCrop:
        {
            qreal ratio = width/srcPixmap.width(); // width is the longer side
            QRect rect(0, 0, srcPixmap.width()*ratio, srcPixmap.height()*ratio);
            rect.moveCenter(QRect(0, 0, width, height).center());
            p_e.drawPixmap(rect, srcPixmap, QRect(0, 0, srcPixmap.width(), srcPixmap.height()));
            break;
        }
        case QQuickImage::Tile:
            p_e.drawTiledPixmap(QRect(0, 0, width, height), srcPixmap, -offset);
            break;
        case QQuickImage::TileVertically:
            transform.scale(width / srcPixmap.width(), 1.0);
            p_e.setTransform(transform);
            p_e.drawTiledPixmap(QRect(0, 0, width, height), srcPixmap, QPoint(0, -offset.y()));
            break;
        case QQuickImage::TileHorizontally:
            transform.scale(1.0, height / srcPixmap.height());
            p_e.setTransform(transform);
            p_e.drawTiledPixmap(QRect(0, 0, width, height), srcPixmap, QPoint(-offset.x(), 0));
            break;
        case QQuickImage::Pad:
            p_e.drawPixmap(offset, srcPixmap);
            break;
        }

        QImage img = expected.toImage();
        QCOMPARE(screenshots[fillMode], img);
    }
}

void tst_qquickimage::svg_data()
{
    QTest::addColumn<QString>("src");
    QTest::addColumn<QByteArray>("format");

    QTest::newRow("svg") << testFileUrl("heart.svg").toString() << QByteArray("svg");
    QTest::newRow("svgz") << testFileUrl("heart.svgz").toString() << QByteArray("svgz");
}

void tst_qquickimage::svg()
{
    QFETCH(QString, src);
    QFETCH(QByteArray, format);
    if (!QImageReader::supportedImageFormats().contains(format))
        QSKIP("svg support not available");

    QString componentStr = "import QtQuick 2.0\nImage { source: \"" + src + "\"; sourceSize.width: 300; sourceSize.height: 300 }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 300.0);
    QCOMPARE(obj->height(), 273.0);
    obj->setSourceSize(QSize(200,200));

    QCOMPARE(obj->width(), 200.0);
    QCOMPARE(obj->height(), 182.0);
    delete obj;
}

void tst_qquickimage::geometry_data()
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

void tst_qquickimage::geometry()
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

    QString src = testFileUrl("rect.png").toString();
    QString componentStr = "import QtQuick 2.0\nImage { source: \"" + src + "\"; fillMode: Image." + fillMode + "; ";

    if (explicitWidth)
        componentStr.append("width: 300; ");
    if (explicitHeight)
        componentStr.append("height: 400; ");
    componentStr.append("}");
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->width(), itemWidth);
    QCOMPARE(obj->paintedWidth(), paintedWidth);
    QCOMPARE(obj->boundingRect().width(), boundingWidth);

    QCOMPARE(obj->height(), itemHeight);
    QCOMPARE(obj->paintedHeight(), paintedHeight);
    QCOMPARE(obj->boundingRect().height(), boundingHeight);
    delete obj;
}

void tst_qquickimage::big()
{
    // If the JPEG loader does not implement scaling efficiently, it would
    // have to build a 400 MB image. That would be a bug in the JPEG loader.

    QString src = testFileUrl("big.jpeg").toString();
    QString componentStr = "import QtQuick 2.0\nImage { source: \"" + src + "\"; width: 100; sourceSize.height: 256 }";

    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->width(), 100.0);
    QCOMPARE(obj->height(), 256.0);

    delete obj;
}

void tst_qquickimage::tiling_QTBUG_6716()
{
    QFETCH(QString, source);

    QQuickView view(testFileUrl(source));
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QQuickImage *tiling = findItem<QQuickImage>(view.rootObject(), "tiling");

    QVERIFY(tiling != 0);
    QImage img = view.grabWindow();
    for (int x = 0; x < tiling->width(); ++x) {
        for (int y = 0; y < tiling->height(); ++y) {
            QVERIFY(img.pixel(x, y) == qRgb(0, 255, 0));
        }
    }
}

void tst_qquickimage::tiling_QTBUG_6716_data()
{
    QTest::addColumn<QString>("source");
    QTest::newRow("vertical_tiling") << "vtiling.qml";
    QTest::newRow("horizontal_tiling") << "htiling.qml";
}

void tst_qquickimage::noLoading()
{
    qRegisterMetaType<QQuickImageBase::Status>();

    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());
    server.addRedirect("oldcolors.png", server.urlString("/colors.png"));

    QString componentStr = "import QtQuick 2.0\nImage { source: srcImage; cache: true }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", testFileUrl("heart.png"));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->status(), QQuickImage::Ready);

    QSignalSpy sourceSpy(obj, SIGNAL(sourceChanged(QUrl)));
    QSignalSpy progressSpy(obj, SIGNAL(progressChanged(qreal)));
    QSignalSpy statusSpy(obj, SIGNAL(statusChanged(QQuickImageBase::Status)));

    // Loading local file
    ctxt->setContextProperty("srcImage", testFileUrl("green.png"));
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 1);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 1);

    // Loading remote file
    ctxt->setContextProperty("srcImage", server.url("/rect.png"));
    QTRY_COMPARE(obj->status(), QQuickImage::Loading);
    QTRY_COMPARE(obj->progress(), 0.0);
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 2);
    QTRY_VERIFY(progressSpy.count() >= 2);
    QTRY_COMPARE(statusSpy.count(), 3);

    // Loading remote file again - should not go through 'Loading' state.
    progressSpy.clear();
    ctxt->setContextProperty("srcImage", testFileUrl("green.png"));
    ctxt->setContextProperty("srcImage", server.url("/rect.png"));
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 4);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 5);

    delete obj;
}

void tst_qquickimage::paintedWidthHeight()
{
    {
        QString src = testFileUrl("heart.png").toString();
        QString componentStr = "import QtQuick 2.0\nImage { source: \"" + src + "\"; width: 200; height: 25; fillMode: Image.PreserveAspectFit }";

        QQmlComponent component(&engine);
        component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
        QVERIFY(obj != 0);
        QCOMPARE(obj->width(), 200.0);
        QCOMPARE(obj->height(), 25.0);
        QCOMPARE(obj->paintedWidth(), 25.0);
        QCOMPARE(obj->paintedHeight(), 25.0);

        delete obj;
    }

    {
        QString src = testFileUrl("heart.png").toString();
        QString componentStr = "import QtQuick 2.0\nImage { source: \"" + src + "\"; width: 26; height: 175; fillMode: Image.PreserveAspectFit }";
        QQmlComponent component(&engine);
        component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
        QVERIFY(obj != 0);
        QCOMPARE(obj->width(), 26.0);
        QCOMPARE(obj->height(), 175.0);
        QCOMPARE(obj->paintedWidth(), 26.0);
        QCOMPARE(obj->paintedHeight(), 26.0);

        delete obj;
    }
}

void tst_qquickimage::sourceSize_QTBUG_14303()
{
    QString componentStr = "import QtQuick 2.0\nImage { source: srcImage }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", testFileUrl("heart200.png"));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());

    QSignalSpy sourceSizeSpy(obj, SIGNAL(sourceSizeChanged()));

    QTRY_VERIFY(obj != 0);
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);

    QTRY_COMPARE(obj->sourceSize().width(), 200);
    QTRY_COMPARE(obj->sourceSize().height(), 200);
    QTRY_COMPARE(sourceSizeSpy.count(), 0);

    ctxt->setContextProperty("srcImage", testFileUrl("colors.png"));
    QTRY_COMPARE(obj->sourceSize().width(), 120);
    QTRY_COMPARE(obj->sourceSize().height(), 120);
    QTRY_COMPARE(sourceSizeSpy.count(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("heart200.png"));
    QTRY_COMPARE(obj->sourceSize().width(), 200);
    QTRY_COMPARE(obj->sourceSize().height(), 200);
    QTRY_COMPARE(sourceSizeSpy.count(), 2);

    delete obj;
}

void tst_qquickimage::sourceSize_QTBUG_16389()
{
    QScopedPointer<QQuickView> window(new QQuickView(0));
    window->setSource(testFileUrl("qtbug_16389.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickImage *image = findItem<QQuickImage>(window->rootObject(), "iconImage");
    QQuickItem *handle = findItem<QQuickItem>(window->rootObject(), "blueHandle");

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

// QTBUG-15690
void tst_qquickimage::nullPixmapPaint()
{
    QScopedPointer<QQuickView> window(new QQuickView(0));
    window->setSource(testFileUrl("nullpixmap.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory(), TestHTTPServer::Delay);

    QQuickImage *image = qobject_cast<QQuickImage*>(window->rootObject());
    QTRY_VERIFY(image != 0);
    image->setSource(server.url("/no-such-file.png"));

    QQmlTestMessageHandler messageHandler;
    // used to print "QTransform::translate with NaN called"
    QPixmap pm = QPixmap::fromImage(window->grabWindow());
    QVERIFY2(messageHandler.messages().size() == 0, qPrintable(messageHandler.messageString()));
    delete image;
}

void tst_qquickimage::imageCrash_QTBUG_22125()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory(), TestHTTPServer::Delay);

    {
        QQuickView view;
        view.rootContext()->setContextProperty(QStringLiteral("serverBaseUrl"), server.baseUrl());
        view.setSource(testFileUrl("qtbug_22125.qml"));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        qApp->processEvents();
        // shouldn't crash when the view drops out of scope due to
        // QQuickPixmapData attempting to dereference a pointer to
        // the destroyed reader.
    }

    // shouldn't crash when deleting cancelled QQmlPixmapReplys.
    server.sendDelayedItem();
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

void tst_qquickimage::imageCrash_QTBUG_32513()
{
    QQuickView view(testFileUrl("qtbug_32513.qml"));
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTest::qWait(1000);
    // shouldn't crash when the image changes sources
}

void tst_qquickimage::sourceSize_data()
{
    QTest::addColumn<int>("sourceWidth");
    QTest::addColumn<int>("sourceHeight");
    QTest::addColumn<qreal>("implicitWidth");
    QTest::addColumn<qreal>("implicitHeight");

    QTest::newRow("unscaled") << 0 << 0 << 300.0 << 300.0;
    QTest::newRow("scale width") << 100 << 0 << 100.0 << 100.0;
    QTest::newRow("scale height") << 0 << 150 << 150.0 << 150.0;
    QTest::newRow("larger sourceSize") << 400 << 400 << 300.0 << 300.0;
}

void tst_qquickimage::sourceSize()
{
    QFETCH(int, sourceWidth);
    QFETCH(int, sourceHeight);
    QFETCH(qreal, implicitWidth);
    QFETCH(qreal, implicitHeight);

    QScopedPointer<QQuickView> window(new QQuickView(0));
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("srcWidth", sourceWidth);
    ctxt->setContextProperty("srcHeight", sourceHeight);

    window->setSource(testFileUrl("sourceSize.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickImage *image = qobject_cast<QQuickImage*>(window->rootObject());
    QVERIFY(image);

    QCOMPARE(image->sourceSize().width(), sourceWidth);
    QCOMPARE(image->sourceSize().height(), sourceHeight);
    QCOMPARE(image->implicitWidth(), implicitWidth);
    QCOMPARE(image->implicitHeight(), implicitHeight);
}

void tst_qquickimage::sourceSizeChanges()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nImage { source: srcImage }", QUrl::fromLocalFile(""));
    QTRY_VERIFY(component.isReady());
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", "");
    QQuickImage *img = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(img != 0);

    QSignalSpy sourceSizeSpy(img, SIGNAL(sourceSizeChanged()));

    // Local
    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(img->status(), QQuickImage::Null);
    QTRY_COMPARE(sourceSizeSpy.count(), 0);

    ctxt->setContextProperty("srcImage", testFileUrl("heart.png"));
    QTRY_COMPARE(img->status(), QQuickImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("heart.png"));
    QTRY_COMPARE(img->status(), QQuickImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("heart_copy.png"));
    QTRY_COMPARE(img->status(), QQuickImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("colors.png"));
    QTRY_COMPARE(img->status(), QQuickImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 2);

    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(img->status(), QQuickImage::Null);
    QTRY_COMPARE(sourceSizeSpy.count(), 3);

    // Remote
    ctxt->setContextProperty("srcImage", server.url("/heart.png"));
    QTRY_COMPARE(img->status(), QQuickImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", server.url("/heart.png"));
    QTRY_COMPARE(img->status(), QQuickImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", server.url("/heart_copy.png"));
    QTRY_COMPARE(img->status(), QQuickImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", server.url("/colors.png"));
    QTRY_COMPARE(img->status(), QQuickImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 5);

    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(img->status(), QQuickImage::Null);
    QTRY_COMPARE(sourceSizeSpy.count(), 6);

    delete img;
}

void tst_qquickimage::progressAndStatusChanges()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QString componentStr = "import QtQuick 2.0\nImage { source: srcImage }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", testFileUrl("heart.png"));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);

    qRegisterMetaType<QQuickImageBase::Status>();
    QSignalSpy sourceSpy(obj, SIGNAL(sourceChanged(QUrl)));
    QSignalSpy progressSpy(obj, SIGNAL(progressChanged(qreal)));
    QSignalSpy statusSpy(obj, SIGNAL(statusChanged(QQuickImageBase::Status)));

    // Same image
    ctxt->setContextProperty("srcImage", testFileUrl("heart.png"));
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 0);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 0);

    // Loading local file
    ctxt->setContextProperty("srcImage", testFileUrl("colors.png"));
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 1);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 1);

    // Loading remote file
    ctxt->setContextProperty("srcImage", server.url("/heart.png"));
    QTRY_COMPARE(obj->status(), QQuickImage::Loading);
    QTRY_COMPARE(obj->progress(), 0.0);
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 2);
    QTRY_VERIFY(progressSpy.count() > 1);
    QTRY_COMPARE(statusSpy.count(), 3);

    ctxt->setContextProperty("srcImage", "");
    QTRY_COMPARE(obj->status(), QQuickImage::Null);
    QTRY_COMPARE(obj->progress(), 0.0);
    QTRY_COMPARE(sourceSpy.count(), 3);
    QTRY_VERIFY(progressSpy.count() > 2);
    QTRY_COMPARE(statusSpy.count(), 4);

    delete obj;
}

class TestQImageProvider : public QQuickImageProvider
{
public:
    TestQImageProvider() : QQuickImageProvider(Image) {}

    QImage requestImage(const QString &id, QSize *size, const QSize& requestedSize)
    {
        Q_UNUSED(requestedSize);
        if (id == QLatin1String("first-image.png")) {
            QTest::qWait(50);
            int width = 100;
            int height = 100;
            QImage image(width, height, QImage::Format_RGB32);
            image.fill(QColor("yellow").rgb());
            if (size)
                *size = QSize(width, height);
            return image;
        }

        QTest::qWait(400);
        int width = 100;
        int height = 100;
        QImage image(width, height, QImage::Format_RGB32);
        image.fill(QColor("green").rgb());
        if (size)
            *size = QSize(width, height);
        return image;
    }
};

void tst_qquickimage::correctStatus()
{
    QQmlEngine engine;
    engine.addImageProvider(QLatin1String("test"), new TestQImageProvider());

    QQmlComponent component(&engine, testFileUrl("correctStatus.qml"));
    QObject *obj = component.create();
    QVERIFY(obj);

    QTest::qWait(200);

    // at this point image1 should be attempting to load second-image.png,
    // and should be in the loading state. Without a clear prior to that load,
    // the status can mistakenly be in the ready state.
    QCOMPARE(obj->property("status").toInt(), int(QQuickImage::Loading));

    QTest::qWait(400);
    delete obj;
}

void tst_qquickimage::highdpi()
{
    QString componentStr = "import QtQuick 2.0\nImage { source: srcImage ;  }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQmlContext *ctxt = engine.rootContext();

    // Testing "@2x" high-dpi image loading:
    // The basic case is as follows. Suppose you have foo.png,
    // which is a 64x64 png that fits in a QML layout. Now,
    // on a high-dpi system that pixmap would not provide
    // enough pixels. To fix this the app developer provides
    // a 128x128 foo@2x.png, which Qt automatically loads.
    // The image continues to be referred to as "foo.png" in
    // the QML sources, and reports a size of 64x64.
    //

    // Load "heart-highdpi@2x.png", which is a 300x300 png. As a 2x scale image it
    // should render and report a geometry of 150x150.
    ctxt->setContextProperty("srcImage", testFileUrl("heart-highdpi@2x.png"));

    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->width(), 150.0);
    QCOMPARE(obj->height(), 150.0);
    QCOMPARE(obj->paintedWidth(), 150.0);
    QCOMPARE(obj->paintedHeight(), 150.0);

    // Load a normal 1x image.
    ctxt->setContextProperty("srcImage", testFileUrl("heart.png"));
    QCOMPARE(obj->width(), 300.0);
    QCOMPARE(obj->height(), 300.0);
    QCOMPARE(obj->paintedWidth(), 300.0);
    QCOMPARE(obj->paintedHeight(), 300.0);

    delete obj;
}

void tst_qquickimage::hugeImages()
{
    QQuickView view;
    view.setSource(testFileUrl("hugeImages.qml"));
    view.setGeometry(0, 0, 200, 200);
    view.create();

    QImage contents = view.grabWindow();

    QCOMPARE(contents.pixel(0, 0), qRgba(255, 0, 0, 255));
    QCOMPARE(contents.pixel(99, 99), qRgba(255, 0, 0, 255));
    QCOMPARE(contents.pixel(100, 0), qRgba(0, 0, 255, 255));
    QCOMPARE(contents.pixel(199, 99), qRgba(0, 0, 255, 255));
}

QTEST_MAIN(tst_qquickimage)

#include "tst_qquickimage.moc"
