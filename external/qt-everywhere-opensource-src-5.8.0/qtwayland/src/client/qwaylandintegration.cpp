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

#include "qwaylandintegration_p.h"

#include "qwaylanddisplay_p.h"
#include "qwaylandshmwindow_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylandinputcontext_p.h"
#include "qwaylandshmbackingstore_p.h"
#include "qwaylandnativeinterface_p.h"
#include "qwaylandclipboard_p.h"
#include "qwaylanddnd_p.h"
#include "qwaylandwindowmanagerintegration_p.h"
#include "qwaylandscreen_p.h"

#include <QtFontDatabaseSupport/private/qgenericunixfontdatabase_p.h>
#include <QtEventDispatcherSupport/private/qgenericunixeventdispatcher_p.h>
#include <QtThemeSupport/private/qgenericunixthemes_p.h>

#include <QtGui/private/qguiapplication_p.h>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformcursor.h>
#include <QtGui/QSurfaceFormat>
#include <QtGui/QOpenGLContext>
#include <QSocketNotifier>

#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qplatformaccessibility.h>
#include <qpa/qplatforminputcontext.h>

#include "qwaylandhardwareintegration_p.h"
#include "qwaylandclientbufferintegration_p.h"
#include "qwaylandclientbufferintegrationfactory_p.h"

#include "qwaylandserverbufferintegration_p.h"
#include "qwaylandserverbufferintegrationfactory_p.h"

#include "qwaylandshellintegration_p.h"
#include "qwaylandshellintegrationfactory_p.h"
#include "qwaylandxdgshellintegration_p.h"
#include "qwaylandwlshellintegration_p.h"

#include "qwaylandinputdeviceintegration_p.h"
#include "qwaylandinputdeviceintegrationfactory_p.h"

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class GenericWaylandTheme: public QGenericUnixTheme
{
public:
    static QStringList themeNames()
    {
        QStringList result;

        if (QGuiApplication::desktopSettingsAware()) {
            const QByteArray desktopEnvironment = QGuiApplicationPrivate::platformIntegration()->services()->desktopEnvironment();

            if (desktopEnvironment == QByteArrayLiteral("KDE")) {
#if QT_CONFIG(settings)
                result.push_back(QStringLiteral("kde"));
#endif
            } else if (!desktopEnvironment.isEmpty() &&
                desktopEnvironment != QByteArrayLiteral("UNKNOWN") &&
                desktopEnvironment != QByteArrayLiteral("GNOME") &&
                desktopEnvironment != QByteArrayLiteral("UNITY") &&
                desktopEnvironment != QByteArrayLiteral("MATE") &&
                desktopEnvironment != QByteArrayLiteral("XFCE") &&
                desktopEnvironment != QByteArrayLiteral("LXDE"))
                // Ignore X11 desktop environments
                result.push_back(QString::fromLocal8Bit(desktopEnvironment.toLower()));
        }

        if (result.isEmpty())
            result.push_back(QLatin1String(QGenericUnixTheme::name));

        return result;
    }
};

QWaylandIntegration::QWaylandIntegration()
    : mClientBufferIntegration(0)
    , mInputDeviceIntegration(Q_NULLPTR)
    , mFontDb(new QGenericUnixFontDatabase())
    , mNativeInterface(new QWaylandNativeInterface(this))
#if QT_CONFIG(accessibility)
    , mAccessibility(new QPlatformAccessibility())
#endif
    , mClientBufferIntegrationInitialized(false)
    , mServerBufferIntegrationInitialized(false)
    , mShellIntegrationInitialized(false)
{
    initializeInputDeviceIntegration();
    mDisplay.reset(new QWaylandDisplay(this));
#if QT_CONFIG(draganddrop)
    mClipboard.reset(new QWaylandClipboard(mDisplay.data()));
    mDrag.reset(new QWaylandDrag(mDisplay.data()));
#endif
    QString icStr = QPlatformInputContextFactory::requested();
    if (!icStr.isNull()) {
        mInputContext.reset(QPlatformInputContextFactory::create(icStr));
    } else {
        //try to use the input context using the wl_text_input interface
        QPlatformInputContext *ctx = new QWaylandInputContext(mDisplay.data());
        mInputContext.reset(ctx);

        //use the traditional way for on screen keyboards for now
        if (!mInputContext.data()->isValid()) {
            ctx = QPlatformInputContextFactory::create();
            mInputContext.reset(ctx);
        }
    }
}

QWaylandIntegration::~QWaylandIntegration()
{
}

QPlatformNativeInterface * QWaylandIntegration::nativeInterface() const
{
    return mNativeInterface.data();
}

bool QWaylandIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps: return true;
    case OpenGL:
        return mDisplay->clientBufferIntegration();
    case ThreadedOpenGL:
        return mDisplay->clientBufferIntegration() && mDisplay->clientBufferIntegration()->supportsThreadedOpenGL();
    case BufferQueueingOpenGL:
        return true;
    case MultipleWindows:
    case NonFullScreenWindows:
        return true;
    case RasterGLSurface:
        return true;
    default: return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformWindow *QWaylandIntegration::createPlatformWindow(QWindow *window) const
{
    if ((window->surfaceType() == QWindow::OpenGLSurface || window->surfaceType() == QWindow::RasterGLSurface)
        && mDisplay->clientBufferIntegration())
        return mDisplay->clientBufferIntegration()->createEglWindow(window);

    return new QWaylandShmWindow(window);
}

#if QT_CONFIG(opengl)
QPlatformOpenGLContext *QWaylandIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    if (mDisplay->clientBufferIntegration())
        return mDisplay->clientBufferIntegration()->createPlatformOpenGLContext(context->format(), context->shareHandle());
    return 0;
}
#endif  // opengl

QPlatformBackingStore *QWaylandIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QWaylandShmBackingStore(window);
}

QAbstractEventDispatcher *QWaylandIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

void QWaylandIntegration::initialize()
{
    QAbstractEventDispatcher *dispatcher = QGuiApplicationPrivate::eventDispatcher;
    QObject::connect(dispatcher, SIGNAL(aboutToBlock()), mDisplay.data(), SLOT(flushRequests()));
    QObject::connect(dispatcher, SIGNAL(awake()), mDisplay.data(), SLOT(flushRequests()));

    int fd = wl_display_get_fd(mDisplay->wl_display());
    QSocketNotifier *sn = new QSocketNotifier(fd, QSocketNotifier::Read, mDisplay.data());
    QObject::connect(sn, SIGNAL(activated(int)), mDisplay.data(), SLOT(flushRequests()));

    if (mDisplay->screens().isEmpty()) {
        qWarning() << "Running on a compositor with no screens is not supported";
        ::exit(EXIT_FAILURE);
    }
}

QPlatformFontDatabase *QWaylandIntegration::fontDatabase() const
{
    return mFontDb.data();
}

#if QT_CONFIG(draganddrop)
QPlatformClipboard *QWaylandIntegration::clipboard() const
{
    return mClipboard.data();
}

QPlatformDrag *QWaylandIntegration::drag() const
{
    return mDrag.data();
}
#endif  // draganddrop

QPlatformInputContext *QWaylandIntegration::inputContext() const
{
    return mInputContext.data();
}

QVariant QWaylandIntegration::styleHint(StyleHint hint) const
{
    if (hint == ShowIsFullScreen && mDisplay->windowManagerIntegration())
        return mDisplay->windowManagerIntegration()->showIsFullScreen();

    switch (hint) {
    case QPlatformIntegration::FontSmoothingGamma:
        return qreal(1.0);
    default:
        break;
    }

    return QPlatformIntegration::styleHint(hint);
}

#if QT_CONFIG(accessibility)
QPlatformAccessibility *QWaylandIntegration::accessibility() const
{
    return mAccessibility.data();
}
#endif

QPlatformServices *QWaylandIntegration::services() const
{
    return mDisplay->windowManagerIntegration();
}

QWaylandDisplay *QWaylandIntegration::display() const
{
    return mDisplay.data();
}

QStringList QWaylandIntegration::themeNames() const
{
    return GenericWaylandTheme::themeNames();
}

QPlatformTheme *QWaylandIntegration::createPlatformTheme(const QString &name) const
{
    return GenericWaylandTheme::createUnixTheme(name);
}

QWaylandClientBufferIntegration *QWaylandIntegration::clientBufferIntegration() const
{
    if (!mClientBufferIntegrationInitialized)
        const_cast<QWaylandIntegration *>(this)->initializeClientBufferIntegration();

    return mClientBufferIntegration && mClientBufferIntegration->isValid() ? mClientBufferIntegration.data() : nullptr;
}

QWaylandServerBufferIntegration *QWaylandIntegration::serverBufferIntegration() const
{
    if (!mServerBufferIntegrationInitialized)
        const_cast<QWaylandIntegration *>(this)->initializeServerBufferIntegration();

    return mServerBufferIntegration.data();
}

QWaylandShellIntegration *QWaylandIntegration::shellIntegration() const
{
    if (!mShellIntegrationInitialized)
        const_cast<QWaylandIntegration *>(this)->initializeShellIntegration();

    return mShellIntegration.data();
}

void QWaylandIntegration::initializeClientBufferIntegration()
{
    mClientBufferIntegrationInitialized = true;

    QString targetKey;
    bool disableHardwareIntegration = qEnvironmentVariableIsSet("QT_WAYLAND_DISABLE_HW_INTEGRATION");
    disableHardwareIntegration = disableHardwareIntegration || !mDisplay->hardwareIntegration();
    if (disableHardwareIntegration) {
        QByteArray clientBufferIntegrationName = qgetenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION");
        if (clientBufferIntegrationName.isEmpty())
            clientBufferIntegrationName = QByteArrayLiteral("wayland-egl");
        targetKey = QString::fromLocal8Bit(clientBufferIntegrationName);
    } else {
        targetKey = mDisplay->hardwareIntegration()->clientBufferIntegration();
    }

    if (targetKey.isEmpty()) {
        qWarning("Failed to determine what client buffer integration to use");
        return;
    }

    QStringList keys = QWaylandClientBufferIntegrationFactory::keys();
    if (keys.contains(targetKey)) {
        mClientBufferIntegration.reset(QWaylandClientBufferIntegrationFactory::create(targetKey, QStringList()));
    }
    if (mClientBufferIntegration)
        mClientBufferIntegration->initialize(mDisplay.data());
    else
        qWarning("Failed to load client buffer integration: %s\n", qPrintable(targetKey));
}

void QWaylandIntegration::initializeServerBufferIntegration()
{
    mServerBufferIntegrationInitialized = true;

    QString targetKey;

    bool disableHardwareIntegration = qEnvironmentVariableIsSet("QT_WAYLAND_DISABLE_HW_INTEGRATION");
    disableHardwareIntegration = disableHardwareIntegration || !mDisplay->hardwareIntegration();
    if (disableHardwareIntegration) {
        QByteArray serverBufferIntegrationName = qgetenv("QT_WAYLAND_SERVER_BUFFER_INTEGRATION");
        QString targetKey = QString::fromLocal8Bit(serverBufferIntegrationName);
    } else {
        targetKey = mDisplay->hardwareIntegration()->serverBufferIntegration();
    }

    if (targetKey.isEmpty()) {
        qWarning("Failed to determine what server buffer integration to use");
        return;
    }

    QStringList keys = QWaylandServerBufferIntegrationFactory::keys();
    if (keys.contains(targetKey)) {
        mServerBufferIntegration.reset(QWaylandServerBufferIntegrationFactory::create(targetKey, QStringList()));
    }
    if (mServerBufferIntegration)
        mServerBufferIntegration->initialize(mDisplay.data());
    else
        qWarning("Failed to load server buffer integration %s\n", qPrintable(targetKey));
}

void QWaylandIntegration::initializeShellIntegration()
{
    mShellIntegrationInitialized = true;

    QByteArray integrationName = qgetenv("QT_WAYLAND_SHELL_INTEGRATION");
    QString targetKey = QString::fromLocal8Bit(integrationName);

    if (!targetKey.isEmpty()) {
        QStringList keys = QWaylandShellIntegrationFactory::keys();
        if (keys.contains(targetKey)) {
            qDebug("Using the '%s' shell integration", qPrintable(targetKey));
            mShellIntegration.reset(QWaylandShellIntegrationFactory::create(targetKey, QStringList()));
        }
    } else {
        QStringList preferredShells;
        if (qEnvironmentVariableIsSet("QT_WAYLAND_USE_XDG_SHELL"))
            preferredShells << QLatin1String("xdg_shell");
        preferredShells << QLatin1String("wl_shell");

        Q_FOREACH (QString preferredShell, preferredShells) {
            if (mDisplay->hasRegistryGlobal(preferredShell)) {
                mShellIntegration.reset(createShellIntegration(preferredShell));
                break;
            }
        }
    }

    if (!mShellIntegration || !mShellIntegration->initialize(mDisplay.data())) {
        mShellIntegration.reset();
        qWarning("Failed to load shell integration %s", qPrintable(targetKey));
    }
}

QWaylandInputDevice *QWaylandIntegration::createInputDevice(QWaylandDisplay *display, int version, uint32_t id)
{
    if (mInputDeviceIntegration) {
        return mInputDeviceIntegration->createInputDevice(display, version, id);
    }
    return new QWaylandInputDevice(display, version, id);
}

void QWaylandIntegration::initializeInputDeviceIntegration()
{
    QByteArray integrationName = qgetenv("QT_WAYLAND_INPUTDEVICE_INTEGRATION");
    QString targetKey = QString::fromLocal8Bit(integrationName);

    if (targetKey.isEmpty()) {
        return;
    }

    QStringList keys = QWaylandInputDeviceIntegrationFactory::keys();
    if (keys.contains(targetKey)) {
        mInputDeviceIntegration.reset(QWaylandInputDeviceIntegrationFactory::create(targetKey, QStringList()));
        qDebug("Using the '%s' input device integration", qPrintable(targetKey));
    } else {
        qWarning("Wayland inputdevice integration '%s' not found, using default", qPrintable(targetKey));
    }
}

QWaylandShellIntegration *QWaylandIntegration::createShellIntegration(const QString &interfaceName)
{
    if (interfaceName == QLatin1Literal("wl_shell")) {
        return new QWaylandWlShellIntegration(mDisplay.data());
    } else if (interfaceName == QLatin1Literal("xdg_shell")) {
        return new QWaylandXdgShellIntegration(mDisplay.data());
    } else {
        return Q_NULLPTR;
    }
}

}

QT_END_NAMESPACE
