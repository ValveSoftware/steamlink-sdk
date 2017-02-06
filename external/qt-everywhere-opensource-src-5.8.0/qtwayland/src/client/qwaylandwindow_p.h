/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the config.tests of the Qt Toolkit.
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

#ifndef QWAYLANDWINDOW_H
#define QWAYLANDWINDOW_H

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

#include <QtCore/QWaitCondition>
#include <QtCore/QMutex>
#include <QtCore/QAtomicPointer>
#include <QtGui/QIcon>
#include <QtCore/QVariant>

#include <qpa/qplatformwindow.h>

#include <QtWaylandClient/private/qwayland-wayland.h>
#include <QtWaylandClient/qtwaylandclientglobal.h>

struct wl_egl_window;

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class QWaylandDisplay;
class QWaylandBuffer;
class QWaylandShellSurface;
class QWaylandSubSurface;
class QWaylandAbstractDecoration;
class QWaylandInputDevice;
class QWaylandScreen;
class QWaylandShmBackingStore;
class QWaylandPointerEvent;

class Q_WAYLAND_CLIENT_EXPORT QWaylandWindowConfigure
{
public:
    QWaylandWindowConfigure()
        : width(0)
        , height(0)
        , edges(0)
    { }

    void clear()
    { width = height = edges = 0; }

    bool isEmpty() const
    { return !height || !width; }

    int width;
    int height;
    uint32_t edges;
};

class Q_WAYLAND_CLIENT_EXPORT QWaylandWindow : public QObject, public QPlatformWindow, public QtWayland::wl_surface
{
    Q_OBJECT
public:
    enum WindowType {
        Shm,
        Egl
    };

    QWaylandWindow(QWindow *window);
    ~QWaylandWindow();

    virtual WindowType windowType() const = 0;
    WId winId() const Q_DECL_OVERRIDE;
    void setVisible(bool visible) Q_DECL_OVERRIDE;
    void setParent(const QPlatformWindow *parent) Q_DECL_OVERRIDE;

    void setWindowTitle(const QString &title) Q_DECL_OVERRIDE;

    inline QIcon windowIcon() const;
    void setWindowIcon(const QIcon &icon) Q_DECL_OVERRIDE;

    void setGeometry(const QRect &rect) Q_DECL_OVERRIDE;

    void configure(uint32_t edges, int32_t width, int32_t height);

    using QtWayland::wl_surface::attach;
    void attach(QWaylandBuffer *buffer, int x, int y);
    void attachOffset(QWaylandBuffer *buffer);
    QPoint attachOffset() const;

    using QtWayland::wl_surface::damage;
    void damage(const QRect &rect);

    void commit(QWaylandBuffer *buffer, const QRegion &damage);

    void waitForFrameSync();

    QMargins frameMargins() const Q_DECL_OVERRIDE;

    static QWaylandWindow *fromWlSurface(::wl_surface *surface);

    QWaylandDisplay *display() const { return mDisplay; }
    QWaylandShellSurface *shellSurface() const;
    QWaylandSubSurface *subSurfaceWindow() const;
    QWaylandScreen *screen() const { return mScreen; }

    void handleContentOrientationChange(Qt::ScreenOrientation orientation) Q_DECL_OVERRIDE;
    void setOrientationMask(Qt::ScreenOrientations mask);

    void setWindowState(Qt::WindowState state) Q_DECL_OVERRIDE;
    void setWindowFlags(Qt::WindowFlags flags) Q_DECL_OVERRIDE;

    void raise() Q_DECL_OVERRIDE;
    void lower() Q_DECL_OVERRIDE;

    void setMask(const QRegion &region) Q_DECL_OVERRIDE;

    int scale() const;
    qreal devicePixelRatio() const Q_DECL_OVERRIDE;

    void requestActivateWindow() Q_DECL_OVERRIDE;
    bool isExposed() const Q_DECL_OVERRIDE;
    void unfocus();

    QWaylandAbstractDecoration *decoration() const;

    void handleMouse(QWaylandInputDevice *inputDevice, const QWaylandPointerEvent &e);
    void handleMouseLeave(QWaylandInputDevice *inputDevice);

    bool touchDragDecoration(QWaylandInputDevice *inputDevice, const QPointF &local, const QPointF &global,
                             Qt::TouchPointState state, Qt::KeyboardModifiers mods);

    bool createDecoration();

    inline bool isMaximized() const { return mState == Qt::WindowMaximized; }
    inline bool isFullscreen() const { return mState == Qt::WindowFullScreen; }

    void setMouseCursor(QWaylandInputDevice *device, const QCursor &cursor);
    void restoreMouseCursor(QWaylandInputDevice *device);

    QWaylandWindow *transientParent() const;

    QMutex *resizeMutex() { return &mResizeLock; }
    void doResize();
    void setCanResize(bool canResize);

    bool setMouseGrabEnabled(bool grab) Q_DECL_OVERRIDE;
    static QWaylandWindow *mouseGrab() { return mMouseGrab; }

    void sendProperty(const QString &name, const QVariant &value);
    void setProperty(const QString &name, const QVariant &value);

    QVariantMap properties() const;
    QVariant property(const QString &name);
    QVariant property(const QString &name, const QVariant &defaultValue);

    void setBackingStore(QWaylandShmBackingStore *backingStore) { mBackingStore = backingStore; }
    QWaylandShmBackingStore *backingStore() const { return mBackingStore; }

    bool setKeyboardGrabEnabled(bool) Q_DECL_OVERRIDE { return false; }
    void propagateSizeHints() Q_DECL_OVERRIDE { }
    void addAttachOffset(const QPoint point);

    void requestUpdate() Q_DECL_OVERRIDE;

public slots:
    void requestResize();

protected:
    QWaylandScreen *mScreen;
    QWaylandDisplay *mDisplay;
    QWaylandShellSurface *mShellSurface;
    QWaylandSubSurface *mSubSurfaceWindow;
    QVector<QWaylandSubSurface *> mChildren;

    QWaylandAbstractDecoration *mWindowDecoration;
    bool mMouseEventsInContentArea;
    Qt::MouseButtons mMousePressedInContentArea;

    WId mWindowId;
    bool mWaitingForFrameSync;
    QAtomicPointer<struct wl_callback> mFrameCallback;
    QWaitCondition mFrameSyncWait;

    QMutex mResizeLock;
    QWaylandWindowConfigure mConfigure;
    bool mRequestResizeSent;
    bool mCanResize;
    bool mResizeDirty;
    bool mResizeAfterSwap;
    QVariantMap m_properties;

    bool mSentInitialResize;
    QPoint mOffset;

    QIcon mWindowIcon;

    Qt::WindowState mState;
    Qt::WindowFlags mFlags;
    QRegion mMask;

    QWaylandShmBackingStore *mBackingStore;

private:
    bool setWindowStateInternal(Qt::WindowState flags);
    void setGeometry_helper(const QRect &rect);
    void initWindow();
    bool shouldCreateShellSurface() const;
    bool shouldCreateSubSurface() const;
    void reset();

    void handleMouseEventWithDecoration(QWaylandInputDevice *inputDevice, const QWaylandPointerEvent &e);

    bool mUpdateRequested;

    static const wl_callback_listener callbackListener;
    static void frameCallback(void *data, struct wl_callback *wl_callback, uint32_t time);

    static QMutex mFrameSyncMutex;
    static QWaylandWindow *mMouseGrab;

    friend class QWaylandSubSurface;
};

inline QIcon QWaylandWindow::windowIcon() const
{
    return mWindowIcon;
}

inline QPoint QWaylandWindow::attachOffset() const
{
    return mOffset;
}

QT_END_NAMESPACE

}

#endif // QWAYLANDWINDOW_H
