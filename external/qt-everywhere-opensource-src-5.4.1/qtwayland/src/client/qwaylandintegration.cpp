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

#include "qwaylandintegration_p.h"

#include "qwaylanddisplay_p.h"
#include "qwaylandshmwindow_p.h"
#include "qwaylandinputcontext_p.h"
#include "qwaylandshmbackingstore_p.h"
#include "qwaylandnativeinterface_p.h"
#include "qwaylandclipboard_p.h"
#include "qwaylanddnd_p.h"
#include "qwaylandwindowmanagerintegration_p.h"
#include "qwaylandscreen_p.h"

#include "QtPlatformSupport/private/qgenericunixfontdatabase_p.h"
#include <QtPlatformSupport/private/qgenericunixeventdispatcher_p.h>
#include <QtPlatformSupport/private/qgenericunixthemes_p.h>

#include <QtGui/private/qguiapplication_p.h>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformcursor.h>
#include <QtGui/QSurfaceFormat>
#include <QtGui/QOpenGLContext>

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

QT_BEGIN_NAMESPACE

class GenericWaylandTheme: public QGenericUnixTheme
{
public:
    static QStringList themeNames()
    {
        QStringList result;

        if (QGuiApplication::desktopSettingsAware()) {
            const QByteArray desktopEnvironment = QGuiApplicationPrivate::platformIntegration()->services()->desktopEnvironment();

            if (desktopEnvironment == QByteArrayLiteral("KDE")) {
#ifndef QT_NO_SETTINGS
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
    , mShellIntegration(Q_NULLPTR)
    , mFontDb(new QGenericUnixFontDatabase())
    , mNativeInterface(new QWaylandNativeInterface(this))
#ifndef QT_NO_ACCESSIBILITY
    , mAccessibility(new QPlatformAccessibility())
#else
    , mAccessibility(0)
#endif
    , mClientBufferIntegrationInitialized(false)
    , mServerBufferIntegrationInitialized(false)
    , mShellIntegrationInitialized(false)
{
    mDisplay = new QWaylandDisplay(this);
    mClipboard = new QWaylandClipboard(mDisplay);
    mDrag = new QWaylandDrag(mDisplay);

    mInputContext.reset(new QWaylandInputContext(mDisplay));
}

QWaylandIntegration::~QWaylandIntegration()
{
    delete mDrag;
    delete mClipboard;
#ifndef QT_NO_ACCESSIBILITY
    delete mAccessibility;
#endif
    delete mNativeInterface;
    delete mDisplay;
}

QPlatformNativeInterface * QWaylandIntegration::nativeInterface() const
{
    return mNativeInterface;
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

#ifndef QT_NO_OPENGL
QPlatformOpenGLContext *QWaylandIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    if (mDisplay->clientBufferIntegration())
        return mDisplay->clientBufferIntegration()->createPlatformOpenGLContext(context->format(), context->shareHandle());
    return 0;
}
#endif // QT_NO_OPENGL

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
    QObject::connect(dispatcher, SIGNAL(aboutToBlock()), mDisplay, SLOT(flushRequests()));
    QObject::connect(dispatcher, SIGNAL(awake()), mDisplay, SLOT(flushRequests()));
}

QPlatformFontDatabase *QWaylandIntegration::fontDatabase() const
{
    return mFontDb;
}

QPlatformClipboard *QWaylandIntegration::clipboard() const
{
    return mClipboard;
}

QPlatformDrag *QWaylandIntegration::drag() const
{
    return mDrag;
}

QPlatformInputContext *QWaylandIntegration::inputContext() const
{
    return mInputContext.data();
}

QVariant QWaylandIntegration::styleHint(StyleHint hint) const
{
    if (hint == ShowIsFullScreen && mDisplay->windowManagerIntegration())
        return mDisplay->windowManagerIntegration()->showIsFullScreen();

    return QPlatformIntegration::styleHint(hint);
}

QPlatformAccessibility *QWaylandIntegration::accessibility() const
{
    return mAccessibility;
}

QPlatformServices *QWaylandIntegration::services() const
{
    return mDisplay->windowManagerIntegration();
}

QWaylandDisplay *QWaylandIntegration::display() const
{
    return mDisplay;
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

    return mClientBufferIntegration && mClientBufferIntegration->isValid() ? mClientBufferIntegration : 0;
}

QWaylandServerBufferIntegration *QWaylandIntegration::serverBufferIntegration() const
{
    if (!mServerBufferIntegrationInitialized)
        const_cast<QWaylandIntegration *>(this)->initializeServerBufferIntegration();

    return mServerBufferIntegration;
}

QWaylandShellIntegration *QWaylandIntegration::shellIntegration() const
{
    if (!mShellIntegrationInitialized)
        const_cast<QWaylandIntegration *>(this)->initializeShellIntegration();

    return mShellIntegration;
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
        mClientBufferIntegration = QWaylandClientBufferIntegrationFactory::create(targetKey, QStringList());
    }
    if (mClientBufferIntegration)
        mClientBufferIntegration->initialize(mDisplay);
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
        mServerBufferIntegration = QWaylandServerBufferIntegrationFactory::create(targetKey, QStringList());
    }
    if (mServerBufferIntegration)
        mServerBufferIntegration->initialize(mDisplay);
    else
        qWarning("Failed to load server buffer integration %s\n", qPrintable(targetKey));
}

void QWaylandIntegration::initializeShellIntegration()
{
    mShellIntegrationInitialized = true;

    QByteArray integrationName = qgetenv("QT_WAYLAND_SHELL_INTEGRATION");
    QString targetKey = QString::fromLocal8Bit(integrationName);

    if (targetKey.isEmpty()) {
        return;
    }

    QStringList keys = QWaylandShellIntegrationFactory::keys();
    if (keys.contains(targetKey)) {
        mShellIntegration = QWaylandShellIntegrationFactory::create(targetKey, QStringList());
    }
    if (mShellIntegration && mShellIntegration->initialize(mDisplay)) {
        qDebug("Using the '%s' shell integration", qPrintable(targetKey));
    } else {
        delete mShellIntegration;
        mShellIntegration = Q_NULLPTR;
        qWarning("Failed to load shell integration %s", qPrintable(targetKey));
    }
}

QT_END_NAMESPACE
