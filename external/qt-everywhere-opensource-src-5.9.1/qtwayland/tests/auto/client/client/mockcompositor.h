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

#ifndef MOCKCOMPOSITOR_H
#define MOCKCOMPOSITOR_H

#include <pthread.h>
#include <qglobal.h>
#include <wayland-server.h>

#include <QImage>
#include <QMutex>
#include <QRect>
#include <QSharedPointer>
#include <QVariant>
#include <QVector>
#include <QWaitCondition>

namespace Impl {

typedef void (**Implementation)(void);

class Keyboard;
class Pointer;
class Touch;
class Seat;
class DataDeviceManager;
class Surface;

class Compositor
{
public:
    Compositor();
    ~Compositor();

    int fileDescriptor() const { return m_fd; }
    void dispatchEvents(int timeout = 0);

    uint32_t nextSerial();
    uint32_t time() { return ++m_time; }

    static void setOutputGeometry(void *compositor, const QList<QVariant> &parameters);

    QVector<Surface *> surfaces() const;

    void addSurface(Surface *surface);
    void removeSurface(Surface *surface);

    static void setKeyboardFocus(void *data, const QList<QVariant> &parameters);
    static void sendMousePress(void *data, const QList<QVariant> &parameters);
    static void sendMouseRelease(void *data, const QList<QVariant> &parameters);
    static void sendKeyPress(void *data, const QList<QVariant> &parameters);
    static void sendKeyRelease(void *data, const QList<QVariant> &parameters);
    static void sendTouchDown(void *data, const QList<QVariant> &parameters);
    static void sendTouchUp(void *data, const QList<QVariant> &parameters);
    static void sendTouchMotion(void *data, const QList<QVariant> &parameters);
    static void sendTouchFrame(void *data, const QList<QVariant> &parameters);
    static void sendDataDeviceDataOffer(void *data, const QList<QVariant> &parameters);
    static void sendDataDeviceEnter(void *data, const QList<QVariant> &parameters);
    static void sendDataDeviceMotion(void *data, const QList<QVariant> &parameters);
    static void sendDataDeviceDrop(void *data, const QList<QVariant> &parameters);
    static void sendDataDeviceLeave(void *data, const QList<QVariant> &parameters);
    static void waitForStartDrag(void *data, const QList<QVariant> &parameters);

public:
    bool m_startDragSeen;

private:
    static void bindCompositor(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void bindOutput(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void bindShell(wl_client *client, void *data, uint32_t version, uint32_t id);

    void initShm();

    void sendOutputGeometry(wl_resource *resource);
    void sendOutputMode(wl_resource *resource);

    QRect m_outputGeometry;

    wl_display *m_display;
    wl_event_loop *m_loop;
    wl_shm *m_shm;
    int m_fd;

    wl_list m_outputResources;
    uint32_t m_time;

    QScopedPointer<Seat> m_seat;
    Pointer *m_pointer;
    Keyboard *m_keyboard;
    Touch *m_touch;
    QScopedPointer<DataDeviceManager> m_data_device_manager;
    QVector<Surface *> m_surfaces;
};

void registerResource(wl_list *list, wl_resource *resource);

}

class MockSurface
{
public:
    Impl::Surface *handle() const { return m_surface; }

    QImage image;

private:
    MockSurface(Impl::Surface *surface);
    friend class Impl::Compositor;
    friend class Impl::Surface;

    Impl::Surface *m_surface;
};

Q_DECLARE_METATYPE(QSharedPointer<MockSurface>)

class MockCompositor
{
public:
    MockCompositor();
    ~MockCompositor();

    void applicationInitialized();

    int waylandFileDescriptor() const;
    void processWaylandEvents();

    void setOutputGeometry(const QRect &rect);
    void setKeyboardFocus(const QSharedPointer<MockSurface> &surface);
    void sendMousePress(const QSharedPointer<MockSurface> &surface, const QPoint &pos);
    void sendMouseRelease(const QSharedPointer<MockSurface> &surface);
    void sendKeyPress(const QSharedPointer<MockSurface> &surface, uint code);
    void sendKeyRelease(const QSharedPointer<MockSurface> &surface, uint code);
    void sendTouchDown(const QSharedPointer<MockSurface> &surface, const QPoint &position, int id);
    void sendTouchMotion(const QSharedPointer<MockSurface> &surface, const QPoint &position, int id);
    void sendTouchUp(const QSharedPointer<MockSurface> &surface, int id);
    void sendTouchFrame(const QSharedPointer<MockSurface> &surface);
    void sendDataDeviceDataOffer(const QSharedPointer<MockSurface> &surface);
    void sendDataDeviceEnter(const QSharedPointer<MockSurface> &surface, const QPoint &position);
    void sendDataDeviceMotion(const QPoint &position);
    void sendDataDeviceDrop(const QSharedPointer<MockSurface> &surface);
    void sendDataDeviceLeave(const QSharedPointer<MockSurface> &surface);
    void waitForStartDrag();

    QSharedPointer<MockSurface> surface();

    void lock();
    void unlock();

private:
    struct Command
    {
        typedef void (*Callback)(void *target, const QList<QVariant> &parameters);

        Callback callback;
        void *target;
        QList<QVariant> parameters;
    };

    static Command makeCommand(Command::Callback callback, void *target);

    void processCommand(const Command &command);
    void dispatchCommands();

    static void *run(void *data);

    bool m_alive;
    bool m_ready;
    pthread_t m_thread;
    QMutex m_mutex;
    QWaitCondition m_waitCondition;

    Impl::Compositor *m_compositor;

    QList<Command> m_commandQueue;
};

#endif
