/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mockclient.h"
#include "testcompositor.h"
#include "testkeyboardgrabber.h"

#include "QtCompositor/private/qwlkeyboard_p.h"
#include "QtCompositor/private/qwlinputdevice_p.h"

#include "qwaylandbufferref.h"

#include <QtTest/QtTest>

#include <QtCompositor/private/qwlinputdevice_p.h>

class tst_WaylandCompositor : public QObject
{
    Q_OBJECT

public:
    tst_WaylandCompositor() {
        setenv("XDG_RUNTIME_DIR", ".", 1);
    }

private slots:
    void inputDeviceCapabilities();
    void keyboardGrab();
    void singleClient();
    void multipleClients();
    void geometry();
    void mapSurface();
    void frameCallback();
};

void tst_WaylandCompositor::singleClient()
{
    TestCompositor compositor;

    MockClient client;

    wl_surface *sa = client.createSurface();
    QTRY_COMPARE(compositor.surfaces.size(), 1);

    wl_surface *sb = client.createSurface();
    QTRY_COMPARE(compositor.surfaces.size(), 2);

    WaylandClient *ca = compositor.surfaces.at(0)->client();
    WaylandClient *cb = compositor.surfaces.at(1)->client();

    QCOMPARE(ca, cb);

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

    MockClient a;
    MockClient b;
    MockClient c;

    wl_surface *sa = a.createSurface();
    wl_surface *sb = b.createSurface();
    wl_surface *sc = c.createSurface();

    QTRY_COMPARE(compositor.surfaces.size(), 3);

    WaylandClient *ca = compositor.surfaces.at(0)->client();
    WaylandClient *cb = compositor.surfaces.at(1)->client();
    WaylandClient *cc = compositor.surfaces.at(2)->client();

    QVERIFY(ca != cb);
    QVERIFY(ca != cc);
    QVERIFY(cb != cc);

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
    TestCompositor compositor((QWaylandCompositor::ExtensionFlag)0);
    MockClient mc;

    mc.createSurface();
    // This is needed for timing purposes, otherwise the query for the
    // compositor surfaces will return null
    QTRY_COMPARE(compositor.surfaces.size(), 1);

    // Set the focused surface so that key event will flow through
    QWaylandSurface *waylandSurface = compositor.surfaces.at(0);
    QWaylandInputDevice* inputDevice = compositor.defaultInputDevice();
    inputDevice->handle()->keyboardDevice()->setFocus(waylandSurface->handle());

    TestKeyboardGrabber grab;
    QSignalSpy grabFocusSpy(&grab, SIGNAL(focusedCalled()));
    QSignalSpy grabKeySpy(&grab, SIGNAL(keyCalled()));
    QSignalSpy grabModifierSpy(&grab, SIGNAL(modifiersCalled()));

    QtWayland::Keyboard *keyboard = inputDevice->handle()->keyboardDevice();
    keyboard->startGrab(&grab);

    QTRY_COMPARE(grabFocusSpy.count(), 1);
    QCOMPARE(grab.m_keyboard, inputDevice->handle()->keyboardDevice());

    QKeyEvent ke(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, 30, 0, 0);
    QKeyEvent ke1(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, 30, 0, 0);
    inputDevice->sendFullKeyEvent(&ke);
    inputDevice->sendFullKeyEvent(&ke1);
    QTRY_COMPARE(grabKeySpy.count(), 2);

    QKeyEvent ke2(QEvent::KeyPress, Qt::Key_Shift, Qt::NoModifier, 50, 0, 0);
    QKeyEvent ke3(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier, 50, 0, 0);
    inputDevice->sendFullKeyEvent(&ke2);
    inputDevice->sendFullKeyEvent(&ke3);
    QTRY_COMPARE(grabModifierSpy.count(), 2);
    // Modifiers are also keys
    QTRY_COMPARE(grabKeySpy.count(), 4);

    // Stop grabbing
    keyboard->endGrab();
    inputDevice->sendFullKeyEvent(&ke);
    inputDevice->sendFullKeyEvent(&ke1);
    QTRY_COMPARE(grabKeySpy.count(), 4);
}

void tst_WaylandCompositor::geometry()
{
    TestCompositor compositor;

    QRect geometry(0, 0, 4096, 3072);
    compositor.setOutputGeometry(geometry);

    MockClient client;

    QTRY_COMPARE(client.geometry, geometry);
}

void tst_WaylandCompositor::mapSurface()
{
    TestCompositor compositor;

    MockClient client;

    wl_surface *surface = client.createSurface();
    QTRY_COMPARE(compositor.surfaces.size(), 1);

    QWaylandSurface *waylandSurface = compositor.surfaces.at(0);

    QSignalSpy mappedSpy(waylandSurface, SIGNAL(mapped()));

    QCOMPARE(waylandSurface->size(), QSize());
    QCOMPARE(waylandSurface->type(), QWaylandSurface::Invalid);

    QSize size(256, 256);
    ShmBuffer buffer(size, client.shm);

    // we need to create a shell surface here or the surface won't be mapped
    client.createShellSurface(surface);
    wl_surface_attach(surface, buffer.handle, 0, 0);
    wl_surface_damage(surface, 0, 0, size.width(), size.height());
    wl_surface_commit(surface);

    QTRY_COMPARE(waylandSurface->size(), size);
    QTRY_COMPARE(waylandSurface->type(), QWaylandSurface::Shm);
    QTRY_COMPARE(mappedSpy.count(), 1);

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
    class BufferAttacher : public QWaylandBufferAttacher
    {
    public:
        void attach(const QWaylandBufferRef &ref) Q_DECL_OVERRIDE
        {
            bufferRef = ref;
        }

        QImage image() const
        {
            if (!bufferRef || !bufferRef.isShm())
                return QImage();
            return bufferRef.image();
        }

        QWaylandBufferRef bufferRef;
    };

    TestCompositor compositor;

    MockClient client;

    wl_surface *surface = client.createSurface();

    int frameCounter = 0;

    QTRY_COMPARE(compositor.surfaces.size(), 1);
    QWaylandSurface *waylandSurface = compositor.surfaces.at(0);
    BufferAttacher attacher;
    waylandSurface->setBufferAttacher(&attacher);
    QSignalSpy damagedSpy(waylandSurface, SIGNAL(damaged(const QRegion &)));

    for (int i = 0; i < 10; ++i) {
        QSize size(i * 8 + 2, i * 8 + 2);
        ShmBuffer buffer(size, client.shm);

        // attach a new buffer every frame, else the damage signal won't be fired
        wl_surface_attach(surface, buffer.handle, 0, 0);
        registerFrameCallback(surface, &frameCounter);
        wl_surface_damage(surface, 0, 0, size.width(), size.height());
        wl_surface_commit(surface);

        QTRY_COMPARE(waylandSurface->type(), QWaylandSurface::Shm);
        QTRY_COMPARE(damagedSpy.count(), i + 1);

        QCOMPARE(static_cast<BufferAttacher *>(waylandSurface->bufferAttacher())->image(), buffer.image);
        compositor.frameStarted();
        compositor.sendFrameCallbacks(QList<QWaylandSurface *>() << waylandSurface);

        QTRY_COMPARE(frameCounter, i + 1);
    }

    wl_surface_destroy(surface);
}

void tst_WaylandCompositor::inputDeviceCapabilities()
{
    TestCompositor compositor;
    QtWayland::InputDevice dev(NULL, compositor.handle(), QWaylandInputDevice::Pointer);

    QTRY_VERIFY(dev.pointerDevice());
    QTRY_VERIFY(!dev.keyboardDevice());
    QTRY_VERIFY(!dev.touchDevice());

    dev.setCapabilities(QWaylandInputDevice::Keyboard | QWaylandInputDevice::Touch);
    QTRY_VERIFY(!dev.pointerDevice());
    QTRY_VERIFY(dev.keyboardDevice());
    QTRY_VERIFY(dev.touchDevice());

    // Test that existing devices do not change when another is removed
    QtWayland::Keyboard *k = dev.keyboardDevice();
    dev.setCapabilities(QWaylandInputDevice::Keyboard);
    QTRY_COMPARE(k, dev.keyboardDevice());
}

#include <tst_compositor.moc>
QTEST_MAIN(tst_WaylandCompositor);
