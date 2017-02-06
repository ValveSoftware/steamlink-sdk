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

#include "mockclient.h"
#include "testcompositor.h"
#include "testkeyboardgrabber.h"
#include "testseat.h"

#include "qwaylandview.h"
#include "qwaylandbufferref.h"
#include "qwaylandseat.h"

#include <QtGui/QScreen>
#include <QtWaylandCompositor/QWaylandXdgShellV5>
#include <QtWaylandCompositor/QWaylandIviApplication>
#include <QtWaylandCompositor/QWaylandIviSurface>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandResource>
#include <qwayland-xdg-shell.h>
#include <qwayland-ivi-application.h>

#include <QtTest/QtTest>

class tst_WaylandCompositor : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void seatCapabilities();
    void keyboardGrab();
    void seatCreation();
    void seatKeyboardFocus();
    void singleClient();
    void multipleClients();
    void geometry();
    void modes();
    void sizeFollowsWindow();
    void mapSurface();
    void frameCallback();

    void advertisesXdgShellSupport();
    void createsXdgSurfaces();
    void reportsXdgSurfaceWindowGeometry();
    void setsXdgAppId();
    void sendsXdgConfigure();

    void advertisesIviApplicationSupport();
    void createsIviSurfaces();
    void emitsErrorOnSameIviId();
    void sendsIviConfigure();
    void destroysIviSurfaces();
};

void tst_WaylandCompositor::init() {
    qputenv("XDG_RUNTIME_DIR", ".");
}

void tst_WaylandCompositor::singleClient()
{
    TestCompositor compositor;
    compositor.create();

    MockClient client;

    wl_surface *sa = client.createSurface();
    QTRY_COMPARE(compositor.surfaces.size(), 1);

    wl_surface *sb = client.createSurface();
    QTRY_COMPARE(compositor.surfaces.size(), 2);

    QWaylandClient *ca = compositor.surfaces.at(0)->client();
    QWaylandClient *cb = compositor.surfaces.at(1)->client();

    QCOMPARE(ca, cb);
    QVERIFY(ca != 0);

    QList<QWaylandSurface *> surfaces = compositor.surfacesForClient(ca);
    QCOMPARE(surfaces.size(), 2);
    QVERIFY((surfaces.at(0) == compositor.surfaces.at(0) && surfaces.at(1) == compositor.surfaces.at(1))
            || (surfaces.at(0) == compositor.surfaces.at(1) && surfaces.at(1) == compositor.surfaces.at(0)));

    wl_surface_destroy(sa);
    QTRY_COMPARE(compositor.surfaces.size(), 1);

    wl_surface_destroy(sb);
    QTRY_COMPARE(compositor.surfaces.size(), 0);
}

void tst_WaylandCompositor::multipleClients()
{
    TestCompositor compositor;
    compositor.create();

    MockClient a;
    MockClient b;
    MockClient c;

    wl_surface *sa = a.createSurface();
    wl_surface *sb = b.createSurface();
    wl_surface *sc = c.createSurface();

    QTRY_COMPARE(compositor.surfaces.size(), 3);

    QWaylandClient *ca = compositor.surfaces.at(0)->client();
    QWaylandClient *cb = compositor.surfaces.at(1)->client();
    QWaylandClient *cc = compositor.surfaces.at(2)->client();

    QVERIFY(ca != cb);
    QVERIFY(ca != cc);
    QVERIFY(cb != cc);
    QVERIFY(ca != 0);

    QCOMPARE(compositor.surfacesForClient(ca).size(), 1);
    QCOMPARE(compositor.surfacesForClient(ca).at(0), compositor.surfaces.at(0));

    QCOMPARE(compositor.surfacesForClient(cb).size(), 1);
    QCOMPARE(compositor.surfacesForClient(cb).at(0), compositor.surfaces.at(1));

    QCOMPARE(compositor.surfacesForClient(cc).size(), 1);
    QCOMPARE(compositor.surfacesForClient(cc).at(0), compositor.surfaces.at(2));

    wl_surface_destroy(sa);
    wl_surface_destroy(sb);
    wl_surface_destroy(sc);

    QTRY_COMPARE(compositor.surfaces.size(), 0);
}

void tst_WaylandCompositor::keyboardGrab()
{
    TestCompositor compositor;
    compositor.create();
    MockClient mc;

    mc.createSurface();
    // This is needed for timing purposes, otherwise the query for the
    // compositor surfaces will return null
    QTRY_COMPARE(compositor.surfaces.size(), 1);

    // Set the focused surface so that key event will flow through
    QWaylandSurface *waylandSurface = compositor.surfaces.at(0);
    QWaylandSeat* seat = compositor.defaultSeat();

    TestKeyboardGrabber* grab = static_cast<TestKeyboardGrabber *>(seat->keyboard());
    QTRY_COMPARE(grab, seat->keyboard());
    QSignalSpy grabFocusSpy(grab, SIGNAL(focusedCalled()));
    QSignalSpy grabKeyPressSpy(grab, SIGNAL(keyPressCalled()));
    QSignalSpy grabKeyReleaseSpy(grab, SIGNAL(keyReleaseCalled()));
    //QSignalSpy grabModifierSpy(grab, SIGNAL(modifiersCalled()));

    seat->setKeyboardFocus(waylandSurface);
    QTRY_COMPARE(grabFocusSpy.count(), 1);

    QKeyEvent ke(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, 30, 0, 0);
    QKeyEvent ke1(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, 30, 0, 0);
    seat->sendFullKeyEvent(&ke);
    seat->sendFullKeyEvent(&ke1);
    QTRY_COMPARE(grabKeyPressSpy.count(), 1);
    QTRY_COMPARE(grabKeyReleaseSpy.count(), 1);

    QKeyEvent ke2(QEvent::KeyPress, Qt::Key_Shift, Qt::NoModifier, 50, 0, 0);
    QKeyEvent ke3(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier, 50, 0, 0);
    seat->sendFullKeyEvent(&ke2);
    seat->sendFullKeyEvent(&ke3);
    //QTRY_COMPARE(grabModifierSpy.count(), 2);
    // Modifiers are also keys
    QTRY_COMPARE(grabKeyPressSpy.count(), 2);
    QTRY_COMPARE(grabKeyReleaseSpy.count(), 2);

    // Stop grabbing
    seat->setKeyboardFocus(Q_NULLPTR);
    seat->sendFullKeyEvent(&ke);
    seat->sendFullKeyEvent(&ke1);
    QTRY_COMPARE(grabKeyPressSpy.count(), 2);
}

void tst_WaylandCompositor::geometry()
{
    TestCompositor compositor;
    compositor.create();

    QWaylandOutputMode mode(QSize(4096, 3072), 60000);
    compositor.defaultOutput()->setPosition(QPoint(1024, 0));
    compositor.defaultOutput()->addMode(mode, true);
    compositor.defaultOutput()->setCurrentMode(mode);

    MockClient client;

    QTRY_COMPARE(client.geometry, QRect(QPoint(1024, 0), QSize(4096, 3072)));
    QTRY_COMPARE(client.resolution, QSize(4096, 3072));
    QTRY_COMPARE(client.refreshRate, 60000);
}

void tst_WaylandCompositor::modes()
{
    TestCompositor compositor;
    compositor.create();

    // mode3 is current, mode4 is preferred
    QWaylandOutputMode mode1(QSize(800, 600), 120000);
    QWaylandOutputMode mode2(QSize(1024, 768), 100000);
    QWaylandOutputMode mode3(QSize(1920, 1080), 60000);
    QWaylandOutputMode mode4(QSize(2560, 1440), 59000);
    compositor.defaultOutput()->addMode(mode1);
    compositor.defaultOutput()->addMode(mode2, true);
    compositor.defaultOutput()->addMode(mode3);
    compositor.defaultOutput()->addMode(mode4, true);
    compositor.defaultOutput()->setCurrentMode(mode3);

    MockClient client;

    QTRY_COMPARE(client.modes.size(), 4);
    QTRY_COMPARE(client.currentMode, mode3);
    QTRY_COMPARE(client.preferredMode, mode4);
    QTRY_COMPARE(client.geometry, QRect(QPoint(0, 0), QSize(1920, 1080)));
}

void tst_WaylandCompositor::sizeFollowsWindow()
{
    TestCompositor compositor;

    QWindow window;
    window.resize(800, 600);

    auto output = new QWaylandOutput(&compositor, &window);
    output->setSizeFollowsWindow(true);

    compositor.create();

    QWaylandOutputMode mode(window.size(), qFloor(window.screen()->refreshRate() * 1000));

    MockClient client;

    QTRY_COMPARE(client.modes.size(), 1);
    QTRY_COMPARE(client.currentMode, mode);
    QTRY_COMPARE(client.preferredMode, mode);
}

void tst_WaylandCompositor::mapSurface()
{
    TestCompositor compositor;
    compositor.create();

    MockClient client;

    wl_surface *surface = client.createSurface();
    QTRY_COMPARE(compositor.surfaces.size(), 1);

    QWaylandSurface *waylandSurface = compositor.surfaces.at(0);

    QSignalSpy hasContentSpy(waylandSurface, SIGNAL(hasContentChanged()));

    QCOMPARE(waylandSurface->size(), QSize());
    QCOMPARE(waylandSurface->hasContent(), false);

    QSize size(256, 256);
    ShmBuffer buffer(size, client.shm);

    // we need to create a shell surface here or the surface won't be mapped
    client.createShellSurface(surface);
    wl_surface_attach(surface, buffer.handle, 0, 0);
    wl_surface_damage(surface, 0, 0, size.width(), size.height());
    wl_surface_commit(surface);

    QTRY_COMPARE(waylandSurface->size(), size);
    QTRY_COMPARE(waylandSurface->hasContent(), true);
    QTRY_COMPARE(hasContentSpy.count(), 1);

    wl_surface_destroy(surface);
}

static void frameCallbackFunc(void *data, wl_callback *callback, uint32_t)
{
    ++*static_cast<int *>(data);
    wl_callback_destroy(callback);
}

static void registerFrameCallback(wl_surface *surface, int *counter)
{
    static const wl_callback_listener frameCallbackListener = {
        frameCallbackFunc
    };

    wl_callback_add_listener(wl_surface_frame(surface), &frameCallbackListener, counter);
}

void tst_WaylandCompositor::frameCallback()
{
    class BufferView : public QWaylandView
    {
    public:
        void bufferCommitted(const QWaylandBufferRef &ref, const QRegion &damage) Q_DECL_OVERRIDE
        {
            Q_UNUSED(damage);
            bufferRef = ref;
        }

        QImage image() const
        {
            if (bufferRef.isNull() || !bufferRef.isSharedMemory())
                return QImage();
            return bufferRef.image();
        }

        QWaylandBufferRef bufferRef;
    };

    TestCompositor compositor;
    compositor.create();

    MockClient client;

    wl_surface *surface = client.createSurface();

    int frameCounter = 0;

    QTRY_COMPARE(compositor.surfaces.size(), 1);
    QWaylandSurface *waylandSurface = compositor.surfaces.at(0);
    BufferView* view = new BufferView;
    view->setSurface(waylandSurface);
    view->setOutput(compositor.defaultOutput());

    QSignalSpy damagedSpy(waylandSurface, SIGNAL(damaged(const QRegion &)));

    for (int i = 0; i < 10; ++i) {
        QSize size(i * 8 + 2, i * 8 + 2);
        ShmBuffer buffer(size, client.shm);

        // attach a new buffer every frame, else the damage signal won't be fired
        wl_surface_attach(surface, buffer.handle, 0, 0);
        registerFrameCallback(surface, &frameCounter);
        wl_surface_damage(surface, 0, 0, size.width(), size.height());
        wl_surface_commit(surface);

        QTRY_COMPARE(waylandSurface->hasContent(), true);
        QTRY_COMPARE(damagedSpy.count(), i + 1);

        QCOMPARE(static_cast<BufferView*>(waylandSurface->views().first())->image(), buffer.image);
        compositor.defaultOutput()->frameStarted();
        compositor.defaultOutput()->sendFrameCallbacks();

        QTRY_COMPARE(frameCounter, i + 1);
    }

    wl_surface_destroy(surface);
}

void tst_WaylandCompositor::seatCapabilities()
{
    TestCompositor compositor;
    compositor.create();

    MockClient client;
    Q_UNUSED(client);

    QWaylandSeat dev(&compositor, QWaylandSeat::Pointer);

    QTRY_VERIFY(dev.pointer());
    QTRY_VERIFY(!dev.keyboard());
    QTRY_VERIFY(!dev.touch());

    QWaylandSeat dev2(&compositor, QWaylandSeat::Keyboard | QWaylandSeat::Touch);
    QTRY_VERIFY(!dev2.pointer());
    QTRY_VERIFY(dev2.keyboard());
    QTRY_VERIFY(dev2.touch());
}

void tst_WaylandCompositor::seatCreation()
{
    TestCompositor compositor(true);
    compositor.create();

    MockClient client;
    Q_UNUSED(client);

    TestSeat* seat = qobject_cast<TestSeat *>(compositor.defaultSeat());
    QTRY_VERIFY(seat);

    // The compositor will create the default input device
    QTRY_VERIFY(seat->isInitialized());

    QList<QMouseEvent *> allEvents;
    allEvents += seat->createMouseEvents(5);
    foreach (QMouseEvent *me, allEvents) {
        compositor.seatFor(me);
    }

    // The default input device will get called exatly the number of times it has created
    // the events
    QTRY_COMPARE(seat->queryCount(), 5);
}

void tst_WaylandCompositor::seatKeyboardFocus()
{
    TestCompositor compositor(true);
    compositor.create();

    // Create client after all the input devices have been set up as the mock client
    // does not dynamically listen to new seats
    MockClient client;
    wl_surface *surface = client.createSurface();
    QTRY_COMPARE(compositor.surfaces.size(), 1);

    QWaylandSurface *waylandSurface = compositor.surfaces.at(0);
    QWaylandSeat* dev = compositor.defaultSeat();
    dev->setKeyboardFocus(waylandSurface);
    QTRY_COMPARE(compositor.defaultSeat()->keyboardFocus(), waylandSurface);

    wl_surface_destroy(surface);
    QTRY_VERIFY(compositor.surfaces.size() == 0);

    QTRY_VERIFY(!compositor.defaultSeat()->keyboardFocus());
}

class XdgTestCompositor: public TestCompositor {
    Q_OBJECT
public:
    XdgTestCompositor() : xdgShell(this) {}
    QWaylandXdgShellV5 xdgShell;
};

void tst_WaylandCompositor::advertisesXdgShellSupport()
{
    XdgTestCompositor compositor;
    compositor.create();

    MockClient client;
    QTRY_VERIFY(&client.xdgShell);
}

void tst_WaylandCompositor::createsXdgSurfaces()
{
    XdgTestCompositor compositor;
    compositor.create();

    MockClient client;
    QTRY_VERIFY(&client.xdgShell);

    QSignalSpy xdgSurfaceCreatedSpy(&compositor.xdgShell, &QWaylandXdgShellV5::xdgSurfaceCreated);
    QWaylandXdgSurfaceV5 *xdgSurface = nullptr;
    QObject::connect(&compositor.xdgShell, &QWaylandXdgShellV5::xdgSurfaceCreated, [&](QWaylandXdgSurfaceV5 *s) {
        xdgSurface = s;
    });

    wl_surface *surface = client.createSurface();
    client.createXdgSurface(surface);
    QTRY_COMPARE(xdgSurfaceCreatedSpy.count(), 1);
    QTRY_VERIFY(xdgSurface);
    QTRY_VERIFY(xdgSurface->surface());
}

void tst_WaylandCompositor::reportsXdgSurfaceWindowGeometry()
{
    XdgTestCompositor compositor;
    compositor.create();

    QWaylandXdgSurfaceV5 *xdgSurface = nullptr;
    QObject::connect(&compositor.xdgShell, &QWaylandXdgShellV5::xdgSurfaceCreated, [&](QWaylandXdgSurfaceV5 *s) {
        xdgSurface = s;
    });

    MockClient client;
    wl_surface *surface = client.createSurface();
    xdg_surface *clientXdgSurface = client.createXdgSurface(surface);
    QSize size(256, 256);
    ShmBuffer buffer(size, client.shm);
    wl_surface_attach(surface, buffer.handle, 0, 0);
    wl_surface_damage(surface, 0, 0, size.width(), size.height());
    wl_surface_commit(surface);

    QTRY_VERIFY(xdgSurface);
    QTRY_COMPARE(xdgSurface->windowGeometry(), QRect(QPoint(0, 0), QSize(256, 256)));

    xdg_surface_set_window_geometry(clientXdgSurface, 10, 20, 100, 50);
    wl_surface_commit(surface);

    QTRY_COMPARE(xdgSurface->windowGeometry(), QRect(QPoint(10, 20), QSize(100, 50)));
}

void tst_WaylandCompositor::setsXdgAppId()
{
    XdgTestCompositor compositor;
    compositor.create();

    QWaylandXdgSurfaceV5 *xdgSurface = nullptr;
    QObject::connect(&compositor.xdgShell, &QWaylandXdgShellV5::xdgSurfaceCreated, [&](QWaylandXdgSurfaceV5 *s) {
        xdgSurface = s;
    });

    MockClient client;
    wl_surface *surface = client.createSurface();
    xdg_surface *clientXdgSurface = client.createXdgSurface(surface);

    xdg_surface_set_app_id(clientXdgSurface, "org.foo.bar");

    QTRY_VERIFY(xdgSurface);
    QTRY_COMPARE(xdgSurface->appId(), QString("org.foo.bar"));
}

void tst_WaylandCompositor::sendsXdgConfigure()
{
    class MockXdgSurface : public QtWayland::xdg_surface
    {
    public:
        MockXdgSurface(::xdg_surface *xdgSurface) : QtWayland::xdg_surface(xdgSurface) {}
        void xdg_surface_configure(int32_t width, int32_t height, wl_array *rawStates, uint32_t serial) Q_DECL_OVERRIDE
        {
            configureSize = QSize(width, height);
            configureSerial = serial;

            uint *states = reinterpret_cast<uint*>(rawStates->data);
            configureStates.clear();
            size_t numStates = rawStates->size / sizeof(uint);
            for (size_t i = 0; i < numStates; ++i) {
                configureStates.push_back(states[i]);
            }
        }

        QList<uint> configureStates;
        QSize configureSize;
        uint configureSerial;
    };

    XdgTestCompositor compositor;
    compositor.create();

    QWaylandXdgSurfaceV5 *xdgSurface = nullptr;
    QObject::connect(&compositor.xdgShell, &QWaylandXdgShellV5::xdgSurfaceCreated, [&](QWaylandXdgSurfaceV5 *s) {
        xdgSurface = s;
    });

    MockClient client;
    wl_surface *surface = client.createSurface();
    xdg_surface *clientXdgSurface = client.createXdgSurface(surface);
    MockXdgSurface mockXdgSurface(clientXdgSurface);

    QTRY_VERIFY(xdgSurface);
    QTRY_VERIFY(!xdgSurface->activated());
    QTRY_VERIFY(!xdgSurface->maximized());
    QTRY_VERIFY(!xdgSurface->fullscreen());
    QTRY_VERIFY(!xdgSurface->resizing());

    xdgSurface->sendConfigure(QSize(10, 20), QVector<QWaylandXdgSurfaceV5::State>{QWaylandXdgSurfaceV5::State::ActivatedState});
    compositor.flushClients();
    QTRY_COMPARE(mockXdgSurface.configureStates, QList<uint>{QWaylandXdgSurfaceV5::State::ActivatedState});
    QTRY_COMPARE(mockXdgSurface.configureSize, QSize(10, 20));

    xdgSurface->sendMaximized(QSize(800, 600));
    compositor.flushClients();
    QTRY_VERIFY(mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::MaximizedState));
    QTRY_VERIFY(mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::ActivatedState));
    QTRY_COMPARE(mockXdgSurface.configureSize, QSize(800, 600));

    // There hasn't been any ack_configures, so state should still be unchanged
    QTRY_VERIFY(!xdgSurface->activated());
    QTRY_VERIFY(!xdgSurface->maximized());
    xdg_surface_ack_configure(clientXdgSurface, mockXdgSurface.configureSerial);
    wl_display_dispatch_pending(client.display);
    wl_display_flush(client.display);
    QTRY_VERIFY(xdgSurface->activated());
    QTRY_VERIFY(xdgSurface->maximized());

    xdgSurface->sendUnmaximized();
    compositor.flushClients();
    QTRY_VERIFY(!mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::MaximizedState));
    QTRY_VERIFY(mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::ActivatedState));
    QTRY_COMPARE(mockXdgSurface.configureSize, QSize(0, 0));

    // The unmaximized configure hasn't been acked, so maximized should still be true
    QTRY_VERIFY(xdgSurface->maximized());
    QTRY_VERIFY(xdgSurface->activated());

    xdgSurface->sendResizing(QSize(800, 600));
    compositor.flushClients();
    QTRY_VERIFY(mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::ResizingState));
    QTRY_COMPARE(mockXdgSurface.configureSize, QSize(800, 600));

    xdgSurface->sendFullscreen(QSize(1024, 768));
    compositor.flushClients();
    QTRY_VERIFY(mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::ActivatedState));
    QTRY_VERIFY(mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::FullscreenState));
    QTRY_COMPARE(mockXdgSurface.configureSize, QSize(1024, 768));
    uint fullscreenSerial = mockXdgSurface.configureSerial;

    xdgSurface->sendUnmaximized();
    compositor.flushClients();
    QTRY_VERIFY(mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::ActivatedState));
    QTRY_VERIFY(!mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::FullscreenState));

    xdgSurface->sendConfigure(QSize(0, 0), QVector<QWaylandXdgSurfaceV5::State>{});
    compositor.flushClients();
    QTRY_VERIFY(!mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::ActivatedState));

    xdgSurface->sendMaximized(QSize(800, 600));
    compositor.flushClients();
    QTRY_VERIFY(!mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::ActivatedState));

    xdgSurface->sendFullscreen(QSize(800, 600));
    compositor.flushClients();
    QTRY_VERIFY(!mockXdgSurface.configureStates.contains(QWaylandXdgSurfaceV5::State::MaximizedState));

    // Verify that acking a configure that's not the most recently sent works
    xdg_surface_ack_configure(clientXdgSurface, fullscreenSerial);
    wl_display_dispatch_pending(client.display);
    wl_display_flush(client.display);
    QTRY_VERIFY(xdgSurface->fullscreen());
    QTRY_VERIFY(xdgSurface->activated());
    QTRY_VERIFY(!xdgSurface->maximized());
    QTRY_VERIFY(!xdgSurface->resizing());
}

class IviTestCompositor: public TestCompositor {
    Q_OBJECT
public:
    IviTestCompositor() : iviApplication(this) {}
    QWaylandIviApplication iviApplication;
};

void tst_WaylandCompositor::advertisesIviApplicationSupport()
{
    IviTestCompositor compositor;
    compositor.create();

    MockClient client;
    QTRY_VERIFY(&client.iviApplication);
}

void tst_WaylandCompositor::createsIviSurfaces()
{
    IviTestCompositor compositor;
    compositor.create();

    MockClient client;
    QTRY_VERIFY(&client.iviApplication);

    QSignalSpy iviSurfaceCreatedSpy(&compositor.iviApplication, &QWaylandIviApplication::iviSurfaceRequested);
    QWaylandIviSurface *iviSurface = nullptr;
    QObject::connect(&compositor.iviApplication, &QWaylandIviApplication::iviSurfaceCreated, [&](QWaylandIviSurface *s) {
        iviSurface = s;
    });

    wl_surface *surface = client.createSurface();
    client.createIviSurface(surface, 123);
    QTRY_COMPARE(iviSurfaceCreatedSpy.count(), 1);
    QTRY_VERIFY(iviSurface);
    QTRY_VERIFY(iviSurface->surface());
    QTRY_COMPARE(iviSurface->iviId(), 123u);
}

void tst_WaylandCompositor::emitsErrorOnSameIviId()
{
    IviTestCompositor compositor;
    compositor.create();

    {
        MockClient firstClient;
        QTRY_VERIFY(&firstClient.iviApplication);

        QWaylandIviSurface *firstIviSurface = nullptr;
        QObject::connect(&compositor.iviApplication, &QWaylandIviApplication::iviSurfaceCreated, [&](QWaylandIviSurface *s) {
            firstIviSurface = s;
        });

        firstClient.createIviSurface(firstClient.createSurface(), 123);
        QTRY_VERIFY(firstIviSurface);
        QTRY_COMPARE(firstIviSurface->iviId(), 123u);

        {
            MockClient secondClient;
            QTRY_VERIFY(&secondClient.iviApplication);
            QTRY_COMPARE(compositor.clients().count(), 2);

            secondClient.createIviSurface(secondClient.createSurface(), 123);
            compositor.flushClients();

            QTRY_COMPARE(secondClient.error, EPROTO);
            QTRY_COMPARE(secondClient.protocolError.interface, &ivi_application_interface);
            QTRY_COMPARE(static_cast<ivi_application_error>(secondClient.protocolError.code), IVI_APPLICATION_ERROR_IVI_ID);
            QTRY_COMPARE(compositor.clients().count(), 1);
        }
    }

    // The other clients have passed out of scope and have been destroyed,
    // it should now be ok to create new application with the same id
    MockClient thirdClient;
    QTRY_VERIFY(&thirdClient.iviApplication);

    QWaylandIviSurface *thirdIviSurface = nullptr;
    QObject::connect(&compositor.iviApplication, &QWaylandIviApplication::iviSurfaceCreated, [&](QWaylandIviSurface *s) {
        thirdIviSurface = s;
    });
    thirdClient.createIviSurface(thirdClient.createSurface(), 123);
    compositor.flushClients();

    QTRY_VERIFY(thirdIviSurface);
    QTRY_COMPARE(thirdIviSurface->iviId(), 123u);
    QTRY_COMPARE(thirdClient.error, 0);
}

void tst_WaylandCompositor::sendsIviConfigure()
{
    class MockIviSurface : public QtWayland::ivi_surface
    {
    public:
        MockIviSurface(::ivi_surface *iviSurface) : QtWayland::ivi_surface(iviSurface) {}
        void ivi_surface_configure(int32_t width, int32_t height) Q_DECL_OVERRIDE
        {
            configureSize = QSize(width, height);
        }
        QSize configureSize;
    };

    IviTestCompositor compositor;
    compositor.create();

    MockClient client;
    QTRY_VERIFY(&client.iviApplication);

    QWaylandIviSurface *iviSurface = nullptr;
    QObject::connect(&compositor.iviApplication, &QWaylandIviApplication::iviSurfaceCreated, [&](QWaylandIviSurface *s) {
        iviSurface = s;
    });

    wl_surface *surface = client.createSurface();
    ivi_surface *clientIviSurface = client.createIviSurface(surface, 123);
    MockIviSurface mockIviSurface(clientIviSurface);

    QTRY_VERIFY(iviSurface);
    iviSurface->sendConfigure(QSize(800, 600));
    compositor.flushClients();

    QTRY_COMPARE(mockIviSurface.configureSize, QSize(800, 600));
}

void tst_WaylandCompositor::destroysIviSurfaces()
{
    IviTestCompositor compositor;
    compositor.create();

    MockClient client;
    QTRY_VERIFY(&client.iviApplication);

    QWaylandIviSurface *iviSurface = nullptr;
    QObject::connect(&compositor.iviApplication, &QWaylandIviApplication::iviSurfaceCreated, [&](QWaylandIviSurface *s) {
        iviSurface = s;
    });

    QtWayland::ivi_surface mockIviSurface(client.createIviSurface(client.createSurface(), 123));
    QTRY_VERIFY(iviSurface);

    QSignalSpy destroySpy(iviSurface, SIGNAL(destroyed()));
    mockIviSurface.destroy();
    QTRY_VERIFY(destroySpy.count() == 1);
}

#include <tst_compositor.moc>
QTEST_MAIN(tst_WaylandCompositor);
