/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWAYLANDDISPLAY_H
#define QWAYLANDDISPLAY_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtCore/QPointer>
#include <QtCore/QVector>

#include <QtCore/QWaitCondition>

#include <wayland-client.h>

#include <QtWaylandClient/private/qwayland-wayland.h>
#include <QtWaylandClient/qtwaylandclientglobal.h>
#include <QtWaylandClient/private/qwayland-xdg-shell.h>
#include <QtWaylandClient/private/qwaylandshm_p.h>

struct wl_cursor_image;

QT_BEGIN_NAMESPACE

class QAbstractEventDispatcher;
class QSocketNotifier;
class QPlatformScreen;

namespace QtWayland {
    class qt_shell;
    class qt_sub_surface_extension;
    class qt_surface_extension;
    class zwp_text_input_manager_v2;
    class xdg_shell;
}

namespace QtWaylandClient {

class QWaylandInputDevice;
class QWaylandBuffer;
class QWaylandScreen;
class QWaylandClientBufferIntegration;
class QWaylandWindowManagerIntegration;
class QWaylandDataDeviceManager;
class QWaylandTouchExtension;
class QWaylandQtKeyExtension;
class QWaylandWindow;
class QWaylandEventThread;
class QWaylandIntegration;
class QWaylandHardwareIntegration;
class QWaylandXdgShell;
class QWaylandShellSurface;

typedef void (*RegistryListener)(void *data,
                                 struct wl_registry *registry,
                                 uint32_t id,
                                 const QString &interface,
                                 uint32_t version);

class Q_WAYLAND_CLIENT_EXPORT QWaylandDisplay : public QObject, public QtWayland::wl_registry {
    Q_OBJECT

public:
    QWaylandDisplay(QWaylandIntegration *waylandIntegration);
    ~QWaylandDisplay(void);

    QList<QWaylandScreen *> screens() const { return mScreens; }

    QWaylandScreen *screenForOutput(struct wl_output *output) const;

    struct wl_surface *createSurface(void *handle);
    QWaylandShellSurface *createShellSurface(QWaylandWindow *window);
    struct ::wl_region *createRegion(const QRegion &qregion);
    struct ::wl_subsurface *createSubSurface(QWaylandWindow *window, QWaylandWindow *parent);

    QWaylandClientBufferIntegration *clientBufferIntegration() const;

    QWaylandWindowManagerIntegration *windowManagerIntegration() const;

    void setCursor(struct wl_buffer *buffer, struct wl_cursor_image *image);
    void setCursor(const QSharedPointer<QWaylandBuffer> &buffer, const QPoint &hotSpot);

    struct wl_display *wl_display() const { return mDisplay; }
    struct ::wl_registry *wl_registry() { return object(); }

    const struct wl_compositor *wl_compositor() const { return mCompositor.object(); }
    QtWayland::wl_compositor *compositor() { return &mCompositor; }
    int compositorVersion() const { return mCompositorVersion; }

    QList<QWaylandInputDevice *> inputDevices() const { return mInputDevices; }
    QWaylandInputDevice *defaultInputDevice() const;
    QWaylandInputDevice *currentInputDevice() const { return defaultInputDevice(); }
#if QT_CONFIG(draganddrop)
    QWaylandDataDeviceManager *dndSelectionHandler() const { return mDndSelectionHandler.data(); }
#endif
    QtWayland::qt_surface_extension *windowExtension() const { return mWindowExtension.data(); }
    QWaylandTouchExtension *touchExtension() const { return mTouchExtension.data(); }
    QtWayland::zwp_text_input_manager_v2 *textInputManager() const { return mTextInputManager.data(); }
    QWaylandHardwareIntegration *hardwareIntegration() const { return mHardwareIntegration.data(); }

    struct RegistryGlobal {
        uint32_t id;
        QString interface;
        uint32_t version;
        struct ::wl_registry *registry;
        RegistryGlobal(uint32_t id_, const QString &interface_, uint32_t version_, struct ::wl_registry *registry_)
            : id(id_), interface(interface_), version(version_), registry(registry_) { }
    };
    QList<RegistryGlobal> globals() const { return mGlobals; }
    bool hasRegistryGlobal(const QString &interfaceName);

    /* wl_registry_add_listener does not add but rather sets a listener, so this function is used
     * to enable many listeners at once. */
    void addRegistryListener(RegistryListener listener, void *data);

    QWaylandShm *shm() const { return mShm.data(); }

    static uint32_t currentTimeMillisec();

    void forceRoundTrip();

    bool supportsWindowDecoration() const;

    uint32_t lastInputSerial() const { return mLastInputSerial; }
    QWaylandInputDevice *lastInputDevice() const { return mLastInputDevice; }
    QWaylandWindow *lastInputWindow() const;
    void setLastInputDevice(QWaylandInputDevice *device, uint32_t serial, QWaylandWindow *window);

    void handleWindowActivated(QWaylandWindow *window);
    void handleWindowDeactivated(QWaylandWindow *window);
    void handleKeyboardFocusChanged(QWaylandInputDevice *inputDevice);
    void handleWindowDestroyed(QWaylandWindow *window);

public slots:
    void blockingReadEvents();
    void flushRequests();

private:
    void waitForScreens();
    void exitWithError();
    void checkError() const;

    void handleWaylandSync();
    void requestWaylandSync();

    struct Listener {
        RegistryListener listener;
        void *data;
    };

    struct wl_display *mDisplay;
    QtWayland::wl_compositor mCompositor;
    QScopedPointer<QWaylandShm> mShm;
    QList<QWaylandScreen *> mScreens;
    QList<QWaylandInputDevice *> mInputDevices;
    QList<Listener> mRegistryListeners;
    QWaylandIntegration *mWaylandIntegration;
#if QT_CONFIG(draganddrop)
    QScopedPointer<QWaylandDataDeviceManager> mDndSelectionHandler;
#endif
    QScopedPointer<QtWayland::qt_surface_extension> mWindowExtension;
    QScopedPointer<QtWayland::wl_subcompositor> mSubCompositor;
    QScopedPointer<QWaylandTouchExtension> mTouchExtension;
    QScopedPointer<QWaylandQtKeyExtension> mQtKeyExtension;
    QScopedPointer<QWaylandWindowManagerIntegration> mWindowManagerIntegration;
    QScopedPointer<QtWayland::zwp_text_input_manager_v2> mTextInputManager;
    QScopedPointer<QWaylandHardwareIntegration> mHardwareIntegration;
    QSocketNotifier *mReadNotifier;
    int mFd;
    int mWritableNotificationFd;
    QList<RegistryGlobal> mGlobals;
    int mCompositorVersion;
    uint32_t mLastInputSerial;
    QWaylandInputDevice *mLastInputDevice;
    QPointer<QWaylandWindow> mLastInputWindow;
    QPointer<QWaylandWindow> mLastKeyboardFocus;
    QVector<QWaylandWindow *> mActiveWindows;
    struct wl_callback *mSyncCallback;
    static const wl_callback_listener syncCallbackListener;

    void registry_global(uint32_t id, const QString &interface, uint32_t version) Q_DECL_OVERRIDE;
    void registry_global_remove(uint32_t id) Q_DECL_OVERRIDE;

    static void shellHandleConfigure(void *data, struct wl_shell *shell,
                                     uint32_t time, uint32_t edges,
                                     struct wl_surface *surface,
                                     int32_t width, int32_t height);
};

}

QT_END_NAMESPACE

#endif // QWAYLANDDISPLAY_H
