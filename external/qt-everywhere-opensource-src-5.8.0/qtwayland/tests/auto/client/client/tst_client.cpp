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

#include "mockcompositor.h"

#include <QBackingStore>
#include <QPainter>
#include <QScreen>
#include <QWindow>
#include <QMimeData>
#include <QPixmap>
#include <QDrag>

#include <QtTest/QtTest>

static const QSize screenSize(1600, 1200);

class TestWindow : public QWindow
{
public:
    TestWindow()
        : focusInEventCount(0)
        , focusOutEventCount(0)
        , keyPressEventCount(0)
        , keyReleaseEventCount(0)
        , mousePressEventCount(0)
        , mouseReleaseEventCount(0)
        , touchEventCount(0)
        , keyCode(0)
    {
        setSurfaceType(QSurface::RasterSurface);
        setGeometry(0, 0, 32, 32);
        create();
    }

    void focusInEvent(QFocusEvent *)
    {
        ++focusInEventCount;
    }

    void focusOutEvent(QFocusEvent *)
    {
        ++focusOutEventCount;
    }

    void keyPressEvent(QKeyEvent *event)
    {
        ++keyPressEventCount;
        keyCode = event->nativeScanCode();
    }

    void keyReleaseEvent(QKeyEvent *event)
    {
        ++keyReleaseEventCount;
        keyCode = event->nativeScanCode();
    }

    void mousePressEvent(QMouseEvent *event)
    {
        ++mousePressEventCount;
        mousePressPos = event->pos();
    }

    void mouseReleaseEvent(QMouseEvent *)
    {
        ++mouseReleaseEventCount;
    }

    void touchEvent(QTouchEvent *event) Q_DECL_OVERRIDE
    {
        ++touchEventCount;
    }

    int focusInEventCount;
    int focusOutEventCount;
    int keyPressEventCount;
    int keyReleaseEventCount;
    int mousePressEventCount;
    int mouseReleaseEventCount;
    int touchEventCount;

    uint keyCode;
    QPoint mousePressPos;
};

class tst_WaylandClient : public QObject
{
    Q_OBJECT
public:
    tst_WaylandClient(MockCompositor *c)
        : compositor(c)
    {
        QSocketNotifier *notifier = new QSocketNotifier(compositor->waylandFileDescriptor(), QSocketNotifier::Read, this);
        connect(notifier, SIGNAL(activated(int)), this, SLOT(processWaylandEvents()));
        // connect to the event dispatcher to make sure to flush out the outgoing message queue
        connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::awake, this, &tst_WaylandClient::processWaylandEvents);
        connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, &tst_WaylandClient::processWaylandEvents);
    }

public slots:
    void processWaylandEvents()
    {
        compositor->processWaylandEvents();
    }

    void cleanup()
    {
        // make sure the surfaces from the last test are properly cleaned up
        // and don't show up as false positives in the next test
        QTRY_VERIFY(!compositor->surface());
    }

private slots:
    void screen();
    void createDestroyWindow();
    void events();
    void backingStore();
    void touchDrag();
    void mouseDrag();

private:
    MockCompositor *compositor;
};

void tst_WaylandClient::screen()
{
    QTRY_COMPARE(QGuiApplication::primaryScreen()->size(), screenSize);
}

void tst_WaylandClient::createDestroyWindow()
{
    TestWindow window;
    window.show();

    QTRY_VERIFY(compositor->surface());

    window.destroy();
    QTRY_VERIFY(!compositor->surface());
}

void tst_WaylandClient::events()
{
    TestWindow window;
    window.show();

    QSharedPointer<MockSurface> surface;
    QTRY_VERIFY(surface = compositor->surface());

    QCOMPARE(window.focusInEventCount, 0);
    compositor->setKeyboardFocus(surface);
    QTRY_COMPARE(window.focusInEventCount, 1);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    QCOMPARE(window.focusOutEventCount, 0);
    compositor->setKeyboardFocus(QSharedPointer<MockSurface>(0));
    QTRY_COMPARE(window.focusOutEventCount, 1);
    QTRY_COMPARE(QGuiApplication::focusWindow(), static_cast<QWindow *>(0));

    compositor->setKeyboardFocus(surface);
    QTRY_COMPARE(window.focusInEventCount, 2);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    uint keyCode = 80; // arbitrarily chosen
    QCOMPARE(window.keyPressEventCount, 0);
    compositor->sendKeyPress(surface, keyCode);
    QTRY_COMPARE(window.keyPressEventCount, 1);
    QTRY_COMPARE(window.keyCode, keyCode);

    QCOMPARE(window.keyReleaseEventCount, 0);
    compositor->sendKeyRelease(surface, keyCode);
    QTRY_COMPARE(window.keyReleaseEventCount, 1);
    QCOMPARE(window.keyCode, keyCode);

    QPoint mousePressPos(16, 16);
    QCOMPARE(window.mousePressEventCount, 0);
    compositor->sendMousePress(surface, mousePressPos);
    QTRY_COMPARE(window.mousePressEventCount, 1);
    QTRY_COMPARE(window.mousePressPos, mousePressPos);

    QCOMPARE(window.mouseReleaseEventCount, 0);
    compositor->sendMouseRelease(surface);
    QTRY_COMPARE(window.mouseReleaseEventCount, 1);

    const int touchId = 0;
    compositor->sendTouchDown(surface, QPoint(10, 10), touchId);
    compositor->sendTouchFrame(surface);
    QTRY_COMPARE(window.touchEventCount, 1);

    compositor->sendTouchUp(surface, touchId);
    compositor->sendTouchFrame(surface);
    QTRY_COMPARE(window.touchEventCount, 2);
}

void tst_WaylandClient::backingStore()
{
    TestWindow window;
    window.show();

    QSharedPointer<MockSurface> surface;
    QTRY_VERIFY(surface = compositor->surface());

    QRect rect(QPoint(), window.size());

    QBackingStore backingStore(&window);
    backingStore.resize(rect.size());

    backingStore.beginPaint(rect);

    QColor color = Qt::magenta;

    QPainter p(backingStore.paintDevice());
    p.fillRect(rect, color);
    p.end();

    backingStore.endPaint();

    QVERIFY(surface->image.isNull());

    backingStore.flush(rect);

    QTRY_COMPARE(surface->image.size(), window.frameGeometry().size());
    QTRY_COMPARE(surface->image.pixel(window.frameMargins().left(), window.frameMargins().top()), color.rgba());

    window.hide();

    // hiding the window should destroy the surface
    QTRY_VERIFY(!compositor->surface());
}

class DndWindow : public QWindow
{
    Q_OBJECT

public:
    DndWindow(QWindow *parent = 0)
        : QWindow(parent)
        , dragStarted(false)
    {
        QImage cursorImage(64,64,QImage::Format_ARGB32);
        cursorImage.fill(Qt::blue);
        m_dragIcon = QPixmap::fromImage(cursorImage);
    }
    ~DndWindow(){}
    bool dragStarted;

protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE
    {
        if (dragStarted)
            return;
        dragStarted = true;

        QByteArray dataBytes;
        QMimeData *mimeData = new QMimeData;
        mimeData->setData("application/x-dnditemdata", dataBytes);
        QDrag *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->setPixmap(m_dragIcon);
        drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
    }
private:
    QPixmap m_dragIcon;
};

void tst_WaylandClient::touchDrag()
{
    DndWindow window;
    window.show();

    QSharedPointer<MockSurface> surface;
    QTRY_VERIFY(surface = compositor->surface());

    compositor->setKeyboardFocus(surface);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    const int id = 0;
    compositor->sendTouchDown(surface, QPoint(10, 10), id);
    compositor->sendTouchMotion(surface, QPoint(20, 20), id);
    compositor->sendTouchFrame(surface);
    compositor->waitForStartDrag();
    compositor->sendDataDeviceDataOffer(surface);
    compositor->sendDataDeviceEnter(surface, QPoint(20, 20));
    compositor->sendDataDeviceMotion( QPoint(21, 21));
    compositor->sendDataDeviceDrop(surface);
    compositor->sendDataDeviceLeave(surface);
    QTRY_VERIFY(window.dragStarted);
}

void tst_WaylandClient::mouseDrag()
{
    DndWindow window;
    window.show();

    QSharedPointer<MockSurface> surface;
    QTRY_VERIFY(surface = compositor->surface());

    compositor->setKeyboardFocus(surface);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    compositor->sendMousePress(surface, QPoint(10, 10));
    compositor->sendDataDeviceDataOffer(surface);
    compositor->sendDataDeviceEnter(surface, QPoint(20, 20));
    compositor->sendDataDeviceMotion( QPoint(21, 21));
    compositor->waitForStartDrag();
    compositor->sendDataDeviceDrop(surface);
    compositor->sendDataDeviceLeave(surface);
    QTRY_VERIFY(window.dragStarted);
}

int main(int argc, char **argv)
{
    setenv("XDG_RUNTIME_DIR", ".", 1);
    setenv("QT_QPA_PLATFORM", "wayland", 1); // force QGuiApplication to use wayland plugin

    // wayland-egl hangs in the test setup when we try to initialize. Until it gets
    // figured out, avoid clientBufferIntegration() from being called in
    // QWaylandWindow::createDecorations().
    setenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1", 1);

    MockCompositor compositor;
    compositor.setOutputGeometry(QRect(QPoint(), screenSize));

    QGuiApplication app(argc, argv);
    compositor.applicationInitialized();

    tst_WaylandClient tc(&compositor);
    return QTest::qExec(&tc, argc, argv);
}

#include <tst_client.moc>
