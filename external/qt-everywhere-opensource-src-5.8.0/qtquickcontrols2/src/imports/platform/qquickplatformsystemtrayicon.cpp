/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Platform module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickplatformsystemtrayicon_p.h"
#include "qquickplatformmenu_p.h"
#include "qquickplatformiconloader_p.h"

#include <QtCore/qloggingcategory.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtGui/private/qguiapplication_p.h>

#include "widgets/qwidgetplatform_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype SystemTrayIcon
    \inherits QtObject
    \instantiates QQuickPlatformSystemTrayIcon
    \inqmlmodule Qt.labs.platform
    \since 5.8
    \brief A system tray icon.

    The SystemTrayIcon type provides an icon for an application in the system tray.

    Many desktop platforms provide a special system tray or notification area,
    where applications can display icons and notification messages.

    \image qtlabsplatform-systemtrayicon.png

    The following example shows how to create a system tray icon, and how to make
    use of the \l activated() signal:

    \code
    SystemTrayIcon {
        visible: true
        iconSource: "qrc:/images/tray-icon.png"

        onActivated: {
            window.show()
            window.raise()
            window.requestActivate()
        }
    }
    \endcode

    \section2 Tray menu

    SystemTrayIcon can have a menu that opens when the icon is activated.

    \image qtlabsplatform-systemtrayicon-menu.png

    The following example illustrates how to assign a \l Menu to a system tray icon:

    \code
    SystemTrayIcon {
        visible: true
        iconSource: "qrc:/images/tray-icon.png"

        menu: Menu {
            MenuItem {
                text: qsTr("Quit")
                onActivated: Qt.quit()
            }
        }
    }
    \endcode

    \section2 Notification messages

    SystemTrayIcon can display notification messages.

    \image qtlabsplatform-systemtrayicon-message.png

    The following example presents how to show a notification message using
    \l showMessage(), and how to make use of the \l messageClicked() signal:

    \code
    SystemTrayIcon {
        visible: true
        iconSource: "qrc:/images/tray-icon.png"

        onMessageClicked: console.log("Message clicked")
        Component.onCompleted: showMessage("Message title", "Something important came up. Click this to know more.")
    }
    \endcode

    \section2 Availability

    A native system tray icon is currently \l available on the following platforms:

    \list
    \li All window managers and independent tray implementations for X11 that implement the
        \l{http://standards.freedesktop.org/systemtray-spec/systemtray-spec-0.2.html}
        {freedesktop.org XEmbed system tray specification}.
    \li All desktop environments that implement the
        \l{http://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/StatusNotifierItem}
        {freedesktop.org D-Bus StatusNotifierItem specification}, including recent versions of KDE and Unity.
    \li All supported versions of macOS. Note that the Growl notification system must be installed
        for showMessage() to display messages on OS X prior to 10.8 (Mountain Lion).
    \endlist

    \input includes/widgets.qdocinc 1

    \labs

    \sa Menu
*/

/*!
    \qmlsignal Qt.labs.platform::SystemTrayIcon::activated(ActivationReason reason)

    This signal is emitted when the system tray icon is activated by the user. The
    \a reason argument specifies how the system tray icon was activated.

    Available reasons:

    \value SystemTrayIcon.Unknown     Unknown reason
    \value SystemTrayIcon.Context     The context menu for the system tray icon was requested
    \value SystemTrayIcon.DoubleClick The system tray icon was double clicked
    \value SystemTrayIcon.Trigger     The system tray icon was clicked
    \value SystemTrayIcon.MiddleClick The system tray icon was clicked with the middle mouse button
*/

/*!
    \qmlsignal Qt.labs.platform::SystemTrayIcon::messageClicked()

    This signal is emitted when a notification message is clicked by the user.

    \sa showMessage()
*/

Q_DECLARE_LOGGING_CATEGORY(qtLabsPlatformTray)

QQuickPlatformSystemTrayIcon::QQuickPlatformSystemTrayIcon(QObject *parent)
    : QObject(parent),
      m_complete(false),
      m_visible(false),
      m_menu(nullptr),
      m_iconLoader(nullptr),
      m_handle(nullptr)
{
    m_handle = QGuiApplicationPrivate::platformTheme()->createPlatformSystemTrayIcon();
    if (!m_handle)
        m_handle = QWidgetPlatform::createSystemTrayIcon(this);
    qCDebug(qtLabsPlatformTray) << "SystemTrayIcon ->" << m_handle;

    if (m_handle) {
        connect(m_handle, &QPlatformSystemTrayIcon::activated, this, &QQuickPlatformSystemTrayIcon::activated);
        connect(m_handle, &QPlatformSystemTrayIcon::messageClicked, this, &QQuickPlatformSystemTrayIcon::messageClicked);
    }
}

QQuickPlatformSystemTrayIcon::~QQuickPlatformSystemTrayIcon()
{
    if (m_menu)
        m_menu->setSystemTrayIcon(nullptr);
    cleanup();
    delete m_iconLoader;
    m_iconLoader = nullptr;
    delete m_handle;
    m_handle = nullptr;
}

QPlatformSystemTrayIcon *QQuickPlatformSystemTrayIcon::handle() const
{
    return m_handle;
}

/*!
    \readonly
    \qmlproperty bool Qt.labs.platform::SystemTrayIcon::available

    This property holds whether the system tray is available.
*/
bool QQuickPlatformSystemTrayIcon::isAvailable() const
{
    return m_handle && m_handle->isSystemTrayAvailable();
}

/*!
    \readonly
    \qmlproperty bool Qt.labs.platform::SystemTrayIcon::supportsMessages

    This property holds whether the system tray icon supports notification messages.

    \sa showMessage()
*/
bool QQuickPlatformSystemTrayIcon::supportsMessages() const
{
    return m_handle && m_handle->supportsMessages();
}

/*!
    \qmlproperty bool Qt.labs.platform::SystemTrayIcon::visible

    This property holds whether the system tray icon is visible.

    The default value is \c false.
*/
bool QQuickPlatformSystemTrayIcon::isVisible() const
{
    return m_visible;
}

void QQuickPlatformSystemTrayIcon::setVisible(bool visible)
{
    if (m_visible == visible)
        return;

    if (m_handle && m_complete) {
        if (visible)
            init();
        else
            cleanup();
    }

    m_visible = visible;
    emit visibleChanged();
}

/*!
    \qmlproperty url Qt.labs.platform::SystemTrayIcon::iconSource

    This property holds the url of the system tray icon.

    \sa iconName
*/
QUrl QQuickPlatformSystemTrayIcon::iconSource() const
{
    if (!m_iconLoader)
        return QUrl();

    return m_iconLoader->iconSource();
}

void QQuickPlatformSystemTrayIcon::setIconSource(const QUrl& source)
{
    if (source == iconSource())
        return;

    iconLoader()->setIconSource(source);
    emit iconSourceChanged();
}

/*!
    \qmlproperty string Qt.labs.platform::SystemTrayIcon::iconName

    This property holds the theme name of the system tray icon.

    \sa iconSource, QIcon::fromTheme()
*/
QString QQuickPlatformSystemTrayIcon::iconName() const
{
    if (!m_iconLoader)
        return QString();

    return m_iconLoader->iconName();
}

void QQuickPlatformSystemTrayIcon::setIconName(const QString& name)
{
    if (name == iconName())
        return;

    iconLoader()->setIconName(name);
    emit iconNameChanged();
}

/*!
    \qmlproperty string Qt.labs.platform::SystemTrayIcon::tooltip

    This property holds the tooltip of the system tray icon.
*/
QString QQuickPlatformSystemTrayIcon::tooltip() const
{
    return m_tooltip;
}

void QQuickPlatformSystemTrayIcon::setTooltip(const QString &tooltip)
{
    if (m_tooltip == tooltip)
        return;

    if (m_handle && m_complete)
        m_handle->updateToolTip(tooltip);

    m_tooltip = tooltip;
    emit tooltipChanged();
}

/*!
    \qmlproperty Menu Qt.labs.platform::SystemTrayIcon::menu

    This property holds a menu for the system tray icon.
*/
QQuickPlatformMenu *QQuickPlatformSystemTrayIcon::menu() const
{
    return m_menu;
}

void QQuickPlatformSystemTrayIcon::setMenu(QQuickPlatformMenu *menu)
{
    if (m_menu == menu)
        return;

    if (m_menu)
        m_menu->setSystemTrayIcon(nullptr);
    if (menu) {
        menu->setSystemTrayIcon(this);
        if (m_handle && m_complete && menu->create())
            m_handle->updateMenu(menu->handle());
    }

    m_menu = menu;
    emit menuChanged();
}

/*!
    \qmlmethod void Qt.labs.platform::SystemTrayIcon::show()

    Shows the system tray icon.
*/
void QQuickPlatformSystemTrayIcon::show()
{
    setVisible(true);
}

/*!
    \qmlmethod void Qt.labs.platform::SystemTrayIcon::hide()

    Hides the system tray icon.
*/
void QQuickPlatformSystemTrayIcon::hide()
{
    setVisible(false);
}

/*!
    \qmlmethod void Qt.labs.platform::SystemTrayIcon::showMessage(string title, string message, MessageIcon icon, int msecs)

    Shows a system tray message with the given \a title, \a message and \a icon
    for the time specified in \a msecs.

    \note System tray messages are dependent on the system configuration and user preferences,
    and may not appear at all. Therefore, it should not be relied upon as the sole means for providing
    critical information.

    \sa supportsMessages, messageClicked()
*/
void QQuickPlatformSystemTrayIcon::showMessage(const QString &title, const QString &msg, QPlatformSystemTrayIcon::MessageIcon icon, int msecs)
{
    if (m_handle)
        m_handle->showMessage(title, msg, QIcon(), icon, msecs);
}

void QQuickPlatformSystemTrayIcon::init()
{
    if (!m_handle)
        return;

    m_handle->init();
    if (m_menu && m_menu->create())
        m_handle->updateMenu(m_menu->handle());
    m_handle->updateToolTip(m_tooltip);
    if (m_iconLoader)
        m_iconLoader->setEnabled(true);
}

void QQuickPlatformSystemTrayIcon::cleanup()
{
    if (m_handle)
        m_handle->cleanup();
    if (m_iconLoader)
        m_iconLoader->setEnabled(false);
}

void QQuickPlatformSystemTrayIcon::classBegin()
{
}

void QQuickPlatformSystemTrayIcon::componentComplete()
{
    m_complete = true;
    if (m_visible)
        init();
}

QQuickPlatformIconLoader *QQuickPlatformSystemTrayIcon::iconLoader() const
{
    if (!m_iconLoader) {
        QQuickPlatformSystemTrayIcon *that = const_cast<QQuickPlatformSystemTrayIcon *>(this);
        static int slot = staticMetaObject.indexOfSlot("updateIcon()");
        m_iconLoader = new QQuickPlatformIconLoader(slot, that);
        m_iconLoader->setEnabled(m_complete);
    }
    return m_iconLoader;
}

void QQuickPlatformSystemTrayIcon::updateIcon()
{
    if (!m_handle || !m_iconLoader)
        return;

    m_handle->updateIcon(m_iconLoader->icon());
}

QT_END_NAMESPACE
