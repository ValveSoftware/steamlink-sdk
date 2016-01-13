/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the config.tests of the Qt Toolkit.
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

#include "qwaylandwindow_p.h"

#include "qwaylandbuffer_p.h"
#include "qwaylanddatadevice_p.h"
#include "qwaylanddisplay_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandshellsurface_p.h"
#include "qwaylandwlshellsurface_p.h"
#include "qwaylandxdgsurface_p.h"
#include "qwaylandsubsurface_p.h"
#include "qwaylandabstractdecoration_p.h"
#include "qwaylandwindowmanagerintegration_p.h"
#include "qwaylandnativeinterface_p.h"
#include "qwaylanddecorationfactory_p.h"
#include "qwaylandshmbackingstore_p.h"

#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtGui/QWindow>

#include <QGuiApplication>
#include <qpa/qwindowsysteminterface.h>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QWaylandWindow *QWaylandWindow::mMouseGrab = 0;

QWaylandWindow::QWaylandWindow(QWindow *window)
    : QObject()
    , QPlatformWindow(window)
    , mScreen(QWaylandScreen::waylandScreenFromWindow(window))
    , mDisplay(mScreen->display())
    , mShellSurface(0)
    , mSubSurfaceWindow(0)
    , mWindowDecoration(0)
    , mMouseEventsInContentArea(false)
    , mMousePressedInContentArea(Qt::NoButton)
    , m_cursorShape(Qt::ArrowCursor)
    , mBuffer(0)
    , mWaitingForFrameSync(false)
    , mFrameCallback(0)
    , mRequestResizeSent(false)
    , mCanResize(true)
    , mResizeDirty(false)
    , mResizeAfterSwap(qEnvironmentVariableIsSet("QT_WAYLAND_RESIZE_AFTER_SWAP"))
    , mSentInitialResize(false)
    , mMouseDevice(0)
    , mMouseSerial(0)
    , mState(Qt::WindowNoState)
    , mBackingStore(Q_NULLPTR)
{
    init(mDisplay->createSurface(static_cast<QtWayland::wl_surface *>(this)));

    static WId id = 1;
    mWindowId = id++;

    if (mDisplay->subSurfaceExtension())
        mSubSurfaceWindow = new QWaylandSubSurface(this, mDisplay->subSurfaceExtension()->get_sub_surface_aware_surface(object()));

    if (!(window->flags() & Qt::BypassWindowManagerHint)) {
        mShellSurface = mDisplay->createShellSurface(this);
    }

    if (mShellSurface) {
        // Set initial surface title
        mShellSurface->setTitle(window->title());

        // Set surface class to the .desktop file name (obtained from executable name)
        QFileInfo exeFileInfo(qApp->applicationFilePath());
        QString className = exeFileInfo.baseName() + QLatin1String(".desktop");
        mShellSurface->setAppId(className);
    }

    if (QPlatformWindow::parent() && mSubSurfaceWindow) {
        mSubSurfaceWindow->setParent(static_cast<const QWaylandWindow *>(QPlatformWindow::parent()));
    } else if (window->transientParent() && mShellSurface) {
        if (window->type() != Qt::Popup) {
            mShellSurface->updateTransientParent(window->transientParent());
        }
    } else if (mShellSurface) {
        mShellSurface->setTopLevel();
    }

    setOrientationMask(window->screen()->orientationUpdateMask());
    setWindowFlags(window->flags());
    setGeometry_helper(window->geometry());
    setWindowStateInternal(window->windowState());
    handleContentOrientationChange(window->contentOrientation());
}

QWaylandWindow::~QWaylandWindow()
{
    delete mWindowDecoration;

    if (isInitialized()) {
        delete mShellSurface;
        destroy();
    }
    if (mFrameCallback)
        wl_callback_destroy(mFrameCallback);

    QList<QWaylandInputDevice *> inputDevices = mDisplay->inputDevices();
    for (int i = 0; i < inputDevices.size(); ++i)
        inputDevices.at(i)->handleWindowDestroyed(this);

    const QWindow *parent = window();
    foreach (QWindow *w, QGuiApplication::topLevelWindows()) {
        if (w->transientParent() == parent)
            QWindowSystemInterface::handleCloseEvent(w);
    }

    if (mMouseGrab == this) {
        mMouseGrab = 0;
    }
}

QWaylandWindow *QWaylandWindow::fromWlSurface(::wl_surface *surface)
{
    return static_cast<QWaylandWindow *>(static_cast<QtWayland::wl_surface *>(wl_surface_get_user_data(surface)));
}

WId QWaylandWindow::winId() const
{
    return mWindowId;
}

void QWaylandWindow::setParent(const QPlatformWindow *parent)
{
    const QWaylandWindow *parentWaylandWindow = static_cast<const QWaylandWindow *>(parent);
    if (subSurfaceWindow()) {
        subSurfaceWindow()->setParent(parentWaylandWindow);
    }
}

void QWaylandWindow::setWindowTitle(const QString &title)
{
    if (mShellSurface) {
        mShellSurface->setTitle(title);
    }

    if (mWindowDecoration && window()->isVisible())
        mWindowDecoration->update();
}

void QWaylandWindow::setWindowIcon(const QIcon &icon)
{
    mWindowIcon = icon;

    if (mWindowDecoration && window()->isVisible())
        mWindowDecoration->update();
}

void QWaylandWindow::setGeometry_helper(const QRect &rect)
{
    QPlatformWindow::setGeometry(QRect(rect.x(), rect.y(),
                qBound(window()->minimumWidth(), rect.width(), window()->maximumWidth()),
                qBound(window()->minimumHeight(), rect.height(), window()->maximumHeight())));

    if (shellSurface() && window()->transientParent() && window()->type() != Qt::Popup)
        shellSurface()->updateTransientParent(window()->transientParent());
}

void QWaylandWindow::setGeometry(const QRect &rect)
{
    setGeometry_helper(rect);

    if (window()->isVisible()) {
        if (mWindowDecoration)
            mWindowDecoration->update();

        if (mResizeAfterSwap && windowType() == Egl && mSentInitialResize)
            mResizeDirty = true;
        else
            QWindowSystemInterface::handleGeometryChange(window(), geometry());

        mSentInitialResize = true;
    }

    QWindowSystemInterface::handleExposeEvent(window(), QRect(QPoint(), geometry().size()));
}

void QWaylandWindow::setVisible(bool visible)
{
    if (visible) {
        if (window()->type() == Qt::Popup && transientParent()) {
            QWaylandWindow *parent = transientParent();
            mMouseDevice = parent->mMouseDevice;
            mMouseSerial = parent->mMouseSerial;

            QWaylandWlShellSurface *wlshellSurface = dynamic_cast<QWaylandWlShellSurface*>(mShellSurface);
            if (mMouseDevice && wlshellSurface) {
                wlshellSurface->setPopup(transientParent(), mMouseDevice, mMouseSerial);
            }
        }

        setGeometry(window()->geometry());
        // Don't flush the events here, or else the newly visible window may start drawing, but since
        // there was no frame before it will be stuck at the waitForFrameSync() in
        // QWaylandShmBackingStore::beginPaint().
    } else {
        QWindowSystemInterface::handleExposeEvent(window(), QRegion());
        // when flushing the event queue, it could contain a close event, in which
        // case 'this' will be deleted. When that happens, we must abort right away.
        QPointer<QWaylandWindow> deleteGuard(this);
        QWindowSystemInterface::flushWindowSystemEvents();
        if (!deleteGuard.isNull()) {
            attach(static_cast<QWaylandBuffer *>(0), 0, 0);
            commit();
            if (mBackingStore) {
                mBackingStore->hidden();
            }
        }
    }
}


void QWaylandWindow::raise()
{
    if (mShellSurface)
        mShellSurface->raise();
}


void QWaylandWindow::lower()
{
    if (mShellSurface)
        mShellSurface->lower();
}

void QWaylandWindow::configure(uint32_t edges, int32_t width, int32_t height)
{
    QMutexLocker resizeLocker(&mResizeLock);
    mConfigure.edges |= edges;
    mConfigure.width = width;
    mConfigure.height = height;

    if (!mRequestResizeSent && !mConfigure.isEmpty()) {
        mRequestResizeSent= true;
        QMetaObject::invokeMethod(this, "requestResize", Qt::QueuedConnection);
    }
}

void QWaylandWindow::doResize()
{
    if (mConfigure.isEmpty()) {
        return;
    }

    int widthWithoutMargins = qMax(mConfigure.width-(frameMargins().left() +frameMargins().right()),1);
    int heightWithoutMargins = qMax(mConfigure.height-(frameMargins().top()+frameMargins().bottom()),1);

    widthWithoutMargins = qMax(widthWithoutMargins, window()->minimumSize().width());
    heightWithoutMargins = qMax(heightWithoutMargins, window()->minimumSize().height());
    QRect geometry = QRect(0,0, widthWithoutMargins, heightWithoutMargins);

    int x = 0;
    int y = 0;
    QSize size = this->geometry().size();
    if (mConfigure.edges & WL_SHELL_SURFACE_RESIZE_LEFT) {
        x = size.width() - geometry.width();
    }
    if (mConfigure.edges & WL_SHELL_SURFACE_RESIZE_TOP) {
        y = size.height() - geometry.height();
    }
    mOffset += QPoint(x, y);

    setGeometry(geometry);

    mConfigure.clear();
}

void QWaylandWindow::setCanResize(bool canResize)
{
    QMutexLocker lock(&mResizeLock);
    mCanResize = canResize;

    if (canResize) {
        if (mResizeDirty) {
            QWindowSystemInterface::handleGeometryChange(window(), geometry());
        }
        if (!mConfigure.isEmpty()) {
            doResize();
            QWindowSystemInterface::handleExposeEvent(window(), QRect(QPoint(), geometry().size()));
        } else if (mResizeDirty) {
            QWindowSystemInterface::handleExposeEvent(window(), QRect(QPoint(), geometry().size()));
            mResizeDirty = false;
        }
    }
}

void QWaylandWindow::requestResize()
{
    QMutexLocker lock(&mResizeLock);

    if (mCanResize) {
        doResize();
    }

    mRequestResizeSent = false;
    lock.unlock();
    QWindowSystemInterface::handleExposeEvent(window(), QRect(QPoint(), geometry().size()));
    QWindowSystemInterface::flushWindowSystemEvents();
}

void QWaylandWindow::attach(QWaylandBuffer *buffer, int x, int y)
{
    mBuffer = buffer;

    if (mBuffer)
        attach(mBuffer->buffer(), x, y);
    else
        QtWayland::wl_surface::attach(0, 0, 0);
}

void QWaylandWindow::attachOffset(QWaylandBuffer *buffer)
{
    attach(buffer, mOffset.x(), mOffset.y());
    mOffset = QPoint();
}

QWaylandBuffer *QWaylandWindow::attached() const
{
    return mBuffer;
}

void QWaylandWindow::damage(const QRect &rect)
{
    //We have to do sync stuff before calling damage, or we might
    //get a frame callback before we get the timestamp
    if (!mWaitingForFrameSync) {
        mFrameCallback = frame();
        wl_callback_add_listener(mFrameCallback,&QWaylandWindow::callbackListener,this);
        mWaitingForFrameSync = true;
    }
    if (mBuffer) {
        damage(rect.x(), rect.y(), rect.width(), rect.height());
    }
}

const wl_callback_listener QWaylandWindow::callbackListener = {
    QWaylandWindow::frameCallback
};

void QWaylandWindow::frameCallback(void *data, struct wl_callback *callback, uint32_t time)
{
    Q_UNUSED(time);
    QWaylandWindow *self = static_cast<QWaylandWindow*>(data);
    if (callback != self->mFrameCallback) // might be a callback caused by the shm backingstore
        return;
    self->mWaitingForFrameSync = false;
    if (self->mFrameCallback) {
        wl_callback_destroy(self->mFrameCallback);
        self->mFrameCallback = 0;
    }
}

QMutex QWaylandWindow::mFrameSyncMutex;

void QWaylandWindow::waitForFrameSync()
{
    QMutexLocker locker(&mFrameSyncMutex);
    if (!mWaitingForFrameSync)
        return;
    mDisplay->flushRequests();
    while (mWaitingForFrameSync)
        mDisplay->blockingReadEvents();
}

QMargins QWaylandWindow::frameMargins() const
{
    if (mWindowDecoration)
        return mWindowDecoration->margins();
    return QPlatformWindow::frameMargins();
}

QWaylandShellSurface *QWaylandWindow::shellSurface() const
{
    return mShellSurface;
}

QWaylandSubSurface *QWaylandWindow::subSurfaceWindow() const
{
    return mSubSurfaceWindow;
}

void QWaylandWindow::handleContentOrientationChange(Qt::ScreenOrientation orientation)
{
    if (mDisplay->compositorVersion() < 2)
        return;

    wl_output_transform transform;
    bool isPortrait = window()->screen() && window()->screen()->primaryOrientation() == Qt::PortraitOrientation;
    switch (orientation) {
        case Qt::PrimaryOrientation:
            transform = WL_OUTPUT_TRANSFORM_NORMAL;
            break;
        case Qt::LandscapeOrientation:
            transform = isPortrait ? WL_OUTPUT_TRANSFORM_270 : WL_OUTPUT_TRANSFORM_NORMAL;
            break;
        case Qt::PortraitOrientation:
            transform = isPortrait ? WL_OUTPUT_TRANSFORM_NORMAL : WL_OUTPUT_TRANSFORM_90;
            break;
        case Qt::InvertedLandscapeOrientation:
            transform = isPortrait ? WL_OUTPUT_TRANSFORM_90 : WL_OUTPUT_TRANSFORM_180;
            break;
        case Qt::InvertedPortraitOrientation:
            transform = isPortrait ? WL_OUTPUT_TRANSFORM_180 : WL_OUTPUT_TRANSFORM_270;
            break;
        default:
            Q_UNREACHABLE();
    }
    set_buffer_transform(transform);
    // set_buffer_transform is double buffered, we need to commit.
    commit();
}

void QWaylandWindow::setOrientationMask(Qt::ScreenOrientations mask)
{
    if (mShellSurface)
        mShellSurface->setContentOrientationMask(mask);
}

void QWaylandWindow::setWindowState(Qt::WindowState state)
{
    if (setWindowStateInternal(state))
        QWindowSystemInterface::flushWindowSystemEvents(); // Required for oldState to work on WindowStateChanged
}

void QWaylandWindow::setWindowFlags(Qt::WindowFlags flags)
{
    if (mShellSurface)
        mShellSurface->setWindowFlags(flags);
}

bool QWaylandWindow::createDecoration()
{
    // so far only xdg-shell support this "unminimize" trick, may be moved elsewhere
    if (mState == Qt::WindowMinimized) {
        QWaylandXdgSurface *xdgSurface = dynamic_cast<QWaylandXdgSurface *>(mShellSurface);
        if ( xdgSurface ) {
            if (xdgSurface->isFullscreen()) {
                setWindowStateInternal(Qt::WindowFullScreen);
            } else if (xdgSurface->isMaximized()) {
                setWindowStateInternal(Qt::WindowMaximized);
            } else {
                setWindowStateInternal(Qt::WindowNoState);
            }
        }
    }

    if (!mDisplay->supportsWindowDecoration())
        return false;

    static bool decorationPluginFailed = false;
    bool decoration = false;
    switch (window()->type()) {
        case Qt::Window:
        case Qt::Widget:
        case Qt::Dialog:
        case Qt::Tool:
        case Qt::Drawer:
            decoration = true;
            break;
        default:
            break;
    }
    if (window()->flags() & Qt::FramelessWindowHint || isFullscreen())
        decoration = false;
    if (window()->flags() & Qt::BypassWindowManagerHint)
        decoration = false;

    if (decoration && !decorationPluginFailed) {
        if (!mWindowDecoration) {
            QStringList decorations = QWaylandDecorationFactory::keys();
            if (decorations.empty()) {
                qWarning() << "No decoration plugins available. Running with no decorations.";
                decorationPluginFailed = true;
                return false;
            }

            QString targetKey;
            QByteArray decorationPluginName = qgetenv("QT_WAYLAND_DECORATION");
            if (!decorationPluginName.isEmpty()) {
                targetKey = QString::fromLocal8Bit(decorationPluginName);
                if (!decorations.contains(targetKey)) {
                    qWarning() << "Requested decoration " << targetKey << " not found, falling back to default";
                    targetKey = QString(); // fallthrough
                }
            }

            if (targetKey.isEmpty())
                targetKey = decorations.first(); // first come, first served.


            mWindowDecoration = QWaylandDecorationFactory::create(targetKey, QStringList());
            if (!mWindowDecoration) {
                qWarning() << "Could not create decoration from factory! Running with no decorations.";
                decorationPluginFailed = true;
                return false;
            }
            mWindowDecoration->setWaylandWindow(this);
            if (subSurfaceWindow()) {
                subSurfaceWindow()->adjustPositionOfChildren();
            }
        }
    } else {
        delete mWindowDecoration;
        mWindowDecoration = 0;
    }

    return mWindowDecoration;
}

QWaylandAbstractDecoration *QWaylandWindow::decoration() const
{
    return mWindowDecoration;
}

static QWindow *topLevelWindow(QWindow *window)
{
    while (QWindow *parent = window->parent())
        window = parent;
    return window;
}

QWaylandWindow *QWaylandWindow::transientParent() const
{
    if (window()->transientParent()) {
        // Take the top level window here, since the transient parent may be a QWidgetWindow
        // or some other window without a shell surface, which is then not able to get mouse
        // events, nor set mMouseSerial and mMouseDevice.
        return static_cast<QWaylandWindow *>(topLevelWindow(window()->transientParent())->handle());
    }
    return 0;
}

void QWaylandWindow::handleMouse(QWaylandInputDevice *inputDevice, ulong timestamp, const QPointF &local, const QPointF &global, Qt::MouseButtons b, Qt::KeyboardModifiers mods)
{
    if (b != Qt::NoButton) {
        mMouseSerial = inputDevice->serial();
        mMouseDevice = inputDevice;
    }

    if (mWindowDecoration) {
        handleMouseEventWithDecoration(inputDevice, timestamp,local,global,b,mods);
        return;
    }

    QWindowSystemInterface::handleMouseEvent(window(),timestamp,local,global,b,mods);
}

void QWaylandWindow::handleMouseEnter(QWaylandInputDevice *inputDevice)
{
    if (!mWindowDecoration) {
        QWindowSystemInterface::handleEnterEvent(window());
    }
    restoreMouseCursor(inputDevice);
}

void QWaylandWindow::handleMouseLeave(QWaylandInputDevice *inputDevice)
{
    if (mWindowDecoration) {
        if (mMouseEventsInContentArea) {
            QWindowSystemInterface::handleLeaveEvent(window());
        }
    } else {
        QWindowSystemInterface::handleLeaveEvent(window());
    }
    restoreMouseCursor(inputDevice);
}

bool QWaylandWindow::touchDragDecoration(QWaylandInputDevice *inputDevice, const QPointF &local, const QPointF &global, Qt::TouchPointState state, Qt::KeyboardModifiers mods)
{
    if (!mWindowDecoration)
        return false;
    return mWindowDecoration->handleTouch(inputDevice, local, global, state, mods);
}

void QWaylandWindow::handleMouseEventWithDecoration(QWaylandInputDevice *inputDevice, ulong timestamp, const QPointF &local, const QPointF &global, Qt::MouseButtons b, Qt::KeyboardModifiers mods)
{
    if (mWindowDecoration->handleMouse(inputDevice,local,global,b,mods))
        return;

    QMargins marg = frameMargins();
    QRect windowRect(0 + marg.left(),
                     0 + marg.top(),
                     geometry().size().width() - marg.right(),
                     geometry().size().height() - marg.bottom());
    if (windowRect.contains(local.toPoint()) || mMousePressedInContentArea != Qt::NoButton) {
        QPointF localTranslated = local;
        QPointF globalTranslated = global;
        localTranslated.setX(localTranslated.x() - marg.left());
        localTranslated.setY(localTranslated.y() - marg.top());
        globalTranslated.setX(globalTranslated.x() - marg.left());
        globalTranslated.setY(globalTranslated.y() - marg.top());
        if (!mMouseEventsInContentArea) {
            restoreMouseCursor(inputDevice);
            QWindowSystemInterface::handleEnterEvent(window());
        }
        QWindowSystemInterface::handleMouseEvent(window(), timestamp, localTranslated, globalTranslated, b, mods);
        mMouseEventsInContentArea = true;
        mMousePressedInContentArea = b;
    } else {
        if (mMouseEventsInContentArea) {
            QWindowSystemInterface::handleLeaveEvent(window());
            mMouseEventsInContentArea = false;
        }
        mWindowDecoration->handleMouse(inputDevice,local,global,b,mods);
    }
}

void QWaylandWindow::setMouseCursor(QWaylandInputDevice *device, Qt::CursorShape shape)
{
    if (m_cursorShape != shape || device->serial() > device->cursorSerial()) {
        device->setCursor(shape, mScreen);
        m_cursorShape = shape;
    }
}

void QWaylandWindow::restoreMouseCursor(QWaylandInputDevice *device)
{
    setMouseCursor(device, window()->cursor().shape());
}

void QWaylandWindow::requestActivateWindow()
{
    // no-op. Wayland does not have activation protocol,
    // we rely on compositor setting keyboard focus based on window stacking.
}

void QWaylandWindow::unfocus()
{
    QWaylandInputDevice *inputDevice = mDisplay->currentInputDevice();
    if (inputDevice && inputDevice->dataDevice()) {
        inputDevice->dataDevice()->invalidateSelectionOffer();
    }
}

bool QWaylandWindow::isExposed() const
{
    if (mShellSurface)
        return window()->isVisible() && mShellSurface->isExposed();
    return QPlatformWindow::isExposed();
}

bool QWaylandWindow::setMouseGrabEnabled(bool grab)
{
    if (window()->type() != Qt::Popup) {
        qWarning("This plugin supports grabbing the mouse only for popup windows");
        return false;
    }

    mMouseGrab = grab ? this : 0;
    return true;
}

bool QWaylandWindow::setWindowStateInternal(Qt::WindowState state)
{
    if (mState == state) {
        return false;
    }

    // As of february 2013 QWindow::setWindowState sets the new state value after
    // QPlatformWindow::setWindowState returns, so we cannot rely on QWindow::windowState
    // here. We use then this mState variable.
    mState = state;

    if (mShellSurface) {
        switch (state) {
            case Qt::WindowFullScreen:
                mShellSurface->setFullscreen();
                break;
            case Qt::WindowMaximized:
                mShellSurface->setMaximized();
                break;
            case Qt::WindowMinimized:
                mShellSurface->setMinimized();
                break;
            default:
                mShellSurface->setNormal();
        }
    }

    QWindowSystemInterface::handleWindowStateChanged(window(), mState);
    return true;
}

void QWaylandWindow::sendProperty(const QString &name, const QVariant &value)
{
    m_properties.insert(name, value);
    QWaylandNativeInterface *nativeInterface = static_cast<QWaylandNativeInterface *>(
                QGuiApplication::platformNativeInterface());
    nativeInterface->emitWindowPropertyChanged(this, name);
    if (mShellSurface)
        mShellSurface->sendProperty(name, value);
}

void QWaylandWindow::setProperty(const QString &name, const QVariant &value)
{
    m_properties.insert(name, value);
    QWaylandNativeInterface *nativeInterface = static_cast<QWaylandNativeInterface *>(
                QGuiApplication::platformNativeInterface());
    nativeInterface->emitWindowPropertyChanged(this, name);
}

QVariantMap QWaylandWindow::properties() const
{
    return m_properties;
}

QVariant QWaylandWindow::property(const QString &name)
{
    return m_properties.value(name);
}

QVariant QWaylandWindow::property(const QString &name, const QVariant &defaultValue)
{
    return m_properties.value(name, defaultValue);
}

QT_END_NAMESPACE
