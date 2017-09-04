/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Richard Moore <rich@kde.org>
** Copyright (C) 2016 David Faure <david.faure@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

//
// An implementation of QX11Info for Qt5. This code only provides the
// static methods of the QX11Info, not the methods for getting information
// about particular widgets or pixmaps.
//

#include "qx11info_x11.h"

#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformwindow.h>
#include <QtPlatformHeaders/qxcbscreenfunctions.h>
#include <qscreen.h>
#include <qwindow.h>
#include <qguiapplication.h>
#include <xcb/xcb.h>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

static QScreen *findScreenForVirtualDesktop(int virtualDesktopNumber)
{
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (QXcbScreenFunctions::virtualDesktopNumber(screen) == virtualDesktopNumber)
            return screen;
    }
    return Q_NULLPTR;
}

/*!
    \class QX11Info
    \inmodule QtX11Extras
    \since 5.1
    \brief Provides information about the X display configuration.

    The class provides two APIs: a set of non-static functions that
    provide information about a specific widget or pixmap, and a set
    of static functions that provide the default information for the
    application.

    \warning This class is only available on X11. For querying
    per-screen information in a portable way, use QDesktopWidget.
*/

/*!
    Constructs an empty QX11Info object.
*/
QX11Info::QX11Info()
{
}

/*!
    Returns true if the application is currently running on X11.

    \since 5.2
 */
bool QX11Info::isPlatformX11()
{
    return QGuiApplication::platformName() == QLatin1String("xcb");
}

/*!
    Returns the horizontal resolution of the given \a screen in terms of the
    number of dots per inch.

    The \a screen argument is an X screen number. Be aware that if
    the user's system uses Xinerama (as opposed to traditional X11
    multiscreen), there is only one X screen. Use QDesktopWidget to
    query for information about Xinerama screens.

    \sa appDpiY()
*/
int QX11Info::appDpiX(int screen)
{
    if (screen == -1) {
        const QScreen *scr = QGuiApplication::primaryScreen();
        if (!scr)
            return 75;
        return qRound(scr->logicalDotsPerInchX());
    }

    QScreen *scr = findScreenForVirtualDesktop(screen);
    if (!scr)
        return 0;

    return scr->logicalDotsPerInchX();
}

/*!
    Returns the vertical resolution of the given \a screen in terms of the
    number of dots per inch.

    The \a screen argument is an X screen number. Be aware that if
    the user's system uses Xinerama (as opposed to traditional X11
    multiscreen), there is only one X screen. Use QDesktopWidget to
    query for information about Xinerama screens.

    \sa appDpiX()
*/
int QX11Info::appDpiY(int screen)
{
    if (screen == -1) {
        const QScreen *scr = QGuiApplication::primaryScreen();
        if (!scr)
            return 75;
        return qRound(scr->logicalDotsPerInchY());
    }

    QScreen *scr = findScreenForVirtualDesktop(screen);
    if (!scr)
        return 0;

    return scr->logicalDotsPerInchY();
}

/*!
    Returns a handle for the applications root window on the given \a screen.

    The \a screen argument is an X screen number. Be aware that if
    the user's system uses Xinerama (as opposed to traditional X11
    multiscreen), there is only one X screen. Use QDesktopWidget to
    query for information about Xinerama screens.

    \sa QApplication::desktop()
*/
unsigned long QX11Info::appRootWindow(int screen)
{
    if (!qApp)
        return 0;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return 0;
    QScreen *scr = screen == -1 ?  QGuiApplication::primaryScreen() : findScreenForVirtualDesktop(screen);
    if (!scr)
        return 0;
    return static_cast<xcb_window_t>(reinterpret_cast<quintptr>(native->nativeResourceForScreen(QByteArrayLiteral("rootwindow"), scr)));
}

/*!
    Returns the number of the screen where the application is being
    displayed.

    This method refers to screens in the original X11 meaning with a
    different DISPLAY environment variable per screen.
    This information is only useful if your application needs to know
    on which X screen it is running.

    In a typical multi-head configuration, multiple physical monitors
    are combined in one X11 screen. This means this method returns the
    same number for each of the physical monitors. In such a setup you
    are interested in the monitor information as provided by the X11
    RandR extension. This is available through QDesktopWidget and QScreen.

    \sa display()
*/
int QX11Info::appScreen()
{
    if (!qApp)
        return 0;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return 0;
    return reinterpret_cast<qintptr>(native->nativeResourceForIntegration(QByteArrayLiteral("x11screen")));
}

/*!
    Returns the X11 time.

    \sa setAppTime(), appUserTime()
*/
unsigned long QX11Info::appTime()
{
    if (!qApp)
        return 0;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return 0;
    QScreen* screen = QGuiApplication::primaryScreen();
    return static_cast<xcb_timestamp_t>(reinterpret_cast<quintptr>(native->nativeResourceForScreen("apptime", screen)));
}

/*!
    Returns the X11 user time.

    \sa setAppUserTime(), appTime()
*/
unsigned long QX11Info::appUserTime()
{
    if (!qApp)
        return 0;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return 0;
    QScreen* screen = QGuiApplication::primaryScreen();
    return static_cast<xcb_timestamp_t>(reinterpret_cast<quintptr>(native->nativeResourceForScreen("appusertime", screen)));
}

/*!
    Sets the X11 time to the value specified by \a time.

    \sa appTime(), setAppUserTime()
*/
void QX11Info::setAppTime(unsigned long time)
{
    if (!qApp)
        return;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return;
    typedef void (*SetAppTimeFunc)(QScreen *, xcb_timestamp_t);
    QScreen* screen = QGuiApplication::primaryScreen();
    SetAppTimeFunc func = reinterpret_cast<SetAppTimeFunc>(native->nativeResourceFunctionForScreen("setapptime"));
    if (func)
        func(screen, time);
    else
        qWarning("Internal error: QPA plugin doesn't implement setAppTime");
}

/*!
    Sets the X11 user time as specified by \a time.

    \sa appUserTime(), setAppTime()
*/
void QX11Info::setAppUserTime(unsigned long time)
{
    if (!qApp)
        return;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return;
    typedef void (*SetAppUserTimeFunc)(QScreen *, xcb_timestamp_t);
    QScreen* screen = QGuiApplication::primaryScreen();
    SetAppUserTimeFunc func = reinterpret_cast<SetAppUserTimeFunc>(native->nativeResourceFunctionForScreen("setappusertime"));
    if (func)
        func(screen, time);
    else
        qWarning("Internal error: QPA plugin doesn't implement setAppUserTime");
}

/*!
    Fetches the current X11 time stamp from the X Server.

    This method creates a property notify event and blocks till it is
    received back from the X Server.

    \since 5.2
*/
unsigned long QX11Info::getTimestamp()
{
    if (!qApp)
        return 0;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return 0;
    QScreen* screen = QGuiApplication::primaryScreen();
    return static_cast<xcb_timestamp_t>(reinterpret_cast<quintptr>(native->nativeResourceForScreen("gettimestamp", screen)));
}

/*!
    Returns the startup ID that will be used for the next window to be shown by this process.

    After the next window is shown, the next startup ID will be empty.

    http://standards.freedesktop.org/startup-notification-spec/startup-notification-latest.txt

    \since 5.4
    \sa setNextStartupId()
*/
QByteArray QX11Info::nextStartupId()
{
    if (!qApp)
        return 0;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return 0;
    return static_cast<char *>(native->nativeResourceForIntegration("startupid"));
}

/*!
    Sets the next startup ID to \a id.

    This is the startup ID that will be used for the next window to be shown by this process.

    The startup ID of the first window comes from the environment variable DESKTOP_STARTUP_ID.
    This method is useful for subsequent windows, when the request comes from another process
    (e.g. via DBus).

    \since 5.4
    \sa nextStartupId()
*/
void QX11Info::setNextStartupId(const QByteArray &id)
{
    if (!qApp)
        return;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return;
    typedef void (*SetStartupIdFunc)(const char*);
    SetStartupIdFunc func = reinterpret_cast<SetStartupIdFunc>(native->nativeResourceFunctionForIntegration("setstartupid"));
    if (func)
        func(id.constData());
    else
        qWarning("Internal error: QPA plugin doesn't implement setStartupId");
}

/*!
    Returns the default display for the application.

    \sa appScreen()
*/
Display *QX11Info::display()
{
    if (!qApp)
        return NULL;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return NULL;

    void *display = native->nativeResourceForIntegration(QByteArray("display"));
    return reinterpret_cast<Display *>(display);
}

/*!
    Returns the default XCB connection for the application.

    \sa display()
*/
xcb_connection_t *QX11Info::connection()
{
    if (!qApp)
        return NULL;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return NULL;

    void *connection = native->nativeResourceForIntegration(QByteArray("connection"));
    return reinterpret_cast<xcb_connection_t *>(connection);
}

/*!
    \since 5.7

    Returns true if there is a compositing manager running for the connection
    attached to \a screen.

    If \a screen equals -1, the application's primary screen is used.
*/
bool QX11Info::isCompositingManagerRunning(int screen)
{
    if (!qApp)
        return false;
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native)
        return false;

    QScreen *scr = screen == -1 ?  QGuiApplication::primaryScreen() : findScreenForVirtualDesktop(screen);
    if (!scr) {
        qWarning() << "isCompositingManagerRunning: Could not find screen number" << screen;
        return false;
    }

    return native->nativeResourceForScreen(QByteArray("compositingEnabled"), scr);
}

QT_END_NAMESPACE
