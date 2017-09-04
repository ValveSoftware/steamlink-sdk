/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

//TESTED_COMPONENT=src/multimedia

#include <qtmultimediaglobal.h>
#include "qgraphicsvideoitem.h"
#include <QtTest/QtTest>
#include "qmediaobject.h"
#include "qmediaservice.h"
#include <private/qpaintervideosurface_p.h>
#include "qvideorenderercontrol.h"

#include <qabstractvideosurface.h>
#include <qvideosurfaceformat.h>

#include <QtWidgets/qapplication.h>
#include <QtWidgets/qgraphicsscene.h>
#include <QtWidgets/qgraphicsview.h>

QT_USE_NAMESPACE
class tst_QGraphicsVideoItem : public QObject
{
    Q_OBJECT
public slots:
    void initTestCase();

private slots:
    void nullObject();
    void nullService();
    void noOutputs();
    void serviceDestroyed();
    void mediaObjectDestroyed();
    void setMediaObject();

    void show();

    void aspectRatioMode();
    void offset();
    void size();
    void nativeSize_data();
    void nativeSize();

    void boundingRect_data();
    void boundingRect();

    void paint();
};

Q_DECLARE_METATYPE(const uchar *)
Q_DECLARE_METATYPE(Qt::AspectRatioMode)

class QtTestRendererControl : public QVideoRendererControl
{
public:
    QtTestRendererControl()
        : m_surface(0)
    {
    }

    QAbstractVideoSurface *surface() const { return m_surface; }
    void setSurface(QAbstractVideoSurface *surface) { m_surface = surface; }

private:
    QAbstractVideoSurface *m_surface;
};

class QtTestVideoService : public QMediaService
{
    Q_OBJECT
public:
    QtTestVideoService(
            QtTestRendererControl *renderer)
        : QMediaService(0)
        , rendererRef(0)
        , rendererControl(renderer)
    {
    }

    ~QtTestVideoService()
    {
        delete rendererControl;
    }

    QMediaControl *requestControl(const char *name)
    {
        if (qstrcmp(name, QVideoRendererControl_iid) == 0 && rendererControl) {
            rendererRef += 1;

            return rendererControl;
        } else {
            return 0;
        }
    }

    void releaseControl(QMediaControl *control)
    {
        Q_ASSERT(control);

        if (control == rendererControl) {
            rendererRef -= 1;

            if (rendererRef == 0)
                rendererControl->setSurface(0);
        }
    }

    int rendererRef;
    QtTestRendererControl *rendererControl;
};

class QtTestVideoObject : public QMediaObject
{
    Q_OBJECT
public:
    QtTestVideoObject(QtTestRendererControl *renderer)
        : QMediaObject(0, new QtTestVideoService(renderer))
    {
        testService = qobject_cast<QtTestVideoService*>(service());
    }

    QtTestVideoObject(QtTestVideoService *service):
        QMediaObject(0, service),
        testService(service)
    {
    }

    ~QtTestVideoObject()
    {
        delete testService;
    }

    QtTestVideoService *testService;
};

class QtTestGraphicsVideoItem : public QGraphicsVideoItem
{
public:
    QtTestGraphicsVideoItem(QGraphicsItem *parent = 0)
        : QGraphicsVideoItem(parent)
        , m_paintCount(0)
    {
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    {
        ++m_paintCount;

        QTestEventLoop::instance().exitLoop();

        QGraphicsVideoItem::paint(painter, option, widget);
    }

    bool waitForPaint(int secs)
    {
        const int paintCount = m_paintCount;

        QTestEventLoop::instance().enterLoop(secs);

        return m_paintCount != paintCount;
    }

    int paintCount() const
    {
        return m_paintCount;
    }

private:
    int m_paintCount;
};

void tst_QGraphicsVideoItem::initTestCase()
{
    qRegisterMetaType<Qt::AspectRatioMode>();
}

void tst_QGraphicsVideoItem::nullObject()
{
    QGraphicsVideoItem item(0);

    QVERIFY(item.boundingRect().isEmpty());
}

void tst_QGraphicsVideoItem::nullService()
{
    QtTestVideoService *service = 0;

    QtTestVideoObject object(service);

    QtTestGraphicsVideoItem *item = new QtTestGraphicsVideoItem;
    object.bind(item);

    QVERIFY(item->boundingRect().isEmpty());

    item->hide();
    item->show();

    QGraphicsScene graphicsScene;
    graphicsScene.addItem(item);
    QGraphicsView graphicsView(&graphicsScene);
    graphicsView.show();
}

void tst_QGraphicsVideoItem::noOutputs()
{
    QtTestRendererControl *control = 0;
    QtTestVideoObject object(control);

    QtTestGraphicsVideoItem *item = new QtTestGraphicsVideoItem;
    object.bind(item);

    QVERIFY(item->boundingRect().isEmpty());

    item->hide();
    item->show();

    QGraphicsScene graphicsScene;
    graphicsScene.addItem(item);
    QGraphicsView graphicsView(&graphicsScene);
    graphicsView.show();
}

void tst_QGraphicsVideoItem::serviceDestroyed()
{
    QtTestVideoObject object(new QtTestRendererControl);

    QGraphicsVideoItem item;
    object.bind(&item);

    QCOMPARE(object.testService->rendererRef, 1);

    QtTestVideoService *service = object.testService;
    object.testService = 0;

    delete service;

    QCOMPARE(item.mediaObject(), static_cast<QMediaObject *>(&object));
    QVERIFY(item.boundingRect().isEmpty());
}

void tst_QGraphicsVideoItem::mediaObjectDestroyed()
{
    QtTestVideoObject *object = new QtTestVideoObject(new QtTestRendererControl);

    QGraphicsVideoItem item;
    object->bind(&item);

    QCOMPARE(object->testService->rendererRef, 1);

    delete object;
    object = 0;

    QCOMPARE(item.mediaObject(), static_cast<QMediaObject *>(object));
    QVERIFY(item.boundingRect().isEmpty());
}

void tst_QGraphicsVideoItem::setMediaObject()
{
    QMediaObject *nullObject = 0;
    QtTestVideoObject object(new QtTestRendererControl);

    QGraphicsVideoItem item;

    QCOMPARE(item.mediaObject(), nullObject);
    QCOMPARE(object.testService->rendererRef, 0);

    object.bind(&item);
    QCOMPARE(item.mediaObject(), static_cast<QMediaObject *>(&object));
    QCOMPARE(object.testService->rendererRef, 1);
    QVERIFY(object.testService->rendererControl->surface() == 0);

    {   // Surface setup is deferred until after the first paint.
        QImage image(320, 240, QImage::Format_RGB32);
        QPainter painter(&image);

        item.paint(&painter, 0);
    }
    QVERIFY(object.testService->rendererControl->surface() != 0);

    object.unbind(&item);
    QCOMPARE(item.mediaObject(), nullObject);

    QCOMPARE(object.testService->rendererRef, 0);
    QVERIFY(object.testService->rendererControl->surface() == 0);

    item.setVisible(false);

    object.bind(&item);
    QCOMPARE(item.mediaObject(), static_cast<QMediaObject *>(&object));
    QCOMPARE(object.testService->rendererRef, 1);
    QVERIFY(object.testService->rendererControl->surface() != 0);
}

void tst_QGraphicsVideoItem::show()
{
    QtTestVideoObject object(new QtTestRendererControl);
    QtTestGraphicsVideoItem *item = new QtTestGraphicsVideoItem;
    object.bind(item);

    // Graphics items are visible by default
    QCOMPARE(object.testService->rendererRef, 1);
    QVERIFY(object.testService->rendererControl->surface() == 0);

    item->hide();
    QCOMPARE(object.testService->rendererRef, 1);

    item->show();
    QCOMPARE(object.testService->rendererRef, 1);
    QVERIFY(object.testService->rendererControl->surface() == 0);

    QGraphicsScene graphicsScene;
    graphicsScene.addItem(item);
    QGraphicsView graphicsView(&graphicsScene);
    graphicsView.show();

    QVERIFY(item->paintCount() || item->waitForPaint(1));
    QVERIFY(object.testService->rendererControl->surface() != 0);

    QVERIFY(item->boundingRect().isEmpty());

    QVideoSurfaceFormat format(QSize(320,240),QVideoFrame::Format_RGB32);
    QVERIFY(object.testService->rendererControl->surface()->start(format));

    QCoreApplication::processEvents();
    QVERIFY(!item->boundingRect().isEmpty());
}

void tst_QGraphicsVideoItem::aspectRatioMode()
{
    QGraphicsVideoItem item;

    QCOMPARE(item.aspectRatioMode(), Qt::KeepAspectRatio);

    item.setAspectRatioMode(Qt::IgnoreAspectRatio);
    QCOMPARE(item.aspectRatioMode(), Qt::IgnoreAspectRatio);

    item.setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
    QCOMPARE(item.aspectRatioMode(), Qt::KeepAspectRatioByExpanding);

    item.setAspectRatioMode(Qt::KeepAspectRatio);
    QCOMPARE(item.aspectRatioMode(), Qt::KeepAspectRatio);
}

void tst_QGraphicsVideoItem::offset()
{
    QGraphicsVideoItem item;

    QCOMPARE(item.offset(), QPointF(0, 0));

    item.setOffset(QPointF(-32.4, 43.0));
    QCOMPARE(item.offset(), QPointF(-32.4, 43.0));

    item.setOffset(QPointF(1, 1));
    QCOMPARE(item.offset(), QPointF(1, 1));

    item.setOffset(QPointF(12, -30.4));
    QCOMPARE(item.offset(), QPointF(12, -30.4));

    item.setOffset(QPointF(-90.4, -75));
    QCOMPARE(item.offset(), QPointF(-90.4, -75));
}

void tst_QGraphicsVideoItem::size()
{
    QGraphicsVideoItem item;

    QCOMPARE(item.size(), QSizeF(320, 240));

    item.setSize(QSizeF(542.5, 436.3));
    QCOMPARE(item.size(), QSizeF(542.5, 436.3));

    item.setSize(QSizeF(-43, 12));
    QCOMPARE(item.size(), QSizeF(0, 0));

    item.setSize(QSizeF(54, -9));
    QCOMPARE(item.size(), QSizeF(0, 0));

    item.setSize(QSizeF(-90, -65));
    QCOMPARE(item.size(), QSizeF(0, 0));

    item.setSize(QSizeF(1000, 1000));
    QCOMPARE(item.size(), QSizeF(1000, 1000));
}

void tst_QGraphicsVideoItem::nativeSize_data()
{
    QTest::addColumn<QSize>("frameSize");
    QTest::addColumn<QRect>("viewport");
    QTest::addColumn<QSize>("pixelAspectRatio");
    QTest::addColumn<QSizeF>("nativeSize");

    QTest::newRow("640x480")
            << QSize(640, 480)
            << QRect(0, 0, 640, 480)
            << QSize(1, 1)
            << QSizeF(640, 480);

    QTest::newRow("800x600, (80,60, 640x480) viewport")
            << QSize(800, 600)
            << QRect(80, 60, 640, 480)
            << QSize(1, 1)
            << QSizeF(640, 480);

    QTest::newRow("800x600, (80,60, 640x480) viewport, 4:3")
            << QSize(800, 600)
            << QRect(80, 60, 640, 480)
            << QSize(4, 3)
            << QSizeF(853, 480);
}

void tst_QGraphicsVideoItem::nativeSize()
{
    QFETCH(QSize, frameSize);
    QFETCH(QRect, viewport);
    QFETCH(QSize, pixelAspectRatio);
    QFETCH(QSizeF, nativeSize);

    QtTestVideoObject object(new QtTestRendererControl);
    QGraphicsVideoItem item;
    object.bind(&item);

    QCOMPARE(item.nativeSize(), QSizeF());

    QSignalSpy spy(&item, SIGNAL(nativeSizeChanged(QSizeF)));

    QVideoSurfaceFormat format(frameSize, QVideoFrame::Format_ARGB32);
    format.setViewport(viewport);
    format.setPixelAspectRatio(pixelAspectRatio);

    {   // Surface setup is deferred until after the first paint.
        QImage image(320, 240, QImage::Format_RGB32);
        QPainter painter(&image);

        item.paint(&painter, 0);
    }
    QVERIFY(object.testService->rendererControl->surface() != 0);
    QVERIFY(object.testService->rendererControl->surface()->start(format));

    QCoreApplication::processEvents();
    QCOMPARE(item.nativeSize(), nativeSize);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().first().toSizeF(), nativeSize);

    object.testService->rendererControl->surface()->stop();

    QCoreApplication::processEvents();
    QVERIFY(item.nativeSize().isEmpty());
    QCOMPARE(spy.count(), 2);
    QVERIFY(spy.last().first().toSizeF().isEmpty());
}

void tst_QGraphicsVideoItem::boundingRect_data()
{
    QTest::addColumn<QSize>("frameSize");
    QTest::addColumn<QPointF>("offset");
    QTest::addColumn<QSizeF>("size");
    QTest::addColumn<Qt::AspectRatioMode>("aspectRatioMode");
    QTest::addColumn<QRectF>("expectedRect");


    QTest::newRow("640x480: (0,0 640x480), Keep")
            << QSize(640, 480)
            << QPointF(0, 0)
            << QSizeF(640, 480)
            << Qt::KeepAspectRatio
            << QRectF(0, 0, 640, 480);

    QTest::newRow("800x600, (0,0, 640x480), Keep")
            << QSize(800, 600)
            << QPointF(0, 0)
            << QSizeF(640, 480)
            << Qt::KeepAspectRatio
            << QRectF(0, 0, 640, 480);

    QTest::newRow("800x600, (0,0, 640x480), KeepByExpanding")
            << QSize(800, 600)
            << QPointF(0, 0)
            << QSizeF(640, 480)
            << Qt::KeepAspectRatioByExpanding
            << QRectF(0, 0, 640, 480);

    QTest::newRow("800x600, (0,0, 640x480), Ignore")
            << QSize(800, 600)
            << QPointF(0, 0)
            << QSizeF(640, 480)
            << Qt::IgnoreAspectRatio
            << QRectF(0, 0, 640, 480);

    QTest::newRow("800x600, (100,100, 640x480), Keep")
            << QSize(800, 600)
            << QPointF(100, 100)
            << QSizeF(640, 480)
            << Qt::KeepAspectRatio
            << QRectF(100, 100, 640, 480);

    QTest::newRow("800x600, (100,-100, 640x480), KeepByExpanding")
            << QSize(800, 600)
            << QPointF(100, -100)
            << QSizeF(640, 480)
            << Qt::KeepAspectRatioByExpanding
            << QRectF(100, -100, 640, 480);

    QTest::newRow("800x600, (-100,-100, 640x480), Ignore")
            << QSize(800, 600)
            << QPointF(-100, -100)
            << QSizeF(640, 480)
            << Qt::IgnoreAspectRatio
            << QRectF(-100, -100, 640, 480);

    QTest::newRow("800x600, (0,0, 1920x1024), Keep")
            << QSize(800, 600)
            << QPointF(0, 0)
            << QSizeF(1920, 1024)
            << Qt::KeepAspectRatio
            << QRectF(832.0 / 3, 0, 4096.0 / 3, 1024);

    QTest::newRow("800x600, (0,0, 1920x1024), KeepByExpanding")
            << QSize(800, 600)
            << QPointF(0, 0)
            << QSizeF(1920, 1024)
            << Qt::KeepAspectRatioByExpanding
            << QRectF(0, 0, 1920, 1024);

    QTest::newRow("800x600, (0,0, 1920x1024), Ignore")
            << QSize(800, 600)
            << QPointF(0, 0)
            << QSizeF(1920, 1024)
            << Qt::IgnoreAspectRatio
            << QRectF(0, 0, 1920, 1024);

    QTest::newRow("800x600, (100,100, 1920x1024), Keep")
            << QSize(800, 600)
            << QPointF(100, 100)
            << QSizeF(1920, 1024)
            << Qt::KeepAspectRatio
            << QRectF(100 + 832.0 / 3, 100, 4096.0 / 3, 1024);

    QTest::newRow("800x600, (100,-100, 1920x1024), KeepByExpanding")
            << QSize(800, 600)
            << QPointF(100, -100)
            << QSizeF(1920, 1024)
            << Qt::KeepAspectRatioByExpanding
            << QRectF(100, -100, 1920, 1024);

    QTest::newRow("800x600, (-100,-100, 1920x1024), Ignore")
            << QSize(800, 600)
            << QPointF(-100, -100)
            << QSizeF(1920, 1024)
            << Qt::IgnoreAspectRatio
            << QRectF(-100, -100, 1920, 1024);
}

void tst_QGraphicsVideoItem::boundingRect()
{
    QFETCH(QSize, frameSize);
    QFETCH(QPointF, offset);
    QFETCH(QSizeF, size);
    QFETCH(Qt::AspectRatioMode, aspectRatioMode);
    QFETCH(QRectF, expectedRect);

    QtTestVideoObject object(new QtTestRendererControl);
    QGraphicsVideoItem item;
    object.bind(&item);

    item.setOffset(offset);
    item.setSize(size);
    item.setAspectRatioMode(aspectRatioMode);

    QVideoSurfaceFormat format(frameSize, QVideoFrame::Format_ARGB32);

    {   // Surface setup is deferred until after the first paint.
        QImage image(320, 240, QImage::Format_RGB32);
        QPainter painter(&image);

        item.paint(&painter, 0);
    }
    QVERIFY(object.testService->rendererControl->surface() != 0);
    QVERIFY(object.testService->rendererControl->surface()->start(format));

    QCoreApplication::processEvents();
    QCOMPARE(item.boundingRect(), expectedRect);
}

static const uchar rgb32ImageData[] =
{
    0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00,
    0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00
};

void tst_QGraphicsVideoItem::paint()
{
    QtTestVideoObject object(new QtTestRendererControl);
    QtTestGraphicsVideoItem *item = new QtTestGraphicsVideoItem;
    object.bind(item);

    QGraphicsScene graphicsScene;
    graphicsScene.addItem(item);
    QGraphicsView graphicsView(&graphicsScene);
    graphicsView.show();
    QVERIFY(item->waitForPaint(1));

    QPainterVideoSurface *surface = qobject_cast<QPainterVideoSurface *>(
            object.testService->rendererControl->surface());
    if (!surface)
        QSKIP("QGraphicsVideoItem is not QPainterVideoSurface based");

    QVideoSurfaceFormat format(QSize(2, 2), QVideoFrame::Format_RGB32);

    QVERIFY(surface->start(format));
    QCOMPARE(surface->isActive(), true);
    QCOMPARE(surface->isReady(), true);

    QVERIFY(item->waitForPaint(1));

    QCOMPARE(surface->isActive(), true);
    QCOMPARE(surface->isReady(), true);

    QVideoFrame frame(sizeof(rgb32ImageData), QSize(2, 2), 8, QVideoFrame::Format_RGB32);

    frame.map(QAbstractVideoBuffer::WriteOnly);
    memcpy(frame.bits(), rgb32ImageData, frame.mappedBytes());
    frame.unmap();

    QVERIFY(surface->present(frame));
    QCOMPARE(surface->isActive(), true);
    QCOMPARE(surface->isReady(), false);

    QVERIFY(item->waitForPaint(1));

    QCOMPARE(surface->isActive(), true);
    QCOMPARE(surface->isReady(), true);
}


QTEST_MAIN(tst_QGraphicsVideoItem)

#include "tst_qgraphicsvideoitem.moc"
